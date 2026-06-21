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
using namespace MyMath;

class CameraManager;

struct Particle{
public:
    Transform transform;
    Vector3 velocity;
    Vector4 color;
    float lifeTime;
    float currentTime;
    bool receivesWind;//風を受けるかどうか
    bool isEndless = false;
    float scaleVelocityY = 0.0f;

    // 螺旋運動をするパーティクルだけが使用する値
    bool isSpiral = false;
    Vector3 spiralCenter{};
    Vector3 spiralRight{ 1.0f, 0.0f, 0.0f };
    Vector3 spiralUp{ 0.0f, 1.0f, 0.0f };
    float spiralAngle = 0.0f;
    float spiralRadius = 0.0f;
    float spiralAngularVelocity = 0.0f;
    float spiralRadialVelocity = 0.0f;

    // 寿命に合わせて大きさと色を変化させる光エフェクト用の値
    bool useColorAndScaleOverLife = false;
    Vector3 startScale{};
    Vector3 endScale{};
    Vector4 startColor{};
    Vector4 endColor{};
};

struct AccelerationField
{
    Vector3 acceleration;//加速度
    AABB area;//範囲
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
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    uint32_t vertexCount = 6;
    bool useBillboard = true;
};

class ParticleManager
{
public:

    // 最大インスタンス数 (1グループあたり)
    static const uint32_t kNumMaxInstance = 1000;

    // シングルトンインスタンスの取得
    static ParticleManager* GetInstance();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update();
    void Draw();
    void Finalize() {
        // インスタンス自体を消すのではなく、中身（グループリストなど）を掃除する
        particleGroups_.clear();
    }

	//パーティクルの発生
	void Emit(const std::string name, const Vector3& position, uint32_t count,bool receivesWind);
	void EmitHitEffect(const std::string name, uint32_t count, const Vector3& translate);
    void EmitRingEffect(const std::string name, uint32_t count, const Vector3& translate);
    void EmitCylinderEffect(const std::string name, uint32_t count, const Vector3& translate);

    // 光柱の周囲を上昇しながら消える星形の光粒を発生させる
    void EmitPillarSparkle(const std::string name, uint32_t count, const Vector3& position);

    // 中心の周りを回転する二本の螺旋状の光を発生させる
    void EmitLightSpiral(const std::string name, uint32_t count, const Vector3& translate);

    // 中心から広がりながら色と透明度が変化する光球を発生させる
    void EmitLightCore(const std::string name, uint32_t count, const Vector3& position);

    // 画面上部から下方向へ流れる細長い光の雨を発生させる
    void EmitLightRain(const std::string name, uint32_t count, const Vector3& position);

	void CreateParticleGroup(const std::string name, const std::string textureFilePath);
    void CreateRingParticleGroup(const std::string name, const std::string textureFilePath);
    void CreateCylinderParticleGroup(const std::string name, const std::string textureFilePath);

    // 全てのパーティクル（粒子）を削除する
    void ClearAllParticles();
    void ClearParticles(const std::string name);

    // 特定のグループのパーティクルだけを削除する
    void ClearAllGroups();

    void SetCameraManager(CameraManager* cameraManager) { cameraManager_ = cameraManager; }

    bool IsCollision(const AABB aabb, const Vector3& point);
private:
    ParticleManager() = default;
    ~ParticleManager() = default;
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;


    // メンバ変数に記録するポインタ
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    CameraManager* cameraManager_ = nullptr;

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

    //場
    AccelerationField accelerationField_;
    float windTimer_ = 0.0f;//吹く時間
    bool isField_ = false;//風が吹いてるかどうか
};

