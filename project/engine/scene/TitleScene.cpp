#include "TitleScene.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "PostEffect.h"
#include "ImGuiManager.h"
#include "Input.h"
#include"SceneManager.h"
#include <dinput.h>
#include <algorithm>
#include <cstddef>
#include <cmath>
using namespace MyMath;

void TitleScene::ScanResourceShelf()
{
	SceneEditor::ScanResourceShelf(shelfState_);
}

bool TitleScene::AddModelToTitle(const std::string& fileName)
{
	if (!ModelManager::GetInstance()->LoadModel(fileName)) {
		return false;
	}
	auto object = std::make_unique<Object3d>();
	object->Initialize(object3dCommon);
	object->SetModel(fileName);
	object->InitializeAnimation();
	object->SetCamera(cameraManager ? cameraManager->GetActiveCamera() : nullptr);
	object->SetDirectionalLight(directionalLight_);
	object->SetPointLight(pointLight_);
	object->SetSpotLight(spotLight_);
	const float offset = static_cast<float>(normalObjects.size()) * 1.4f;
	object->SetTranslate({ -2.0f + offset, 0.0f, 6.0f });
	object->SetScale({ 1.0f, 1.0f, 1.0f });
	normalObjects.push_back(std::move(object));
	return true;
}

bool TitleScene::AddTextureToTitle(const std::string& textureFilePath)
{
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon, textureFilePath);
	sprite->SetAnchorPoint({ 0.5f, 0.5f });
	const Vector2 originalSize = sprite->GetSize();
	const float largestSide = (std::max)(originalSize.x, originalSize.y);
	if (largestSide > 180.0f && largestSide > 0.0f) {
		const float scale = 180.0f / largestSide;
		sprite->SetSize({ originalSize.x * scale, originalSize.y * scale });
	}
	const float x = 180.0f + static_cast<float>(addedSprites_.size() % 4) * 190.0f;
	const float y = 160.0f + static_cast<float>(addedSprites_.size() / 4) * 160.0f;
	sprite->SetPosition({ x, y });
	addedSprites_.push_back(std::move(sprite));
	selectedTitleSpriteIndex_ = addedSprites_.size() - 1;
	inspectorAutoSelectSpriteFrames_ = 2;
	return true;
}

void TitleScene::DrawTitleModelShelfImGui()
{
	SceneEditor::ShelfCallbacks callbacks{};
	callbacks.sceneLabel = "Title";
	callbacks.addedModelCount = normalObjects.size() > baseNormalObjectCount_ ? normalObjects.size() - baseNormalObjectCount_ : 0;
	callbacks.addedTextureCount = addedSprites_.size();
	callbacks.addModel = [this](const std::string& fileName) { return AddModelToTitle(fileName); };
	callbacks.addTexture = [this](const std::string& textureFilePath) { return AddTextureToTitle(textureFilePath); };
	callbacks.clearAdded = [this]() {
		if (normalObjects.size() > baseNormalObjectCount_) {
			normalObjects.resize(baseNormalObjectCount_);
			obj = normalObjects.empty() ? nullptr : normalObjects.front().get();
		}
		addedSprites_.clear();
		selectedTitleSpriteIndex_ = 0;
		inspectorAutoSelectSpriteFrames_ = 0;
	};
	SceneEditor::DrawModelShelf(shelfState_, callbacks);
}

void TitleScene::DrawTitleInspectorImGui()
{
	const int forcedSpriteIndex = inspectorAutoSelectSpriteFrames_ > 0 && selectedTitleSpriteIndex_ < addedSprites_.size()
		? static_cast<int>(selectedTitleSpriteIndex_)
		: -1;
	if (inspectorAutoSelectSpriteFrames_ > 0) {
		--inspectorAutoSelectSpriteFrames_;
	}
	SceneEditor::InspectorOptions options{};
	options.description = "Adjust TitleScene models and 2D textures added from Model Shelf.";
	options.sprites = &addedSprites_;
	options.normalObjects = &normalObjects;
	options.animationObjects = &animationObjects_;
	options.directionalLight = &directionalLight_;
	options.pointLight = &pointLight_;
	options.spotLight = &spotLight_;
	options.addedSpriteCount = addedSprites_.size();
	options.protectedNormalObjectCount = baseNormalObjectCount_;
	options.forcedSpriteIndex = forcedSpriteIndex;
	options.selectSpriteTab = forcedSpriteIndex >= 0;
	options.removeSprite = [this](size_t index) {
		if (index >= addedSprites_.size()) {
			return;
		}
		DirectXCommon::GetInstance()->WaitForGPU();
		addedSprites_.erase(addedSprites_.begin() + static_cast<std::ptrdiff_t>(index));
		selectedTitleSpriteIndex_ = addedSprites_.empty() ? 0 : (std::min)(index, addedSprites_.size() - 1);
		inspectorAutoSelectSpriteFrames_ = addedSprites_.empty() ? 0 : 2;
	};
	SceneEditor::DrawInspector(options);
}

void TitleScene::HandleTitleShelfDropOnGameView()
{
	SceneEditor::ShelfCallbacks callbacks{};
	callbacks.sceneLabel = "Title";
	callbacks.addModel = [this](const std::string& fileName) { return AddModelToTitle(fileName); };
	callbacks.addTexture = [this](const std::string& textureFilePath) { return AddTextureToTitle(textureFilePath); };
	SceneEditor::HandleShelfDropOnGameView(shelfState_, callbacks);
}

void TitleScene::Initialize()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);

	cameraManager = std::make_unique<CameraManager>();
	mainCamera = std::make_unique<Camera>();

	mainCamera->SetRotate({ 0.0f,0.0f,0.0f });
	mainCamera->SetTranslate({ 0.0f,0.0f,-10.0f });
	cameraManager->AddCamera("MainCamera", mainCamera.get());
	cameraManager->SetActiveCamera("MainCamera");

	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());

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
	spotLight_.direction = Normalize({ -1.0f, -1.0f, 0.0f });
	spotLight_.decay = 2.0f;
	spotLight_.cosAngle = std::cos(0.45f);
	spotLight_.cosFalloffStart = 1.0f;

	ModelManager::GetInstance()->LoadModel("plane.obj");
	auto terrain = std::make_unique<Object3d>();

	terrain->Initialize(object3dCommon);
	terrain->SetModel("plane.obj");
	terrain->SetDirectionalLight(directionalLight_);
	terrain->SetPointLight(pointLight_);
	terrain->SetSpotLight(spotLight_);
	terrain->GetTransform().translate = { 1.0f, -2.0f, 10.0f };
	obj = terrain.get();
	normalObjects.push_back(std::move(terrain));
	baseNormalObjectCount_ = normalObjects.size();

	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(dxCommon);

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(spriteCommon, "Resources/uvChecker.png");
	sprite_->SetPosition({ 0.0f, 0.0f });

	// skyBoxの背景
	TextureManager::GetInstance()->LoadTexture("Resources/qwantani_moonrise_puresky_1k.dds");

	//Skybox
	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(dxCommon, cameraManager->GetActiveCamera());
	// 添付されていたDDSテクスチャのパスを指定する
	skyBox_->SetTexture("Resources/qwantani_moonrise_puresky_1k.dds");

	//音声読み込み
	Audio::GetInstance()->LoadFile("Resources/Alarm01.wav");
	//音声再生
	Audio::GetInstance()->PlayWave("Resources/Alarm01.wav");

	ScanResourceShelf();
	isFinished_ = false;
}

void TitleScene::Update()
{
	//カメラの更新
	cameraManager->Update();

	for (auto& object3d : normalObjects) {
		object3d->SetCamera(cameraManager->GetActiveCamera());
		float length = Length(directionalLight_.direction);
		if (length > 0.0f) {
			directionalLight_.direction = Normalize(directionalLight_.direction);
		} else {
			directionalLight_.direction = { 0.0f, -1.0f, 0.0f };
		}
		object3d->SetDirectionalLight(directionalLight_);
		object3d->SetPointLight(pointLight_);
		object3d->SetSpotLight(spotLight_);
		object3d->Update();
	}

	//カメラのビュープロジェクション行列を渡して更新
	Matrix4x4 viewMatrix = cameraManager->GetActiveCamera()->GetViewMatrix();
	Matrix4x4 projectionMatrix = cameraManager->GetActiveCamera()->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

	sprite_->Update();
	for (auto& sprite : addedSprites_) {
		sprite->Update();
	}
	skyBox_->Update();

	ImGuiManager::GetInstance()->Begin("Title");
	DrawTitleModelShelfImGui();
	DrawTitleInspectorImGui();
	HandleTitleShelfDropOnGameView();
	ImGuiManager::GetInstance()->End();

	//sapceキーが押されていたら
	if (Input::GetInstance()->TriggerKey(DIK_SPACE) || Input::GetInstance()->IsPadButtonPressed(0, 1))
	{
		//シーン切り替え
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}
}

void TitleScene::Draw()
{
	//描画前処理
	DirectXCommon::GetInstance()->PreDraw();
	SrvManager::GetInstance()->PreDraw();

	object3dCommon->SetCommonDrawSetting();
	for (const auto& object : normalObjects) {
		if (object) {
			object->Draw();
		}
	}
	//skyBox描画
	if (skyBox_) {
		skyBox_->Draw();
	}

	spriteCommon->SetCommonDrawSetting();
	sprite_->Draw();
	for (const auto& sprite : addedSprites_) {
		sprite->Draw();
	}

	// SceneはRenderTextureへ描画済みなので、ImGuiの直前にSwapChainへ切り替える
	if (PostEffect::GetInstance()->IsEnabled()) {
		DirectXCommon::GetInstance()->PreDrawForPostEffectTexture();
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), true);
	}
	DirectXCommon::GetInstance()->PreDrawForSwapChain(PostEffect::GetInstance()->IsEnabled());
#ifndef USE_IMGUI
	// RenderTextureのSceneを全画面三角形でSwapChainへコピーする
	if (PostEffect::GetInstance()->IsEnabled()) {
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetPostEffectTextureSrvIndex(), false);
	} else {
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), false);
	}
#endif
	ImGuiManager::GetInstance()->Draw(DirectXCommon::GetInstance());

	DirectXCommon::GetInstance()->PostDraw();
	
}

void TitleScene::Finalize()
{
	//GPUの完了待ち
	DirectXCommon::GetInstance()->WaitForGPU();
	obj = nullptr;
	sprite_.reset();
	addedSprites_.clear();
	skyBox_.reset();
	normalObjects.clear();
	animationObjects_.clear();
	shelfState_.entries.clear();
	shelfState_.selectedEntry.clear();
	shelfState_.message.clear();
	baseNormalObjectCount_ = 0;
	selectedTitleSpriteIndex_ = 0;
	inspectorAutoSelectSpriteFrames_ = 0;
	mainCamera.reset();
	cameraManager.reset();
	isFinished_ = false;

}
