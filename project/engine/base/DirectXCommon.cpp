#include "DirectXCommon.h"
#include "Input.h"
#include"Logger.h"
#include"StringUtility.h"
#include "SrvManager.h"
#include "externals/DirectXTex/d3dx12.h"
#include<algorithm>
#include<cassert>
#include <cstdint>
#include <cstring>
#include <tmmintrin.h>
#include <thread>
#include<vector>
using namespace Logger;

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
using namespace Microsoft::WRL;

void DirectXCommon::Initialize(WinApp* winApp)
{
	//NULL検出
	assert(winApp);

	//メンバ変数に記録
	this->winApp = winApp;
	width = winApp->GetClientWidth();
	height = winApp->GetClientHeight();

	//FPS固定初期化
	InitializeFixFPS();

	//デバイスの生成();
	InitializeDevice();
	//コマンド関連の初期化();
	commandList();
	//スワップチェーンの生成();
	SwapChain();
	//深度バッファの生成();
	depthBuffer();
	//各種デスクリプタヒープの生成();
	DescriptorHeap();
	//レンダーターゲットビューの初期化();
	RenderTargetView();
	//深度ステンシルビューの初期化();
	DepthStencilView();
	//フェンスの初期化();
	CreateFence();
	//ビューポート矩形の初期化();
	viewportRect();
	//シザリング矩形の初期化();
	scissorRect();
	//DXCコンパイラの生成();
	CreateDxcCompiler();
}

void DirectXCommon::ResizeIfNeeded()
{
	const uint32_t newWidth = winApp->GetClientWidth();
	const uint32_t newHeight = winApp->GetClientHeight();
	if (newWidth == 0 || newHeight == 0 || (newWidth == width && newHeight == height)) {
		return;
	}

	WaitForGPU();
	for (auto& swapChainResource : swapChainResources) {
		swapChainResource.Reset();
	}
	renderTextureResource_.Reset();
	postEffectTextureResource_.Reset();
	depthStencilResource.Reset();
	resource.Reset();

	width = newWidth;
	height = newHeight;
	const HRESULT resizeResult = swapChain->ResizeBuffers(
		static_cast<UINT>(GetSwapChainResourcesNum()), width, height,
		swapChainDesc_.Format, swapChainDesc_.Flags);
	assert(SUCCEEDED(resizeResult));

	for (UINT i = 0; i < GetSwapChainResourcesNum(); ++i) {
		const HRESULT getBufferResult = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainResources[i]));
		assert(SUCCEEDED(getBufferResult));
	}

	RenderTargetView();
	depthBuffer();
	DepthStencilView();
	viewportRect();
	scissorRect();
	CreateRenderTextureSRV(SrvManager::GetInstance());
	isRenderTextureShaderResource_ = false;
	isPostEffectTextureShaderResource_ = false;
}

void DirectXCommon::InitializeDevice()//デバイスの初期化
{
	HRESULT hr;

#if defined(_DEBUG) && defined(ENABLE_D3D12_DEBUG_LAYER)

	ComPtr < ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		//デバックレイヤーを有効化する
		debugController->EnableDebugLayer();
	}
#endif
	//HRESULTはWindows系のエラーコードであり、
	//関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	//初期化の根本的な部分でエラーが出た場合はプログラムが間違ってるか、
	// どうにもできない場合が多いのでassertにしておく
	assert(SUCCEEDED(hr));

	ComPtr < IDXGIAdapter4> useAdapter = nullptr;
	//良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
		i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i)
	{
		//アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));//取得できないのは一大事
		//ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
		{
			//採用したアダプタの情報をログに出力。wstringの方なので注意
			Logger::Log(StringUtility::ConvertString(std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr;//ソフトウェアアダプタの場合は見なかったことにする
	}
	assert(useAdapter != nullptr);

	//機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] =
	{
	D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};

	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };

	//高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i)
	{
		//採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		//指定した機能レベルでデバイスが生成できたのでログ出力を行ってループを抜ける
		if (SUCCEEDED(hr))
		{
			Logger::Log(std::format("Feature:{}\n", featureLevelStrings[i]));
			break;
		}
	}
	//デバイスの生成がうまくいかなかったので起動できない
	assert(device_ != nullptr);

#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		//ヤバイエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		//緊急時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] =
		{
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
		//抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		//指定したメッセージの表示の描画を抑制する
		infoQueue->PushStorageFilter(&filter);

	}
#endif
}

void DirectXCommon::commandList()
{
	//コマンドアロケータ
	for (auto& commandAllocator : commandAllocators_) {
		hr = device_->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(commandAllocator.GetAddressOf())
		);
		assert(SUCCEEDED(hr));
	}

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device_->CreateCommandQueue(&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue));
	//コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
	//描画用のDescriptorHeapの設定

	//コマンドリストの実行
}

void DirectXCommon::SwapChain()
{
	//スワップチェーン
	//コマンドを生成する
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators_[frameIndex_].Get(), nullptr,
		IID_PPV_ARGS(&commandList_));
	assert(SUCCEEDED(hr));
	swapChainDesc_.Width = WinApp::kClientWidth;//画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc_.Height = WinApp::kClientHeight;//画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//色の形式
	swapChainDesc_.SampleDesc.Count = 1;//マルチサンプルしない
	swapChainDesc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;//描画のターゲットとして利用する
	swapChainDesc_.BufferCount = 2;//ダブルバッファ
	swapChainDesc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//モニタにうつしたら、中身を破棄
	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	swapChainDesc_.Width = width;
	swapChainDesc_.Height = height;
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), winApp->GetHwnd(), &swapChainDesc_, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

	//SwapChainからResourceを引っ張ってくる
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	//うまく取得できなければ起動できない
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));
}

void DirectXCommon::depthBuffer()
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

	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	depthStencilResource = resource;


}

void DirectXCommon::DescriptorHeap()
{
	//RTV,SRV,DSVデスクリプタヒープの生成
	//DescriptorSizeを取得しておく
	descriptorSizeSRV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	//ディスクリプタヒープの生成
	//RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	// SwapChainの2枚にRenderTextureの1枚を加え、RTVを3個確保する
	rtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 4, false);
	//SRV用のヒープでディスクリプタのの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	srvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	//DSV用のヒープでディスクリプタの数は1。DSVはShader内で触れるものではないので、ShaderVisibleはfalse
	dsvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
}

void DirectXCommon::RenderTargetView()
{//レンダーターゲットビューの初期化
	 // RTV 設定
	 
	// インクリメントサイズ
	UINT increment = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	 
	// ディスクリプタの先頭
	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	rtvDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc_.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// 2つのバックバッファに対して RTV を作成
	for (int i = 0; i < 2; ++i)
	{
		assert(swapChainResources[i]);
		device_->CreateRenderTargetView(
			swapChainResources[i].Get(),
			&rtvDesc_,
			handle
		);

		// 次のディスクリプタ位置へ
		rtvHandle_[i] = handle;

		handle.ptr += increment;
	}

	// Sceneを描画するRenderTextureをSwapChainと同じ大きさ・Formatで生成する
	renderTextureResource_ = CreateRenderTextureResource(
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		renderTextureClearColor_);

	// RTVヒープの3個目にRenderTexture用RTVを生成する
	renderTextureRtvHandle_ = handle;
	device_->CreateRenderTargetView(
		renderTextureResource_.Get(),
		&rtvDesc_,
		renderTextureRtvHandle_);

	handle.ptr += increment;

	postEffectTextureResource_ = CreateRenderTextureResource(
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		renderTextureClearColor_);

	postEffectTextureRtvHandle_ = handle;
	device_->CreateRenderTargetView(
		postEffectTextureResource_.Get(),
		&rtvDesc_,
		postEffectTextureRtvHandle_);
}

void DirectXCommon::DepthStencilView()
{
	assert(device_);
	assert(dsvDescriptorHeap);
	assert(depthStencilResource);
	//深度ステンシルビューの初期化
	//描画先のRTVとDSVを設定する
	dsvHandle_ = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	//DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2dTexture

	//DSVHeapの先頭にDSVを作る
	device_->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

}

void DirectXCommon::CreateFence()
{
	hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	//FenceのSignalを待つためのイベントを作成する
	assert(fenceEvent_ != nullptr);
}

void DirectXCommon::viewportRect()
{
	//ビューポート矩形の設定

	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::scissorRect()
{
	//基本的にはビューポートと同じ短形が構成されるようにする
	scissorRect_.left = 0;
	scissorRect_.right = static_cast<LONG>(width);
	scissorRect_.top = 0;
	scissorRect_.bottom = static_cast<LONG>(height);
}


void DirectXCommon::CreateDxcCompiler()
{
	//DXCコンパイラの生成
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::PreDraw()
{
	// 2フレーム目以降は、読み取り状態からSceneを描ける状態へ戻す
	if (isRenderTextureShaderResource_)
	{

	//TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier{};
	//今回のバリアはTransition
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//Noneにしておく
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//バリアを張る対象のリソース。現在のバックバッファに対して行う
	barrier.Transition.pResource = renderTextureResource_.Get();
	//遷移前(現在)のResourceState
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	//遷移後のResourceState
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	//TransitionBarrierを張る
	commandList_->ResourceBarrier(1, &barrier);
		isRenderTextureShaderResource_ = false;
	}

	// Sceneの描画先をRenderTextureにする。Sceneでは深度も使用する
	commandList_->OMSetRenderTargets(1, &renderTextureRtvHandle_, false, &dsvHandle_);

	// Resource生成時のClearValueと同じ赤色でRenderTextureをクリアする
	const float clearColor[] = {
		renderTextureClearColor_.x,
		renderTextureClearColor_.y,
		renderTextureClearColor_.z,
		renderTextureClearColor_.w
	};
	commandList_->ClearRenderTargetView(renderTextureRtvHandle_, clearColor, 0, nullptr);

	//指定した深度で画面全体をクリアする
	commandList_->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	commandList_->RSSetViewports(1, &viewport_);

	commandList_->RSSetScissorRects(1, &scissorRect_);//scirssorを設定

}

void DirectXCommon::PreDrawForPostEffectTexture()
{
	if (!isRenderTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER renderTextureBarrier{};
		renderTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		renderTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		renderTextureBarrier.Transition.pResource = renderTextureResource_.Get();
		renderTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		renderTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		renderTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &renderTextureBarrier);
		isRenderTextureShaderResource_ = true;
	}

	if (isPostEffectTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER postEffectTextureBarrier{};
		postEffectTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		postEffectTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		postEffectTextureBarrier.Transition.pResource = postEffectTextureResource_.Get();
		postEffectTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		postEffectTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		postEffectTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &postEffectTextureBarrier);
		isPostEffectTextureShaderResource_ = false;
	}

	commandList_->OMSetRenderTargets(1, &postEffectTextureRtvHandle_, false, nullptr);

	const float clearColor[] = {
		renderTextureClearColor_.x,
		renderTextureClearColor_.y,
		renderTextureClearColor_.z,
		renderTextureClearColor_.w
	};
	commandList_->ClearRenderTargetView(postEffectTextureRtvHandle_, clearColor, 0, nullptr);

	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PreDrawForSwapChain(bool usePostEffectTexture)
{
	// Sceneの描画結果を後でSRVから読めるよう、RenderTextureを書き込み状態から読み取り状態へ変更する
	if (!isRenderTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER renderTextureBarrier{};
		renderTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		renderTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		renderTextureBarrier.Transition.pResource = renderTextureResource_.Get();
		renderTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		renderTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		renderTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &renderTextureBarrier);
		isRenderTextureShaderResource_ = true;
	}

	if (usePostEffectTexture && !isPostEffectTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER postEffectTextureBarrier{};
		postEffectTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		postEffectTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		postEffectTextureBarrier.Transition.pResource = postEffectTextureResource_.Get();
		postEffectTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		postEffectTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		postEffectTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &postEffectTextureBarrier);
		isPostEffectTextureShaderResource_ = true;
	}

	// ImGuiを表示するため、SwapChainを描画可能な状態へ変更する
	const UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
	D3D12_RESOURCE_BARRIER swapChainBarrier{};
	swapChainBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	swapChainBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	swapChainBarrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	swapChainBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	swapChainBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	swapChainBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList_->ResourceBarrier(1, &swapChainBarrier);

	// SwapChainにはImGuiだけを描画する。Scene用の深度は不要なのでDSVはnullptrにする
	commandList_->OMSetRenderTargets(1, &rtvHandle_[backBufferIndex], false, nullptr);

	// ImGuiの背景となるSwapChainを青色でクリアする
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	commandList_->ClearRenderTargetView(rtvHandle_[backBufferIndex], clearColor, 0, nullptr);

	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PostDraw()
{
	//バックバッファの番号取得
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	//TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier{};
	//今回のバリアはTransition
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//Noneにしておく
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//バリアを張る対象のリソース。現在のバックバッファに対して行う
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	//遷移前(現在)のResourceState
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//遷移後のResourceState
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	
	//TransitionBarrierを張る
	commandList_->ResourceBarrier(1, &barrier);

	hr = GetCommandList()->Close();
	assert(SUCCEEDED(hr));

	//GPUにコマンドリストの実行を行わせる
	Microsoft::WRL::ComPtr < ID3D12CommandList> commandLists[] = { GetCommandList() };
	commandQueue->ExecuteCommandLists(1, commandLists->GetAddressOf());
	//GPUとOSに画面の交渉を行うように通知する
	swapChain->Present(1, 0);

	//Fenceの値を更新
	const UINT64 signalValue = ++fenceVal;
	//GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	commandQueue->Signal(fence.Get(), signalValue);
	frameFenceValues_[frameIndex_] = signalValue;
	//Fenceの値が指定したSignal値にたどり着いているか確認する
	//GetCompletedValueの初期値はFence作成時に渡した初期値
	frameIndex_ = swapChain->GetCurrentBackBufferIndex();
	if (frameFenceValues_[frameIndex_] != 0 && fence->GetCompletedValue() < frameFenceValues_[frameIndex_])
	{
		//指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
		fence->SetEventOnCompletion(frameFenceValues_[frameIndex_], fenceEvent_);
		//イベント待つ
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	//次のフレーム用のコマンドリストを準備
	hr = commandAllocators_[frameIndex_]->Reset();
	assert(SUCCEEDED(hr));
	hr = GetCommandList()->Reset(commandAllocators_[frameIndex_].Get(), nullptr);
	assert(SUCCEEDED(hr));

	//FPS固定
	UpdateFixFPS();

}


D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index)
{
	return GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index)
{
	return GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}

void DirectXCommon::InitializeFixFPS()
{
	//現在時間を記録する
	reference_ = std::chrono::steady_clock::now();

	//1/60秒ぴったりの時間
	kMinTime_ = std::chrono::microseconds(uint64_t(1000000.0f / 60.0f));
	//1/60秒よりわずかに短い時間
	kMinCheckTime_ = std::chrono::microseconds(uint64_t(1000000.0f / 65.0f));
}

void DirectXCommon::UpdateFixFPS()
{
	const std::chrono::steady_clock::time_point target = reference_ + kMinTime_;
	std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
	if (currentTime < target) {
		// Sleepの粒度で大きく遅れないよう、最後の1msだけ短く待つ。
		const std::chrono::steady_clock::time_point sleepUntil = target - std::chrono::milliseconds(1);
		if (currentTime < sleepUntil) {
			std::this_thread::sleep_until(sleepUntil);
		}
		while (std::chrono::steady_clock::now() < target) {
			std::this_thread::yield();
		}
	}
	const std::chrono::steady_clock::time_point frameEndTime = std::chrono::steady_clock::now();
	const std::chrono::duration<float> elapsedTime = frameEndTime - reference_;
	deltaTime_ = std::clamp(elapsedTime.count(), 0.0f, 0.1f);
	reference_ = frameEndTime;
	return;

	//現在時間を取得する
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	//前回記録からの経過時間を取得する
	std::chrono::microseconds elapsed =
		std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

	//1/60秒(よりわずかに短い時間)経ってない場合
	if (elapsed < kMinCheckTime_)
	{//1/60秒経過するまで微小なスリープを繰り返す
		while (std::chrono::steady_clock::now() - reference_ < kMinTime_)
		{
			//1マイクロ秒スリープ
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
	//現在の時間を記録する
	reference_ = std::chrono::steady_clock::now();
}


void DirectXCommon::WaitForGPU() {
	// 1. フェンス値を加算してシグナルを送る
	fenceVal++;
	commandQueue->Signal(fence.Get(), fenceVal);

	// 2. GPUがその値に到達するまで待機する
	if (fence->GetCompletedValue() < fenceVal) {
		fence->SetEventOnCompletion(fenceVal, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}
	for (UINT64& frameFenceValue : frameFenceValues_) {
		frameFenceValue = fenceVal;
	}
}


bool DirectXCommon::CaptureGameTexturePixels(std::vector<unsigned char>& pixels, int& outWidth, int& outHeight, bool usePostEffectTexture)
{
	ID3D12Resource* sourceResource = usePostEffectTexture
		? postEffectTextureResource_.Get()
		: renderTextureResource_.Get();
	if (!sourceResource || !device_ || !commandList_ || !commandQueue) {
		return false;
	}

	const D3D12_RESOURCE_DESC sourceDesc = sourceResource->GetDesc();
	if (sourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		sourceDesc.Width == 0 || sourceDesc.Height == 0 ||
		sourceDesc.DepthOrArraySize != 1 || sourceDesc.MipLevels == 0) {
		return false;
	}

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT numRows = 0;
	UINT64 rowSizeInBytes = 0;
	UINT64 totalBytes = 0;
	device_->GetCopyableFootprints(&sourceDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);
	if (totalBytes == 0 || numRows == 0 || rowSizeInBytes == 0) {
		return false;
	}

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC readbackDesc{};
	readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	readbackDesc.Width = totalBytes;
	readbackDesc.Height = 1;
	readbackDesc.DepthOrArraySize = 1;
	readbackDesc.MipLevels = 1;
	readbackDesc.SampleDesc.Count = 1;
	readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Microsoft::WRL::ComPtr<ID3D12Resource> readbackResource;
	HRESULT result = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&readbackDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&readbackResource));
	if (FAILED(result)) {
		return false;
	}

	const bool sourceWasShaderResource = usePostEffectTexture
		? isPostEffectTextureShaderResource_
		: isRenderTextureShaderResource_;
	const D3D12_RESOURCE_STATES sourceStateBefore = sourceWasShaderResource
		? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		: D3D12_RESOURCE_STATE_RENDER_TARGET;

	D3D12_RESOURCE_BARRIER toCopySource{};
	toCopySource.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toCopySource.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	toCopySource.Transition.pResource = sourceResource;
	toCopySource.Transition.StateBefore = sourceStateBefore;
	toCopySource.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	toCopySource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList_->ResourceBarrier(1, &toCopySource);

	D3D12_TEXTURE_COPY_LOCATION copySource{};
	copySource.pResource = sourceResource;
	copySource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	copySource.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION copyDestination{};
	copyDestination.pResource = readbackResource.Get();
	copyDestination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	copyDestination.PlacedFootprint = footprint;

	commandList_->CopyTextureRegion(&copyDestination, 0, 0, 0, &copySource, nullptr);

	D3D12_RESOURCE_BARRIER restoreState = toCopySource;
	restoreState.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	restoreState.Transition.StateAfter = sourceStateBefore;
	commandList_->ResourceBarrier(1, &restoreState);

	result = commandList_->Close();
	if (FAILED(result)) {
		return false;
	}

	ID3D12CommandList* commandLists[] = { commandList_.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);
	WaitForGPU();

	outWidth = static_cast<int>(sourceDesc.Width);
	outHeight = static_cast<int>(sourceDesc.Height);
	const size_t tightImageSize = static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight) * 4;
	pixels.resize(tightImageSize);
	unsigned char* mappedData = nullptr;
	D3D12_RANGE readRange{ 0, totalBytes };
	result = readbackResource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
	if (FAILED(result) || !mappedData) {
		commandAllocators_[frameIndex_]->Reset();
		commandList_->Reset(commandAllocators_[frameIndex_].Get(), nullptr);
		return false;
	}

	const size_t sourceRowPitch = static_cast<size_t>(footprint.Footprint.RowPitch);
	const size_t tightRowPitch = static_cast<size_t>(outWidth) * 4;
	for (int y = 0; y < outHeight; ++y) {
		const unsigned char* sourceRow = mappedData + footprint.Offset + static_cast<size_t>(y) * sourceRowPitch;
		unsigned char* destinationRow = pixels.data() + static_cast<size_t>(y) * tightRowPitch;
		for (int x = 0; x < outWidth; ++x) {
			const unsigned char* sourcePixel = sourceRow + static_cast<size_t>(x) * 4;
			unsigned char* destinationPixel = destinationRow + static_cast<size_t>(x) * 4;
			destinationPixel[0] = sourcePixel[2];
			destinationPixel[1] = sourcePixel[1];
			destinationPixel[2] = sourcePixel[0];
			destinationPixel[3] = sourcePixel[3];
		}
	}
	D3D12_RANGE writtenRange{ 0, 0 };
	readbackResource->Unmap(0, &writtenRange);

	result = commandAllocators_[frameIndex_]->Reset();
	if (FAILED(result)) {
		return false;
	}
	result = commandList_->Reset(commandAllocators_[frameIndex_].Get(), nullptr);
	return SUCCEEDED(result);
}

bool DirectXCommon::QueueGameTextureCapture(bool usePostEffectTexture)
{
	ID3D12Resource* sourceResource = usePostEffectTexture
		? postEffectTextureResource_.Get()
		: renderTextureResource_.Get();
	if (!sourceResource || !device_ || !commandQueue || !fence) {
		return false;
	}

	CaptureReadbackSlot* slot = nullptr;
	for (CaptureReadbackSlot& candidate : captureReadbackSlots_) {
		if (!candidate.pending) {
			slot = &candidate;
			break;
		}
	}
	if (!slot) {
		return false;
	}

	const D3D12_RESOURCE_DESC sourceDesc = sourceResource->GetDesc();
	if (sourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		sourceDesc.Width == 0 || sourceDesc.Height == 0 ||
		sourceDesc.DepthOrArraySize != 1 || sourceDesc.MipLevels == 0) {
		return false;
	}

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT numRows = 0;
	UINT64 rowSizeInBytes = 0;
	UINT64 totalBytes = 0;
	device_->GetCopyableFootprints(
		&sourceDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);
	if (totalBytes == 0 || numRows == 0 || rowSizeInBytes == 0) {
		return false;
	}

	if (!slot->readbackResource || slot->totalBytes != totalBytes) {
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC readbackDesc{};
		readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		readbackDesc.Width = totalBytes;
		readbackDesc.Height = 1;
		readbackDesc.DepthOrArraySize = 1;
		readbackDesc.MipLevels = 1;
		readbackDesc.SampleDesc.Count = 1;
		readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		slot->readbackResource.Reset();
		const HRESULT createResult = device_->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&readbackDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&slot->readbackResource));
		if (FAILED(createResult)) {
			return false;
		}
	}

	HRESULT result = S_OK;
	if (!slot->commandAllocator) {
		result = device_->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&slot->commandAllocator));
		if (FAILED(result)) {
			return false;
		}
	}
	if (!slot->commandList) {
		result = device_->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			slot->commandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&slot->commandList));
	} else {
		result = slot->commandAllocator->Reset();
		if (SUCCEEDED(result)) {
			result = slot->commandList->Reset(slot->commandAllocator.Get(), nullptr);
		}
	}
	if (FAILED(result)) {
		return false;
	}

	const bool sourceWasShaderResource = usePostEffectTexture
		? isPostEffectTextureShaderResource_
		: isRenderTextureShaderResource_;
	const D3D12_RESOURCE_STATES sourceStateBefore = sourceWasShaderResource
		? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		: D3D12_RESOURCE_STATE_RENDER_TARGET;

	D3D12_RESOURCE_BARRIER toCopySource{};
	toCopySource.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toCopySource.Transition.pResource = sourceResource;
	toCopySource.Transition.StateBefore = sourceStateBefore;
	toCopySource.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	toCopySource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	slot->commandList->ResourceBarrier(1, &toCopySource);

	D3D12_TEXTURE_COPY_LOCATION copySource{};
	copySource.pResource = sourceResource;
	copySource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	copySource.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION copyDestination{};
	copyDestination.pResource = slot->readbackResource.Get();
	copyDestination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	copyDestination.PlacedFootprint = footprint;
	slot->commandList->CopyTextureRegion(&copyDestination, 0, 0, 0, &copySource, nullptr);

	D3D12_RESOURCE_BARRIER restoreState = toCopySource;
	restoreState.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	restoreState.Transition.StateAfter = sourceStateBefore;
	slot->commandList->ResourceBarrier(1, &restoreState);

	result = slot->commandList->Close();
	if (FAILED(result)) {
		return false;
	}
	ID3D12CommandList* commandLists[] = { slot->commandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);
	const UINT64 signalValue = ++fenceVal;
	result = commandQueue->Signal(fence.Get(), signalValue);
	if (FAILED(result)) {
		return false;
	}

	slot->footprint = footprint;
	slot->totalBytes = totalBytes;
	slot->fenceValue = signalValue;
	slot->sequence = ++captureSequence_;
	slot->width = static_cast<int>(sourceDesc.Width);
	slot->height = static_cast<int>(sourceDesc.Height);
	slot->pending = true;
	return true;
}

bool DirectXCommon::TryGetGameTextureCapturePixels(
	std::vector<unsigned char>& pixels,
	int& outWidth,
	int& outHeight)
{
	CaptureReadbackSlot* oldestSlot = nullptr;
	for (CaptureReadbackSlot& candidate : captureReadbackSlots_) {
		if (candidate.pending && (!oldestSlot || candidate.sequence < oldestSlot->sequence)) {
			oldestSlot = &candidate;
		}
	}
	if (!oldestSlot || !fence || fence->GetCompletedValue() < oldestSlot->fenceValue) {
		return false;
	}

	outWidth = oldestSlot->width;
	outHeight = oldestSlot->height;
	const size_t tightImageSize =
		static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight) * 4;
	pixels.resize(tightImageSize);

	unsigned char* mappedData = nullptr;
	D3D12_RANGE readRange{ 0, oldestSlot->totalBytes };
	const HRESULT mapResult = oldestSlot->readbackResource->Map(
		0, &readRange, reinterpret_cast<void**>(&mappedData));
	if (FAILED(mapResult) || !mappedData) {
		pixels.clear();
		outWidth = 0;
		outHeight = 0;
		oldestSlot->pending = false;
		return true;
	}

	const size_t sourceRowPitch =
		static_cast<size_t>(oldestSlot->footprint.Footprint.RowPitch);
	const size_t tightRowPitch = static_cast<size_t>(outWidth) * 4;
	const __m128i bgraShuffle = _mm_setr_epi8(
		2, 1, 0, 3,
		6, 5, 4, 7,
		10, 9, 8, 11,
		14, 13, 12, 15);
	const __m128i opaqueAlpha = _mm_setr_epi8(
		0, 0, 0, static_cast<char>(0xFF),
		0, 0, 0, static_cast<char>(0xFF),
		0, 0, 0, static_cast<char>(0xFF),
		0, 0, 0, static_cast<char>(0xFF));
	for (int y = 0; y < outHeight; ++y) {
		const unsigned char* sourceRow =
			mappedData + oldestSlot->footprint.Offset + static_cast<size_t>(y) * sourceRowPitch;
		unsigned char* destinationRow = pixels.data() + static_cast<size_t>(y) * tightRowPitch;
		int x = 0;
		for (; x + 4 <= outWidth; x += 4) {
			const __m128i rgba = _mm_loadu_si128(reinterpret_cast<const __m128i*>(
				sourceRow + static_cast<size_t>(x) * 4));
			const __m128i bgra = _mm_or_si128(_mm_shuffle_epi8(rgba, bgraShuffle), opaqueAlpha);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(
				destinationRow + static_cast<size_t>(x) * 4), bgra);
		}
		for (; x < outWidth; ++x) {
			uint32_t sourcePixel = 0;
			std::memcpy(&sourcePixel, sourceRow + static_cast<size_t>(x) * 4, sizeof(sourcePixel));
			const uint32_t destinationPixel =
				0xFF000000u |
				((sourcePixel & 0x000000FFu) << 16) |
				(sourcePixel & 0x0000FF00u) |
				((sourcePixel & 0x00FF0000u) >> 16);
			std::memcpy(destinationRow + static_cast<size_t>(x) * 4, &destinationPixel, sizeof(destinationPixel));
		}
	}
	D3D12_RANGE writtenRange{ 0, 0 };
	oldestSlot->readbackResource->Unmap(0, &writtenRange);
	oldestSlot->pending = false;
	return true;
}

Microsoft::WRL::ComPtr < IDxcBlob> DirectXCommon::CompileShader(
	//CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	//Compilerに使用するProfile
	const wchar_t* profile
)
{
	//これからシェーダーをコンバイルする旨をログに出す
	Log(StringUtility::ConvertString(std::format(L"Begin CompileShader,path:{},profile:{}\n", filePath, profile)));
	//hlslファイルを読む
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	//読めなかったら止める
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;//UTF8の文字コードであることを通知

	LPCWSTR arguments[] = {
		filePath.c_str(),//コンバイル対象のh|s|ファイル名
		L"-E",L"main",//エントリーポイントの指定。基本的にmain以外にはしない
		L"-T",profile,//shaderProfileの設定
		L"-Zi",L"-Qembed_debug",//デバック用の情報を埋め込む
		L"-Od",//最適解を外しておく
		L"-Zpr",//メモリレイアウトは行優先
	};
	Microsoft::WRL::ComPtr < IDxcResult> shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,//読み込んだファイル
		arguments,//コンバイルオプション
		_countof(arguments),//コンバイルオプションの数
		includeHandler.Get(),//includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult)//コンパイル結果
	);
	//コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	//警告・エラーが出てたらログに出して止める
	Microsoft::WRL::ComPtr < IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0)
	{
		Log(shaderError->GetStringPointer());
		//警告・エラーダメ絶対
		assert(false);
	}
	//コンパイル結果から実行用のバイナリ部分を取得
	Microsoft::WRL::ComPtr < IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	//成功したログを出す
	Log(StringUtility::ConvertString(std::format(L"Compile,Succeeded,path:{},profile:{}\n", filePath, profile)));
	//実行用のバイナリを返却
	return shaderBlob;
}


Microsoft::WRL::ComPtr < ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	UINT numDescriptors,
	bool shaderVisible)
{
	Microsoft::WRL::ComPtr < ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	
	HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));

	return descriptorHeap;

}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(Microsoft::WRL::ComPtr < ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(Microsoft::WRL::ComPtr < ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

const uint32_t DirectXCommon::kMaxSRVCount = 512;

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata)
{
	// 1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width); // Textureの幅
	resourceDesc.Height = UINT(metadata.height); // Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元など。基本2D。

	// 2. 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う

	// 3. Resourceを生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST, // データ転送される前提の初期状態
		nullptr, // Clear最適化設定。使わないのでnullptr。
		IID_PPV_ARGS(&resource)); // 作成するResourceへのポインタ
	assert(SUCCEEDED(hr));

	return resource;
}


Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateRenderTextureResource(
	uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor)
{
	// RenderTextureとして使う2Dテクスチャの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;// テクスチャの幅
	resourceDesc.Height = height;// テクスチャの高さ
	resourceDesc.MipLevels = 1;// ミップマップは使用しない
	resourceDesc.DepthOrArraySize = 1;// 2Dテクスチャ1枚分
	resourceDesc.Format = format;// RTVとSRVでも同じFormatを使う
	resourceDesc.SampleDesc.Count = 1;// マルチサンプリングは使用しない
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;// 2Dテクスチャとして生成する
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;// RTVとして書き込み可能にする

	// GPU上のVRAMにリソースを作るため、DEFAULT Heapを使用する
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// ClearRenderTargetViewで頻繁に使う色を最適化用のClearValueとして指定する
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = format;
	clearValue.Color[0] = clearColor.x;
	clearValue.Color[1] = clearColor.y;
	clearValue.Color[2] = clearColor.z;
	clearValue.Color[3] = clearColor.w;

	// RenderTargetとして使用することを前提に、初期状態をRENDER_TARGETにして生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,// 使用するHeapの設定
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,// RenderTextureの設定
		D3D12_RESOURCE_STATE_RENDER_TARGET,// 生成直後から描画先として使う
		&clearValue,// 最適化するクリア色
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	// 作成したRenderTextureを呼び出し元へ返す
	return resource;
}

void DirectXCommon::CreateRenderTextureSRV(SrvManager* srvManager)
{
	assert(srvManager);
	assert(renderTextureResource_);
	assert(postEffectTextureResource_);

	// RenderTextureの描画結果をShaderから読めるよう、SRVの場所を1個確保する
	if (renderTextureSrvIndex_ == UINT32_MAX) {
		renderTextureSrvIndex_ = srvManager->Allocate();
	}
	if (postEffectTextureSrvIndex_ == UINT32_MAX) {
		postEffectTextureSrvIndex_ = srvManager->Allocate();
	}

	// Resourceと同じFormat、ミップレベル1でTexture2D用SRVを生成する
	srvManager->CreateSRVforTexture2D(
		renderTextureSrvIndex_,
		renderTextureResource_.Get(),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		1,
		true);

	srvManager->CreateSRVforTexture2D(
		postEffectTextureSrvIndex_,
		postEffectTextureResource_.Get(),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		1,
		true);
}

//CPUのMap/memcpy
[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::WriteToIntermediateResource(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device)
{
	//テクスチャの情報を取得
	D3D12_RESOURCE_DESC desc = texture->GetDesc();

	//ミップマップの数だけ配列を用意する
	const size_t numSubresources = mipImages.GetImageCount();
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>footprints(numSubresources);
	std::vector<UINT> numRows(numSubresources);
	std::vector<UINT64>rowSizeInBytes(numSubresources);
	UINT64 totalBytes = 0;

	// DirectX12に配置図(Footprint)を計算してもらう
	device->GetCopyableFootprints(
		&desc,//テクスチャの設定
		0,//ミップレベル0から1つ分
		UINT(numSubresources),//結果の配置図がここに入る
		0,
		footprints.data(),
		numRows.data(),//行数(高さ)
		rowSizeInBytes.data(),//実際の1桁のデータサイズ(パディング抜き)
		&totalBytes//必要な中間バッファのサイズ
	);

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(totalBytes);

	uint8_t* mappedData = nullptr;
	intermediateResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));

	const DirectX::Image* images = mipImages.GetImages();
	for (size_t i = 0; i < numSubresources; ++i)
	{
		const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint = footprints[i];
		//書き込み先のスタート位置
		uint8_t* dest = mappedData + footprint.Offset;
		//読み込み元の画像データ
		const uint8_t* src = images[i].pixels;

		//1行ずつ、256バイトの隙間ルール(RowPitch)を考慮してコピー
		for (UINT y = 0; y < numRows[i]; ++y)
		{
			uint8_t* destRow = dest + (y * footprint.Footprint.RowPitch);
			const uint8_t* srcRow = src + (y * images[i].rowPitch);

			std::memcpy(destRow, srcRow, rowSizeInBytes[i]);
		}
	}

	//CPUでの書き込みが終わったのでここでUnmap
	intermediateResource->Unmap(0, nullptr);

	//作成してデータを入れた中間リソースを消さないように外へ返す
	return intermediateResource;
}


//GPUのCopyTextureRegion
void DirectXCommon::RecordTextureCopyCommand(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const Microsoft::WRL::ComPtr<ID3D12Resource>& intermediateResource,
	size_t numSubresources,ID3D12Device* device,ID3D12GraphicsCommandList* commandList)
{
	// footprints配置図はここでもう一度計算すれば前の関数から引き回さなくて済む
	D3D12_RESOURCE_DESC desc = texture->GetDesc();
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>footprints(numSubresources);
	// DirectX12にすべてのサブリソース（ミップマップ・配列面）の配置図を計算してもらう
	device->GetCopyableFootprints(
		&desc,//テクスチャの設定
		0,//ミップレベル0から1つ分
		UINT(numSubresources),//結果の配置図がここに入る
		0,
		footprints.data(),
		nullptr,
		nullptr,
		nullptr
	);

	// 全てのサブリソースをループで転送
	for (size_t i = 0; i < numSubresources; ++i)
	{
		// コピー先（VRAMテクスチャ）
		D3D12_TEXTURE_COPY_LOCATION dst{};
		dst.pResource = texture.Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = UINT(i);

		// コピー元（中間バッファの特定の位置）
		D3D12_TEXTURE_COPY_LOCATION src{};
		src.pResource = intermediateResource.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint = footprints[i];

		// ここでコマンドリストに転送命令を積む
		commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	}

	// Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCEへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	
	commandList->ResourceBarrier(1, &barrier);
}


Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う

	// 頂点リソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際にリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

void DirectXCommon::ExecuteTextureTransfer(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages)
{

	// メインの commandList_ を汚さないよう、この関数内だけで使う一時的なコマンドリストを作る
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> tempAllocator = nullptr;
	HRESULT hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator));
	assert(SUCCEEDED(hr));

	// 新しく作った tempAllocator を使って、一時的なコマンドリストを作成する
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> tempCommandList = nullptr;
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAllocator.Get(), nullptr, IID_PPV_ARGS(&tempCommandList));
	assert(SUCCEEDED(hr));

	// CPUの処理：中間バッファにデータを書き込んで、そのポインタを受け取る
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = WriteToIntermediateResource(texture, mipImages, device_.Get());

	// 転送コマンドを積む対象を、新しく作った「tempCommandList.Get()」にする
	RecordTextureCopyCommand(texture, intermediateResource, mipImages.GetImageCount(), device_.Get(), tempCommandList.Get());

	// 一時的なコマンドリストをCloseする
	hr = tempCommandList->Close();
	assert(SUCCEEDED(hr));

	// 実行するコマンドリストを「tempCommandList.Get()」にする
	ID3D12CommandList* commandLists[] = { tempCommandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);

	// GPUの実行完了を安全に待つ
	WaitForGPU();

}

DirectXCommon::~DirectXCommon() {
	if (fenceEvent_) {
		CloseHandle(fenceEvent_);
	}
}
