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
#include "BaseScene.h"

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
    //タイトルシーン固有のデータ

    Sprite* sprite_ = nullptr;

    bool isFinished_ = false;

};