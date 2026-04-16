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
#include "GamePlayScene.h"
#include "TitleScene.h"

using namespace MyMath;

void Game::Initialize()
{
	Framework::Initialize();

	//タイトルシーン
	scene_ = new TitleScene();

	scene_->Initialize();
	currentScene_ = SceneType::TITLE;

}

void Game::Update()
{
	Framework::Update();

	//今のシーンのupdateが呼ばれる
	scene_->Update();

	if (currentScene_ == SceneType::TITLE && scene_->IsFinished())
	{
		scene_->Finalize();
		delete scene_;

		scene_ = new GamePlayScene();
		scene_->Initialize();
		currentScene_ = SceneType::GAMEPLAY;
	}
}

void Game::Draw()
{
	scene_->Draw();
}

void Game::Finalize()
{
	//最後に残っているシーンを消す
	scene_->Finalize();
	delete scene_;

	Framework::Finalize();
}