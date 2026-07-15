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
#include "BaseScene.h"
#include "SceneEditor.h"
#include "SkyBox.h"
#include <memory>
#include <string>
#include <vector>

class TitleScene : public BaseScene
{
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	bool IsFinished() const { return isFinished_; }

private:
	void ScanResourceShelf();
	void DrawTitleModelShelfImGui();
	void DrawTitleInspectorImGui();
	void HandleTitleShelfDropOnGameView();
	bool AddModelToTitle(const std::string& fileName);
	bool AddTextureToTitle(const std::string& textureFilePath);

	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<SkyBox> skyBox_;

	std::vector<std::unique_ptr<Object3d>> normalObjects;
	std::vector<std::unique_ptr<Object3d>> animationObjects_;
	std::vector<std::unique_ptr<Sprite>> addedSprites_;
	SceneEditor::ShelfState shelfState_;
	Object3d::DirectionalLight directionalLight_{};
	Object3d::PointLight pointLight_{};
	Object3d::SpotLight spotLight_{};
	size_t baseNormalObjectCount_ = 0;
	size_t selectedTitleSpriteIndex_ = 0;
	int inspectorAutoSelectSpriteFrames_ = 0;
	Object3d* obj = nullptr;
	bool isFinished_ = false;
};
