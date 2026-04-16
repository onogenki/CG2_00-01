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
#include <vector>
#include"BaseScene.h"

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

    Camera* upCamera = nullptr;

    Object3d* objectPlane = nullptr;
    Object3d* objectAxis = nullptr;
    std::vector<Object3d*> objects;
    Object3d::DirectionalLight lightData;

    std::vector<Sprite*> sprites;

    // パーティクル関連
    ParticleEmitter* emitterCircle = nullptr;
    ParticleEmitter* emitterPlane = nullptr;
    ParticleEmitter* activeEmitter = nullptr;

    // パーティクルのトランスフォーム
    Transform emitterTransform{};

    int selectedUI = 0;
   
};

