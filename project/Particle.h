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

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& filePath);
	void Update(Matrix4x4 viewProjectionMatrix);
	void Draw(ID3D12GraphicsCommandList* commandList, SrvManager* srvManager);

	void MakeNewParticle(uint32_t index);

	void SetTextureFilePath(const std::string& filePath) { textureFilePath_ = filePath; }

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

	struct ParticleForGPU
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Vector4 color;
	};

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;

	const float kDeltaTime_ = 1.0f / 60.0f;

	// 頂点バッファ用の変数
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	std::string textureFilePath_;

	//インスタンスの最大数
	static const uint32_t kNumMaxInstance = 10;

	//インスタンス1枚
	uint32_t numInstance = 0;

	//各パーティクルの位置や回転
	Transform transforms_[kNumMaxInstance];

	Vector3 velocities_[kNumMaxInstance];

	Vector4 colors_[kNumMaxInstance];

	float currentTimes_[kNumMaxInstance]; // 経過時間
	float lifeTimes_[kNumMaxInstance];    

	//SrvManagerからもらった番号を覚えておく
	uint32_t instancingSrvIndex_;

	//バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;

	ParticleForGPU* instancingData_ = nullptr;
};

