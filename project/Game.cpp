#include "Game.h"

#include <filesystem>
#include <chrono>
#include <fstream>
#include <format>
#include <mfapi.h>
#include <cassert>

#include "WinApp.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "Logger.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "Object3dCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "SpriteCommon.h"

void Game::Initialize()
{
	//ログのディレクトリを用意
	std::filesystem::create_directory("logs");

	//ここからファイルを作成し
	//現在時刻を取得(UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	//ログファイルの名前にコンマ何秒はいらないので削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	//日本時間(pcの設定時間)に変換
	std::chrono::zoned_time localTime{ std::chrono::current_zone(),nowSeconds };
	//formatを使って年月日_時分秒の文字列に変換
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	//時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + dateString + ".log";
	//ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);

	Logger::Log("\nHello DirectX!\n");

	winApp = new WinApp();
	winApp->Initialize();

	//DirectXの初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	//SRVマネージャの初期化
	srvManager = SrvManager::GetInstance();
	srvManager->Initialize(dxCommon);

	TextureManager::GetInstance()->Initialize(dxCommon, srvManager);

	imGuiManager = new ImGuiManager();
	imGuiManager->Initialize(winApp, dxCommon, srvManager);

	ModelManager::GetInstance()->Initialize(dxCommon);

	//入力の初期化
	input = new Input();
	input->Initialize(winApp);

	//XAudioエンジンのインスタンスを生成
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	//マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);
	//Windows Media Foundationの初期化(ローカルファイル版)
	result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(result));

	//音声読み込み
	soundData1 = SoundLoadFile("Resources/Alarm01.wav");
	//音声再生
	SoundPlayWave(xAudio2.Get(), soundData1);

	//3Dオブジェクト共通部の初期化
	object3dCommon = new Object3dCommon;
	object3dCommon->Initialize(dxCommon);

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
	spriteCommon->Initialize(dxCommon);

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
	// マネージャの初期化（シングルトンなのでGetInstanceを使う）
	ParticleManager::GetInstance()->Initialize(dxCommon, srvManager);
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