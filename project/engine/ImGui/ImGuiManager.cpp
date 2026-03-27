#include "ImGuiManager.h"

void ImGuiManager::Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager)
{
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	//フォントビルドの強制
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Build();

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	int index = srvManager->Allocate();

	assert(srvManager->GetDescriptorHeap() != nullptr && "SRV Descriptor Heap is null!");

	ImGui_ImplDX12_Init(dxCommon->GetDevice(),
		2, // ダブルバッファリングなら2（Swapchainの数に合わせる）
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, // バックバッファのフォーマット
		srvManager->GetDescriptorHeap(),
		srvManager->GetCPUDescriptorHandle(index),
		srvManager->GetGPUDescriptorHandle(index)
	);
}

void ImGuiManager::Begin()
{
	//ゲームの処理
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiManager::End()
{
	//ImGuiの内部コマンドを生成する
	ImGui::Render();
}

void ImGuiManager::Draw(DirectXCommon* dxCommon)
{
	//実際のcommandListのImGuiの描画コマンドを積む
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());
}

void ImGuiManager::Finalize()
{
	//ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}