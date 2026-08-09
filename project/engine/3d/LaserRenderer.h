#pragma once

#include "Laser.h"
#include "Matrix4x4.h"
#include "Vector4.h"
#include <cstddef>
#include <d3d12.h>
#include <wrl.h>

class Camera;
class DirectXCommon;

// Laserが計算した線分を、ゲーム本体の3D空間へ描画します。
class LaserRenderer
{
public:
	bool Initialize(DirectXCommon* dxCommon, size_t maximumSegmentCount = 32);
	void Draw(const std::vector<LaserSegment>& segments, const Camera& camera);
	// 3D空間における光線の幅を設定する。
	void SetBeamWidth(float width) { beamWidth_ = width; }

private:
	struct Vertex
	{
		Vector4 position;
	};

	struct ConstantData
	{
		Matrix4x4 viewProjection;
		Vector4 color;
	};

	bool CreateRootSignature();
	bool CreateGraphicsPipeline();

	DirectXCommon* dxCommon_ = nullptr;
	size_t maximumSegmentCount_ = 32;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> constantResource_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	Vertex* mappedVertices_ = nullptr;
	ConstantData* mappedConstantData_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	Vector4 color_{ 1.0f, 0.05f, 0.02f, 1.0f };
	float beamWidth_ = 0.12f;
};
