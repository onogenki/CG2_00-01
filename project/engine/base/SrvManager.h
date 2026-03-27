#pragma once
#include "DirectXCommon.h"
#include <cassert>
#include<queue>

class SrvManager
{
public:

	// シングルトンインスタンスの取得
	static SrvManager* GetInstance();

	//初期化
	void Initialize(DirectXCommon* dxCommon);

	uint32_t Allocate();
	void Free(uint32_t index);//解放用の関数
	bool CanAllocate() const;//空きがあるか確認する関数

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	//SRV生成(テクスチャ用)
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
	//SRV生成(Structured Buffer用)
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	void PreDraw();

	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

	//デスクリプタヒープを取得
	ID3D12DescriptorHeap* GetDescriptorHeap() { return descriptorHeap.Get(); }

private:
	DirectXCommon* directXCommon = nullptr;

	//最大SRV数(最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;

	//SRV用のデスクリプタサイズ
	uint32_t descriptorSize;

	//SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	//次に使用するSRVインデックス
	//uint32_t useIndex = 0;

	//空き番号を管理するキュー
	std::queue<uint32_t> freeList_;

};

