#pragma once
#include <cstdint>
#include <wrl.h>
#include "MyMath.h"
#include "SrvManager.h"
#include "Object3d.h"
#include <random>

class DirectXCommon;

class Particle
{
public:

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Update(Matrix4x4 viewProjectionMatrix);
	void Draw(ID3D12GraphicsCommandList* commandList, SrvManager* srvManager);

	void MakeNewParticle(uint32_t index);

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];      // サイズ合わせのダミー
		Matrix4x4 uvTransform; // UV用（これがないとエラーになる）
	};

	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;

	const float kDeltaTime_ = 1.0f / 60.0f;

	// 頂点バッファ用の変数
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	//インスタンスの最大数
	static const uint32_t kNumInstance = 10;

	//各パーティクルの位置や回転
	Transform transforms_[kNumInstance];

	Vector3 velocities_[kNumInstance];

	//SrvManagerからもらった番号を覚えておく
	uint32_t instancingSrvIndex_;

	//バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
	MyMath::TransformationMatrix* instancingData_ = nullptr;
	

};

