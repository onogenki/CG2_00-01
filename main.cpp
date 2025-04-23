#include <Windows.h>
#include <cstdint>
#include<string>
//#include<format>
#include<filesystem>
#include<chrono>
#include<fstream>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>
#include <dbghelp.h>
#include <strsafe.h>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"Dbghelp.lib")

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

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

void Log(std::ofstream& os, const std::string& message)
{
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}


//ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT  msg, WPARAM wparam, LPARAM lparam)
{
	//メッセージに応じてゲーム固有の処理を行う
	switch (msg)
	{
		//ウィンドウが破棄された
	case WM_DESTROY:
		//OSに対して、アプリの終了をつたえる
		PostQuitMessage(0);
		return 0;
	}

	//標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	//誰も補足しなかった場合に(Unhandled),補足する関数を登録
	//main関数はじまってすぐに登録すると良い
	SetUnhandledExceptionFilter(ExportDump);

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



	WNDCLASS wc{};
	//ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	//ウィンドウクラス名(何でもよい)
	wc.lpszClassName = L"CG2WindowClass";
	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウィンドウクラスを登録する
	RegisterClass(&wc);
	//クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;
	//ウィンドウサイズを表す構造体にクライエント領域を入れる
	RECT wrc = { 0,0,kClientWidth,kClientHeight };
	//クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,    //利用するクラス名
		L"CG2",              //タイトルバーの文字
		WS_OVERLAPPEDWINDOW, //よく見るウィンドウスタイル
		CW_USEDEFAULT,       //表示X座標(windowsに任せる)
		CW_USEDEFAULT,       //座標Y座標(windowsOSに任せる)
		wrc.right - wrc.left,//ウィンドウ横幅
		wrc.bottom - wrc.top,//ウィンドウ縦幅
		nullptr,             //親ウィンドウハンドル
		nullptr,            //メニューハンドル
		wc.hInstance,       //インスタンスハンドル
		nullptr);           //オプション

	//ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);
	//出力ウィンドウへの文字出力
	Log(logStream, "HELLOW DIRECTX!\n");
	Log(logStream,
		ConvertString(
			std::format(
				L"ClientSize:{},{}\n",
				kClientWidth,
				kClientHeight
			)
		)
	);

	//DXGIファクトリーの生成
	IDXGIFactory7* dxgiFactory = nullptr;
	//HRESULTはWindows系のエラーコードであり、
	//関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	//初期化の根本的な部分でエラーが出た場合はプログラムが間違ってるか、
	// どうにもできない場合が多いのでassertにしておく
	assert(SUCCEEDED(hr));

	//使用するアダプタ用の変数。最初にnullptrを入れておく
	IDXGIAdapter4* useAdapter = nullptr;
	//良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
		i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i)
	{
		//アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));//取得できないのは一大事
		//ソフトウェアアダプタでなければ採用!
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
		{
			//採用したアダプタの情報をログに出力。wstringの方なので注意
			Log(logStream, ConvertString(std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr;//ソフトウェアアダプタの場合は見なかったことにする
	}
	//適切なアダプタが見つからなかったので起動できない
	assert(useAdapter != nullptr);

	ID3D12Device* device = nullptr;
	//機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] =
	{
	D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};

	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };

	//高い順に生成できるか試していく
	for (size_t i = 0; i <_countof(featureLevels); ++i)
	{
		//採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
		//指定した機能レベルでデバイスが生成できたのでログ出力を行ってループを抜ける
		if (SUCCEEDED(hr))
		{
			Log(logStream, std::format("Feature:{}\n", featureLevelStrings[i]));
			break;
		}
	}
//デバイスの生成がうまくいかなかったので起動できない
assert(device != nullptr);
//喜びの舞い
Log( logStream,"Complete create D3D12Device!!!\n");//初期化完了のログを出す

//コマンドキューを生成する
ID3D12CommandQueue* commandQueue = nullptr;
D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
hr = device->CreateCommandQueue(&commandQueueDesc,
	IID_PPV_ARGS(&commandQueue));
//コマンドキューの生成がうまくいかなかったので起動できない
assert(SUCCEEDED(hr));

//コマンドアロケータを生成する
ID3D12CommandAllocator* commandAllocator = nullptr;
hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
//コマンドアロケータの生成がうまくいかなかったので起動できない
assert(SUCCEEDED(hr));

//コマンドを生成する
ID3D12GraphicsCommandList* commandList = nullptr;
hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr,
	IID_PPV_ARGS(&commandList));
//コマンドリストの生成がうまくいかなかったので起動できない
assert(SUCCEEDED(hr));

//スワップチェーンを生成する
IDXGISwapChain4* swapChain = nullptr;
DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
swapChainDesc.Width = kClientWidth;//画面の幅。ウィンドウのクライアント領域を同じものにしておく
swapChainDesc.Height = kClientHeight;//画面の高さ。ウィンドウのクライアント領域を同じものにしておく
swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//色の形式
swapChainDesc.SampleDesc.Count = 1;//マルチサンプルしない
swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;//描画のターゲットとして利用する
swapChainDesc.BufferCount = 2;//ダブルバッファ
swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//モニタにうつしたら、中身を破棄
//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
assert(SUCCEEDED(hr));

MSG msg{};
//ウィンドウのxボタンが押されるまでループ
while (msg.message != WM_QUIT)
{//ウィンドウにメッセージが来てたら最優先で処理される
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	} else {
		//ゲームの処理
	}

}

	return 0;
}