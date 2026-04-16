#pragma once
#include "BaseScene.h"
#include"TitleScene.h"
#include"GamePlayScene.h"


class SceneManager
{
public:

	// インスタンス取得関数
	static SceneManager* GetInstance();

	void Update();
	void Draw();

	//次シーン予約
	void SetNextScene(BaseScene* nextScene) { nextScene_ = nextScene; }

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
};

