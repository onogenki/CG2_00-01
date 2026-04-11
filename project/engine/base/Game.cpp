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

using namespace MyMath;

void Game::Initialize()
{
	Framework::Initialize();

	//音声読み込み
	soundData1 = SoundLoadFile("Resources/Alarm01.wav");
	//音声再生
	SoundPlayWave(xAudio2_.Get(), soundData1);


	//3Dオブジェクト共通部の初期化
	object3dCommon = new Object3dCommon;
	object3dCommon->Initialize(dxCommon_);

	//カメラマネージャ
	cameraManager = new CameraManager();

	//メインカメラ
	mainCamera = new Camera();
	mainCamera->SetRotate({ 0.0f,0.0f,0.0f });
	mainCamera->SetTranslate({ 0.0f,0.0f,-10.0f });
	cameraManager->AddCamera("MainCamera", mainCamera);

	//上アングルカメラ
	upCamera = new Camera();
	upCamera->SetRotate({ 0.785f,0.0f,0.0f });
	upCamera->SetTranslate({ 0.0f,5.0f,-5.0f });
	cameraManager->AddCamera("UpCamera", upCamera);

	//MainCameraをアクティブ
	cameraManager->SetActiveCamera("MainCamera");
	//共通部にはマネージャのアクティブカメラを渡す
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());

	//.objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("axis.obj");

	objectPlane = new Object3d();
	objectPlane->Initialize(object3dCommon);
	objectPlane->SetModel("plane.obj");
	objectPlane->GetTransform().translate = { 1.0f, 0.0f, 0.0f }; // 左に配置
	objects.push_back(objectPlane); // リストに追加

	// --- 2つ目: Axis ---
	objectAxis = new Object3d();
	objectAxis->Initialize(object3dCommon);
	objectAxis->SetModel("axis.obj");
	objectAxis->GetTransform().translate = { 2.0f, 0.0f, 0.0f }; // 右に配置
	objects.push_back(objectAxis); // リストに追加

	spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon_);

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");//1枚目
	TextureManager::GetInstance()->LoadTexture("Resources/monsterBall.png");//2枚目
	TextureManager::GetInstance()->LoadTexture("Resources/circle.png");

	for (uint32_t i = 0; i < 1; ++i)
	{
		Sprite* sprite = new Sprite();
		sprite->Initialize(spriteCommon, "Resources/monsterBall.png");

		if (i % 2 == 0) {
			// 偶数にモンスターボールpng
			sprite->SetTexture("Resources/uvChecker.png");
		}

		Vector2 pos = { 0.0f + i * 0.0f,0.0f + i * 50.0f };
		sprite->SetPosition(pos);
		sprites.push_back(sprite);
	}

	//パーティクル
	//座標、1回の発生数、発生頻度[秒]
	emitterTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	//Circleパーティクル
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "Resources/circle.png");
	emitterCircle = new ParticleEmitter("Circle", emitterTransform, 1, 0.1f);

	//四角形のパーティクル
	ParticleManager::GetInstance()->CreateParticleGroup("Plane", "Resources/uvChecker.png");
	emitterPlane = new ParticleEmitter("Plane", emitterTransform, 1, 0.1f);

	//最初はcircleにする
	activeEmitter = new ParticleEmitter("Circle", emitterTransform, 1, 0.1f);

	selectedUI = 0;
}

void Game::Update()
{

	//UIの更新
	lightData = objectAxis->GetDirectionalLight();

	Framework::Update();

	//ゲームの処理
	imGuiManager_->Begin();
	imGuiManager_->DemoWindow();
	imGuiManager_->FPSWindow();
	imGuiManager_->SpriteWindow(sprites);
	imGuiManager_->ModelWindow(objects, lightData);
	imGuiManager_->ParticleWindow(activeEmitter, emitterTransform);
	imGuiManager_->CameraWindow(cameraManager);
	imGuiManager_->End();

	//カメラの更新
	cameraManager->Update();
	//カメラのビュープロジェクション行列を渡して更新
	Matrix4x4 viewMatrix = cameraManager->GetActiveCamera()->GetViewMatrix();
	Matrix4x4 projectionMatrix = cameraManager->GetActiveCamera()->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

	//時間がきたら自動でパーティクル発生
	activeEmitter->Update();
	//パーティクル全体の更新
	ParticleManager::GetInstance()->Update(viewProjectionMatrix);

	//3Dオブジェクトの更新
	for (Object3d* object3d : objects) {
		//毎フレーム、マネージャから今のアクティブカメラをもらう
		object3d->SetCamera(cameraManager->GetActiveCamera());
		lightData.direction = Normalize(lightData.direction);
		object3d->SetDirectionalLight(lightData);//光を他のモデルにも分け与える
		object3d->Update();

	}

	//スプライトの更新
	for (Sprite* sprite : sprites)
	{
		sprite->Update();
	}

	////数字の0キーが押されていたら
	if (input_->PushKey(DIK_0))
	{
		OutputDebugStringA("Hit 0\n");//出力ウィンドウに「Hit 0」と表示
	}

	////数字の0キーが押されていたら
	if (input_->TriggerKey(DIK_P))
	{
		OutputDebugStringA("Hit p\n");//出力ウィンドウに「Hit p」と表示
	}

}

void Game::Draw()
{
	//描画前処理
	dxCommon_->PreDraw();
	srvManager_->PreDraw();

	//3Dオブジェクトの描画準備3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
	object3dCommon->SetCommonDrawSetting();

	for (Object3d* object3d : objects) {
		object3d->Draw();
	}

	//パーティクル描画
	ParticleManager::GetInstance()->Draw();

	// スプライト描画
	//Spriteの描画準備Spriteの描画に共通のグラフィックスコマンドを積む
	spriteCommon->SetCommonDrawSetting();
	for (Sprite* sprite : sprites)
	{
		sprite->Draw();
	}

	imGuiManager_->Draw(dxCommon_);


	dxCommon_->PostDraw();
}

void Game::Finalize()
{
	//GPUの完了待ち
	dxCommon_->WaitForGPU();

	//音声データ解放
	SoundUnload(&soundData1);

	//パーティクル全体解放
	delete activeEmitter;

	for (Sprite* sprite : sprites)
	{
		delete sprite;
	}
	sprites.clear();
	for (Object3d* object3d : objects) {
		delete object3d;
	}
	objects.clear();

	delete spriteCommon;
	delete object3dCommon;

	delete cameraManager;

	Framework::Finalize();
}