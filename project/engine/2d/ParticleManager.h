#pragma once
#include <string>
#include <unordered_map>
#include <list>
#include <random>
#include <cassert>
#include <d3d12.h>
#include <wrl.h>
#include "Vector3.h"
#include "MyMath.h"
#include "Object3d.h"
#include "DirectXCommon.h"
#include "SrvManager.h"

struct Particle{
public:
    Transform transform;
    Vector3 velocity;
    Vector4 color;
    float lifeTime;
    float currentTime;
};

struct ParticleForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4 color;
};

// マテリアル（定数バッファ用）
struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvTransform;
};

// 頂点データ
struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

struct ParticleGroup
{
    // マテリアルデータ
    std::string textureFilePath;
    uint32_t textureSrvIndex;
    // パーティクルのリスト
    std::list<Particle> particles;
    // インスタンシング用データ
    uint32_t instancingSrvIndex;
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
    uint32_t instanceCount;
    // インスタンシングデータを書き込むためのポインタ
    ParticleForGPU* mappedData;
};

class ParticleManager
{
public:

    // 最大インスタンス数 (1グループあたり)
    static const uint32_t kNumMaxInstance = 1000;

    // シングルトンインスタンスの取得
    static ParticleManager* GetInstance();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update(const Matrix4x4 viewProjectionMatrix, const Matrix4x4& cameraMatrix);
    void Draw();
    void Finalize() {
        // インスタンス自体を消すのではなく、中身（グループリストなど）を掃除する
        particleGroups_.clear();
    }

	//パーティクルの発生
	void Emit(const std::string name, const Vector3& position, uint32_t count);

	void CreateParticleGroup(const std::string name, const std::string textureFilePath);

    // 全てのパーティクル（粒子）を削除する
    void ClearAllParticles();

    // 特定のグループのパーティクルだけを削除する
    void ClearAllGroups();

private:
    ParticleManager() = default;
    ~ParticleManager() = default;
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;


    // メンバ変数に記録するポインタ
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    //グループを複数持てるように
    std::unordered_map<std::string, ParticleGroup> particleGroups_;
    // ランダムエンジン
    std::mt19937 randomEngine_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};

