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

    // 6頂点分のバッファを作成
    vertexResource_ = CreateBufferResource(dxCommon->GetDevice(), sizeof(VertexData) * 6);

    // バッファビューの設定
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // 頂点データの書き込み（法線も含める）
    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

    // Zの法線は手前(-1.0f)を向かせる
    Vector3 normal = { 0.0f, 0.0f, -1.0f };

    // 1枚目の三角形
    vertexData[0] = { {-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, normal }; // 左下
    vertexData[1] = { {-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, normal }; // 左上
    vertexData[2] = { { 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, normal }; // 右下

    // 2枚目の三角形
    vertexData[3] = { {-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, normal }; // 左上
    vertexData[4] = { { 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, normal }; // 右上
    vertexData[5] = { { 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, normal }; // 右下

    // 初期位置の設定など
    for (uint32_t index = 0; index < kNumInstance; ++index) {
        transforms_[index].scale = { 1.0f,1.0f,1.0f };
        transforms_[index].rotate = { 0.0f,0.0f,0.0f };
        transforms_[index].translate = { index * 0.1f, index * 0.1f, index * 0.1f };
    }

    // --- 変更: マテリアルデータの初期化 ---
    materialResource_ = CreateBufferResource(dxCommon->GetDevice(), sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    // 変なゴミデータが入らないようにゼロクリア
    memset(materialData_, 0, sizeof(Material));

    // 色を真っ白（不透明）に
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    // ライティングをオフに
    materialData_->enableLighting = 0;
    // UVトランスフォームを「等倍・回転なし・移動なし」の行列にする（必須）
    materialData_->uvTransform = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });

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

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // SrvManager経由でDescriptorTableをセット
    srvManager->SetGraphicsRootDescriptorTable(1, instancingSrvIndex_);

    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle =
        TextureManager::GetInstance()->GetSrvHandleGPU("Resources/uvChecker.png");
    commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

    // 10個まとめて
    commandList->DrawInstanced(6, kNumInstance, 0, 0);
}