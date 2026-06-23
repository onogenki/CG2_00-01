#pragma once

#include "CameraManager.h"
#include "DirectXCommon.h"
#include "Object3d.h"
#include "ParticleEmitter.h"
#include "SrvManager.h"
#include "WinApp.h"

#include <vector>
#include <string>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif

class Sprite;
struct Transform;

class ImGuiManager
{
public:
	static ImGuiManager* GetInstance()
	{
		static ImGuiManager instance;
		return &instance;
	}

	ImGuiManager(const ImGuiManager&) = delete;
	ImGuiManager& operator=(const ImGuiManager&) = delete;

	// ImGuiの初期化とフレーム制御
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
	void Begin(const char* sceneName = nullptr);
	void End();
	void Draw(DirectXCommon* dxCommon);
	void Finalize();

	// 共通デバッグウィンドウ
	void DemoWindow();
	void FPSWindow();
	void SpriteWindow(const std::vector<std::unique_ptr<Sprite>>& sprites);
	void ModelWindow(
		const std::vector<std::unique_ptr<Object3d>>& normalObjects,
		const std::vector<std::unique_ptr<Object3d>>& animationObjects,
		Object3d::DirectionalLight& light,
		Object3d::PointLight& pointLight,
		Object3d::SpotLight& spotLight);
	void CameraWindow(CameraManager* cameraManager);
	std::string ParticleWindow(Transform& emitterTransform);

	// Game View上へスケルトンを重ねて描画する
	void SkeletonDebugDraw(
		const Model::Skeleton& skeleton,
		const Matrix4x4& worldMatrix,
		const Matrix4x4& viewProjectionMatrix);

private:
	ImGuiManager() = default;
	~ImGuiManager() = default;

#ifdef USE_IMGUI
	void BeginDockSpace(const char* sceneName);
	void BuildDefaultDockLayout(ImGuiID dockspaceId);
	void GameViewWindow();
	void SceneWindow(const char* sceneName);

	bool showGameView_ = true;
	bool showSceneWindow_ = true;
	bool showFpsWindow_ = true;
	bool showDemoWindow_ = false;
	bool resetDockLayout_ = false;
	std::string layoutSceneName_;
	int layoutResetFrames_ = 0;

	WinApp* winApp_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	ImVec2 gameViewImageMin_{};
	ImVec2 gameViewImageSize_{};
	ImDrawList* gameViewDrawList_ = nullptr;
#endif
};
