#pragma once
#include "DirectXCommon.h"
#include "Object3dCommon.h"
#include "SpriteCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Object3d.h"
#include "Sprite.h"
#include "ParticleEmitter.h"
#include "Audio.h" // SoundDataを使うために必要
#include "BaseScene.h"
#include "SkyBox.h"
#include <vector>
#include <memory>

//BaseSceneを継承する(publicをつけることで公認の親子関係)
class TitleScene : public BaseScene
{
public:
    //overrideをつけて、親の純粋仮想関数を実装
    void Initialize()override;
    void Finalize()override;
    void Update()override;
    void Draw()override;

    //タイトルシーン独自の関数
    bool IsFinished() const { return isFinished_; }

private:
    // 全エフェクトの基準位置を、カメラ正面の見える位置へ移動する
    void UpdateEffectPosition();

    // 30秒間の経過時間から、現在再生するエフェクトを決める
    void UpdateEffectTimeline();

    // 0～5秒に、光の中心と小さな波紋を表示する
    void PlayLightCoreEffect(const Vector3& position);

    // 5～10秒に、放射状の光と複数の波紋を表示する
    void PlayLightBurstEffect(const Vector3& position);

    // 10～15秒に、光の柱と上昇する光粒を表示する
    void PlayLightPillarEffect(const Vector3& position);

    // 15～20秒に、水色と紫色の二本の螺旋光を表示する
    void PlayLightSpiralEffect(const Vector3& position);

    // 20～25秒に、上から降る光の雨と中心の閃光を表示する
    void PlayLightRainEffect(const Vector3& position);

    // 25～30秒に、これまでの光を重ねてフィナーレを表示する
    void PlayLightFinaleEffect(const Vector3& position);

    //タイトルシーン固有のデータ

    std::unique_ptr<Sprite> sprite_;
    std::unique_ptr<SkyBox> skyBox_;

    std::vector<std::unique_ptr<Object3d>> normalObjects;//通常モデル  
    Object3d* obj = nullptr;
    bool isFinished_ = false;

    // ゲームシーンから流用したパーティクルエミッター
    std::unique_ptr<ParticleEmitter> emitterCircle;
    std::unique_ptr<ParticleEmitter> emitterPlane;
    ParticleEmitter* activeEmitter = nullptr;

    // タイトルシーンで表示するエフェクトの基準位置
    Transform emitterTransform{};
    bool isCylinderEffectVisible_ = false;

    // 30秒の自動再生で使用する時間と現在の区間
    float effectTimer_ = 0.0f;
    float emitTimer_ = 0.0f;
    int currentEffectPhase_ = -1;

};
