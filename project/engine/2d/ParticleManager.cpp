#include "ParticleManager.h"
#include "TextureManager.h"
#include "CameraManager.h"

using namespace MyMath;

// バッファ作成関数
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
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
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
    vertexResource_ = CreateBufferResource(device, sizeof(VertexData) * 6);
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
    materialResource_ = CreateBufferResource(device, sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    memset(materialData_, 0, sizeof(Material));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 0;
    materialData_->uvTransform = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
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
    newGroup.instancingResource = CreateBufferResource(dxCommon_->GetDevice(), sizeof(ParticleForGPU) * kNumMaxInstance);

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

// パーティクルの発生
void ParticleManager::Emit(const std::string name, const Vector3& position, uint32_t count) {
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

        // グループに登録
        group.particles.push_back(newParticle);
    }
}

// 更新処理
void ParticleManager::Update() {

    if (!cameraManager_)return;
    //毎フレーム今アクティブなカメラを取得する
    Camera* activeCamera = cameraManager_->GetActiveCamera();

    //万が一カメラがない場合は安全のために抜ける
    if (!activeCamera)return;

    Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);

    //取得したactiveカメラからビュー作成もらう
    Matrix4x4 view = activeCamera->GetViewMatrix();
    view.m[3][0] = 0.0f; view.m[3][1] = 0.0f; view.m[3][2] = 0.0f;
    Matrix4x4 billboardMatrix = Inverse(view);

    Matrix4x4 viewProjectionMatrix = Multiply(activeCamera->GetViewMatrix(), activeCamera->GetProjectionMatrix());

    const float kDeltaTime_ = 1.0f / 60.0f;

    // 全てのパーティクルグループについて処理する
    for (auto& pair : particleGroups_) {
        ParticleGroup& group = pair.second;
        // インスタンス数をリセット
        group.instanceCount = 0;

        // グループ内の全てのパーティクルについて処理する
        for (auto it = group.particles.begin(); it != group.particles.end();) {
            Particle& particle = *it;

            if (particle.currentTime >= particle.lifeTime) {
                it = group.particles.erase(it);
                continue;
            }

            particle.transform.translate.x += particle.velocity.x * kDeltaTime_;
            particle.transform.translate.y += particle.velocity.y * kDeltaTime_;
            particle.transform.translate.z += particle.velocity.z * kDeltaTime_;
            particle.currentTime += kDeltaTime_;

            float alpha = 1.0f - (particle.currentTime / particle.lifeTime);

            Matrix4x4 scale = MakeScaleMatrix(particle.transform.scale);
            Matrix4x4 translate = MakeTranslateMatrix(particle.transform.translate);

            Matrix4x4 worldMatrix = Multiply(Multiply(scale, billboardMatrix), translate);

            // 取得したactiveCameraからビュープロジェクション行列をもらう
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
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // マテリアル定数バッファ
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    for (auto& pair : particleGroups_) {
        ParticleGroup& group = pair.second;

        if (group.instanceCount == 0) continue;

        // 5. インスタンシング用 SRV
        srvManager_->SetGraphicsRootDescriptorTable(1, group.instancingSrvIndex);

        // 6. テクスチャ用 SRV
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(group.textureFilePath);
        commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

        // 7. インスタンシング描画
        commandList->DrawInstanced(6, group.instanceCount, 0, 0);
    }
}