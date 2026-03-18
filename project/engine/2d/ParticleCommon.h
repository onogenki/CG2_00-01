#pragma once
#include <d3d12.h>
#include <vector>
#include "externals/DirectXTex/d3dx12.h"
#include "Model.h"

class DirectXCommon;

class ParticleCommon
{
public:

	void Initialize(DirectXCommon* dxCommon);
	// 描画の前に呼ぶ共通設定（パイプラインなどをセットする）
	void PreDraw(ID3D12GraphicsCommandList* commandList);

private:

	DirectXCommon* dxCommon_ = nullptr;

	Model::ModelData modelData_;

	// 共通のルートシグネチャとパイプラインステート
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
};

