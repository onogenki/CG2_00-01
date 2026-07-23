#include "GPUParticle.h"
#include "Camera.h"
#include "MyMath.h"
#include "TextureManager.h"
#include <cassert>

using namespace MyMath;

void GPUParticle::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	assert(dxCommon);
	assert(srvManager);
	assert(srvManager->CanAllocate(3));

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(Particle) * kMaxParticles;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&particleResource_));
	assert(SUCCEEDED(hr));

	D3D12_RESOURCE_DESC counterResourceDesc{};
	counterResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	counterResourceDesc.Width = sizeof(int32_t);
	counterResourceDesc.Height = 1;
	counterResourceDesc.DepthOrArraySize = 1;
	counterResourceDesc.MipLevels = 1;
	counterResourceDesc.SampleDesc.Count = 1;
	counterResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	counterResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&counterResourceDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&freeCounterResource_));
	assert(SUCCEEDED(hr));

	particleSrvIndex_ = srvManager_->Allocate();
	particleUavIndex_ = srvManager_->Allocate();
	freeCounterUavIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(particleSrvIndex_, particleResource_.Get(), kMaxParticles, sizeof(Particle));
	srvManager_->CreateUAVforStructuredBuffer(particleUavIndex_, particleResource_.Get(), kMaxParticles, sizeof(Particle));
	srvManager_->CreateUAVforStructuredBuffer(freeCounterUavIndex_, freeCounterResource_.Get(), 1, sizeof(int32_t));

	CreateComputePipeline();
	CreateGraphicsPipeline();

	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * 6);
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	VertexData* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	vertexData[0] = { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } };
	vertexData[1] = { { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } };
	vertexData[2] = { {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } };
	vertexData[3] = { { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } };
	vertexData[4] = { {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } };
	vertexData[5] = { {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } };

	perViewResource_ = dxCommon_->CreateBufferResource(sizeof(PerView));
	perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
	emitterResource_ = dxCommon_->CreateBufferResource(sizeof(EmitterSphere));
	emitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterData_));
	emitterData_->translate = { 0.0f, 0.0f, 0.0f };
	emitterData_->radius = 1.0f;
	emitterData_->count = 10;
	emitterData_->frequency = 0.5f;
	emitterData_->frequencyTime = 0.0f;
	emitterData_->emit = 0;
	perFrameResource_ = dxCommon_->CreateBufferResource(sizeof(PerFrame));
	perFrameResource_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));
	perFrameData_->time = 0.0f;
	perFrameData_->deltaTime = 0.0f;

	InitializeParticles();
}

void GPUParticle::Update(float deltaTime)
{
	perFrameData_->time += deltaTime;
	perFrameData_->deltaTime = deltaTime;
	emitterData_->frequencyTime += deltaTime;
	if (emitterData_->frequency <= emitterData_->frequencyTime) {
		emitterData_->frequencyTime -= emitterData_->frequency;
		emitterData_->emit = 1;
	} else {
		emitterData_->emit = 0;
	}

	if (emitterData_->emit == 0) {
		return;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	srvManager_->PreDraw();
	D3D12_RESOURCE_BARRIER transitionToUav{};
	transitionToUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionToUav.Transition.pResource = particleResource_.Get();
	transitionToUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionToUav.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	transitionToUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	commandList->ResourceBarrier(1, &transitionToUav);

	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(emitPipelineState_.Get());
	srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);
	commandList->SetComputeRootConstantBufferView(1, emitterResource_->GetGPUVirtualAddress());
	commandList->SetComputeRootConstantBufferView(2, perFrameResource_->GetGPUVirtualAddress());
	commandList->Dispatch(1, 1, 1);

	D3D12_RESOURCE_BARRIER uavBarriers[2] = {};
	uavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarriers[0].UAV.pResource = particleResource_.Get();
	uavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarriers[1].UAV.pResource = freeCounterResource_.Get();
	commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);

	D3D12_RESOURCE_BARRIER transitionToSrv{};
	transitionToSrv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionToSrv.Transition.pResource = particleResource_.Get();
	transitionToSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionToSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionToSrv.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &transitionToSrv);
}

void GPUParticle::Draw(const Camera* camera)
{
	if (!camera) {
		return;
	}

	perViewData_->viewProjectionMatrix = camera->GetViewProjectionMatrix();
	Matrix4x4 viewMatrix = camera->GetViewMatrix();
	viewMatrix.m[3][0] = 0.0f;
	viewMatrix.m[3][1] = 0.0f;
	viewMatrix.m[3][2] = 0.0f;
	perViewData_->billboardMatrix = Inverse(viewMatrix);

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(graphicsRootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, perViewResource_->GetGPUVirtualAddress());
	srvManager_->SetGraphicsRootDescriptorTable(1, particleSrvIndex_);
	commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU("Resources/circle2.png"));
	commandList->DrawInstanced(6, kMaxParticles, 0, 0);
}

void GPUParticle::Finalize()
{
	if (srvManager_) {
		srvManager_->Free(particleSrvIndex_);
		srvManager_->Free(particleUavIndex_);
		srvManager_->Free(freeCounterUavIndex_);
	}
	particleSrvIndex_ = SrvManager::kInvalidSrvIndex;
	particleUavIndex_ = SrvManager::kInvalidSrvIndex;
	freeCounterUavIndex_ = SrvManager::kInvalidSrvIndex;
	particleResource_.Reset();
	freeCounterResource_.Reset();
	vertexResource_.Reset();
	perViewResource_.Reset();
	emitterResource_.Reset();
	perFrameResource_.Reset();
	computeRootSignature_.Reset();
	initializePipelineState_.Reset();
	emitPipelineState_.Reset();
	graphicsRootSignature_.Reset();
	graphicsPipelineState_.Reset();
	perViewData_ = nullptr;
	emitterData_ = nullptr;
	perFrameData_ = nullptr;
	dxCommon_ = nullptr;
	srvManager_ = nullptr;
}

void GPUParticle::CreateComputePipeline()
{
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.BaseShaderRegister = 0;
	descriptorRange.NumDescriptors = 2;
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(L"resources/shaders/InitializeParticle.CS.hlsl", L"cs_6_0");
	D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc{};
	pipelineStateDesc.pRootSignature = computeRootSignature_.Get();
	pipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&initializePipelineState_));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> emitShaderBlob = dxCommon_->CompileShader(L"resources/shaders/EmitParticle.CS.hlsl", L"cs_6_0");
	pipelineStateDesc.CS = { emitShaderBlob->GetBufferPointer(), emitShaderBlob->GetBufferSize() };
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&emitPipelineState_));
	assert(SUCCEEDED(hr));
}

void GPUParticle::CreateGraphicsPipeline()
{
	D3D12_DESCRIPTOR_RANGE particleDescriptorRange{};
	particleDescriptorRange.BaseShaderRegister = 0;
	particleDescriptorRange.NumDescriptors = 1;
	particleDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	particleDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	D3D12_DESCRIPTOR_RANGE textureDescriptorRange{};
	textureDescriptorRange.BaseShaderRegister = 0;
	textureDescriptorRange.NumDescriptors = 1;
	textureDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	textureDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &particleDescriptorRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &textureDescriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSampler{};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ShaderRegister = 0;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pStaticSamplers = &staticSampler;
	rootSignatureDesc.NumStaticSamplers = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&graphicsRootSignature_));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/GPUParticle.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/GPUParticle.PS.hlsl", L"ps_6_0");
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
	pipelineStateDesc.pRootSignature = graphicsRootSignature_.Get();
	pipelineStateDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	pipelineStateDesc.BlendState = blendDesc;
	pipelineStateDesc.RasterizerState = rasterizerDesc;
	pipelineStateDesc.DepthStencilState = depthStencilDesc;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateDesc.SampleDesc.Count = 1;
	pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

void GPUParticle::InitializeParticles()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	srvManager_->PreDraw();
	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(initializePipelineState_.Get());
	srvManager_->SetComputeRootDescriptorTable(0, particleUavIndex_);
	commandList->Dispatch(1, 1, 1);

	D3D12_RESOURCE_BARRIER uavBarriers[2] = {};
	uavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarriers[0].UAV.pResource = particleResource_.Get();
	uavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarriers[1].UAV.pResource = freeCounterResource_.Get();
	commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);

	D3D12_RESOURCE_BARRIER transitionBarrier{};
	transitionBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionBarrier.Transition.pResource = particleResource_.Get();
	transitionBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &transitionBarrier);
}
