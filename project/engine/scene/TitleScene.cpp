#include "TitleScene.h"
#include "Audio.h"
#include "Camera.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "Object3dCommon.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "SceneRenderPipeline.h"
#include "SkyBox.h"
#include "TextureManager.h"
#include "Object3dFactory.h"
#include "Object3dRenderContext.h"
#include "PostEffect.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "SceneManager.h"
#include <dinput.h>
#include <algorithm>
#include <cstddef>
#include <cmath>
using namespace MyMath;

// TitleSceneが所有する前方宣言型を、完全な型を読み込んだ場所で生成します。
TitleScene::TitleScene() = default;

// TitleSceneが所有する前方宣言型を、完全な型を読み込んだ場所で解放します。
TitleScene::~TitleScene() = default;

// Shelfから選ばれたモデルを作り、通常モデルまたはAnimationモデルの一覧へ追加します。
bool TitleScene::AddModelToTitle(const std::string& fileName)
{
	auto object = Object3dFactory::Create(object3dCommon, fileName, true);
	if (!object) {
		return false;
	}
	// Camera・LightはUpdateのObject3dRenderContextが全モデルへ一括設定します。
	// 生成時はTitle固有の位置・Animationだけを決めます。
	// 追加順にX方向へずらし、同じ場所にモデルが重なって見えない状態を防ぎます。
	const float offset = static_cast<float>(normalObjects_.size() + animationObjects_.size()) * 1.4f;
	object->SetTranslate({ -2.0f + offset, 0.0f, 6.0f });
	object->SetScale({ 1.0f, 1.0f, 1.0f });
	if (object->IsSkeletal()) {
		Object3dFactory::LoadAndPlayAnimation(*object, fileName);
		animationObjects_.push_back(std::move(object));
	} else {
		normalObjects_.push_back(std::move(object));
	}
	return true;
}

// Shelfから選ばれたTextureをSpriteにし、Title画面のグリッド位置へ追加します。
bool TitleScene::AddTextureToTitle(const std::string& textureFilePath)
{
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon, textureFilePath);
	sprite->SetAnchorPoint({ 0.5f, 0.5f });
	const Vector2 originalSize = sprite->GetSize();
	// 縦横比は保ったまま、長辺だけを180px以内へ縮小します。
	const float largestSide = (std::max)(originalSize.x, originalSize.y);
	if (largestSide > 180.0f && largestSide > 0.0f) {
		const float scale = 180.0f / largestSide;
		sprite->SetSize({ originalSize.x * scale, originalSize.y * scale });
	}
	//初期スプライトを数えず、追加したテクスチャだけを並べる
	// 追加分だけを0番から数え、4列ごとのグリッド位置へ並べます。
	const size_t addedSpriteIndex = addedSprites_.size() - baseSpriteCount_;
	const float x = 180.0f + static_cast<float>(addedSpriteIndex % 4) * 190.0f;
	const float y = 160.0f + static_cast<float>(addedSpriteIndex / 4) * 160.0f;
	sprite->SetPosition({ x, y });
	addedSprites_.push_back(std::move(sprite));
	return true;
}

// Model Shelfから追加した要素だけを消し、Title開始時の背景モデル・Spriteは残します。
void TitleScene::ClearAddedTitleObjects()
{
	if (normalObjects_.size() > baseNormalObjectCount_) {
		normalObjects_.resize(baseNormalObjectCount_);
	}
	if (animationObjects_.size() > baseAnimationObjectCount_) {
		animationObjects_.resize(baseAnimationObjectCount_);
	}
	if (addedSprites_.size() > baseSpriteCount_) {
		addedSprites_.resize(baseSpriteCount_);
	}
}

// TitleEditorへ、TitleSceneが所有する一覧とTitle固有の生成・削除ルールを渡します。
TitleEditor::Context TitleScene::MakeTitleEditorContext()
{
	TitleEditor::Context context{};
	context.camera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	context.normalObjects = &normalObjects_;
	context.animationObjects = &animationObjects_;
	context.sprites = &addedSprites_;
	context.directionalLight = &directionalLight_;
	context.pointLight = &pointLight_;
	context.spotLight = &spotLight_;
	context.baseNormalObjectCount = baseNormalObjectCount_;
	context.baseAnimationObjectCount = baseAnimationObjectCount_;
	context.baseSpriteCount = baseSpriteCount_;
	context.addModel = [this](const std::string& fileName)
	{
		return AddModelToTitle(fileName);
	};
	context.addTexture = [this](const std::string& textureFilePath)
	{
		return AddTextureToTitle(textureFilePath);
	};
	context.clearAdded = [this]()
	{
		ClearAddedTitleObjects();
	};
	return context;
}

void TitleScene::Initialize()
{
	InitializeRenderSystems();
	InitializeDefaultLighting();
	if (!InitializeTitleObjects()) {
		return;
	}
	InitializeSkyBoxAndAudio();
	titleEditor_.ScanResourceShelf();
	isFinished_ = false;
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

// Titleの初期3DモデルとSpriteを作り、Edit Viewで保護する件数を記録します。
bool TitleScene::InitializeTitleObjects()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	auto terrain = Object3dFactory::Create(object3dCommon, "plane.obj");
	if (!terrain) {
		return false;
	}
	// Camera・LightはUpdateのObject3dRenderContextが通常モデル一覧へ一括設定します。
	terrain->GetTransform().translate = { 1.0f, -2.0f, 10.0f };
	normalObjects_.push_back(std::move(terrain));
	baseNormalObjectCount_ = normalObjects_.size();
	baseAnimationObjectCount_ = animationObjects_.size();

	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(dxCommon);

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");

	//初期スプライトもInspectorで編集できる一覧へ入れる
	auto titleSprite = std::make_unique<Sprite>();
	titleSprite->Initialize(spriteCommon, "Resources/uvChecker.png");
	titleSprite->SetPosition({ 0.0f, 0.0f });
	addedSprites_.push_back(std::move(titleSprite));
	baseSpriteCount_ = addedSprites_.size();
	titleEditor_.SelectSprite(0);
	return true;
}

// SkyBoxとTitle開始時の音声を、TitleSceneが表示される一度だけ準備します。
void TitleScene::InitializeSkyBoxAndAudio()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	// SkyBoxの背景Textureです。
	TextureManager::GetInstance()->LoadTexture("Resources/qwantani_moonrise_puresky_1k.dds");
	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(dxCommon, cameraManager->GetActiveCamera());
	skyBox_->SetTexture("Resources/qwantani_moonrise_puresky_1k.dds");

	Audio::GetInstance()->LoadFile("Resources/Alarm01.wav");
	Audio::GetInstance()->PlayWave("Resources/Alarm01.wav");
}

void TitleScene::Update()
{
	// Game Viewが操作対象かどうかと入力機器は、このフレームで一度だけ取得します。
	const bool isGameViewActive = ImGuiManager::GetInstance()->IsGameViewActive();
	UpdateSceneContent();
	UpdateEditorUi(isGameViewActive);
	UpdateSceneTransition(isGameViewActive);
}

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
	renderContext.UpdateObjects(normalObjects_);
	renderContext.UpdateObjects(animationObjects_);

	for (auto& sprite : addedSprites_) {
		sprite->Update();
	}
	skyBox_->Update();
}


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
		SceneManager::GetInstance()->ChangeScene("STAGE1");
	}
}

void TitleScene::Draw()
{
	// 共通PipelineがRenderTextureへの描画開始を行い、Titleは描画対象だけを並べます。
	SceneRenderPipeline::Begin(DirectXCommon::GetInstance());

	SceneRenderPipeline::DrawObjects(object3dCommon, normalObjects_);
	SceneRenderPipeline::DrawObjects(object3dCommon, animationObjects_);
	//skyBox描画
	if (skyBox_) {
		skyBox_->Draw();
	}

	SceneRenderPipeline::DrawSprites(spriteCommon, addedSprites_);

	// PostEffect・SwapChain・ImGui・Presentは、全Scene共通のPipelineへ任せます。
	SceneRenderPipeline::End(
		DirectXCommon::GetInstance(),
		cameraManager ? cameraManager->GetActiveCamera() : nullptr);
}

void TitleScene::Finalize()
{
	//GPUの完了待ち
	DirectXCommon::GetInstance()->WaitForGPU();
	addedSprites_.clear();
	skyBox_.reset();
	normalObjects_.clear();
	animationObjects_.clear();
	titleEditor_.Finalize();
	baseNormalObjectCount_ = 0;
	baseAnimationObjectCount_ = 0;
	baseSpriteCount_ = 0;
	mainCamera.reset();
	cameraManager.reset();
	isFinished_ = false;

}
