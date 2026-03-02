#pragma once
#include "DirectXCommon.h"
#include <cassert>

class SrvManager
{
public:

	//初期化
	void Initialize(DirectXCommon* dxCommon);

	uint32_t Allocate();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);
private:
	DirectXCommon* directXCommon = nullptr;

	//最大SRV数(最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;

	//SRV用のデスクリプタサイズ
	uint32_t descriptorSize;

	//SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	//次に使用するSRVインデックス
	uint32_t useIndex = 0;

};

