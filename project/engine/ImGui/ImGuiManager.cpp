#include "ImGuiManager.h"
#include "SrvManager.h"
#include "Sprite.h"
#include "ImGuiManager.h"
#include "Object3d.h"
#include "ParticleEmitter.h"
#include "CameraManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "SceneManager.h"
#include "PostEffect.h"
#include "CaptureManager.h"
#include <cmath>
#ifdef USE_IMGUI
#include "externals/imgui/imgui_internal.h"
#endif
using namespace MyMath;

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]]DirectXCommon* directXCommon, [[maybe_unused]]SrvManager* srvManager)
{
#ifdef USE_IMGUI

	//ImGuiのコンテキストを生成
	ImGui::CreateContext();
	winApp_ = winApp;
	dxCommon_ = directXCommon;
	srvManager_ = srvManager;
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = "imgui_docking.ini";
	//ImGuiのスタイルを設定
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 3.0f;
	style.FrameRounding = 3.0f;
	style.TabRounding = 3.0f;

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	assert(srvManager->GetDescriptorHeap() != nullptr && "SRV Descriptor Heap is null!");

	//DirectX12用の初期化情報
	ImGui_ImplDX12_InitInfo initInfo = {};

	//初期化情報を設定する
	initInfo.Device = directXCommon->GetDevice();
	initInfo.CommandQueue = directXCommon->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<int>(directXCommon->GetSwapChainResourcesNum());
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	initInfo.SrvDescriptorHeap = srvManager->GetDescriptorHeap();

	//SRV確保用関数の設定(ラムダ式)
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
		{
			SrvManager* srvManager = SrvManager::GetInstance();
			uint32_t index = srvManager->Allocate();
			if (index == SrvManager::kInvalidSrvIndex) {
				*out_cpu_handle = {};
				*out_gpu_handle = {};
				return;
			}
			*out_cpu_handle = srvManager->GetCPUDescriptorHandle(index);
			*out_gpu_handle = srvManager->GetGPUDescriptorHandle(index);
		};

	//SRV解放用関数の設定
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
		{
			SrvManager::GetInstance()->Free(cpu_handle);
			//SrvManagerに解放機能を作っていないためここでは何もしない
		};

	//DirectX12用の初期化を行う
	ImGui_ImplDX12_Init(&initInfo);

#endif

}

void ImGuiManager::Begin(const char* sceneName)
{
#ifdef USE_IMGUI
	//ゲームの処理
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	BeginDockSpace(sceneName != nullptr ? sceneName : "Unknown");
#endif
}

void ImGuiManager::End()
{
#ifdef USE_IMGUI
	ImGui::End();
	//ImGuiの内部コマンドを生成する
	ImGui::Render();
#endif
}

#ifdef USE_IMGUI
void ImGuiManager::BeginDockSpace(const char* sceneName)
{
	gameViewDrawList_ = nullptr;
	gameViewImageMin_ = {};
	gameViewImageSize_ = {};
	if (layoutSceneName_ != sceneName) {
		layoutSceneName_ = sceneName;
		layoutResetFrames_ = 120;
	}
	if (layoutResetFrames_ > 0) {
		resetDockLayout_ = true;
		--layoutResetFrames_;
	}

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	const ImGuiWindowFlags hostFlags =
		ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Debug Dockspace", nullptr, hostFlags);
	ImGui::PopStyleVar(3);

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("Display")) {
			const WinApp::WindowMode currentMode = winApp_->GetWindowMode();
			if (ImGui::MenuItem("Windowed (Current Size)", nullptr, currentMode == WinApp::WindowMode::Windowed)) {
				winApp_->SetWindowMode(WinApp::WindowMode::Windowed);
			}
			if (ImGui::MenuItem("Maximized (With Close Button)", nullptr, currentMode == WinApp::WindowMode::Maximized)) {
				winApp_->SetWindowMode(WinApp::WindowMode::Maximized);
			}
			if (ImGui::MenuItem("Borderless Fullscreen", nullptr, currentMode == WinApp::WindowMode::BorderlessFullscreen)) {
				winApp_->SetWindowMode(WinApp::WindowMode::BorderlessFullscreen);
			}
			ImGui::Separator();
			ImGui::TextDisabled("Current: %u x %u", winApp_->GetClientWidth(), winApp_->GetClientHeight());
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Windows")) {
			ImGui::MenuItem("Game View", nullptr, &showGameView_);
			ImGui::MenuItem("Scene", nullptr, &showSceneWindow_);
			ImGui::TextDisabled("Top Tools contains FPS / Capture / Post Effect");
			ImGui::Separator();
			ImGui::MenuItem("Inspector", nullptr, &showModelWindow_);
			ImGui::Separator();
			ImGui::MenuItem("ImGui Demo", nullptr, &showDemoWindow_);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Layout")) {
			if (ImGui::MenuItem("Reset to Default")) {
				ResetLayoutToDefault();
			}
			ImGui::EndMenu();
	}
	ImGui::TextDisabled("Scene: %s", sceneName);
	ImGui::SameLine();
	ImGui::TextDisabled("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::EndMenuBar();
	}

	const ImGuiID dockspaceId = ImGui::GetID("MainDockspaceV10");
	ImGuiDockNode* dockNode = ImGui::DockBuilderGetNode(dockspaceId);
	const bool hasUsableViewport = viewport->WorkSize.x >= 640.0f && viewport->WorkSize.y >= 360.0f;
	const bool hasTinySavedLayout =
		dockNode != nullptr && (dockNode->Size.x < 128.0f || dockNode->Size.y < 128.0f);
	if (hasUsableViewport && (resetDockLayout_ || dockNode == nullptr || hasTinySavedLayout)) {
		BuildDefaultDockLayout(dockspaceId);
		resetDockLayout_ = false;
	}
	ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	const std::string currentSceneName = sceneName ? sceneName : "";
	if (showModelWindow_ && currentSceneName != "GamePlay") {
		if (inspectorDockId_ != 0) {
			ImGui::SetNextWindowDockID(inspectorDockId_, ImGuiCond_Always);
		}
		ImGui::Begin("Inspector", &showModelWindow_);
		ImGui::TextDisabled("Scene-specific controls are shown below when available.");
		ImGui::Separator();
		ImGui::End();
	}

	if (showGameView_) {
		GameViewWindow();
	}
	if (showSceneWindow_) {
		SceneWindow(sceneName);
	}
	if (ImGui::Begin("Top Tools")) {
		const ImGuiTableFlags tableFlags =
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV;
		if (ImGui::BeginTable("SharedTopToolsTable", 3, tableFlags, ImVec2(-1.0f, 0.0f))) {
			ImGui::TableSetupColumn("FPS", ImGuiTableColumnFlags_WidthFixed, 200.0f);
			ImGui::TableSetupColumn("Capture", ImGuiTableColumnFlags_WidthFixed, 620.0f);
			ImGui::TableSetupColumn("PostEffect", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("FPS");
			static float sharedFpsValues[90] = {};
			static int sharedFpsValueOffset = 0;
			sharedFpsValues[sharedFpsValueOffset] = ImGui::GetIO().Framerate;
			sharedFpsValueOffset = (sharedFpsValueOffset + 1) % 90;
			char overlay[32];
			snprintf(overlay, sizeof(overlay), "%.1f FPS", ImGui::GetIO().Framerate);
			const float fpsGraphWidth = (std::max)(120.0f, ImGui::GetContentRegionAvail().x);
			ImGui::PlotLines(
				"##SharedTopToolsFPS",
				sharedFpsValues,
				90,
				sharedFpsValueOffset,
				overlay,
				0.0f,
				120.0f,
				ImVec2(fpsGraphWidth, 58.0f));

			ImGui::TableSetColumnIndex(1);
			CaptureManager::GetInstance()->DrawImGui();

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted("Post Effect");
			bool isGrayscale = PostEffect::GetInstance()->IsGrayscale();
			bool isSepia = PostEffect::GetInstance()->IsSepia();
			if (ImGui::Checkbox("Gray", &isGrayscale) && isGrayscale) {
				isSepia = false;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Sepia", &isSepia) && isSepia) {
				isGrayscale = false;
			}
			PostEffect::GetInstance()->SetGrayscale(isGrayscale);
			PostEffect::GetInstance()->SetSepia(isSepia);
			ParticleManager* particleManager = ParticleManager::GetInstance();
			bool particlesReturning = particleManager->IsReturning();
			if (ImGui::Checkbox("Particle return##TopTools", &particlesReturning)) {
				particleManager->SetReturning(particlesReturning);
			}
			ImGui::TextDisabled("Capture uses Game View in every scene.");
			ImGui::EndTable();
		}
	}
	ImGui::End();

	if (currentSceneName != "GamePlay") {
		if (ImGui::Begin("Model Shelf")) {
			ImGui::TextUnformatted("Common Dock Model Shelf");
			ImGui::Separator();
			ImGui::TextWrapped("This bottom area is reserved in every scene so the layout stays consistent.");
			ImGui::BulletText("GamePlay: model / 2D Texture cards, preview, drag placement.");
			ImGui::BulletText("Title: shared dock placeholder and scene navigation.");
			if (SceneManager::GetInstance()->GetCurrentSceneName() != "GAMEPLAY") {
				if (ImGui::Button("Go to GamePlay")) {
					SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset Dock Layout")) {
				ResetLayoutToDefault();
			}
		}
		ImGui::End();
	}
	if (showDemoWindow_) {
		ImGui::ShowDemoWindow(&showDemoWindow_);
	}
}

void ImGuiManager::ResetLayoutToDefault()
{
	showGameView_ = true;
	showSceneWindow_ = true;
	showFpsWindow_ = true;
	showSpriteWindow_ = true;
	showModelWindow_ = true;
	showParticleWindow_ = true;
	showCameraWindow_ = true;
	showPostEffectWindow_ = true;
	showDemoWindow_ = false;
	resetDockLayout_ = true;
}

void ImGuiManager::BuildDefaultDockLayout(ImGuiID dockspaceId)
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

	ImGuiID centerId = dockspaceId;
	const ImGuiID leftId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.20f, nullptr, &centerId);
	const ImGuiID bottomId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.39f, nullptr, &centerId);
	const ImGuiID topId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Up, 0.18f, nullptr, &centerId);
	const ImGuiID rightId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.40f, nullptr, &centerId);
	inspectorDockId_ = rightId;

	ImGui::DockBuilderDockWindow("Game View", centerId);
	ImGui::DockBuilderDockWindow("Scene", leftId);
	ImGui::DockBuilderDockWindow("Top Tools", topId);
	ImGui::DockBuilderDockWindow("Inspector", rightId);
	ImGui::DockBuilderDockWindow("Model Shelf", bottomId);
	ImGui::DockBuilderFinish(dockspaceId);
}

void ImGuiManager::GameViewWindow()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.015f, 0.015f, 0.015f, 1.0f));
	const bool isVisible = ImGui::Begin(
		"Game View",
		&showGameView_,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	if (isVisible) {
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float textureWidth = static_cast<float>(dxCommon_->GetClientWidth());
		const float textureHeight = static_cast<float>(dxCommon_->GetClientHeight());
		if (available.x > 0.0f && available.y > 0.0f && textureWidth > 0.0f && textureHeight > 0.0f) {
			const float scale = (available.x / textureWidth < available.y / textureHeight)
				? available.x / textureWidth
				: available.y / textureHeight;
			gameViewImageSize_ = ImVec2(textureWidth * scale, textureHeight * scale);
			const ImVec2 contentStart = ImGui::GetCursorScreenPos();
			gameViewImageMin_ = ImVec2(
				contentStart.x + (available.x - gameViewImageSize_.x) * 0.5f,
				contentStart.y + (available.y - gameViewImageSize_.y) * 0.5f);
			gameViewDrawList_ = ImGui::GetWindowDrawList();

			const uint32_t textureSrvIndex = PostEffect::GetInstance()->IsEnabled()
				? dxCommon_->GetPostEffectTextureSrvIndex()
				: dxCommon_->GetRenderTextureSrvIndex();
			const D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = srvManager_->GetGPUDescriptorHandle(textureSrvIndex);
			ImGui::SetCursorScreenPos(gameViewImageMin_);
			ImGui::Image(ImTextureRef(static_cast<ImTextureID>(textureHandle.ptr)), gameViewImageSize_);
		}
	}
	ImGui::End();
}

void ImGuiManager::SceneWindow(const char* sceneName)
{
	if (!ImGui::Begin("Scene", &showSceneWindow_)) {
		ImGui::End();
		return;
	}
	SceneManager* sceneManager = SceneManager::GetInstance();
	const std::string& currentScene = sceneManager->GetCurrentSceneName();
	const char* currentLabel = currentScene.empty() ? sceneName : currentScene.c_str();

	ImGui::Text("Current Scene");
	ImGui::Separator();
	ImGui::TextUnformatted(currentLabel);
	ImGui::Spacing();

		ImGui::BeginDisabled(sceneManager->HasPendingScene());
	if (ImGui::BeginCombo("Scene", currentLabel)) {
		for (const std::string& availableScene : sceneManager->GetAvailableSceneNames()) {
			const bool isCurrent = availableScene == currentScene;
			if (ImGui::Selectable(availableScene.c_str(), isCurrent) && !isCurrent) {
				sceneManager->ChangeScene(availableScene);
			}
			if (isCurrent) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Restart Current Scene")) {
		sceneManager->RestartCurrentScene();
	}
	ImGui::EndDisabled();

	if (sceneManager->HasPendingScene()) {
		const std::string& pendingSceneName = sceneManager->GetPendingSceneName();
		if (!pendingSceneName.empty()) {
			ImGui::TextDisabled("Loading: %s", pendingSceneName.c_str());
		} else {
			ImGui::TextDisabled("Scene reset is settling...");
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Exit Game")) {
		ImGui::OpenPopup("Exit Game?");
	}
	if (ImGui::BeginPopupModal("Exit Game?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted("Exit game?");
		ImGui::Separator();
		if (ImGui::Button("Yes", ImVec2(96.0f, 0.0f))) {
			winApp_->RequestClose();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(96.0f, 0.0f))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Drag any debug window by its tab to rearrange it.");
	ImGui::TextDisabled("Use Layout > Reset to Default to restore the layout.");
	ImGui::End();
}
#endif

bool ImGuiManager::IsMouseOverGameView(float mouseScreenX, float mouseScreenY) const
{
#ifdef USE_IMGUI
	return gameViewImageSize_.x > 0.0f &&
		gameViewImageSize_.y > 0.0f &&
		mouseScreenX >= gameViewImageMin_.x &&
		mouseScreenX <= gameViewImageMin_.x + gameViewImageSize_.x &&
		mouseScreenY >= gameViewImageMin_.y &&
		mouseScreenY <= gameViewImageMin_.y + gameViewImageSize_.y;
#else
	return false;
#endif
}

bool ImGuiManager::GetGameViewRect(float& x, float& y, float& width, float& height) const
{
#ifdef USE_IMGUI
	if (gameViewImageSize_.x <= 0.0f || gameViewImageSize_.y <= 0.0f) {
		return false;
	}
	x = gameViewImageMin_.x;
	y = gameViewImageMin_.y;
	width = gameViewImageSize_.x;
	height = gameViewImageSize_.y;
	return true;
#else
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	return false;
#endif
}

bool ImGuiManager::GetGameViewScreenRect(int& x, int& y, int& width, int& height) const
{
#ifdef USE_IMGUI
	float localX = 0.0f;
	float localY = 0.0f;
	float localWidth = 0.0f;
	float localHeight = 0.0f;
	if (!GetGameViewRect(localX, localY, localWidth, localHeight)) {
		return false;
	}

	x = static_cast<int>(std::round(localX));
	y = static_cast<int>(std::round(localY));
	width = static_cast<int>(std::round(localWidth));
	height = static_cast<int>(std::round(localHeight));
	return width > 0 && height > 0;
#else
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	return false;
#endif
}

unsigned int ImGuiManager::GetInspectorDockId() const
{
#ifdef USE_IMGUI
	return inspectorDockId_;
#else
	return 0;
#endif
}

void ImGuiManager::DemoWindow()
{
#ifdef USE_IMGUI
	ImGui::ShowDemoWindow();
#endif
}

//FPS
void ImGuiManager::FPSWindow()
{

#ifdef USE_IMGUI
	// ウィンドウ作成
	if (!ImGui::Begin("FPS", &showFpsWindow_)) {
		ImGui::End();
		return;
	}

	// FPS波形グラフの描画
	static float fps_values[90] = {};
	static int values_offset = 0;

	// ImGuiの便利機能で、現在のFPS（1秒間のコマ数）を取得して配列に保存
	fps_values[values_offset] = ImGui::GetIO().Framerate;

	// 90個データを入れたらまた0番目から古いデータを上書きしていく(配列にデータを追加し続けるとパンクするため）
	values_offset = (values_offset + 1) % 90;

	// グラフの上に表示するテキスト（現在のFPS）を作成
	char overlay[32];
	snprintf(overlay, sizeof(overlay), "FPS: %.1f", ImGui::GetIO().Framerate);//FPSの計算

	// グラフ描画（0.0f〜120.0f の範囲で表示。サイズは横幅おまかせ、縦幅80ピクセル）
	ImGui::PlotLines("Performance", fps_values, 90, values_offset, overlay, 0.0f, 120.0f, ImVec2(0, 80));

	ImGui::End();
#endif

}

//スプライトデバック
int ImGuiManager::SpriteWindow(const std::vector<std::unique_ptr<Sprite>>& sprites, bool embedded, int forcedSpriteIndex)
{
#ifdef USE_IMGUI

	static float my_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static int selectedSpriteIndex = 0;

	
	if (!embedded && !showSpriteWindow_) {
		return -1;
	}
	if (!embedded && !ImGui::Begin("Editing UVTranslate ( Sprite )", &showSpriteWindow_)) {
		ImGui::End();
		return -1;
	}
	ImGui::Separator();

	// ここで色を変えたら、配列内の全スプライトに色を適用する
	if (forcedSpriteIndex >= 0 && forcedSpriteIndex < static_cast<int>(sprites.size())) {
		selectedSpriteIndex = forcedSpriteIndex;
		Sprite* targetSprite = sprites[forcedSpriteIndex].get();
		if (targetSprite) {
			ImGui::Text("Selected 2D Texture / Sprite %d", forcedSpriteIndex);
			if (ImGui::ColorEdit4("Color", my_color)) {
				targetSprite->SetColor({ my_color[0], my_color[1], my_color[2], my_color[3] });
			}
			Vector2 pos = targetSprite->GetPosition();
			if (ImGui::DragFloat2("Pos", &pos.x, 1.0f)) {
				targetSprite->SetPosition(pos);
			}
			float rot = targetSprite->GetRotation();
			if (ImGui::DragFloat("Rot", &rot, 0.01f)) {
				targetSprite->SetRotation(rot);
			}
			Vector2 size = targetSprite->GetSize();
			if (ImGui::DragFloat2("Size", &size.x, 1.0f)) {
				targetSprite->SetSize(size);
			}
			Vector2 anchor = targetSprite->GetAnchorPoint();
			if (ImGui::DragFloat2("Anchor", &anchor.x, 0.01f, 0.0f, 1.0f)) {
				targetSprite->SetAnchorPoint(anchor);
			}
			bool isFlipX = targetSprite->GetIsFlipX();
			if (ImGui::Checkbox("isFlipX", &isFlipX)) {
				targetSprite->SetIsFlipX(isFlipX);
			}
			bool isFlipY = targetSprite->GetIsFlipY();
			if (ImGui::Checkbox("isFlipY", &isFlipY)) {
				targetSprite->SetIsFlipY(isFlipY);
			}
			Vector2 texBase = targetSprite->GetTextureLeftTop();
			if (ImGui::DragFloat2("TexLeftTop", &texBase.x, 1.0f)) {
				targetSprite->SetTextureLeftTop(texBase);
			}
			Vector2 texSize = targetSprite->GetTextureSize();
			if (ImGui::DragFloat2("TexSize", &texSize.x, 1.0f)) {
				targetSprite->SetTextureSize(texSize);
			}
		}
		if (!embedded) {
			ImGui::End();
		}
		return forcedSpriteIndex;
	}

	if (sprites.empty()) {
		ImGui::TextDisabled("No Sprite / 2D Texture objects.");
		if (!embedded) {
			ImGui::End();
		}
		return -1;
	}
	if (selectedSpriteIndex < 0 || selectedSpriteIndex >= static_cast<int>(sprites.size())) {
		selectedSpriteIndex = 0;
	}
	std::string spritePreview = "Sprite " + std::to_string(selectedSpriteIndex);
	if (ImGui::BeginCombo("Target", spritePreview.c_str())) {
		for (int i = 0; i < static_cast<int>(sprites.size()); ++i) {
			std::string item = "Sprite " + std::to_string(i);
			if (ImGui::Selectable(item.c_str(), selectedSpriteIndex == i)) {
				selectedSpriteIndex = i;
			}
			if (selectedSpriteIndex == i) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::ColorEdit4("Color", my_color) && sprites[selectedSpriteIndex]) {
		sprites[selectedSpriteIndex]->SetColor({ my_color[0], my_color[1], my_color[2], my_color[3] });
	}

	ImGui::Separator();

	// std::vector の全要素に対して処理
	for (int i = 0; i < sprites.size(); ++i)
	{
		if (i != selectedSpriteIndex) {
			continue;
		}
		// IDをプッシュ（これが無いと全部のスプライトが同時に動いてしまう）
		ImGui::PushID(i);
		ImGui::Text("Sprite %d", i);
		//座標
		Vector2 pos = sprites[i]->GetPosition();
		if (ImGui::DragFloat2("Pos", &pos.x, 1.0f)) {
			sprites[i]->SetPosition(pos);
		}//回転
		float rot = sprites[i]->GetRotation();
		if (ImGui::DragFloat("Rot", &rot, 0.01f)) {
			sprites[i]->SetRotation(rot);
		}//サイズ
		Vector2 size = sprites[i]->GetSize();
		if (ImGui::DragFloat2("Size", &size.x, 1.0f)) {
			sprites[i]->SetSize(size);
		}//アンカーポイント
		Vector2 anchor = sprites[i]->GetAnchorPoint();
		// 0.0～1.0 の範囲で動かす
		if (ImGui::DragFloat2("Anchor", &anchor.x, 0.01f, 0.0f, 1.0f)) {
			sprites[i]->SetAnchorPoint(anchor);
		}//左右反転
		bool isFlipX = sprites[i]->GetIsFlipX();
		if (ImGui::Checkbox("isFlipX", &isFlipX)) {
			sprites[i]->SetIsFlipX(isFlipX);
		}
		//上下反転
		bool isFlipY = sprites[i]->GetIsFlipY();
		if (ImGui::Checkbox("isFlipY", &isFlipY)) {
			sprites[i]->SetIsFlipY(isFlipY);
		}//左上座標
		Vector2 texBase = sprites[i]->GetTextureLeftTop();
		if (ImGui::DragFloat2("TexLeftTop", &texBase.x, 1.0f)) {
			sprites[i]->SetTextureLeftTop(texBase);
		}//切り出しサイズ
		Vector2 texSize = sprites[i]->GetTextureSize();
		if (ImGui::DragFloat2("TexSize", &texSize.x, 1.0f)) {
			sprites[i]->SetTextureSize(texSize);
		}

		ImGui::Separator();
		ImGui::PopID();// IDをポップ
	}

	if (!embedded) {
		ImGui::End();
	}
	return selectedSpriteIndex;
#else
	(void)sprites;
	(void)embedded;
	(void)forcedSpriteIndex;
	return -1;
#endif
}

void ImGuiManager::ModelWindow(
	std::vector<std::unique_ptr<Object3d>>& normalObjects,
	std::vector<std::unique_ptr<Object3d>>& animationObjects,
	Object3d::DirectionalLight& light,
	Object3d::PointLight& pointLight,
	Object3d::SpotLight& spotLight,
	bool embedded,
	size_t protectedNormalObjectCount,
	size_t protectedAnimationObjectCount,
	int forcedNormalObjectIndex,
	int forcedAnimationObjectIndex,
	const std::function<void(bool animationObject, size_t index)>& onObjectRemoved)
{
#ifdef USE_IMGUI

	static int selectedNormalIndex = 0;// 0:アニメーションなし 1:アニメーションあり
	static int selectedAnimationIndex = 0;//選択されている番号
	static int previousNormalCount = 0;
	static int previousAnimationCount = 0;
	static bool useMonsterBall = false;//png入れ替え
	if (!embedded && !showModelWindow_) {
		return;
	}
	if (!embedded && !ImGui::Begin("Editing Object", &showModelWindow_)) {
		ImGui::End();
		return;
	}

	auto syncSelectionForChangedList = [](int objectCount, size_t protectedObjectCount, int& selectedIndex, int& previousCount) {
		const int protectedCount = static_cast<int>(protectedObjectCount);
		if (objectCount <= 0) {
			selectedIndex = 0;
			previousCount = 0;
			return;
		}
		if (objectCount != previousCount) {
			const bool hasAddedObjects = objectCount > protectedCount;
			if (objectCount > previousCount || selectedIndex >= objectCount || (hasAddedObjects && selectedIndex < protectedCount)) {
				selectedIndex = hasAddedObjects ? objectCount - 1 : 0;
			}
			previousCount = objectCount;
		}
	};

	syncSelectionForChangedList(static_cast<int>(normalObjects.size()), protectedNormalObjectCount, selectedNormalIndex, previousNormalCount);
	syncSelectionForChangedList(static_cast<int>(animationObjects.size()), protectedAnimationObjectCount, selectedAnimationIndex, previousAnimationCount);
	const bool forceNormalSelection =
		forcedNormalObjectIndex >= 0 && forcedNormalObjectIndex < static_cast<int>(normalObjects.size());
	const bool forceAnimationSelection =
		forcedAnimationObjectIndex >= 0 && forcedAnimationObjectIndex < static_cast<int>(animationObjects.size());
	if (forceNormalSelection) {
		selectedNormalIndex = forcedNormalObjectIndex;
	}
	if (forceAnimationSelection) {
		selectedAnimationIndex = forcedAnimationObjectIndex;
	}

	auto drawObjectEditor = [this, &onObjectRemoved](const char* label, std::vector<std::unique_ptr<Object3d>>& objects, int& selectedIndex, size_t protectedObjectCount, bool animationObject) {
		if (objects.empty()) {
			ImGui::TextDisabled("No %s objects.", label);
			return;
		}
		if (selectedIndex < 0 || selectedIndex >= static_cast<int>(objects.size())) {
			selectedIndex = 0;
		}

		auto makeObjectLabel = [label](const Object3d* object, int index) {
			const std::string modelName = object ? object->GetModelName() : std::string();
			if (!modelName.empty()) {
				return std::string(label) + " " + std::to_string(index) + " : " + modelName;
			}
			return std::string(label) + " " + std::to_string(index);
		};

		std::string preview = makeObjectLabel(objects[selectedIndex].get(), selectedIndex);
		if (ImGui::BeginCombo("Target", preview.c_str())) {
			for (int index = 0; index < static_cast<int>(objects.size()); ++index) {
				std::string item = makeObjectLabel(objects[index].get(), index);
				if (ImGui::Selectable(item.c_str(), selectedIndex == index)) {
					selectedIndex = index;
				}
			}
			ImGui::EndCombo();
		}

		Object3d* targetObject = objects[selectedIndex].get();
		ImGui::PushID(targetObject);
		if (!targetObject->GetModelName().empty()) {
			ImGui::Text("Model File: %s", targetObject->GetModelName().c_str());
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Time Playback");
		if (targetObject->IsAnimating()) {
			bool animationReturning = targetObject->IsAnimationReturning();
			if (ImGui::Checkbox("return##Animation", &animationReturning)) {
				targetObject->SetAnimationReturning(animationReturning);
			}
			ImGui::SameLine();
			ImGui::TextDisabled(
				"Animation %.2f / %.2f sec",
				targetObject->GetAnimationTime(),
				targetObject->GetAnimationDuration());
		}

		bool transformReturning = targetObject->IsTransformReturning();
		ImGui::BeginDisabled(!targetObject->HasTransformHistory());
		if (ImGui::Checkbox("return##Transform", &transformReturning)) {
			targetObject->SetTransformReturning(transformReturning);
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!targetObject->CanMoveTransformForward());
		if (ImGui::Button("Move")) {
			targetObject->MoveTransformForward();
		}
		ImGui::EndDisabled();
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (targetObject->IsTransformReturning()) {
			ImGui::TextDisabled("Transform returning %.0f%%", targetObject->GetTransformPlaybackProgress() * 100.0f);
		} else if (targetObject->IsTransformMovingForward()) {
			ImGui::TextDisabled("Transform moving %.0f%%", targetObject->GetTransformPlaybackProgress() * 100.0f);
		} else if (!targetObject->HasTransformHistory()) {
			ImGui::TextDisabled("Edit a transform first");
		}
		if (targetObject->HasTransformHistory()) {
			ImGui::TextDisabled(
				"Transform timeline %.2f / %.2f sec (1x)",
				targetObject->GetTransformPlaybackTime(),
				targetObject->GetTransformPlaybackDuration());
		}

		Transform& transform = targetObject->GetTransform();
		const Transform transformBeforeEdit = transform;
		bool transformEdited = false;
		const bool transformPlaybackActive =
			targetObject->IsTransformReturning() || targetObject->IsTransformMovingForward();
		ImGui::BeginDisabled(transformPlaybackActive);
		transformEdited |= ImGui::DragFloat3("Translate", &transform.translate.x, 0.01f);
		transformEdited |= ImGui::DragFloat3("Rotate", &transform.rotate.x, 0.01f);
		transformEdited |= ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);

		ImGui::Separator();
		ImGui::TextDisabled("Quick Adjust");
		if (ImGui::Button("Reset Transform")) {
			transform.translate = { 0.0f, 0.0f, 0.0f };
			transform.rotate = { 0.0f, 0.0f, 0.0f };
			transform.scale = { 1.0f, 1.0f, 1.0f };
			transformEdited = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Rotation")) {
			transform.rotate = { 0.0f, 0.0f, 0.0f };
			transformEdited = true;
		}

		float uniformScale = (transform.scale.x + transform.scale.y + transform.scale.z) / 3.0f;
		if (ImGui::DragFloat("Uniform Scale", &uniformScale, 0.01f, 0.001f, 100.0f)) {
			transform.scale = { uniformScale, uniformScale, uniformScale };
			transformEdited = true;
		}
		ImGui::EndDisabled();
		if (transformEdited) {
			targetObject->RecordTransformEdit(transformBeforeEdit);
		}

		float environmentCoefficient = targetObject->GetEnvironmentCoefficient();
		if (ImGui::SliderFloat("Environment Reflection", &environmentCoefficient, 0.0f, 1.0f)) {
			targetObject->SetEnvironmentCoefficient(environmentCoefficient);
		}

		ImGui::Separator();
		const bool canRemoveObject = static_cast<size_t>(selectedIndex) >= protectedObjectCount;
		ImGui::BeginDisabled(!canRemoveObject);
		const bool removeSelected = ImGui::Button("Remove Added Model") ||
			(canRemoveObject && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
				!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false));
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip(canRemoveObject
				? "Remove this model from the scene. Delete key also works while Inspector is focused."
				: "Initial scene models are protected. Only models added from Model Shelf can be removed here.");
		}
		if (removeSelected && canRemoveObject) {
			const size_t removedIndex = static_cast<size_t>(selectedIndex);
			if (dxCommon_) {
				dxCommon_->WaitForGPU();
			}
			objects.erase(objects.begin() + selectedIndex);
			if (onObjectRemoved) {
				onObjectRemoved(animationObject, removedIndex);
			}
			if (selectedIndex >= static_cast<int>(objects.size())) {
				selectedIndex = static_cast<int>(objects.size()) - 1;
			}
			if (selectedIndex < 0) {
				selectedIndex = 0;
			}
		}
		ImGui::PopID();
	};

	if (ImGui::BeginTabBar("ModelInspectorTabs")) {
		const ImGuiTabItemFlags modelTabFlags = forceNormalSelection ? ImGuiTabItemFlags_SetSelected : 0;
		const ImGuiTabItemFlags animationTabFlags = forceAnimationSelection ? ImGuiTabItemFlags_SetSelected : 0;
		if (ImGui::BeginTabItem("Model", nullptr, modelTabFlags)) {
			drawObjectEditor("Model", normalObjects, selectedNormalIndex, protectedNormalObjectCount, false);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Animation Model", nullptr, animationTabFlags)) {
			ImGui::Checkbox("Skeleton Debug", &showSkeletonDebugDraw_);
			drawObjectEditor("Animation", animationObjects, selectedAnimationIndex, protectedAnimationObjectCount, true);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Light")) {
			ImGui::Text("Directional Light");
			ImGui::DragFloat3("DirectoinalLight:direction", &light.direction.x, 0.01f);
			ImGui::DragFloat("DirectoinalLight:intensity", &light.intensity, 0.01f);
			ImGui::DragFloat3("DirectoinalLight:color", &light.color.x, 0.01f);

			ImGui::Separator();

			ImGui::Text("Point Light");
			ImGui::DragFloat3("PointLight:position", &pointLight.position.x, 0.01f);
			ImGui::DragFloat("PointLight:intensity", &pointLight.intensity, 0.01f, 0.0f, 10.0f);
			ImGui::ColorEdit3("PointLight:color", &pointLight.color.x);
			ImGui::DragFloat("PointLight:radius", &pointLight.radius, 0.1f);
			ImGui::DragFloat("PointLight:decay", &pointLight.decay, 0.1f, 10.0f);

			ImGui::Separator();

			ImGui::Text("Spot Light");
			ImGui::DragFloat3("SpotLight:position", &spotLight.position.x, 0.01f);
			ImGui::DragFloat3("SpotLight:direction", &spotLight.direction.x, 0.01f);
			ImGui::DragFloat("SpotLight:intensity", &spotLight.intensity, 0.01f, 0.0f, 20.0f);
			ImGui::ColorEdit3("SpotLight:color", &spotLight.color.x);
			ImGui::SliderFloat("SpotLight:distance", &spotLight.distance, 0.0f, 100.0f);
			ImGui::SliderFloat("SpotLight:decay", &spotLight.decay, 0.1f, 10.0f);
			ImGui::SliderFloat("SpotLight:cosAngle", &spotLight.cosAngle, -1.0f, 1.0f);
			ImGui::SliderFloat("SpotLight:cosFalloffStart", &spotLight.cosFalloffStart, -1.0f, 1.0f);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Texture")) {
			if (ImGui::Checkbox("Use MonsterBall", &useMonsterBall))
			{//切り替えたいモデル
				Model* targetModel = ModelManager::GetInstance()->FindModel("sphere.obj");
				if (targetModel)
				{
					if (useMonsterBall) {
						targetModel->SetTexture("Resources/monsterBall.png");
					} else {
						targetModel->SetTexture("Resources/uvChecker.png");
					}
				}
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	
	if (!embedded) {
		ImGui::End();
	}
#endif
}

std::string ImGuiManager::ParticleWindow(Transform& emitterTransform, bool embedded)
{
#ifdef USE_IMGUI
	std::string result = ""; // 何も押されていなければ空文字を返す

	if (!embedded && !showParticleWindow_) {
		return "";
	}
	if (!embedded && !ImGui::Begin("Editing Particle", &showParticleWindow_)) {
		ImGui::End();
		return result;
	}
	ImGui::Separator();

	if (ImGui::Button("Circle Texture"))
	{
		result = "Circle"; // Circleが押されたと報告
	}
	ImGui::SameLine();

	if (ImGui::Button("Plane Texture"))
	{
		result = "Plane"; // Planeが押されたと報告
	}

	ImGui::Separator();
	ImGui::Text("Emitter Transform");
	ImGui::DragFloat3("Emitter Translate", &emitterTransform.translate.x, 0.01f);
	ImGui::DragFloat3("Emitter Rotate", &emitterTransform.rotate.x, 0.01f);
	ImGui::DragFloat3("Emitter Scale", &emitterTransform.scale.x, 0.01f, 0.01f, 10.0f);

	ImGui::Separator();
	ParticleManager* particleManager = ParticleManager::GetInstance();
	bool particlesReturning = particleManager->IsReturning();
	if (ImGui::Checkbox("return##Particles", &particlesReturning)) {
		particleManager->SetReturning(particlesReturning);
	}
	ImGui::SameLine();
	ImGui::TextDisabled(particlesReturning ? "Particles reverse continuously" : "Particles play forward");

	bool autoWindSwitch = particleManager->IsAutoWindSwitchEnabled();
	if (ImGui::Checkbox("Auto Wind Every 5s", &autoWindSwitch)) {
		particleManager->SetAutoWindSwitchEnabled(autoWindSwitch);
	}
	bool windEnabled = particleManager->IsWindEnabled();
	if (ImGui::Checkbox("Wind Now", &windEnabled)) {
		particleManager->SetWindEnabled(windEnabled);
	}
	ImGui::SameLine();
	if (ImGui::Button("Switch Wind On/Off")) {
		particleManager->ToggleWind();
	}
	Vector3 windAcceleration = particleManager->GetWindAcceleration();
	if (ImGui::DragFloat3("Wind Strength", &windAcceleration.x, 0.01f)) {
		particleManager->SetWindAcceleration(windAcceleration);
	}

	if (!embedded) {
		ImGui::End();
	}
	return result;
#else
	return "";
#endif
}

void ImGuiManager::CameraWindow(CameraManager* cameraManager, bool embedded)
{
#ifdef USE_IMGUI

	if (!embedded && !showCameraWindow_) {
		return;
	}
	if (!embedded && !ImGui::Begin("Camera Control", &showCameraWindow_)) {
		ImGui::End();
		return;
	}
	ImGui::Separator();

	//カメラ
	Camera* activeCamera = cameraManager->GetActiveCamera();
	if (activeCamera)
	{
		//位置
		Vector3 cameraPos = activeCamera->GetTranslate();
		if (ImGui::DragFloat3("CameraTranslate", &cameraPos.x, 0.01f))
		{
			activeCamera->SetTranslate(cameraPos);
		}
		//角度
		Vector3 cameraRot = activeCamera->GetRotate();
		if (ImGui::DragFloat3("CameraRotate", &cameraRot.x, 0.01f))
		{
			activeCamera->SetRotate(cameraRot);
		}
	}

	ImGui::Separator();

	if (ImGui::Button("Use MainCamera"))
	{//メインカメラ
		cameraManager->SetActiveCamera("MainCamera");
	}
	ImGui::SameLine();
	if (ImGui::Button("Use UpCamera"))
	{//上空カメラ
		cameraManager->SetActiveCamera("UpCamera");
	}

	if (!embedded) {
		ImGui::End();
	}
#endif
}

void ImGuiManager::PostEffectWindow()
{
#ifdef USE_IMGUI
	if (!showPostEffectWindow_) {
		return;
	}
	if (!ImGui::Begin("Post Effect", &showPostEffectWindow_)) {
		ImGui::End();
		return;
	}
	bool isGrayscale = PostEffect::GetInstance()->IsGrayscale();
	bool isSepia = PostEffect::GetInstance()->IsSepia();
	if (ImGui::Checkbox("Grayscale", &isGrayscale) && isGrayscale) {
		isSepia = false;
	}
	if (ImGui::Checkbox("Sepia", &isSepia) && isSepia) {
		isGrayscale = false;
	}
	PostEffect::GetInstance()->SetGrayscale(isGrayscale);
	PostEffect::GetInstance()->SetSepia(isSepia);
	ImGui::End();
#else
#endif
}

//アニメーションデバック
bool ImGuiManager::IsSkeletonDebugDrawEnabled() const
{
#ifdef USE_IMGUI
	return showSkeletonDebugDraw_;
#else
	return false;
#endif
}

void ImGuiManager::SkeletonDebugDraw(const Model::Skeleton& skeleton, const Matrix4x4& worldMatrix, const Matrix4x4& viewProjectionMatrix)
{
#ifdef USE_IMGUI
	if (!showSkeletonDebugDraw_) {
		return;
	}
	ImDrawList* drawList = gameViewDrawList_;
	ImVec2 imageMin = gameViewImageMin_;
	ImVec2 imageSize = gameViewImageSize_;
	if (drawList == nullptr || imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
		return;
	}

	const Matrix4x4 matWVP = Multiply(worldMatrix, viewProjectionMatrix);
	const auto projectToGameView = [&](const Vector3& position, ImVec2& screenPosition) {
		const float x = position.x * matWVP.m[0][0] + position.y * matWVP.m[1][0] + position.z * matWVP.m[2][0] + matWVP.m[3][0];
		const float y = position.x * matWVP.m[0][1] + position.y * matWVP.m[1][1] + position.z * matWVP.m[2][1] + matWVP.m[3][1];
		const float w = position.x * matWVP.m[0][3] + position.y * matWVP.m[1][3] + position.z * matWVP.m[2][3] + matWVP.m[3][3];
		if (w <= 0.0f) {
			return false;
		}
		screenPosition = ImVec2(
			imageMin.x + (x / w + 1.0f) * 0.5f * imageSize.x,
			imageMin.y + (1.0f - y / w) * 0.5f * imageSize.y);
		return true;
	};

	drawList->PushClipRect(imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), true);
	for (const auto& joint : skeleton.joints) {
		if (!joint.parent.has_value()) {
			continue;
		}
		const auto& parentJoint = skeleton.joints[joint.parent.value()];
		const Vector3 jointPosition{
			joint.skeletonSpaceMatrix.m[3][0], joint.skeletonSpaceMatrix.m[3][1], joint.skeletonSpaceMatrix.m[3][2] };
		const Vector3 parentPosition{
			parentJoint.skeletonSpaceMatrix.m[3][0], parentJoint.skeletonSpaceMatrix.m[3][1], parentJoint.skeletonSpaceMatrix.m[3][2] };
		ImVec2 jointScreenPosition{};
		ImVec2 parentScreenPosition{};
		if (projectToGameView(jointPosition, jointScreenPosition) &&
			projectToGameView(parentPosition, parentScreenPosition)) {
			drawList->AddLine(parentScreenPosition, jointScreenPosition, IM_COL32(255, 255, 0, 255), 2.0f);
		}
	}

	for (const auto& joint : skeleton.joints) {
		const Vector3 jointPosition{
			joint.skeletonSpaceMatrix.m[3][0], joint.skeletonSpaceMatrix.m[3][1], joint.skeletonSpaceMatrix.m[3][2] };
		ImVec2 jointScreenPosition{};
		if (projectToGameView(jointPosition, jointScreenPosition)) {
			drawList->AddCircleFilled(jointScreenPosition, 5.0f, IM_COL32(255, 0, 0, 255));
		}
	}
	drawList->PopClipRect();
#endif
}

void ImGuiManager::Draw(DirectXCommon* dxCommon)
{
#ifdef USE_IMGUI

	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

	//デスクリプタヒープの配列をセットするコマンド
	SrvManager* srvManager = SrvManager::GetInstance();
	ID3D12DescriptorHeap* ppHeaps[] = { srvManager->GetDescriptorHeap() };

	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	//描画コマンドを発行
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}

void ImGuiManager::Finalize()
{
#ifdef USE_IMGUI

	//ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}
