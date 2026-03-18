#pragma once
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

class ImGuiManager
{
public:

	//初期化
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
	//受付開始
	void Begin();
	//ImGui受付終了
	void End();
	//描画
	void Draw(DirectXCommon* dxCommon);
	//終了処理
	void Finalize();

};

