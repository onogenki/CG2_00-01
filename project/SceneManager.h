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
	void ChangeScene(const std::string& sceneName);

private:
	// シングルトン化：コンストラクタとデストラクタをprivateにする
	SceneManager() = default;
	~SceneManager();

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	//今のシーン
	BaseScene* scene_ = nullptr;
	//次のシーン
	BaseScene* nextScene_ = nullptr;

	//シーンファクトリー(借りてくる)
	AbstractSceneFactory* sceneFactory_ = nullptr;
};

