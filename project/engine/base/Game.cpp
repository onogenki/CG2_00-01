#include "Game.h"

#include "SrvManager.h"
#include "ImGuiManager.h"
#include "Logger.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "Object3dCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "SpriteCommon.h"
#include"SceneFactory.h"
#include "SceneManager.h"
#include <algorithm>
#include <cstdlib>
#include <string>

using namespace MyMath;

namespace {
std::string GetEnvironmentString(const char* name)
{
	// _dupenv_sで確保された文字列は、この関数内で必ず解放する。
	char* value = nullptr;
	size_t size = 0;
	if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
		return {};
	}

	std::string result(value);
	std::free(value);
	return result;
}
}

void Game::Initialize()
{
	Framework::Initialize();

	//シーンファクトリーを生成し、マネージャにセット
	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());

	// 環境変数を指定すると、通常のタイトル画面を経由せず対象シーンを起動できる。
	const std::string startSceneName = GetEnvironmentString("CG2_START_SCENE");
	SceneManager::GetInstance()->ChangeScene(startSceneName.empty() ? "TITLE" : startSceneName);
	InitializeSceneStressFromEnvironment();
}

void Game::Update()
{
	//基底クラスのUpdateを呼ぶだけで、マネージャー経由でシーンを更新
	Framework::Update();
	UpdateSceneStress();
}

void Game::Draw()
{
	SceneManager::GetInstance()->Draw();
}

void Game::Finalize()
{
	Framework::Finalize();
}

void Game::InitializeSceneStressFromEnvironment()
{
	// 自動テスト時だけ有効にするため、設定がなければ何も変更しない。
	const std::string restartCountText = GetEnvironmentString("CG2_SCENE_STRESS_RESTARTS");
	if (restartCountText.empty()) {
		return;
	}

	sceneStressRequestedRestarts_ = (std::max)(0, std::atoi(restartCountText.c_str()));
	if (sceneStressRequestedRestarts_ <= 0) {
		return;
	}

	const std::string targetSceneText = GetEnvironmentString("CG2_SCENE_STRESS_SCENE");
	if (!targetSceneText.empty()) {
		sceneStressTarget_ = targetSceneText;
	}
	const std::string intervalText = GetEnvironmentString("CG2_SCENE_STRESS_INTERVAL");
	if (!intervalText.empty()) {
		sceneStressIntervalFrames_ = (std::max)(1, std::atoi(intervalText.c_str()));
	}

	// 再起動回数が有効なら、UpdateSceneStressでフレームごとに進行する。
	sceneStressEnabled_ = true;
	sceneStressCompletedRestarts_ = 0;
	sceneStressFrameCounter_ = 0;
}

void Game::UpdateSceneStress()
{
	if (!sceneStressEnabled_) {
		return;
	}

	SceneManager* sceneManager = SceneManager::GetInstance();
	// シーン切り替え中は次の要求を積まず、完了を待つ。
	if (sceneManager->HasPendingScene()) {
		return;
	}

	// まず対象シーンへ移動してから再起動回数を数える。
	if (sceneManager->GetCurrentSceneName() != sceneStressTarget_) {
		sceneManager->ChangeScene(sceneStressTarget_);
		return;
	}

	++sceneStressFrameCounter_;
	if (sceneStressCompletedRestarts_ < sceneStressRequestedRestarts_ &&
		sceneStressFrameCounter_ >= sceneStressIntervalFrames_) {
		sceneStressFrameCounter_ = 0;
		if (sceneManager->RestartCurrentScene()) {
			++sceneStressCompletedRestarts_;
		}
		return;
	}

	// 指定回数を終えたらフレームワークの終了要求を出す。
	if (sceneStressCompletedRestarts_ >= sceneStressRequestedRestarts_ &&
		!sceneManager->HasPendingScene()) {
		endRequest_ = true;
	}
}
