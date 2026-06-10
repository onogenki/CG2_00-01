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
  
    //アニメーション
    Model::Animation walkAnimation_;
    Model::Animation sneakWalkAnimation_;
    Model::Animation humanAnimation_;
    Model::Animation hissatu_;
    
    // アニメーションの再生時間を管理
    // (ダメージ発生タイミングやコンボ時間、イベントシーンなどをアニメーションの時間タイミングで合わせれる)
    float animationTime_ = 0.0f;
};

