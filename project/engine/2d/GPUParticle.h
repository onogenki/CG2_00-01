#pragma once
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"

class Camera;

class GPUParticle
{
public:
	static constexpr uint32_t kMaxParticles = 1024;

	static GPUParticle* GetInstance() {
		static GPUParticle instance;
		return &instance;
	}

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Draw(const Camera* camera);
	void Finalize();

private:
	struct Particle
	{
		Vector3 translate;
		Vector3 scale;
		float lifeTime;
		Vector3 velocity;
		float currentTime;
		Vector4 color;
	};

	struct PerView
	{
		Matrix4x4 viewProjectionMatrix;
		Matrix4x4 billboardMatrix;
	};

	struct VertexData
	{
		Vector4 position;
		float texcoord[2];
	};

	GPUParticle() = default;
	~GPUParticle() = default;
	GPUParticle(const GPUParticle&) = delete;
	GPUParticle& operator=(const GPUParticle&) = delete;

	void CreateComputePipeline();
	void CreateGraphicsPipeline();
	void InitializeParticles();

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	uint32_t particleSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t particleUavIndex_ = SrvManager::kInvalidSrvIndex;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	PerView* perViewData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
};
