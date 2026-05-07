#include "TitleScene.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "ImGuiManager.h"
#include "Input.h"
#include"SceneManager.h"
#include <dinput.h>
using namespace MyMath;

void TitleScene::Initialize()
{

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);

	ModelManager::GetInstance()->LoadModel("plane.obj");
	auto terrain = std::make_unique<Object3d>();

	terrain->Initialize(object3dCommon);
	terrain->SetModel("plane.obj");
	terrain->GetTransform().translate = { 1.0f, -2.0f, 10.0f };
	obj = terrain.get();
	normalObjects.push_back(std::move(terrain));

	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(dxCommon);

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(spriteCommon, "Resources/uvChecker.png");
	sprite_->SetPosition({ 0.0f, 0.0f });

	cameraManager = std::make_unique<CameraManager>();
	mainCamera = std::make_unique<Camera>();

	mainCamera->SetRotate({ 0.0f,0.0f,0.0f });
	mainCamera->SetTranslate({ 0.0f,0.0f,-10.0f });
	cameraManager->AddCamera("MainCamera", mainCamera.get());
	cameraManager->SetActiveCamera("MainCamera");

	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());

	//音声読み込み
	Audio::GetInstance()->LoadFile("Resources/Alarm01.wav");
	//音声再生
	Audio::GetInstance()->PlayWave("Resources/Alarm01.wav");

	isFinished_ = false;
}

void TitleScene::Update()
{
	//カメラの更新
	cameraManager->Update();

	for (auto& object3d : normalObjects) {
		object3d->SetCamera(cameraManager->GetActiveCamera());
		obj->Update();
	}

	//カメラのビュープロジェクション行列を渡して更新
	Matrix4x4 viewMatrix = cameraManager->GetActiveCamera()->GetViewMatrix();
	Matrix4x4 projectionMatrix = cameraManager->GetActiveCamera()->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

	sprite_->Update();

	ImGuiManager::GetInstance()->Begin();
	ImGuiManager::GetInstance()->End();

	//sapceキーが押されていたら
	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		//シーン切り替え
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}
}

void TitleScene::Draw()
{
	//描画前処理
	DirectXCommon::GetInstance()->PreDraw();
	SrvManager::GetInstance()->PreDraw();

	object3dCommon->SetCommonDrawSetting();
	for (const auto& object : normalObjects) {
		if (!obj->IsSkeletal()) {
			obj->Draw();
		}
	}
	spriteCommon->SetCommonDrawSetting();
	sprite_->Draw();

	ImGuiManager::GetInstance()->Draw(DirectXCommon::GetInstance());

	DirectXCommon::GetInstance()->PostDraw();
	
}

void TitleScene::Finalize()
{
	//GPUの完了待ち
	DirectXCommon::GetInstance()->WaitForGPU();

}