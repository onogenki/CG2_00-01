#include "TitleScene.h"
#include "Camera.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "Object3dCommon.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "SceneRenderPipeline.h"
#include "SkyBox.h"
#include "TextureManager.h"
#include "Object3dRenderContext.h"
#include "PostEffect.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "SceneManager.h"
#include "Logger.h"
#include <dinput.h>
#include <cmath>
using namespace MyMath;

// TitleSceneが所有する前方宣言型を、完全な型を読み込んだ場所で生成します。
TitleScene::TitleScene() = default;

// TitleSceneが所有する前方宣言型を、完全な型を読み込んだ場所で解放します。
TitleScene::~TitleScene() = default;

// TitleEditorへManager所有の一覧を貸し、生成・削除操作も同じManagerへ渡します。
TitleEditor::Context TitleScene::MakeTitleEditorContext()
{
	TitleEditor::Context context{};
	context.camera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	context.normalObjects = &titleObjects_.GetNormalObjects();
	context.animationObjects = &titleObjects_.GetAnimationObjects();
	context.sprites = &titleObjects_.GetSprites();
	context.directionalLight = &directionalLight_;
	context.pointLight = &pointLight_;
	context.spotLight = &spotLight_;
	context.baseNormalObjectCount = titleObjects_.GetBaseNormalObjectCount();
	context.baseAnimationObjectCount = titleObjects_.GetBaseAnimationObjectCount();
	context.baseSpriteCount = titleObjects_.GetBaseSpriteCount();
	context.addModel = [this](const std::string& fileName)
	{
		return titleObjects_.AddModel(object3dCommon, fileName);
	};
	context.addTexture = [this](const std::string& textureFilePath)
	{
		return titleObjects_.AddTexture(spriteCommon, textureFilePath);
	};
	context.clearAdded = [this]()
	{
		titleObjects_.ClearAdded();
	};
	return context;
}

// 必須素材がすべて読めた時だけ、Titleの更新・描画を有効にします。
void TitleScene::Initialize()
{
	isInitialized_ = false;
	isFinished_ = false;
	InitializeRenderSystems();
	InitializeDefaultLighting();
	if (!InitializeTitleObjects()) {
		Logger::Log("Title initialization failed: a required model or sprite texture could not be loaded.");
		return;
	}
	if (!InitializeSkyBoxAndEditorResources()) {
		Logger::Log("Title initialization failed: the skybox texture could not be loaded.");
		return;
	}
	isInitialized_ = true;
}

// DirectX・Camera・Object3d共通設定を、TitleScene用に初期化します。
void TitleScene::InitializeRenderSystems()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);

	// Title専用のCameraManagerを作り、ほかのSceneのCamera状態を持ち込まないようにします。
	InitializeMainCamera({ 0.0f, 0.0f, -10.0f });
	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());
}

// Title画面の背景モデルを照らす三種類のLightを、初期値へ設定します。
void TitleScene::InitializeDefaultLighting()
{
	directionalLight_.direction = { 1.0f, -1.0f, 1.0f };
	directionalLight_.intensity = 0.0f;
	directionalLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };

	pointLight_.position = { 0.0f, 2.0f, 0.0f };
	pointLight_.intensity = 1.0f;
	pointLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLight_.radius = 10.0f;
	pointLight_.decay = 1.0f;

	spotLight_.position = { 2.0f, 1.25f, 0.0f };
	spotLight_.intensity = 4.0f;
	spotLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	spotLight_.distance = 7.0f;
	// SpotLightの方向は長さが1である前提なので、必ず正規化します。
	spotLight_.direction = Normalize({ -1.0f, -1.0f, 0.0f });
	spotLight_.decay = 2.0f;
	spotLight_.cosAngle = std::cos(0.45f);
	spotLight_.cosFalloffStart = 1.0f;
}

// Titleの生成はManagerへ任せ、Sceneは成功後のUI選択だけを決めます。
bool TitleScene::InitializeTitleObjects()
{
	spriteCommon = SpriteCommon::GetInstance();
	if (!titleObjects_.Initialize(object3dCommon, spriteCommon, DirectXCommon::GetInstance())) {
		return false;
	}
	titleEditor_.SelectSprite(0);
	return true;
}

// Titleを選んだ時だけ、SkyBox TextureとEditor用Resource一覧をまとめて準備します。
bool TitleScene::InitializeSkyBoxAndEditorResources()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	// DDSが無いままSkyBoxを描くと、未登録SRVを参照してしまいます。
	if (!TextureManager::GetInstance()->LoadTexture("Resources/qwantani_moonrise_puresky_1k.dds")) {
		return false;
	}
	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(dxCommon, cameraManager->GetActiveCamera());
	skyBox_->SetTexture("Resources/qwantani_moonrise_puresky_1k.dds");
	// Releaseには編集UIがないため、素材棚の走査・サムネイル読込を行いません。
#ifdef USE_IMGUI
	titleEditor_.ScanResourceShelf();
#endif
	return true;
}

// 素材欠損時はScene切替入力だけを扱い、未生成の描画物を更新しません。
void TitleScene::Update()
{
	// Game Viewが操作対象かどうかと入力機器は、このフレームで一度だけ取得します。
	const bool isGameViewActive = ImGuiManager::GetInstance()->IsGameViewActive();
	if (!isInitialized_) {
		// 素材欠損時も入力で次Sceneへ進めます。未生成のSkyBox・Spriteには触れません。
		UpdateSceneTransition(isGameViewActive);
		return;
	}
	UpdateSceneContent();
	UpdateEditorUi(isGameViewActive);
	UpdateSceneTransition(isGameViewActive);
}

// Cameraを更新してから、RenderContextでManager所有の描画対象を準備します。
void TitleScene::UpdateSceneContent()
{
	// Cameraの更新は、モデルがCameraを参照する前に完了させます。
	cameraManager->Update();

	// 方向が0でもLight計算が壊れないよう、共通の安全な単位方向へそろえます。
	Object3dRenderContext::NormalizeDirectionalLight(directionalLight_);
	// Titleが所有するCamera・Lightをまとめ、全モデルへ同じ描画準備を行います。
	Object3dRenderContext renderContext(
		cameraManager->GetActiveCamera(),
		directionalLight_,
		pointLight_,
		spotLight_);
	renderContext.UpdateObjects(titleObjects_.GetNormalObjects());
	renderContext.UpdateObjects(titleObjects_.GetAnimationObjects());

	for (auto& sprite : titleObjects_.GetSprites()) {
		sprite->Update();
	}
	skyBox_->Update();
}


// 更新済みの描画対象をEditorへ貸し、最後にViewport操作を受け付けます。
void TitleScene::UpdateEditorUi(bool isGameViewActive)
{
	// Titleの編集UIは、3DモデルとSpriteを更新した後の状態を表示します。
	ImGuiManager::GetInstance()->Begin("Title");
	titleEditor_.Draw(MakeTitleEditorContext());
	if (isGameViewActive) {
		SceneEditor::UpdateViewportCamera(cameraManager ? cameraManager->GetActiveCamera() : nullptr);
	}
	ImGuiManager::GetInstance()->End();
}


// 入力先がGame Viewの時だけ、従来のキーで次のSceneを予約します。
void TitleScene::UpdateSceneTransition(bool isGameViewActive)
{
	// Game Viewが操作対象でない時は、Editor操作をScene切替入力として扱いません。
	if (!isGameViewActive) {
		return;
	}

	Input* input = Input::GetInstance();
	// SpaceまたはPadの1ボタンで、Debug Sceneを開きます。
	if (input->TriggerKey(DIK_SPACE) || input->IsPadButtonPressed(0, 1))
	{
		SceneManager::GetInstance()->ChangeScene("DEBUG");
	}

	// EnterまたはPadの3ボタンで、本編のStage1を開きます。
	if (input->TriggerKey(DIK_RETURN) || input->IsPadButtonPressed(0, 3))
	{
		SceneManager::GetInstance()->ChangeSceneWithLoading("STAGE1");
	}
}

// 初期化に失敗したFrameも共通Pipelineを閉じ、未登録Textureは描画しません。
void TitleScene::Draw()
{
	// 共通PipelineがRenderTextureへの描画開始を行い、Titleは描画対象だけを並べます。
	SceneRenderPipeline::Begin(DirectXCommon::GetInstance());
	if (!isInitialized_) {
		// 失敗はログへ記録済みです。未生成の描画物を使わず、このFrameを閉じます。
		SceneRenderPipeline::End(DirectXCommon::GetInstance(), nullptr, false);
		return;
	}

	SceneRenderPipeline::DrawObjects(object3dCommon, titleObjects_.GetNormalObjects());
	SceneRenderPipeline::DrawObjects(object3dCommon, titleObjects_.GetAnimationObjects());
	//skyBox描画
	if (skyBox_) {
		skyBox_->Draw();
	}

	SceneRenderPipeline::DrawSprites(spriteCommon, titleObjects_.GetSprites());

	// PostEffect・SwapChain・ImGui・Presentは、全Scene共通のPipelineへ任せます。
	SceneRenderPipeline::End(
		DirectXCommon::GetInstance(),
		cameraManager ? cameraManager->GetActiveCamera() : nullptr);
}

// 成功・途中失敗のどちらでも、Titleが所有した資源を同じ順番で解放します。
void TitleScene::Finalize()
{
	//GPUの完了待ち
	DirectXCommon::GetInstance()->WaitForGPU();
	titleObjects_.Finalize();
	skyBox_.reset();
	titleEditor_.Finalize();
	mainCamera.reset();
	cameraManager.reset();
	isFinished_ = false;
	isInitialized_ = false;

}
