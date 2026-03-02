#include "SrvManager.h"

const uint32_t SrvManager::kMaxSRVCount = 512;

void SrvManager::Initialize(DirectXCommon* dxCommon){
	//引数で受け取ってメンバ変数に記録する
	this->directXCommon = dxCommon;

	//デスクリプタヒープの作成
	descriptorHeap = directXCommon->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	//デスクリプタ1個分のサイズを取得して記録
	descriptorSize = directXCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}

uint32_t SrvManager::Allocate()
{
	assert(useIndex < kMaxSRVCount);
	//returnする番号をいったん記録しておく
	int index = useIndex;
	//次回のために番号を1進める
	useIndex++;
	//上で記録した番号をreturn
	return index;
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize + index);

	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize + index);

	return handleGPU;
}