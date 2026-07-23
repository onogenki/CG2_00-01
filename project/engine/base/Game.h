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

// 現在Gameが管理する大まかな画面状態。
enum class SceneType {
	TITLE,
	GAMEPLAY
};

class Game : public Framework
{
public:

	// Frameworkのライフサイクルに合わせてゲーム全体を管理する。
	void Initialize() override;
	void Finalize()override;
	void Update()override;
	void Draw()override;

private:

	// 環境変数で指定されたシーン再起動テストの設定を読む。
	void InitializeSceneStressFromEnvironment();
	// 有効時は一定フレームごとにシーンを切り替え、初期化・破棄を検証する。
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

