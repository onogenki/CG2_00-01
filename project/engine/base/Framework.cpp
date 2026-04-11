#include "Framework.h"
#include "Logger.h"

#include <filesystem>
#include <chrono>
#include <fstream>
#include <format>
#include <mfapi.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>

#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"

#pragma comment(lib, "Dbghelp.lib")

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

void Framework::Initialize()
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
	Logger::SetLogPath(logFilePath);

	Logger::Log("\nHello DirectX!\n");

	winApp_ = new WinApp();
	winApp_->Initialize();

	//DirectXの初期化
	dxCommon_ = new DirectXCommon();
	dxCommon_->Initialize(winApp_);

	//SRVマネージャの初期化
	srvManager_ = SrvManager::GetInstance();
	srvManager_->Initialize(dxCommon_);

	TextureManager::GetInstance()->Initialize(dxCommon_, srvManager_);
	ModelManager::GetInstance()->Initialize(dxCommon_);
	ParticleManager::GetInstance()->Initialize(dxCommon_, srvManager_);

	//入力の初期化
	input_ = new Input();
	input_->Initialize(winApp_);

	imGuiManager_ = new ImGuiManager();
	imGuiManager_->Initialize(winApp_, dxCommon_, srvManager_);

	//XAudioエンジンのインスタンスを生成
	HRESULT result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	//マスターボイスを生成
	result = xAudio2_->CreateMasteringVoice(&masterVoice_);
	//Windows Media Foundationの初期化(ローカルファイル版)
	result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(result));

	// クラッシュ時にExportDumpを呼ぶように登録
	SetUnhandledExceptionFilter(ExportDump);

}

void Framework::Update()
{
	//ウィンドウにメッセージが来てたら最優先で処理される
	if (winApp_->ProcessMessage())
	{
		//終了フラグを立てて、このフレームの更新処理をする抜ける
		endRequest_ = true;
	}
	input_->Update();
}

void Framework::Finalize()
{

	imGuiManager_->Finalize();
	TextureManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();

	xAudio2_.Reset();
	MFShutdown();

	delete imGuiManager_;
	delete input_;
	delete dxCommon_;
	winApp_->Finalize();
	delete winApp_;
}

void Framework::Run()
{
	//ゲームの初期化
	Initialize();

	while (true) //ゲームループ
	{
		//毎フレーム更新
		Update();
		//終了リクエストがきたら抜ける
		if (IsEndRequest())
		{
			break;
		}
		//描画
		Draw();
	}
	//ゲームの終了
	Finalize();
}