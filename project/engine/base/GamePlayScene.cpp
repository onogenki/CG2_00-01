#include "GamePlayScene.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "ImGuiManager.h"
#include "Input.h"
#include"SceneManager.h"
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
	cameraManager = std::make_unique<CameraManager>();


	ParticleManager::GetInstance()->SetCameraManager(cameraManager.get());

	//メインカメラ
	mainCamera = std::make_unique<Camera>();
	mainCamera->SetTranslate({ 0.0f,0.0f,-10.0f });
	cameraManager->AddCamera("MainCamera", mainCamera.get());

	//上アングルカメラ
	upCamera = std::make_unique<Camera>();
	upCamera->SetRotate({ 0.785f,0.0f,0.0f });
	upCamera->SetTranslate({ 0.0f,5.0f,-5.0f });
	cameraManager->AddCamera("UpCamera", upCamera.get());

	//MainCameraをアクティブ
	cameraManager->SetActiveCamera("MainCamera");
	//共通部にはマネージャのアクティブカメラを渡す
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");//1枚目
	TextureManager::GetInstance()->LoadTexture("Resources/monsterBall.png");//2枚目
	TextureManager::GetInstance()->LoadTexture("Resources/grass.png");//terrainのpng
	TextureManager::GetInstance()->LoadTexture("Resources/circle.png");

	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "Resources/circle.png");

	ParticleManager::GetInstance()->CreateParticleGroup("Plane", "Resources/uvChecker.png");
	
	//.objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("plane.gltf");
	//音声読み込み
	Audio::GetInstance()->LoadFile("Resources/Alarm01.wav");
	//音声再生
	Audio::GetInstance()->PlayWave("Resources/Alarm01.wav");

	// 2. 3Dオブジェクト生成
	// 一時的に unique_ptr を作り、初期化してから vector に move する
	auto objPlane = std::make_unique<Object3d>();
	objPlane->Initialize(object3dCommon);
	objPlane->SetModel("terrain.obj");
	objPlane->GetTransform().translate = { 1.0f, -2.0f, 10.0f };
	objectPlane = objPlane.get();           // 中身を指すだけのポインタを保存
	objects.push_back(std::move(objPlane)); // ここで所有権が vector に移る

	auto objAxis = std::make_unique<Object3d>();
	objAxis->Initialize(object3dCommon);
	objAxis->SetModel("plane.gltf");
	objAxis->GetTransform().translate = { 2.0f, 0.0f, 0.0f };
	objAxis->GetTransform().rotate = { 0.0f,0.0f,0.0f };
	objectAxis = objAxis.get();
	objects.push_back(std::move(objAxis));

	for (uint32_t i = 0; i < 1; ++i)
	{
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon, "Resources/monsterBall.png");

		if (i % 2 == 0) {
			// 偶数にモンスターボールpng
			sprite->SetTexture("Resources/uvChecker.png");
		}
		Vector2 pos = { 0.0f + i * 0.0f, 0.0f + i * 50.0f };
		sprite->SetPosition(pos);

		sprites.push_back(std::move(sprite));
	}

	// ライトの初期値を設定する
	// 平行光源はOFF (Intensity = 0.0f)
	directionalLight.direction = { 1.0f, -1.0f, 1.0f };
	directionalLight.intensity = 0.0f;
	directionalLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 点光源はON (初期位置 0, 2, 0)
	pointLight.position = { 0.0f, 2.0f, 0.0f };
	pointLight.intensity = 1.0f;
	pointLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLight.radius = 10.0f;
	pointLight.decay = 1.0f;

	//スポットライト
	spotLight.position = { 2.0f,1.25f,0.0f };
	spotLight.intensity = 4.0f;
	spotLight.color = { 1.0f,1.0f,1.0f,1.0f };
	spotLight.distance = 7.0f;
	spotLight.direction =
		Normalize({ -1.0f,-1.0f,0.0f });
	spotLight.decay = 2.0f;
	spotLight.cosAngle =
		std::cos(std::numbers::pi_v<float> / 3.0f);
	spotLight.cosFalloffStart = 1.0f;

	//パーティクル
	//座標、1回の発生数、発生頻度[秒]
	emitterTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	//Circleパーティクル
	emitterCircle = std::make_unique<ParticleEmitter>("Circle", emitterTransform, 1, 0.1f,false);
	//四角形のパーティクル(風に吹かれる方)
	emitterPlane = std::make_unique<ParticleEmitter>("Plane", emitterTransform, 1, 0.3f,true);

	//最初はcircleにする
	activeEmitter = emitterCircle.get();

	selectedUI = 0;
}

void GamePlayScene::Update()
{

	//UIの更新
	directionalLight = objectAxis->GetDirectionalLight();
	//ゲームの処理
	ImGuiManager::GetInstance()->Begin();
	ImGuiManager::GetInstance()->DemoWindow();
	ImGuiManager::GetInstance()->FPSWindow();
	ImGuiManager::GetInstance()->SpriteWindow(sprites);
	ImGuiManager::GetInstance()->ModelWindow(objects, directionalLight,pointLight,spotLight);
	// ImGuiのParticleWindowから、どのボタンが押されたかの結果（文字列）を受け取る
	std::string particleRequest = ImGuiManager::GetInstance()->ParticleWindow(emitterTransform);

	// 結果に応じて、アクティブなエミッター（指し示す先）を切り替える
	if (particleRequest == "Circle")
	{
		activeEmitter = emitterCircle.get(); // 既に作ってあるCircleの方を指す
	} else if (particleRequest == "Plane")
	{
		activeEmitter = emitterPlane.get();  // 既に作ってあるPlaneの方を指す
	}
	ImGuiManager::GetInstance()->CameraWindow(cameraManager.get());
	ImGuiManager::GetInstance()->End();

	//カメラの更新
	cameraManager->Update();


	//カメラのビュープロジェクション行列を渡して更新
	Matrix4x4 viewMatrix = cameraManager->GetActiveCamera()->GetViewMatrix();
	Matrix4x4 projectionMatrix = cameraManager->GetActiveCamera()->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

	// 安全のために Nullチェックを追加
	if (activeEmitter) {
		activeEmitter->Update();
	}
	//パーティクル全体の更新
	ParticleManager::GetInstance()->Update();

	//3Dオブジェクトの更新
	for (auto& object3d : objects) {
		//毎フレーム、マネージャから今のアクティブカメラをもらう
		object3d->SetCamera(cameraManager->GetActiveCamera());
		float length = Length(directionalLight.direction);
		if (length > 0.0f) {
			directionalLight.direction = Normalize(directionalLight.direction);
		} else {
			// 0の場合は適当な下向きにするなど、エラーを回避する
			directionalLight.direction = { 0.0f, -1.0f, 0.0f };
		}
		object3d->SetDirectionalLight(directionalLight);//光を他のモデルにも分け与える
		object3d->SetPointLight(pointLight);
		object3d->SetSpotLight(spotLight);
		object3d->Update();

	}

	//スプライトの更新
	for (auto& sprite : sprites)
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

	for (const auto& object3d : objects) {
		object3d->Draw();
	}

	//パーティクル描画
	ParticleManager::GetInstance()->Draw();

	// スプライト描画
	//Spriteの描画準備Spriteの描画に共通のグラフィックスコマンドを積む
	spriteCommon->SetCommonDrawSetting();
	for (const auto& sprite : sprites)
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

	// これにより、中途半端に生き残っている粒子が原因のアクセス違反を防げます
	ParticleManager::GetInstance()->ClearAllParticles();

	sprites.clear();
	objects.clear();
}

