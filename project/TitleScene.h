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

class TitleScene
{
public:
    void Initialize();

    void Finalize();

    void Update();

    void Draw();

private:
    Object3dCommon* object3dCommon = nullptr;
    SpriteCommon* spriteCommon = nullptr;

    CameraManager* cameraManager = nullptr;
    Camera* mainCamera = nullptr;
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

    // 音声
    SoundData soundData1;

};