#pragma once

#include <vector>
#include <wrl.h>
#include <xaudio2.h>
#include "Audio.h"
#include "Object3d.h"
#include "Sprite.h"
#include "ParticleEmitter.h"
#include "Transform.h"

class WinApp;
class DirectXCommon;
class SrvManager;
class ImGuiManager;
class Input;
class Object3dCommon;
class CameraManager;
class Camera;
class SpriteCommon;

class Game
{
public:

	void Initialize();
	void Finalize();
	void Update();
	void Draw();
	//終了フラグのチェック
	bool IsEndRequest() { return endRequest_; }

private:
	// --- 基盤システム・マネージャー系 ---
	WinApp* winApp = nullptr;
	DirectXCommon* dxCommon = nullptr;
	SrvManager* srvManager = nullptr;
	ImGuiManager* imGuiManager = nullptr;
	Input* input = nullptr;

	// --- オーディオ系 ---
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice = nullptr;
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

	//ゲーム終了フラグ
	bool endRequest_ = false;
};

