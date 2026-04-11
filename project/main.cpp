#include <Windows.h>
#include<string>
#include<filesystem>
#include<chrono>
#include<fstream>
#include <dbghelp.h>
#include <strsafe.h>
#include<dxgidebug.h>
#include<dxcapi.h>
#include<vector>
#include <numbers>
#include<sstream>
#include<xaudio2.h>
#include <format>
#include<mfapi.h>
#include <mfidl.h>
#include<mfreadwrite.h>
#include "Input.h"
#include"Logger.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Camera.h"
#include "CameraManager.h"
#include "Game.h"

#include "externals/DirectXTex/DirectXTex.h"
#include"externals/DirectXTex/d3dx12.h"

#include "SpriteCommon.h"
#include "Sprite.h"

#include "Vector2.h"
#include"Vector3.h"
#include"Vector4.h"
#include"Matrix4x4.h"
#include"Transform.h"
#include"MyMath.h"
#include "ModelCommon.h"
#include"Model.h"
#include"Audio.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include"ImGuiManager.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"

#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"mfplat.lib")
#pragma comment(lib,"Mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")

using namespace Microsoft::WRL;
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

Microsoft::WRL::ComPtr < ID3D12Resource> CreateDepthStencilTextureResoource(Microsoft::WRL::ComPtr < ID3D12Device> device, int32_t width, int32_t height)
{
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;//Textureの幅
	resourceDesc.Height = height;//Textureの高さ
	resourceDesc.MipLevels = 1;//mipmapの数
	resourceDesc.DepthOrArraySize = 1;//奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//DepthStencilとして使う通知

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//VRAM上に作る

	//深度数のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;//1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//フォーマット。Resourceと合わせる

	//Resourceの生成
	Microsoft::WRL::ComPtr < ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

HRESULT Present(
	UINT SyncInterval,
	UINT Flags
);

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	//誰も補足しなかった場合に(Unhandled),補足する関数を登録
	//main関数はじまってすぐに登録すると良い
	SetUnhandledExceptionFilter(ExportDump);

	struct D3DResourceLeakChecker
	{
		~D3DResourceLeakChecker()
		{
			Microsoft::WRL::ComPtr<IDXGIFactory7>dxgiFactory;
			Microsoft::WRL::ComPtr<ID3D12Device>device;
		}
	};
	
	Game* game = new Game();
	game->Initialize();

	MSG msg{};
	//ウィンドウのxボタンが押されるまでループ
	while (true)
	{//ウィンドウにメッセージが来てたら最優先で処理される
		if (winApp->ProcessMessage())
		{
			break;
		}

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


		for (Object3d* object3d : objects) {
			//毎フレーム、マネージャから今のアクティブカメラをもらう
			object3d->SetCamera(cameraManager->GetActiveCamera());
			lightData.direction = Normalize(lightData.direction);
			object3d->SetDirectionalLight(lightData);//光を他のモデルにも分け与える

			object3d->Update();

		}

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

		//描画前処理
		dxCommon->PreDraw();
		srvManager->PreDraw();

		//3Dオブジェクトの描画準備3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
		object3dCommon->SetCommonDrawSetting();

		// Object のみ描画
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

	//GPUの完了待ち
	dxCommon->WaitForGPU();

	//XAudio2解放
	xAudio2.Reset();
	//音声データ解放
	SoundUnload(&soundData1);

	//Windows Media Foundationの処理
	result = MFShutdown();
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

#ifdef _DEBUG
#endif


	winApp->Finalize();

	delete input;
	delete cameraManager;
	delete dxCommon;
	delete winApp;

	return 0;
}