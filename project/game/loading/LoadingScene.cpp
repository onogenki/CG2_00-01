#include "LoadingScene.h"

#include "DirectXCommon.h"
#include "PostEffect.h"
#include "SceneManager.h"
#include "SceneRenderPipeline.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "WinApp.h"

// LoadingSceneが前方宣言したSprite型を、この実装ファイルで生成・破棄します。
LoadingScene::LoadingScene() = default;
LoadingScene::~LoadingScene() = default;

// Loading中に前Sceneの色補正が残らないようにし、白い背景だけを準備します。
void LoadingScene::Initialize()
{
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);
	hasRequestedNextScene_ = false;
	nextSceneName_ = SceneManager::GetInstance()->TakeLoadingDestinationSceneName();
	if (nextSceneName_.empty()) {
		nextSceneName_ = "TITLE";
	}
	InitializeLoadingSprite();
}

// SpriteCommonは共通描画設定を所有し、LoadingSceneは自分の白い一枚だけを所有します。
void LoadingScene::InitializeLoadingSprite()
{
	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(DirectXCommon::GetInstance());

	loadingSprite_ = std::make_unique<Sprite>();
	loadingSprite_->Initialize(spriteCommon, "resources/white.png");
	loadingSprite_->SetPosition({ 0.0f, 0.0f });
	loadingSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	loadingSprite_->SetSize({
		static_cast<float>(WinApp::kClientWidth),
		static_cast<float>(WinApp::kClientHeight),
	});
}

// 次Sceneの初期化中も、直前に描いた白画面がウィンドウへ残ります。
void LoadingScene::Update()
{
	if (!hasRequestedNextScene_) {
		// 通常起動はTitleへ、本編開始ではSceneManagerが指定したStage1などへ進みます。
		hasRequestedNextScene_ = SceneManager::GetInstance()->ChangeScene(
			nextSceneName_);
	}

	if (loadingSprite_) {
		loadingSprite_->Update();
	}
}

// Loading中はDockやDebug UIを描かず、白い画面だけをSwapChainへ出します。
void LoadingScene::Draw()
{
	SceneRenderPipeline::Begin(DirectXCommon::GetInstance());
	if (loadingSprite_) {
		spriteCommon->SetCommonDrawSetting();
		loadingSprite_->Draw();
	}
	SceneRenderPipeline::End(DirectXCommon::GetInstance(), nullptr, false);
}

// LoadingSceneは白背景だけを所有するため、それだけを解放します。
void LoadingScene::Finalize()
{
	loadingSprite_.reset();
}
