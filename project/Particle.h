#pragma once
#include <cstdint>
#include <wrl.h>
#include "MyMath.h"
#include "SrvManager.h"
#include "Object3d.h"

class DirectXCommon;

class Particle
{
public:

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Update(Matrix4x4 viewProjectionMatrix);
	void Draw(ID3D12GraphicsCommandList* commandList, SrvManager* srvManager);

private:

	//インスタンスの最大数
	static const uint32_t kNumInstance = 10;

	//各パーティクルの位置や回転
	Transform transforms_[kNumInstance];

	//SrvManagerからもらった番号を覚えておく
	uint32_t instancingSrvIndex_;

	//バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
	MyMath::TransformationMatrix* instancingData_ = nullptr;

};

