#pragma once
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Object3d.h"
#include "ParticleEmitter.h"
#include "CameraManager.h"
#include<vector>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif

class WinApp;
class Sprite;
struct Transform;
struct DirectionalLightData;
class CameraManager;
class Object3d;
class ParticleEmitter;

class ImGuiManager
{
public:

	static ImGuiManager* GetInstance() {
		static ImGuiManager instance;
		return &instance;
	}

	ImGuiManager(const ImGuiManager&) = delete;
	ImGuiManager& operator=(const ImGuiManager&) = delete;

	//初期化
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
	//受付開始
	void Begin();
	//ImGui受付終了
	void End();
	//デモウィンドウ
	void DemoWindow();
	//FPS確認
	void FPSWindow();
	//スプライト編集
	void SpriteWindow(const std::vector<std::unique_ptr<Sprite>>& sprites);
	//3Dモデル編集
	void ModelWindow(const std::vector<std::unique_ptr<Object3d>>& objects, Object3d::DirectionalLight& light);
	//カメラ編集
	void CameraWindow(CameraManager* cameraManager);
	//描画
	void Draw(DirectXCommon* dxCommon);
	//終了処理
	void Finalize();

	//パーティクル編集
	std::string ParticleWindow(Transform& emitterTransform);
private:
	ImGuiManager() = default;
	~ImGuiManager() = default;
};

