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
			delete scene_;
		}
		//シーン切り替え
		scene_ = nextScene_;
		nextScene_ = nullptr;

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

SceneManager::~SceneManager()
{
	//最後のシーンの終了と開放
	if (scene_)
	{
		scene_->Finalize();
		delete scene_;
	}
}