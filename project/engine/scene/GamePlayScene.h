#pragma once
#include "DirectXCommon.h"
#include "Object3dCommon.h"
#include "SpriteCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Object3d.h"
#include "Sprite.h"
#include "ParticleEmitter.h"
#include "Audio.h"
#include"BaseScene.h"
#include "SkyBox.h"
#include <vector>
#include <memory>

//BaseSceneを継承する(publicをつけることで公認の親子関係)
class GamePlayScene : public BaseScene
{
public:
    // overrideをつけて、親の純粋仮想関数を実装する
    void Initialize()override;
	void Finalize()override;
	void Update()override;
	void Draw()override;

private:
    void UpdateGameViewCameraControl();
    void DrawInspectorImGui();
    void DrawParticleEffectImGui(bool embedded = false);
    void UpdateParticleEffectEmission();
    Vector3 GetParticleEffectPosition() const;

    struct ParticleEffectControl {
        bool enabled = false;
        int emitCount = 1;
        float scale = 1.0f;
        bool billboard = true;
    };

    //ゲームプレイシーン固有のデータ

    std::unique_ptr<Camera> upCamera;


    std::vector<std::unique_ptr<Object3d>> normalObjects;//通常モデル  
    std::vector<std::unique_ptr<Object3d>> animationObjects;//アニメーションモデル 

    std::vector<std::unique_ptr<Sprite>> sprites;
    std::unique_ptr<SkyBox> skyBox_;

    // パーティクル関連
    std::unique_ptr<ParticleEmitter> emitterCircle;
    std::unique_ptr<ParticleEmitter> emitterPlane;

    // objects vectorの中にある特定のオブジェクトを指すためのポインタ
    Object3d* objectPlane = nullptr;
    Object3d* objectAxis = nullptr;
    // 今使っているエミッターを指すだけ
    ParticleEmitter* activeEmitter = nullptr;

    Object3d::DirectionalLight directionalLight;//平行光源
    Object3d::PointLight pointLight;//点光源
    Object3d::SpotLight spotLight;//スポットライト

    // パーティクルのトランスフォーム
    Transform emitterTransform{};

    int selectedUI = 0;
    bool isCylinderEffectVisible_ = false;
    ParticleEffectControl hitEffect_{ false, 8, 1.0f, true };
    ParticleEffectControl ringEffect_{ false, 3, 1.0f, true };
    ParticleEffectControl cylinderEffect_{ false, 1, 1.0f, false };
    ParticleEffectControl pillarSparkleEffect_{ false, 10, 1.0f, true };
    ParticleEffectControl lightCoreEffect_{ false, 1, 1.0f, true };
    ParticleEffectControl lightRainEffect_{ false, 8, 1.0f, true };
    ParticleEffectControl lightSpiralEffect_{ false, 24, 1.0f, true };
    float particleEffectEmitTimer_ = 0.0f;
    bool lastCylinderEffectEnabled_ = false;
    int lastCylinderEffectEmitCount_ = 1;
    float lastCylinderEffectScale_ = 1.0f;
    bool lastCylinderEffectBillboard_ = false;
    bool refreshCylinderEffect_ = false;
    bool isGameViewCameraDragging_ = false;
  
    //アニメーション
    Model::Animation walkAnimation_;
    Model::Animation sneakWalkAnimation_;
    Model::Animation humanAnimation_;
    Model::Animation hissatu_;
    
    // アニメーションの再生時間を管理
    // (ダメージ発生タイミングやコンボ時間、イベントシーンなどをアニメーションの時間タイミングで合わせれる)
    float animationTime_ = 0.0f;
};

