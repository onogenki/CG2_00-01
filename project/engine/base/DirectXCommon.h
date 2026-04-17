#pragma once
#include <d3d12.h>
#include<dxgi1_6.h>
#include<wrl.h>
#include <DirectXMath.h>
#include "WinApp.h"
#include <array>
#include "externals/DirectXTex/DirectXTex.h"
#include<string>
#include <dxcapi.h>
#include<chrono>

class DirectXCommon
{
public:

	void Initialize(WinApp* winApp);

	void InitializeDevice();

	void commandList();

	void SwapChain();

	void depthBuffer();

	void DescriptorHeap();

	void RenderTargetView();

	void DepthStencilView();

	void CreateFence();

	void viewportRect();

	void scissorRect();

	void CreateDxcCompiler();

	//描画前処理
	void PreDraw();
	//描画後処理
	void PostDraw();

	static DirectXCommon* GetInstance() {
		static DirectXCommon instance;
		return &instance;
	}

	DirectXCommon(const DirectXCommon&) = delete;
	DirectXCommon& operator=(const DirectXCommon&) = delete;


	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	//SRVの指定番号のCPUデスクリプタハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);

	//SRVの指定番号のGPUデスクリプタハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);


	Microsoft::WRL::ComPtr < IDxcBlob> CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile);

	//テクスチャリソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource>CreateTextureResource(const DirectX::TexMetadata& metadata);

	//テクスチャデータの転送
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);

	//getter
	ID3D12Device* GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList()const { return commandList_.Get(); }

	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator.Get(); }

	//最大SRV数(最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;

	//スワップチェーンリソースの数を取得
	size_t GetSwapChainResourcesNum() const { return 2;}



	//GPU待ち関数
	void WaitForGPU();

private:

	DirectXCommon() = default;
	~DirectXCommon();

	Microsoft::WRL::ComPtr<ID3D12Device>device_;
	Microsoft::WRL::ComPtr<IDXGIFactory7>dxgiFactory;
	Microsoft::WRL::ComPtr < ID3D12Debug1> debugController;
	Microsoft::WRL::ComPtr < IDXGIAdapter4> useAdapter;
	Microsoft::WRL::ComPtr < ID3D12InfoQueue> infoQueue;
	Microsoft::WRL::ComPtr < ID3D12CommandAllocator> commandAllocator;
	Microsoft::WRL::ComPtr < ID3D12CommandQueue> commandQueue;
	Microsoft::WRL::ComPtr < ID3D12Resource> resource;
	Microsoft::WRL::ComPtr < ID3D12Fence> fence;
	Microsoft::WRL::ComPtr < IDxcUtils> dxcUtils;
	Microsoft::WRL::ComPtr < IDxcCompiler3> dxcCompiler;
	//現時点でincludeはしないが、includeに対応するための設定を行っておく
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;

	Microsoft::WRL::ComPtr < ID3D12GraphicsCommandList> commandList_;

	//フェンス値
	UINT64 fenceVal = 0;

	//スワップチェーンを生成する
	Microsoft::WRL::ComPtr < IDXGISwapChain4> swapChain;
	Microsoft::WRL::ComPtr < ID3D12Resource> swapChainResources[2];

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc_;
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_;

	//Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;

	//フェンス生成
	uint64_t fenceValue_ = 0;

	HANDLE fenceEvent_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;
	UINT descriptorSizeSRV = 0;
	UINT descriptorSizeRTV = 0;
	UINT descriptorSizeDSV = 0;

	//描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_[2];

	//ビューポート
	D3D12_VIEWPORT viewport_;

	HRESULT hr = S_OK;

	//シザー短形
	D3D12_RECT scissorRect_;

	//WindowsAPI
	WinApp* winApp = nullptr;

	//FPS固定初期化
	void InitializeFixFPS();
	//FPS固定更新
	void UpdateFixFPS();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	//記録時間(FPS固定用)
	std::chrono::steady_clock::time_point reference_;
	std::chrono::microseconds kMinTime_;
	std::chrono::microseconds kMinCheckTime_;

	uint32_t width = WinApp::kClientWidth; // 幅・高さをメンバに
	uint32_t height = WinApp::kClientHeight;
};

