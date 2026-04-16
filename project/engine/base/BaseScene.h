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
#include <vector>

//前方宣言(クラスがあることだけ伝える)
class SceneManager;

class BaseScene
{
public:
	//仮想デストラクタ
	virtual ~BaseScene() = default;

	//純粋仮想関数
	virtual void Initialize() = 0;
	virtual void Finalize() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
	virtual bool IsFinished() const { return false; }

protected:

	Object3dCommon* object3dCommon = nullptr;
	SpriteCommon* spriteCommon = nullptr;

	CameraManager* cameraManager = nullptr;
	Camera* mainCamera = nullptr;

};

