#include "LaserRenderer.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "MyMath.h"
#include <algorithm>

using namespace MyMath;

bool LaserRenderer::Initialize(DirectXCommon* dxCommon, size_t maximumSegmentCount)
{
	if (!dxCommon) {
		return false;
	}
	dxCommon_ = dxCommon;
	maximumSegmentCount_ = (std::max)(maximumSegmentCount, size_t{ 1 });

	vertexResource_ = dxCommon_->CreateBufferResource(
		// 一本のLightを二枚の板で交差させ、どの方向から見ても円柱に近い太さに見せます。
		sizeof(Vertex) * maximumSegmentCount_ * 12);
	constantResource_ = dxCommon_->CreateBufferResource(sizeof(ConstantData));
	if (!vertexResource_ || !constantResource_) {
		return false;
	}

	if (FAILED(vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices_))) ||
		FAILED(constantResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedConstantData_)))) {
		return false;
	}

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(Vertex) * maximumSegmentCount_ * 12);
	vertexBufferView_.StrideInBytes = sizeof(Vertex);
	return CreateRootSignature() && CreateGraphicsPipeline();
}

void LaserRenderer::Draw(const std::vector<LaserSegment>& segments, const Camera& camera)
{
	if (!mappedVertices_ || !mappedConstantData_ || segments.empty()) {
		return;
	}

	const size_t drawSegmentCount = (std::min)(segments.size(), maximumSegmentCount_);
	size_t drawVertexCount = 0;
	for (size_t index = 0; index < drawSegmentCount; ++index) {
		const LaserSegment& segment = segments[index];
		const Vector3 beamVector{
			segment.end.x - segment.start.x,
			segment.end.y - segment.start.y,
			segment.end.z - segment.start.z,
		};
		if (Length(beamVector) <= 0.0001f) {
			continue;
		}
		const Vector3 beamDirection = Normalize(beamVector);
		const Vector3 center{
			(segment.start.x + segment.end.x) * 0.5f,
			(segment.start.y + segment.end.y) * 0.5f,
			(segment.start.z + segment.end.z) * 0.5f,
		};
		Vector3 cameraDirection{
			camera.GetTranslate().x - center.x,
			camera.GetTranslate().y - center.y,
			camera.GetTranslate().z - center.z,
		};
		Vector3 side = Cross(beamDirection, cameraDirection);
		if (Length(side) <= 0.0001f) {
			side = Cross(beamDirection, { 0.0f, 1.0f, 0.0f });
		}
		if (Length(side) <= 0.0001f) {
			side = { 1.0f, 0.0f, 0.0f };
		} else {
			side = Normalize(side);
		}
		const float halfWidth = (std::max)(beamWidth_, 0.001f) * 0.5f;
		side = Multiply(halfWidth, side);

		auto appendQuad = [&](const Vector3& quadSide) {
			const Vector3 startLeft{
				segment.start.x - quadSide.x,
				segment.start.y - quadSide.y,
				segment.start.z - quadSide.z,
			};
			const Vector3 startRight{
				segment.start.x + quadSide.x,
				segment.start.y + quadSide.y,
				segment.start.z + quadSide.z,
			};
			const Vector3 endLeft{
				segment.end.x - quadSide.x,
				segment.end.y - quadSide.y,
				segment.end.z - quadSide.z,
			};
			const Vector3 endRight{
				segment.end.x + quadSide.x,
				segment.end.y + quadSide.y,
				segment.end.z + quadSide.z,
			};
			const Vertex quadVertices[6]{
				{ { startLeft.x, startLeft.y, startLeft.z, 1.0f }, 0.0f, 0.0f },
				{ { startRight.x, startRight.y, startRight.z, 1.0f }, 1.0f, 0.0f },
				{ { endRight.x, endRight.y, endRight.z, 1.0f }, 1.0f, 1.0f },
				{ { startLeft.x, startLeft.y, startLeft.z, 1.0f }, 0.0f, 0.0f },
				{ { endRight.x, endRight.y, endRight.z, 1.0f }, 1.0f, 1.0f },
				{ { endLeft.x, endLeft.y, endLeft.z, 1.0f }, 0.0f, 1.0f },
			};
			for (const Vertex& vertex : quadVertices) {
				mappedVertices_[drawVertexCount++] = vertex;
			}
		};
		appendQuad(side);
		// 一枚目に対して直角の板を足し、平面の線ではなく円柱状のLightに近づけます。
		Vector3 crossSide = Cross(beamDirection, side);
		if (Length(crossSide) > 0.0001f) {
			crossSide = Multiply(halfWidth, Normalize(crossSide));
			appendQuad(crossSide);
		}
	}
	mappedConstantData_->viewProjection = camera.GetViewProjectionMatrix();
	mappedConstantData_->color = color_;

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, constantResource_->GetGPUVirtualAddress());
	commandList->DrawInstanced(static_cast<UINT>(drawVertexCount), 1, 0, 0);
}

bool LaserRenderer::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameter{};
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter.Descriptor.ShaderRegister = 0;
	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = &rootParameter;
	rootSignatureDesc.NumParameters = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob);
	if (FAILED(result)) {
		return false;
	}

	result = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	return SUCCEEDED(result);
}

bool LaserRenderer::CreateGraphicsPipeline()
{
	D3D12_INPUT_ELEMENT_DESC inputElement{};
	inputElement.SemanticName = "POSITION";
	inputElement.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElement.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_ELEMENT_DESC inputElements[2]{};
	inputElements[0] = inputElement;
	inputElements[1].SemanticName = "TEXCOORD";
	inputElements[1].SemanticIndex = 0;
	inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	const D3D12_INPUT_LAYOUT_DESC inputLayout{ inputElements, _countof(inputElements) };

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShader = dxCommon_->CompileShader(
		L"resources/shaders/Laser.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShader = dxCommon_->CompileShader(
		L"resources/shaders/Laser.PS.hlsl", L"ps_6_0");
	if (!vertexShader || !pixelShader) {
		return false;
	}

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.DepthClipEnable = true;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.InputLayout = inputLayout;
	pipelineDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	pipelineDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
	pipelineDesc.BlendState = blendDesc;
	pipelineDesc.RasterizerState = rasterizerDesc;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	const HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(&graphicsPipelineState_));
	return SUCCEEDED(result);
}
