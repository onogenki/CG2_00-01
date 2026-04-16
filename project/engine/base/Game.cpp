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

	//ゲーム固有の最初のシーン
	BaseScene* scene = new TitleScene();

	//マネージャーに最初のシーンを予約
	SceneManager::GetInstance()->SetNextScene(scene);

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