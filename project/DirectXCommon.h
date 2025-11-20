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

	void fence();

	void viewportRect();

	void scissorRect();

	void dxcCompiler();

	void ImGui();

	//描画前処理
	void PreDraw();
	//描画後処理
	void PostDraw();


	//SRVの指定番号のCPUデスクリプタハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);

	//SRVの指定番号のGPUデスクリプタハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

	//スワップチェーンリソース
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2>swapChainResource;

	//バッファリソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource>CreateBufferResource(size_t sizeInBytes);

	//テクスチャリソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource>CreateTextureResource(const DirectX::TexMetadata& metadata);

	//テクスチャデータの転送
	void UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);

	//テクスチャファイルの読み込み
	static DirectX::ScratchImage UploadTexture(const std::string& filePath);

	//getter
	ID3D12Device* GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList()const { return commandList_.Get(); }

private:

	Microsoft::WRL::ComPtr<ID3D12Device>device;
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

	//DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device_;

	Microsoft::WRL::ComPtr < ID3D12GraphicsCommandList> commandList_;
	//DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;

	//フェンス値
	UINT64 fenceVal = 0;

	//スワップチェーンを生成する
	Microsoft::WRL::ComPtr < IDXGISwapChain4> swapChain;


	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	Microsoft::WRL::ComPtr < IDxcBlob> CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile);

	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;

	ComPtr<ID3D12Resource> depthStencilResource;
	UINT descriptorSizeSRV;
	UINT descriptorSizeRTV;
	UINT descriptorSizeDSV;

	HRESULT hr;

	//WindowsAPI
	WinApp* winApp = nullptr;

	//指定番号のCPUデスクリプタハンドルを取得する
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComposableBase<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	//指定番号のGPUデスクリプタハンドルを取得する
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComposableBase<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	//FPS固定初期化
	void InitializeFixFPS();
	//FPS固定更新
	void UpdateFixFPS();

	//記録時間(FPS固定用)
	std::chrono::steady_clock::time_point reference_;

	uint32_t width;
	uint32_t height;
};

