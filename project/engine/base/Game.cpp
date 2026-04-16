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

using namespace MyMath;

void Game::Initialize()
{
	Framework::Initialize();

	//シーンファクトリーを生成し、マネージャにセット
	sceneFactory_ = new SceneFactory();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_);

	SceneManager::GetInstance()->ChangeScene("TITLE");
}

void Game::Update()
{
	//基底クラスのUpdateを呼ぶだけで、マネージャー経由でシーンを更新
	Framework::Update();
}

void Game::Draw()
{
	SceneManager::GetInstance()->Draw();
}

void Game::Finalize()
{
	Framework::Finalize();
}