#pragma once

#include <vector>
#include <wrl.h>
#include <xaudio2.h>
#include "Audio.h"
#include "Object3d.h"
#include "Sprite.h"
#include "ParticleEmitter.h"
#include "Transform.h"
#include "Framework.h"

class WinApp;
class DirectXCommon;
class SrvManager;
class ImGuiManager;
class Input;
class Object3dCommon;
class CameraManager;
class Camera;
class SpriteCommon;

class Game : public Framework
{
public:

	void Initialize() override;
	void Finalize()override;
	void Update()override;
	void Draw()override;

private:
	// --- オーディオ系 ---
	SoundData soundData1;

	// --- 3Dオブジェクト・カメラ系 ---
	Object3dCommon* object3dCommon = nullptr;
	CameraManager* cameraManager = nullptr;
	Camera* mainCamera = nullptr;
	Camera* upCamera = nullptr;

	std::vector<Object3d*> objects;
	Object3d* objectPlane = nullptr;
	Object3d* objectAxis = nullptr;
	Object3d::DirectionalLight lightData{};

	// --- スプライト系 ---
	SpriteCommon* spriteCommon = nullptr;
	std::vector<Sprite*> sprites;

	// --- パーティクル系 ---
	Transform emitterTransform{};
	ParticleEmitter* emitterCircle = nullptr;
	ParticleEmitter* emitterPlane = nullptr;
	ParticleEmitter* activeEmitter = nullptr;

	// --- UI・その他 ---
	int selectedUI = 0;
};

