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

class DirectXCommon
{
public:

	void Initialize();

	//描画前処理
	void PreDraw();
	//描画後処理
	void PostDraw();


	//SRVの指定番号のCPUデスクリプタハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);

	//SRVの指定番号のGPUデスクリプタハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

	//スワップチェーンリソース
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2>swapChainRsource;

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


	//DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device_;

	Microsoft::WRL::ComPtr < ID3D12GraphicsCommandList> commandList_ = nullptr;
	//DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;

	//フェンス値
	UINT64 fenceVal = 0;

	//スワップチェーンを生成する
	Microsoft::WRL::ComPtr < IDXGISwapChain4> swapChain = nullptr;


	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	Microsoft::WRL::ComPtr < IDxcBlob> CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile);

	//WindowsAPI
	WinApp* winApp = nullptr;

	//指定番号のCPUデスクリプタハンドルを取得する
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComposableBase<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	//指定番号のGPUデスクリプタハンドルを取得する
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComposableBase<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

};

