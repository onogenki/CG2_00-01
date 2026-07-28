#pragma once

#include "CameraManager.h"
#include "DirectXCommon.h"
#include "LevelLoader.h"
#include "Object3d.h"
#include "ParticleEmitter.h"
#include "SrvManager.h"
#include "WinApp.h"
#include "MyMath.h"

#include <functional>
#include <vector>
#include <string>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif

class Sprite;
struct Transform;
class Mirror;

// Stage Map Editorで押された操作と、編集中の変更をSceneへ返します。
struct LevelEditorResult
{
	bool reloadRequested = false;
	bool saveRequested = false;
	bool dataChanged = false;
	bool addSphereRequested = false;
	bool addEventPairRequested = false;
	bool addPathSphereRequested = false;
	bool removeSelectedRequested = false;
};

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
	int SpriteWindow(const std::vector<std::unique_ptr<Sprite>>& sprites, bool embedded = false, int forcedSpriteIndex = -1);
	void ModelWindow(
		std::vector<std::unique_ptr<Object3d>>& normalObjects,
		std::vector<std::unique_ptr<Object3d>>& animationObjects,
		Object3d::DirectionalLight& light,
		Object3d::PointLight& pointLight,
		Object3d::SpotLight& spotLight,
		bool embedded = false,
		size_t protectedNormalObjectCount = 0,
		size_t protectedAnimationObjectCount = 0,
		int forcedNormalObjectIndex = -1,
		int forcedAnimationObjectIndex = -1,
		const std::function<void(bool animationObject, size_t index)>& onObjectRemoved = {});
	void CameraWindow(CameraManager* cameraManager, bool embedded = false);
	std::string ParticleWindow(Transform& emitterTransform, bool embedded = false);
	void PostEffectWindow();
	// レベルファイルの再読込と、マップ内オブジェクトの編集UIを表示します。
	LevelEditorResult LevelHotReloadWindow(
		bool& autoReload,
		const std::string& filePath,
		const std::string& status,
		LevelLoader::LevelData* levelData,
		int& selectedObjectIndex);
	// 鏡の中心・大きさ・回転を編集するデバッグ用ウィンドウを表示します。
	// 値が変更されたフレームだけ true を返します。
	bool MirrorDebugWindow(Mirror& mirror, float& mirrorYaw, const Camera& reflectionCamera);
	//OBBをゲーム画面へワイヤー表示し、衝突中は赤、非衝突時は青で描画する
	void DrawObbCollisionDebug(const MyMath::OBB& obb, const MyMath::Sphere& sphere, const Camera* camera, bool isColliding);
	// 制御点をGame Viewへ線と番号で重ねて表示します。
	void DrawControlPointPathDebug(
		const Vector3& basePosition,
		const std::vector<Vector3>& controlPoints,
		const Camera* camera);
	bool IsSkeletonDebugDrawEnabled() const;
	bool IsMouseOverGameView(float mouseScreenX, float mouseScreenY) const;
	bool GetGameViewRect(float& x, float& y, float& width, float& height) const;
	bool GetGameViewScreenRect(int& x, int& y, int& width, int& height) const;
	unsigned int GetInspectorDockId() const;

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
	void ResetLayoutToDefault();
	void BuildDefaultDockLayout(ImGuiID dockspaceId);
	void GameViewWindow();
	void SceneWindow(const char* sceneName);

	bool showGameView_ = true;
	bool showSceneWindow_ = true;
	bool showFpsWindow_ = true;
	bool showSpriteWindow_ = true;
	bool showModelWindow_ = true;
	bool showParticleWindow_ = true;
	bool showCameraWindow_ = true;
	bool showPostEffectWindow_ = true;
	bool showDemoWindow_ = false;
	bool showSkeletonDebugDraw_ = false;
	bool resetDockLayout_ = false;
	std::string layoutSceneName_;
	int layoutResetFrames_ = 0;

	WinApp* winApp_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	ImVec2 gameViewImageMin_{};
	ImVec2 gameViewImageSize_{};
	ImDrawList* gameViewDrawList_ = nullptr;
	ImGuiID inspectorDockId_ = 0;
#endif
};
