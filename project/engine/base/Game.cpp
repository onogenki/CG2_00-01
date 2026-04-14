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

	scene_ = new TitleScene();

	scene_ = new GamePlayScene();
	scene_->Initialize();
}

void Game::Update()
{
	Framework::Update();

	//ゲーム落ち対策
	if (scene_) {
		scene_->Update();
	}
}

void Game::Draw()
{
	scene_->Draw();
}

void Game::Finalize()
{
	scene_->Finalize();
	delete scene_;

	Framework::Finalize();
}