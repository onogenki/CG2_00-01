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
#include "TitleScene.h"
#include "GamePlayScene.h"
#include <string>

class WinApp;
class DirectXCommon;
class SrvManager;
class ImGuiManager;
class Input;
class Object3dCommon;
class CameraManager;
class Camera;
class SpriteCommon;

enum class SceneType {
	TITLE,
	GAMEPLAY
};

class Game : public Framework
{
public:

	void Initialize() override;
	void Finalize()override;
	void Update()override;
	void Draw()override;

private:

	void InitializeSceneStressFromEnvironment();
	void UpdateSceneStress();

	SceneType currentScene_ = SceneType::TITLE;

	int selectedUI = 0;

	bool sceneStressEnabled_ = false;
	std::string sceneStressTarget_ = "GAMEPLAY";
	int sceneStressRequestedRestarts_ = 0;
	int sceneStressCompletedRestarts_ = 0;
	int sceneStressIntervalFrames_ = 8;
	int sceneStressFrameCounter_ = 0;
};

