#include "SceneManager.h"

SceneManager* SceneManager::GetInstance()
{
	static SceneManager instance;
	return &instance;
}

void SceneManager::Update()
{
	//TODO:シーン切り替え機構
	//次のシーンの予約があるなら
	if (nextScene_)
	{
		//旧シーンの終了
		if (scene_)
		{
			scene_->Finalize();
			scene_.reset();
		}
		//シーン切り替え
		scene_ = std::move(nextScene_);
		nextScene_ = nullptr;
		currentSceneName_ = pendingSceneName_;
		pendingSceneName_.clear();

		//次シーンを初期化する
		scene_->Initialize();
	}

	//実行中シーンを更新する
	if (scene_)
	{
		scene_->Update();
	}
}

void SceneManager::Draw()
{
	if (scene_)
	{
		scene_->Draw();
	}
}

bool SceneManager::ChangeScene(const std::string& sceneName)
{
	if (sceneFactory_ == nullptr || nextScene_ != nullptr) {
		return false;
	}

	std::unique_ptr<BaseScene> newScene(sceneFactory_->CreateScene(sceneName));
	if (!newScene) {
		return false;
	}

	nextScene_ = std::move(newScene);
	pendingSceneName_ = sceneName;
	return true;
}

bool SceneManager::RestartCurrentScene()
{
	return !currentSceneName_.empty() && ChangeScene(currentSceneName_);
}

const std::vector<std::string>& SceneManager::GetAvailableSceneNames() const
{
	static const std::vector<std::string> empty;
	return sceneFactory_ != nullptr ? sceneFactory_->GetSceneNames() : empty;
}

SceneManager::~SceneManager()
{
	//最後のシーンの終了と開放
	if (scene_)
	{
		scene_->Finalize();
	}
}
