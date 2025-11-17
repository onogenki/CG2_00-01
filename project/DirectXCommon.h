#pragma once
#include <d3d12.h>
#include<dxgi1_6.h>
#include<wrl.h>
#include "WinApp.h"

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
	std::array<Microsoft::WRL::Comptr<ID3D12Resource>, 2>swapChainRsource;

	//フェンス値
	UINT64 fenceVal = 0;

private:

	//DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	//DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	//WindowsAPI
	WinApp* winApp = nullptr;

	//指定番号のCPUデスクリプタハンドルを取得する
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComposableBase<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	//指定番号のGPUデスクリプタハンドルを取得する
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComposableBase<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

};

