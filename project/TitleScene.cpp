#include "TitleScene.h"
#include "GamePlayScene.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "ImGuiManager.h"
#include "Input.h"
#include <dinput.h>
using namespace MyMath;

void TitleScene::Initialize()
{

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);

	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(dxCommon);

	cameraManager = new CameraManager();
	mainCamera = new Camera();
	mainCamera->SetRotate({ 0.0f,0.0f,0.0f });
	mainCamera->SetTranslate({ 0.0f,0.0f,-10.0f });
	cameraManager->AddCamera("MainCamera", mainCamera);
	cameraManager->SetActiveCamera("MainCamera");
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");

	Sprite* sprite = new Sprite();
	sprite->Initialize(spriteCommon, "Resources/monsterBall.png");

	//音声読み込み
	soundData1 = Audio::GetInstance()->LoadFile("Resources/Alarm01.wav");
	//音声再生
	Audio::GetInstance()->PlayWave(soundData1);

}

void TitleScene::Update()
{
	//カメラの更新
	cameraManager->Update();

	//カメラのビュープロジェクション行列を渡して更新
	Matrix4x4 viewMatrix = cameraManager->GetActiveCamera()->GetViewMatrix();
	Matrix4x4 projectionMatrix = cameraManager->GetActiveCamera()->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

	Sprite* sprite;
	sprite->Update();

	//sapceキーが押されていたら
	if (Input::GetInstance()->PushKey(DIK_SPACE))
	{
		GamePlayScene;
	}
}

void TitleScene::Draw()
{
	//描画前処理
	DirectXCommon::GetInstance()->PreDraw();
	SrvManager::GetInstance()->PreDraw();

	//3Dオブジェクトの描画準備3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
	object3dCommon->SetCommonDrawSetting();

	ImGuiManager::GetInstance()->Draw(DirectXCommon::GetInstance());

	spriteCommon->SetCommonDrawSetting();
	Sprite* sprite;
	sprite->Draw();

	DirectXCommon::GetInstance()->PostDraw();

}

void TitleScene::Finalize()
{
	//GPUの完了待ち
	DirectXCommon::GetInstance()->WaitForGPU();

	//音声データ解放
	Audio::GetInstance()->Unload(&soundData1);


	Sprite* sprite;
	delete sprite;

	delete cameraManager;
}