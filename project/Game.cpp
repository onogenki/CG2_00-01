#include "Game.h"

#include <filesystem>
#include <chrono>
#include <fstream>
#include <format>
#include <mfapi.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>

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
using namespace MyMath;

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception)
{
	//時刻を取得して、時刻を名前に入れたファイルを作成。
	// Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	//processId(このexeのID)とクラッシュ(例外)の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	//設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	//Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
	//他に関連づけられていSEH例外ハンドラあれば実行。通常はプロセスを終了する
	return EXCEPTION_EXECUTE_HANDLER;
}

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

	// クラッシュ時にExportDumpを呼ぶように登録
	SetUnhandledExceptionFilter(ExportDump);

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

void Game::Update()
{
	//ウィンドウにメッセージが来てたら最優先で処理される
	if (winApp->ProcessMessage())
	{
		//終了フラグを立てて、このフレームの更新処理をする抜ける
		endRequest_ = true;
		return;
	}

	//UIの更新
	lightData = objectAxis->GetDirectionalLight();
	//ゲームの処理
	imGuiManager->Begin();
	imGuiManager->DemoWindow();
	imGuiManager->FPSWindow();
	imGuiManager->SpriteWindow(sprites);
	imGuiManager->ModelWindow(objects, lightData);
	imGuiManager->ParticleWindow(activeEmitter, emitterTransform);
	imGuiManager->CameraWindow(cameraManager);
	imGuiManager->End();

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

	//入力の更新
	input->Update();

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
	if (input->PushKey(DIK_0))
	{
		OutputDebugStringA("Hit 0\n");//出力ウィンドウに「Hit 0」と表示
	}

	////数字の0キーが押されていたら
	if (input->TriggerKey(DIK_P))
	{
		OutputDebugStringA("Hit p\n");//出力ウィンドウに「Hit p」と表示
	}

}

void Game::Draw()
{
	//描画前処理
	dxCommon->PreDraw();
	srvManager->PreDraw();

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

	imGuiManager->Draw(dxCommon);


	dxCommon->PostDraw();
}

void Game::Finalize()
{
	//GPUの完了待ち
	dxCommon->WaitForGPU();

	//XAudio2解放
	xAudio2.Reset();
	//音声データ解放
	SoundUnload(&soundData1);

	//Windows Media Foundationの処理
	HRESULT result = MFShutdown();
	assert(SUCCEEDED(result));

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

	if (imGuiManager) {
		imGuiManager->Finalize();
	}
	TextureManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();

	delete spriteCommon;
	delete object3dCommon;
	delete imGuiManager;

	winApp->Finalize();

	delete input;
	delete cameraManager;
	delete dxCommon;
	delete winApp;
}