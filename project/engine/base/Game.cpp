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
	if (sceneManager->HasPendingScene()) {
		return;
	}

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

	if (sceneStressCompletedRestarts_ >= sceneStressRequestedRestarts_ &&
		!sceneManager->HasPendingScene()) {
		endRequest_ = true;
	}
}
