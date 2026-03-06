#include "Particle.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
using namespace MyMath;

static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bufferResourceDesc{};
    bufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferResourceDesc.Width = sizeInBytes;
    bufferResourceDesc.Height = 1;
    bufferResourceDesc.DepthOrArraySize = 1;
    bufferResourceDesc.MipLevels = 1;
    bufferResourceDesc.SampleDesc.Count = 1;
    bufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
    return resource;
}

void Particle::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{

    instancingResource_ = CreateBufferResource(dxCommon->GetDevice(), sizeof(TransformationMatrix) * kNumInstance);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

    // 初期位置の設定など
    for (uint32_t index = 0; index < kNumInstance; ++index) {
        transforms_[index].scale = { 1.0f,1.0f,1.0f };
        transforms_[index].rotate = { 0.0f,0.0f,0.0f };
        transforms_[index].translate = { index * 1.0f, index * 1.0f, index * 1.0f };
    }

    instancingSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforStructuredBuffer(
        instancingSrvIndex_,
        instancingResource_.Get(),
        kNumInstance,
        sizeof(TransformationMatrix)
    );

}

void Particle::Update(Matrix4x4 viewProjectionMatrix)
{
    for (uint32_t index = 0; index < kNumInstance; ++index) {
        Matrix4x4 worldMatrix = MakeAffineMatrix(transforms_[index].scale, transforms_[index].rotate, transforms_[index].translate);
        Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);

        instancingData_[index].WVP = worldViewProjectionMatrix;
        instancingData_[index].World = worldMatrix;
    }

}

void Particle::Draw(ID3D12GraphicsCommandList* commandList, SrvManager* srvManager)
{
    // SrvManager経由でDescriptorTableをセット
    srvManager->SetGraphicsRootDescriptorTable(1, instancingSrvIndex_);

    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle =
        TextureManager::GetInstance()->GetSrvHandleGPU("Resources/uvChecker.png");
    commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

    // 10個まとめて
    commandList->DrawInstanced(6, kNumInstance, 0, 0);
}