#include "GamePlayScene.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "ImGuiManager.h"
#include "Input.h"
#include <dinput.h>
using namespace MyMath;

void GamePlayScene::Initialize()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	//3Dオブジェクト共通部の初期化
	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);

	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(dxCommon);

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

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");//1枚目
	TextureManager::GetInstance()->LoadTexture("Resources/monsterBall.png");//2枚目
	TextureManager::GetInstance()->LoadTexture("Resources/circle.png");

	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "Resources/circle.png");

	ParticleManager::GetInstance()->CreateParticleGroup("Plane", "Resources/uvChecker.png");
	
	//.objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("axis.obj");

	//音声読み込み
	Audio::GetInstance()->LoadFile("Resources/Alarm01.wav");
	//音声再生
	Audio::GetInstance()->PlayWave("Resources/Alarm01.wav");

	//1つ目Plane
	objectPlane = new Object3d();
	objectPlane->Initialize(object3dCommon);
	objectPlane->SetModel("plane.obj");
	objectPlane->GetTransform().scale = { 1.0f, 1.0f, 1.0f };
	objectPlane->GetTransform().rotate = { 0.0f, 0.0f, 0.0f };
	objectPlane->GetTransform().translate = { 1.0f, 0.0f, 0.0f }; // 左に配置
	objects.push_back(objectPlane); // リストに追加

	//2つ目Axis
	objectAxis = new Object3d();
	objectAxis->Initialize(object3dCommon);
	objectAxis->SetModel("axis.obj");
	objectAxis->GetTransform().scale = { 1.0f, 1.0f, 1.0f };
	objectAxis->GetTransform().rotate = { 0.0f, 0.0f, 0.0f };
	objectAxis->GetTransform().translate = { 2.0f, 0.0f, 0.0f }; // 右に配置
	objects.push_back(objectAxis); // リストに追加

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
	emitterCircle = new ParticleEmitter("Circle", emitterTransform, 1, 0.1f);

	//四角形のパーティクル
	emitterPlane = new ParticleEmitter("Plane", emitterTransform, 1, 0.1f);

	//最初はcircleにする
	activeEmitter = new ParticleEmitter("Circle", emitterTransform, 1, 0.1f);

	selectedUI = 0;

}

void GamePlayScene::Update()
{

	//UIの更新
	lightData = objectAxis->GetDirectionalLight();
	//ゲームの処理
	ImGuiManager::GetInstance()->Begin();
	ImGuiManager::GetInstance()->DemoWindow();
	ImGuiManager::GetInstance()->FPSWindow();
	ImGuiManager::GetInstance()->SpriteWindow(sprites);
	ImGuiManager::GetInstance()->ModelWindow(objects, lightData);
	ImGuiManager::GetInstance()->ParticleWindow(activeEmitter, emitterTransform);
	ImGuiManager::GetInstance()->CameraWindow(cameraManager);
	ImGuiManager::GetInstance()->End();

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
		float length = Length(lightData.direction);
		if (length > 0.0f) {
			lightData.direction = Normalize(lightData.direction);
		} else {
			// 0の場合は適当な下向きにするなど、エラーを回避する
			lightData.direction = { 0.0f, -1.0f, 0.0f };
		}
		object3d->SetDirectionalLight(lightData);//光を他のモデルにも分け与える
		object3d->Update();

	}

	//スプライトの更新
	for (Sprite* sprite : sprites)
	{
		sprite->Update();
	}

	//数字の0キーが押されていたら
	if (Input::GetInstance()->PushKey(DIK_0))
	{
		OutputDebugStringA("Hit 0\n");//出力ウィンドウに「Hit 0」と表示
	}

	//数字の0キーが押されていたら
	if (Input::GetInstance()->TriggerKey(DIK_P))
	{
		OutputDebugStringA("Hit p\n");//出力ウィンドウに「Hit p」と表示
	}
}

void GamePlayScene::Draw()
{

	//描画前処理
	DirectXCommon::GetInstance()->PreDraw();
	SrvManager::GetInstance()->PreDraw();

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

	ImGuiManager::GetInstance()->Draw(DirectXCommon::GetInstance());


	DirectXCommon::GetInstance()->PostDraw();

}

void GamePlayScene::Finalize()
{
	//GPUの完了待ち
	DirectXCommon::GetInstance()->WaitForGPU();

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

	delete cameraManager;
}

