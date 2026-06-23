#pragma once
#include "BaseScene.h"
#include "AbstractSceneFactory.h"
#include<string>

class SceneManager
{
public:

	// インスタンス取得関数
	static SceneManager* GetInstance();

	void Update();
	void Draw();

	//シーンファクトリーのsetter
	void SetSceneFactory(AbstractSceneFactory* sceneFactory) { sceneFactory_ = sceneFactory; }

	//次シーン予約
	bool ChangeScene(const std::string& sceneName);
	bool RestartCurrentScene();

	const std::string& GetCurrentSceneName() const { return currentSceneName_; }
	const std::string& GetPendingSceneName() const { return pendingSceneName_; }
	const std::vector<std::string>& GetAvailableSceneNames() const;
	bool HasPendingScene() const { return nextScene_ != nullptr; }

private:
	// シングルトン化：コンストラクタとデストラクタをprivateにする
	SceneManager() = default;
	~SceneManager();

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	//今のシーン
	std::unique_ptr<BaseScene> scene_ = nullptr;
	//次のシーン
	std::unique_ptr<BaseScene> nextScene_ = nullptr;
	std::string currentSceneName_;
	std::string pendingSceneName_;

	//シーンファクトリー(借りてくる)
	AbstractSceneFactory* sceneFactory_ = nullptr;
};

