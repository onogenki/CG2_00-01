#include "ParticleManager.h"
#include "TextureManager.h"
#include "CameraManager.h"
#include <algorithm>

using namespace MyMath;

// シングルトンインスタンスの取得
ParticleManager* ParticleManager::GetInstance() {
    static ParticleManager instance;
    return &instance;
}

// 初期化処理
void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    // 引数でポインタを受け取ってメンバ変数に記録
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    // ランダムエンジンの初期化
    std::random_device seed_gen;
    randomEngine_.seed(seed_gen());

    ID3D12Device* device = dxCommon_->GetDevice();
    HRESULT hr = S_FALSE;

    D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
    descriptorRangeForInstancing[0].BaseShaderRegister = 0;
    descriptorRangeForInstancing[0].NumDescriptors = 1;
    descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    assert(SUCCEEDED(hr));
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));

    //頂点データの初期化
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * 6);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    Vector3 normal = { 0.0f, 0.0f, -1.0f };
    vertexData[0] = { {-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, normal };
    vertexData[1] = { {-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, normal };
    vertexData[2] = { { 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, normal };
    vertexData[3] = { {-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, normal };
    vertexData[4] = { { 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, normal };
    vertexData[5] = { { 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, normal };

    // マテリアル（定数バッファ）の初期化
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    memset(materialData_, 0, sizeof(Material));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 0;
    materialData_->uvTransform = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });

    //フィールドの設定
    accelerationField_.acceleration = { 15.0f,0.0f,0.0f };
    accelerationField_.area.min = { -1.0f,-1.0f,-1.0f };
    accelerationField_.area.max = { 1.0f,1.0f,1.0f };

    isField_ = true;
}

// パーティクルグループの生成
void ParticleManager::CreateParticleGroup(const std::string name, const std::string textureFilePath) {
    // 登録済みの名前かチェックしてassert
    assert(particleGroups_.find(name) == particleGroups_.end() && "その名前のグループは既に存在します");

    // 新たな空のパーティクルグループを作成
    ParticleGroup newGroup;
    // マテリアルデータにテクスチャファイルパスを設定
    newGroup.textureFilePath = textureFilePath;

    // インスタンシング用リソースの生成 (上限を決めてリソースを作る)
    newGroup.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);

    // ポインタを取得
    newGroup.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.mappedData));

    // インスタンシング用にSRVを確保してSRVインデックスを記録
    newGroup.instancingSrvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        newGroup.instancingSrvIndex,
        newGroup.instancingResource.Get(),
        kNumMaxInstance,
        sizeof(ParticleForGPU)
    );

    // コンテナに登録
    particleGroups_[name] = newGroup;
}

void ParticleManager::CreateRingParticleGroup(const std::string name, const std::string textureFilePath)
{
    assert(particleGroups_.find(name) == particleGroups_.end() && "Particle group already exists");

    ParticleGroup newGroup;
    newGroup.textureFilePath = textureFilePath;
    // Ringごとの傾きを残すため、描画時の自動ビルボードは使用しない
    newGroup.useBillboard = false;

    newGroup.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);
    newGroup.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.mappedData));

    newGroup.instancingSrvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        newGroup.instancingSrvIndex,
        newGroup.instancingResource.Get(),
        kNumMaxInstance,
        sizeof(ParticleForGPU)
    );

    const uint32_t kRingDivide = 32;
    const float kOuterRadius = 1.0f;
    const float kInnerRadius = 0.6f;
    const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);
    const Vector3 normal = { 0.0f, 0.0f, -1.0f };

    newGroup.vertexCount = kRingDivide * 6;
    newGroup.vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * newGroup.vertexCount);
    newGroup.vertexBufferView.BufferLocation = newGroup.vertexResource->GetGPUVirtualAddress();
    newGroup.vertexBufferView.SizeInBytes = sizeof(VertexData) * newGroup.vertexCount;
    newGroup.vertexBufferView.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    newGroup.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

    for (uint32_t index = 0; index < kRingDivide; ++index) {
        float sin = std::sin(index * radianPerDivide);
        float cos = std::cos(index * radianPerDivide);
        float sinNext = std::sin((index + 1) * radianPerDivide);
        float cosNext = std::cos((index + 1) * radianPerDivide);
        float u = float(index) / float(kRingDivide);
        float uNext = float(index + 1) / float(kRingDivide);

        uint32_t vertexIndex = index * 6;
        vertexData[vertexIndex + 0] = { { -sin * kOuterRadius, cos * kOuterRadius, 0.0f, 1.0f }, { u, 0.0f }, normal };
        vertexData[vertexIndex + 1] = { { -sinNext * kOuterRadius, cosNext * kOuterRadius, 0.0f, 1.0f }, { uNext, 0.0f }, normal };
        vertexData[vertexIndex + 2] = { { -sin * kInnerRadius, cos * kInnerRadius, 0.0f, 1.0f }, { u, 1.0f }, normal };
        vertexData[vertexIndex + 3] = { { -sinNext * kOuterRadius, cosNext * kOuterRadius, 0.0f, 1.0f }, { uNext, 0.0f }, normal };
        vertexData[vertexIndex + 4] = { { -sinNext * kInnerRadius, cosNext * kInnerRadius, 0.0f, 1.0f }, { uNext, 1.0f }, normal };
        vertexData[vertexIndex + 5] = { { -sin * kInnerRadius, cos * kInnerRadius, 0.0f, 1.0f }, { u, 1.0f }, normal };
    }

    particleGroups_[name] = newGroup;
}

void ParticleManager::CreateCylinderParticleGroup(const std::string name, const std::string textureFilePath)
{
    assert(particleGroups_.find(name) == particleGroups_.end() && "Particle group already exists");

    ParticleGroup newGroup;
    newGroup.textureFilePath = textureFilePath;
    newGroup.useBillboard = false;

    newGroup.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);
    newGroup.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.mappedData));

    newGroup.instancingSrvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        newGroup.instancingSrvIndex,
        newGroup.instancingResource.Get(),
        kNumMaxInstance,
        sizeof(ParticleForGPU)
    );

    const uint32_t kCylinderDivide = 32;
    const float kTopRadius = 1.0f;
    const float kBottomRadius = 1.0f;
    const float kHeight = 3.0f;
    const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kCylinderDivide);

    newGroup.vertexCount = kCylinderDivide * 6;
    newGroup.vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * newGroup.vertexCount);
    newGroup.vertexBufferView.BufferLocation = newGroup.vertexResource->GetGPUVirtualAddress();
    newGroup.vertexBufferView.SizeInBytes = sizeof(VertexData) * newGroup.vertexCount;
    newGroup.vertexBufferView.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    newGroup.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

    for (uint32_t index = 0; index < kCylinderDivide; ++index) {
        float sin = std::sin(index * radianPerDivide);
        float cos = std::cos(index * radianPerDivide);
        float sinNext = std::sin((index + 1) * radianPerDivide);
        float cosNext = std::cos((index + 1) * radianPerDivide);
        float u = float(index) / float(kCylinderDivide);
        float uNext = float(index + 1) / float(kCylinderDivide);

        Vector3 normal0 = { -sin, 0.0f, cos };
        Vector3 normal1 = { -sinNext, 0.0f, cosNext };

        uint32_t vertexIndex = index * 6;
        vertexData[vertexIndex + 0] = { { -sin * kTopRadius, kHeight, cos * kTopRadius, 1.0f }, { u, 1.0f }, normal0 };
        vertexData[vertexIndex + 1] = { { -sinNext * kTopRadius, kHeight, cosNext * kTopRadius, 1.0f }, { uNext, 1.0f }, normal1 };
        vertexData[vertexIndex + 2] = { { -sin * kBottomRadius, 0.0f, cos * kBottomRadius, 1.0f }, { u, 0.0f }, normal0 };
        vertexData[vertexIndex + 3] = { { -sinNext * kTopRadius, kHeight, cosNext * kTopRadius, 1.0f }, { uNext, 1.0f }, normal1 };
        vertexData[vertexIndex + 4] = { { -sinNext * kBottomRadius, 0.0f, cosNext * kBottomRadius, 1.0f }, { uNext, 0.0f }, normal1 };
        vertexData[vertexIndex + 5] = { { -sin * kBottomRadius, 0.0f, cos * kBottomRadius, 1.0f }, { u, 0.0f }, normal0 };
    }

    particleGroups_[name] = newGroup;
}

void ParticleManager::ClearAllParticles()
{
    for (auto& pair : particleGroups_) {
        pair.second.particles.clear();
        pair.second.instanceCount = 0; // インスタンス数もリセット
    }
}

void ParticleManager::ClearAllGroups()
{
    particleGroups_.clear();
}

void ParticleManager::ClearParticles(const std::string name)
{
    auto it = particleGroups_.find(name);
    assert(it != particleGroups_.end() && "Particle group not found");
    it->second.particles.clear();
    it->second.instanceCount = 0;
}

bool ParticleManager::IsCollision(const AABB aabb, const Vector3& point)
{
    return (point.x >= aabb.min.x && point.x <= aabb.max.x) &&
        (point.y >= aabb.min.y && point.y <= aabb.max.y) &&
        (point.z >= aabb.min.z && point.z <= aabb.max.z);
}

// パーティクルの発生
void ParticleManager::Emit(const std::string name, const Vector3& position, uint32_t count,bool receivesWind) {
    // 登録済みのパーティクルグループ名かチェックしてassert
    assert(particleGroups_.find(name) != particleGroups_.end() && "指定されたグループ名が見つかりません");

    // 指定されたグループの参照を取得
    ParticleGroup& group = particleGroups_[name];

    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    auto it = particleGroups_.find(name);
    assert(it != particleGroups_.end() && "発生させようとしたパーティクルグループ名が存在しません。");

    for (uint32_t i = 0; i < count; ++i) {
        Particle newParticle;
        newParticle.transform.scale = { 1.0f, 1.0f, 1.0f };
        newParticle.transform.rotate = { 0.0f, 0.0f, 0.0f };
        newParticle.transform.translate = position;

        newParticle.velocity = {
            distribution(randomEngine_),
            distribution(randomEngine_),
            distribution(randomEngine_)
        };
        newParticle.color = {
            distColor(randomEngine_), distColor(randomEngine_), distColor(randomEngine_), 1.0f
        };
        newParticle.lifeTime = distTime(randomEngine_);
        newParticle.currentTime = 0.0f;

        newParticle.receivesWind = receivesWind;
        // グループに登録
        group.particles.push_back(newParticle);
    }
}

//ヒット(斬撃みたいな細長い円)エフェクト発生
void ParticleManager::EmitHitEffect(const std::string name, uint32_t count, const Vector3& translate) {
    assert(particleGroups_.find(name) != particleGroups_.end() && "Particle group not found");

    ParticleGroup& group = particleGroups_[name];

    std::uniform_real_distribution<float> distRotate(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> distScale(0.15f, 0.55f);
    std::uniform_real_distribution<float> distTime(0.15f, 0.35f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle newParticle;
        newParticle.transform.scale = { 0.02f, distScale(randomEngine_), 1.0f };
        newParticle.transform.rotate = { 0.0f, 0.0f, distRotate(randomEngine_) };
        newParticle.transform.translate = translate;
        newParticle.velocity = { 0.0f, 0.0f, 0.0f };
        newParticle.color = { 1.0f, 0.9f, 0.55f, 1.0f };
        newParticle.lifeTime = distTime(randomEngine_);
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;

        group.particles.push_back(newParticle);
    }
}

//インパクト(円)エフェクト発生
void ParticleManager::EmitRingEffect(const std::string name, uint32_t count, const Vector3& translate)
{
    assert(particleGroups_.find(name) != particleGroups_.end() && "Particle group not found");

    ParticleGroup& group = particleGroups_[name];
    std::uniform_real_distribution<float> distRotate(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> distTilt(-0.55f, 0.55f);
    std::uniform_real_distribution<float> distScale(0.25f, 0.6f);
    std::uniform_real_distribution<float> distLifeTime(0.3f, 0.6f);

    // カメラ正面を基準にすることで、真横を向いて細線になることを防ぐ
    Vector3 cameraRotation = { 0.0f, 0.0f, 0.0f };
    if (cameraManager_ && cameraManager_->GetActiveCamera()) {
        cameraRotation = cameraManager_->GetActiveCamera()->GetRotate();
    }

    for (uint32_t i = 0; i < count; ++i) {
        Particle newParticle;
        float scale = distScale(randomEngine_);
        newParticle.transform.scale = { scale, scale, 1.0f };
        newParticle.transform.rotate = {
            cameraRotation.x + distTilt(randomEngine_),
            cameraRotation.y + distTilt(randomEngine_),
            distRotate(randomEngine_)
        };
        newParticle.transform.translate = translate;
        newParticle.velocity = { 0.0f, 0.0f, 0.0f };
        newParticle.color = { 1.0f, 0.78f, 0.28f, 0.8f };
        newParticle.lifeTime = distLifeTime(randomEngine_);
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;

        group.particles.push_back(newParticle);
    }
}

//ポータル(円柱)エフェクト発生
void ParticleManager::EmitCylinderEffect(const std::string name, uint32_t count, const Vector3& translate)
{
    assert(particleGroups_.find(name) != particleGroups_.end() && "Particle group not found");

    ParticleGroup& group = particleGroups_[name];
    std::uniform_real_distribution<float> distHeight(0.2f, 0.45f);
    std::uniform_real_distribution<float> distStretchSpeed(0.15f, 0.25f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle newParticle;
        newParticle.transform.scale = { 0.35f, distHeight(randomEngine_), 0.35f };
        newParticle.transform.rotate = { 0.0f, 0.0f, 0.0f };
        newParticle.transform.translate = translate;
        newParticle.velocity = { 0.0f, 0.0f, 0.0f };
        // 柱本体は薄くし、周囲のキラキラを主役にする
        newParticle.color = { 1.0f, 0.9f, 0.55f, 0.38f };
        newParticle.lifeTime = 0.0f;
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;
        newParticle.isEndless = true;
        newParticle.scaleVelocityY = distStretchSpeed(randomEngine_);

        group.particles.push_back(newParticle);
    }
}

// 光柱の周囲へ星形PNGを散らし、上昇しながら小さく消える光粒を発生させる
void ParticleManager::EmitPillarSparkle(const std::string name, uint32_t count, const Vector3& position)
{
    assert(particleGroups_.find(name) != particleGroups_.end() && "Particle group not found");

    ParticleGroup& group = particleGroups_[name];
    std::uniform_real_distribution<float> rightOffsetDistribution(-0.45f, 0.45f);
    std::uniform_real_distribution<float> upOffsetDistribution(-0.1f, 1.25f);
    std::uniform_real_distribution<float> sideSpeedDistribution(-0.18f, 0.18f);
    std::uniform_real_distribution<float> upSpeedDistribution(0.4f, 1.1f);
    std::uniform_real_distribution<float> scaleDistribution(0.025f, 0.07f);
    std::uniform_real_distribution<float> lifeTimeDistribution(0.35f, 0.9f);
    std::uniform_real_distribution<float> rotateDistribution(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);

    Vector3 cameraRight = { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };
    if (cameraManager_ && cameraManager_->GetActiveCamera()) {
        const Matrix4x4& cameraWorld = cameraManager_->GetActiveCamera()->GetWorldMatrix();
        cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
        cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
    }

    for (uint32_t index = 0; index < count; ++index) {
        Particle newParticle{};
        const float offsetRight = rightOffsetDistribution(randomEngine_);
        const float offsetUp = upOffsetDistribution(randomEngine_);
        const float sideSpeed = sideSpeedDistribution(randomEngine_);
        const float upSpeed = upSpeedDistribution(randomEngine_);
        const float startScale = scaleDistribution(randomEngine_);

        newParticle.transform.translate = {
            position.x + cameraRight.x * offsetRight + cameraUp.x * offsetUp,
            position.y + cameraRight.y * offsetRight + cameraUp.y * offsetUp,
            position.z + cameraRight.z * offsetRight + cameraUp.z * offsetUp
        };
        newParticle.velocity = {
            cameraRight.x * sideSpeed + cameraUp.x * upSpeed,
            cameraRight.y * sideSpeed + cameraUp.y * upSpeed,
            cameraRight.z * sideSpeed + cameraUp.z * upSpeed
        };
        newParticle.transform.rotate.z = rotateDistribution(randomEngine_);

        newParticle.useColorAndScaleOverLife = true;
        newParticle.startScale = { startScale, startScale, startScale };
        newParticle.endScale = { startScale * 0.2f, startScale * 0.2f, startScale * 0.2f };
        newParticle.startColor = { 1.0f, 1.0f, 0.92f, 1.0f };
        newParticle.endColor = { 1.0f, 0.62f, 0.08f, 0.0f };
        newParticle.transform.scale = newParticle.startScale;
        newParticle.color = newParticle.startColor;
        newParticle.lifeTime = lifeTimeDistribution(randomEngine_);
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;

        group.particles.push_back(newParticle);
    }
}

// 大きさと色の異なる円形PNGを重ね、中心で脈動する光球を発生させる
void ParticleManager::EmitLightCore(const std::string name, uint32_t count, const Vector3& position)
{
    assert(particleGroups_.find(name) != particleGroups_.end() && "Particle group not found");

    ParticleGroup& group = particleGroups_[name];
    std::uniform_real_distribution<float> offsetDistribution(-0.08f, 0.08f);

    // カメラから見た横方向と上方向を使い、画面上で自然に散らす
    Vector3 cameraRight = { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };
    if (cameraManager_ && cameraManager_->GetActiveCamera()) {
        const Matrix4x4& cameraWorld = cameraManager_->GetActiveCamera()->GetWorldMatrix();
        cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
        cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
    }

    for (uint32_t index = 0; index < count; ++index) {
        Particle newParticle{};

        // 少しだけ位置をずらして、複数の光が重なって見えるようにする
        const float offsetRight = offsetDistribution(randomEngine_);
        const float offsetUp = offsetDistribution(randomEngine_);
        newParticle.transform.translate = {
            position.x + cameraRight.x * offsetRight + cameraUp.x * offsetUp,
            position.y + cameraRight.y * offsetRight + cameraUp.y * offsetUp,
            position.z + cameraRight.z * offsetRight + cameraUp.z * offsetUp
        };
        newParticle.velocity = { 0.0f, 0.0f, 0.0f };

        newParticle.useColorAndScaleOverLife = true;
        newParticle.startScale = { 0.08f, 0.08f, 0.08f };
        newParticle.endScale = { 0.9f, 0.9f, 0.9f };
        newParticle.startColor = { 1.0f, 1.0f, 0.92f, 1.0f };
        newParticle.endColor = { 1.0f, 0.65f, 0.12f, 0.0f };
        newParticle.transform.scale = newParticle.startScale;
        newParticle.color = newParticle.startColor;

        newParticle.lifeTime = 1.2f;
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;
        group.particles.push_back(newParticle);
    }
}

// 細長い円形PNGを画面上部に散らし、下方向へ流れる光の雨を発生させる
void ParticleManager::EmitLightRain(const std::string name, uint32_t count, const Vector3& position)
{
    assert(particleGroups_.find(name) != particleGroups_.end() && "Particle group not found");

    ParticleGroup& group = particleGroups_[name];
    std::uniform_real_distribution<float> positionXDistribution(-1.7f, 1.7f);
    std::uniform_real_distribution<float> positionYDistribution(1.8f, 3.5f);
    std::uniform_real_distribution<float> speedDistribution(-4.0f, -2.5f);
    std::uniform_real_distribution<float> lifeTimeDistribution(1.2f, 2.2f);

    // 雨の横幅をカメラの右方向へ広げる
    Vector3 cameraRight = { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };
    if (cameraManager_ && cameraManager_->GetActiveCamera()) {
        const Matrix4x4& cameraWorld = cameraManager_->GetActiveCamera()->GetWorldMatrix();
        cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
        cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
    }

    for (uint32_t index = 0; index < count; ++index) {
        Particle newParticle{};

        const float offsetRight = positionXDistribution(randomEngine_);
        const float offsetUp = positionYDistribution(randomEngine_);
        const float fallSpeed = speedDistribution(randomEngine_);
        newParticle.transform.translate = {
            position.x + cameraRight.x * offsetRight + cameraUp.x * offsetUp,
            position.y + cameraRight.y * offsetRight + cameraUp.y * offsetUp,
            position.z + cameraRight.z * offsetRight + cameraUp.z * offsetUp
        };
        newParticle.velocity = {
            cameraUp.x * fallSpeed,
            cameraUp.y * fallSpeed,
            cameraUp.z * fallSpeed
        };

        newParticle.useColorAndScaleOverLife = true;
        newParticle.startScale = { 0.035f, 0.45f, 0.035f };
        newParticle.endScale = { 0.01f, 0.15f, 0.01f };
        newParticle.startColor = { 1.0f, 0.98f, 0.72f, 0.9f };
        newParticle.endColor = { 1.0f, 0.55f, 0.05f, 0.0f };
        newParticle.transform.scale = newParticle.startScale;
        newParticle.color = newParticle.startColor;

        newParticle.lifeTime = lifeTimeDistribution(randomEngine_);
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;
        group.particles.push_back(newParticle);
    }
}

// 円形PNGを二本の螺旋状に並べ、光が渦を巻くエフェクトを発生させる
void ParticleManager::EmitLightSpiral(const std::string name, uint32_t count, const Vector3& translate)
{
    assert(particleGroups_.find(name) != particleGroups_.end() && "Particle group not found");

    ParticleGroup& group = particleGroups_[name];

    // 片方の螺旋に最低1個は配置する
    const uint32_t particlesPerArm = (std::max)(count / 2, 1u);
    const float minimumRadius = 0.2f;
    const float maximumRadius = 1.4f;
    const float twoRotations = std::numbers::pi_v<float> * 4.0f;

    Vector3 cameraRight = { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };
    if (cameraManager_ && cameraManager_->GetActiveCamera()) {
        const Matrix4x4& cameraWorld = cameraManager_->GetActiveCamera()->GetWorldMatrix();
        cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
        cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
    }

    for (uint32_t index = 0; index < count; ++index) {
        Particle newParticle{};

        // 前半を1本目、後半を半回転ずらした2本目として扱う
        const uint32_t armIndex = index / particlesPerArm;
        const uint32_t indexInArm = index % particlesPerArm;
        const float progress = static_cast<float>(indexInArm) / static_cast<float>(particlesPerArm);
        const float armOffset = (armIndex % 2) * std::numbers::pi_v<float>;

        newParticle.isSpiral = true;
        newParticle.spiralCenter = translate;
        newParticle.spiralRight = cameraRight;
        newParticle.spiralUp = cameraUp;
        newParticle.spiralAngle = twoRotations * progress + armOffset;
        newParticle.spiralRadius = minimumRadius + (maximumRadius - minimumRadius) * progress;
        newParticle.spiralAngularVelocity = 1.8f;
        newParticle.spiralRadialVelocity = -0.08f;

        const float spiralX = std::cos(newParticle.spiralAngle) * newParticle.spiralRadius;
        const float spiralY = std::sin(newParticle.spiralAngle) * newParticle.spiralRadius;
        newParticle.transform.translate = {
            translate.x + cameraRight.x * spiralX + cameraUp.x * spiralY,
            translate.y + cameraRight.y * spiralX + cameraUp.y * spiralY,
            translate.z + cameraRight.z * spiralX + cameraUp.z * spiralY
        };
        newParticle.transform.scale = { 0.08f, 0.08f, 0.08f };
        newParticle.velocity = { 0.0f, 0.0f, 0.0f };

        // 2本の螺旋を白寄りの黄色と濃い金色に分ける
        if (armIndex % 2 == 0) {
            newParticle.color = { 1.0f, 0.96f, 0.72f, 0.9f };
        } else {
            newParticle.color = { 1.0f, 0.62f, 0.08f, 0.9f };
        }

        newParticle.lifeTime = 4.0f;
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;
        group.particles.push_back(newParticle);
    }
}

// 全パーティクルの移動、寿命、透明度、描画用データを毎フレーム更新する
void ParticleManager::Update() {

    if (!cameraManager_)return;
    //毎フレーム今アクティブなカメラを取得する
    Camera* activeCamera = cameraManager_->GetActiveCamera();
    //万が一カメラがない場合は安全のために抜ける
    if (!activeCamera)return;

    Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);

    const float kDeltaTime_ = 1.0f / 60.0f;

    //風タイマー
    windTimer_ += kDeltaTime_;
    if (windTimer_ >= 5.0f)
    {//5秒経過したら
        isField_ = !isField_;//trueとfalse切り替え
        windTimer_ = 0.0f;
    }

    //取得したactiveカメラからビュー作成もらう
    Matrix4x4 view = activeCamera->GetViewMatrix();
    view.m[3][0] = 0.0f; view.m[3][1] = 0.0f; view.m[3][2] = 0.0f;
    Matrix4x4 billboardMatrix = Inverse(view);

    Matrix4x4 viewProjectionMatrix = Multiply(activeCamera->GetViewMatrix(), activeCamera->GetProjectionMatrix());

    // 全てのパーティクルグループについて処理する
    for (auto& pair : particleGroups_) {
        ParticleGroup& group = pair.second;
        // インスタンス数をリセット
        group.instanceCount = 0;

        // グループ内の全てのパーティクルについて処理する
        for (auto it = group.particles.begin(); it != group.particles.end();) {
            Particle& particle = *it;

            if (!particle.isEndless && particle.currentTime >= particle.lifeTime) {
                it = group.particles.erase(it);
                continue;
            }

            //Fieldの範囲内のParticleには加速度を適用
            if (pair.first == "Cylinder") {
                particle.transform.rotate.y += 1.5f * kDeltaTime_;
                particle.transform.scale.y += particle.scaleVelocityY * kDeltaTime_;

                if (particle.transform.scale.y >= 0.45f) {
                    particle.transform.scale.y = 0.45f;
                    particle.scaleVelocityY *= -1.0f;
                } else if (particle.transform.scale.y <= 0.2f) {
                    particle.transform.scale.y = 0.2f;
                    particle.scaleVelocityY *= -1.0f;
                }
            }

            // 螺旋の角度を進め、中心の周りを回転させる
            if (particle.isSpiral) {
                particle.spiralAngle += particle.spiralAngularVelocity * kDeltaTime_;
                particle.spiralRadius += particle.spiralRadialVelocity * kDeltaTime_;
                particle.spiralRadius = (std::max)(particle.spiralRadius, 0.1f);

                const float spiralX = std::cos(particle.spiralAngle) * particle.spiralRadius;
                const float spiralY = std::sin(particle.spiralAngle) * particle.spiralRadius;
                particle.transform.translate = {
                    particle.spiralCenter.x + particle.spiralRight.x * spiralX + particle.spiralUp.x * spiralY,
                    particle.spiralCenter.y + particle.spiralRight.y * spiralX + particle.spiralUp.y * spiralY,
                    particle.spiralCenter.z + particle.spiralRight.z * spiralX + particle.spiralUp.z * spiralY
                };
            }

            if (isField_ && particle.receivesWind)
            {
                if (IsCollision(accelerationField_.area, particle.transform.translate))
                {
                    particle.velocity.x += accelerationField_.acceleration.x * kDeltaTime_;
                    particle.velocity.y += accelerationField_.acceleration.y * kDeltaTime_;
                    particle.velocity.z += accelerationField_.acceleration.z * kDeltaTime_;
                }
            }

            particle.transform.translate.x += particle.velocity.x * kDeltaTime_;
            particle.transform.translate.y += particle.velocity.y * kDeltaTime_;
            particle.transform.translate.z += particle.velocity.z * kDeltaTime_;

            float alpha = particle.color.w;
            if (!particle.isEndless) {
                particle.currentTime += kDeltaTime_;

                const float progress = std::clamp(particle.currentTime / particle.lifeTime, 0.0f, 1.0f);

                if (particle.useColorAndScaleOverLife) {
                    // progressが0なら開始値、1なら終了値になるように補間する
                    particle.transform.scale = Lerp(particle.startScale, particle.endScale, progress);
                    particle.color.x = particle.startColor.x + (particle.endColor.x - particle.startColor.x) * progress;
                    particle.color.y = particle.startColor.y + (particle.endColor.y - particle.startColor.y) * progress;
                    particle.color.z = particle.startColor.z + (particle.endColor.z - particle.startColor.z) * progress;
                    particle.color.w = particle.startColor.w + (particle.endColor.w - particle.startColor.w) * progress;
                    alpha = particle.color.w;
                } else {
                    // 従来のパーティクルは、色を変えずに透明度だけを下げる
                    alpha = particle.color.w * (1.0f - progress);
                }
            }

            Matrix4x4 scale = MakeScaleMatrix(particle.transform.scale);
            Matrix4x4 rotateZ = MakeRotateZMatrix(particle.transform.rotate.z);
            Matrix4x4 translate = MakeTranslateMatrix(particle.transform.translate);

            Matrix4x4 rotateY = MakeRotateYMatrix(particle.transform.rotate.y);
            Matrix4x4 rotateX = MakeRotateXMatrix(particle.transform.rotate.x);
            Matrix4x4 rotate = Multiply(Multiply(rotateX, rotateY), rotateZ);
            Matrix4x4 rotateOrBillboard = group.useBillboard ? Multiply(rotateZ, billboardMatrix) : rotate;
            Matrix4x4 worldMatrix = Multiply(Multiply(scale, rotateOrBillboard), translate);

            // 取得したactiveCameraからビュープロジェクション行列をもらう(カメラの向きを向く)
            Matrix4x4 worldViewProjectionMatrix = Multiply(activeCamera->GetViewMatrix(),activeCamera->GetProjectionMatrix());

            if (group.instanceCount < kNumMaxInstance) {
                group.mappedData[group.instanceCount].WVP = worldViewProjectionMatrix;
                group.mappedData[group.instanceCount].World = worldMatrix;
                group.mappedData[group.instanceCount].color = particle.color;
                group.mappedData[group.instanceCount].color.w = alpha;
                group.instanceCount++;
            }
            ++it;
        }
    }
}

// 描画処理
void ParticleManager::Draw() {

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // 1〜4. パイプラインと頂点データの設定
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(graphicsPipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // マテリアル定数バッファ
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    for (auto& pair : particleGroups_) {
        ParticleGroup& group = pair.second;

        if (group.instanceCount == 0) continue;

        D3D12_VERTEX_BUFFER_VIEW vertexBufferView = group.vertexResource ? group.vertexBufferView : vertexBufferView_;
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

        // 5. インスタンシング用 SRV
        srvManager_->SetGraphicsRootDescriptorTable(1, group.instancingSrvIndex);

        // 6. テクスチャ用 SRV
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(group.textureFilePath);
        commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

        // 7. インスタンシング描画
        commandList->DrawInstanced(group.vertexCount, group.instanceCount, 0, 0);
    }
}
