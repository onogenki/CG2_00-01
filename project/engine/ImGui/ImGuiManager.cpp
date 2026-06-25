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
#ifdef USE_IMGUI
#include "externals/imgui/imgui_internal.h"
#endif
using namespace MyMath;

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]]DirectXCommon* directXCommon, [[maybe_unused]]SrvManager* srvManager)
{
#ifdef USE_IMGUI

	//ImGui縺ｮ繧ｳ繝ｳ繝・く繧ｹ繝医ｒ逕滓・
	ImGui::CreateContext();
	winApp_ = winApp;
	dxCommon_ = directXCommon;
	srvManager_ = srvManager;
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	//ImGui縺ｮ繧ｹ繧ｿ繧､繝ｫ繧定ｨｭ螳・
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 3.0f;
	style.FrameRounding = 3.0f;
	style.TabRounding = 3.0f;

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	assert(srvManager->GetDescriptorHeap() != nullptr && "SRV Descriptor Heap is null!");

	//DirectX12逕ｨ縺ｮ蛻晄悄蛹匁ュ蝣ｱ
	ImGui_ImplDX12_InitInfo initInfo = {};

	//蛻晄悄蛹匁ュ蝣ｱ繧定ｨｭ螳壹☆繧・
	initInfo.Device = directXCommon->GetDevice();
	initInfo.CommandQueue = directXCommon->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<int>(directXCommon->GetSwapChainResourcesNum());
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	initInfo.SrvDescriptorHeap = srvManager->GetDescriptorHeap();

	//SRV遒ｺ菫晉畑髢｢謨ｰ縺ｮ險ｭ螳・繝ｩ繝繝蠑・
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
		{
			SrvManager* srvManager = SrvManager::GetInstance();
			uint32_t index = srvManager->Allocate();
			*out_cpu_handle = srvManager->GetCPUDescriptorHandle(index);
			*out_gpu_handle = srvManager->GetGPUDescriptorHandle(index);
		};

	//SRV隗｣謾ｾ逕ｨ髢｢謨ｰ縺ｮ險ｭ螳・
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
		{
			//SrvManager縺ｫ隗｣謾ｾ讖溯・繧剃ｽ懊▲縺ｦ縺・↑縺・◆繧√％縺薙〒縺ｯ菴輔ｂ縺励↑縺・
		};

	//DirectX12逕ｨ縺ｮ蛻晄悄蛹悶ｒ陦後≧
	ImGui_ImplDX12_Init(&initInfo);

#endif

}

void ImGuiManager::Begin(const char* sceneName)
{
#ifdef USE_IMGUI
	//繧ｲ繝ｼ繝縺ｮ蜃ｦ逅・
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
	//ImGui縺ｮ蜀・Κ繧ｳ繝槭Φ繝峨ｒ逕滓・縺吶ｋ
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
		// 1繝輔Ξ繝ｼ繝逶ｮ縺ｧ繧ｦ繧｣繝ｳ繝峨え繧堤匳骭ｲ縺励・繝輔Ξ繝ｼ繝逶ｮ縺ｧ驟咲ｽｮ繧堤｢ｺ螳壹☆繧九・
		layoutSceneName_ = sceneName;
		layoutResetFrames_ = 2;
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
			ImGui::MenuItem("Performance", nullptr, &showFpsWindow_);
			ImGui::MenuItem("Post Effect", nullptr, &showPostEffectWindow_);
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

	const ImGuiID dockspaceId = ImGui::GetID("MainDockspaceV3");
	if (resetDockLayout_ || ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
		BuildDefaultDockLayout(dockspaceId);
		resetDockLayout_ = false;
	}
	ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	if (showGameView_) {
		GameViewWindow();
	}
	if (showSceneWindow_) {
		SceneWindow(sceneName);
	}
	if (showFpsWindow_) {
		FPSWindow();
	}
	if (showPostEffectWindow_) {
		PostEffectWindow();
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
	const ImGuiID leftId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.18f, nullptr, &centerId);
	const ImGuiID rightId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.24f, nullptr, &centerId);

	ImGui::DockBuilderDockWindow("Game View", centerId);
	ImGui::DockBuilderDockWindow("Scene", leftId);
	ImGui::DockBuilderDockWindow("FPS", leftId);
	ImGui::DockBuilderDockWindow("Post Effect", leftId);
	ImGui::DockBuilderDockWindow("Inspector", rightId);
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
			// 譏蜒丞・菴薙′蠢・★蜿弱∪繧句咲紫繧帝∈縺ｳ縲∽ｽ吶▲縺滄Κ蛻・ｒ繝ｬ繧ｿ繝ｼ繝懊ャ繧ｯ繧ｹ縺ｫ縺吶ｋ縲・
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

	// Factory縺ｸ逋ｻ骭ｲ縺励◆繧ｷ繝ｼ繝ｳ繧剃ｸ隕ｧ陦ｨ遉ｺ縺励・∈謚樊凾縺ｫ蛻・ｊ譖ｿ縺医ｒ莠育ｴ・☆繧九・
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
		ImGui::TextDisabled("Loading: %s", sceneManager->GetPendingSceneName().c_str());
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
	// 繧ｦ繧｣繝ｳ繝峨え菴懈・
	if (!ImGui::Begin("FPS", &showFpsWindow_)) {
		ImGui::End();
		return;
	}

	// FPS豕｢蠖｢繧ｰ繝ｩ繝輔・謠冗判
	static float fps_values[90] = {};
	static int values_offset = 0;

	// ImGui縺ｮ萓ｿ蛻ｩ讖溯・縺ｧ縲∫樟蝨ｨ縺ｮFPS・・遘帝俣縺ｮ繧ｳ繝樊焚・峨ｒ蜿門ｾ励＠縺ｦ驟榊・縺ｫ菫晏ｭ・
	fps_values[values_offset] = ImGui::GetIO().Framerate;

	// 90蛟九ョ繝ｼ繧ｿ繧貞・繧後◆繧峨∪縺・逡ｪ逶ｮ縺九ｉ蜿､縺・ョ繝ｼ繧ｿ繧剃ｸ頑嶌縺阪＠縺ｦ縺・￥(驟榊・縺ｫ繝・・繧ｿ繧定ｿｽ蜉縺礼ｶ壹￠繧九→繝代Φ繧ｯ縺吶ｋ縺溘ａ・・
	values_offset = (values_offset + 1) % 90;

	// 繧ｰ繝ｩ繝輔・荳翫↓陦ｨ遉ｺ縺吶ｋ繝・く繧ｹ繝茨ｼ育樟蝨ｨ縺ｮFPS・峨ｒ菴懈・
	char overlay[32];
	snprintf(overlay, sizeof(overlay), "FPS: %.1f", ImGui::GetIO().Framerate);//FPS縺ｮ險育ｮ・

	// 繧ｰ繝ｩ繝墓緒逕ｻ・・.0f縲・20.0f 縺ｮ遽・峇縺ｧ陦ｨ遉ｺ縲ゅし繧､繧ｺ縺ｯ讓ｪ蟷・♀縺ｾ縺九○縲∫ｸｦ蟷・0繝斐け繧ｻ繝ｫ・・
	ImGui::PlotLines("Performance", fps_values, 90, values_offset, overlay, 0.0f, 120.0f, ImVec2(0, 80));

	ImGui::End();
#endif

}

//繧ｹ繝励Λ繧､繝医ョ繝舌ャ繧ｯ
void ImGuiManager::SpriteWindow(const std::vector<std::unique_ptr<Sprite>>& sprites, bool embedded)
{
#ifdef USE_IMGUI

	static float my_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // 繧ｫ繝ｩ繝ｼ繝斐ャ繧ｫ繝ｼ縺ｮ濶ｲ

	// 繧ｦ繧｣繝ｳ繝峨え菴懈・
	if (!embedded && !showSpriteWindow_) {
		return;
	}
	if (!embedded && !ImGui::Begin("Editing UVTranslate ( Sprite )", &showSpriteWindow_)) {
		ImGui::End();
		return;
	}
	ImGui::Separator();

	// 縺薙％縺ｧ濶ｲ繧貞､峨∴縺溘ｉ縲・・蛻怜・縺ｮ蜈ｨ繧ｹ繝励Λ繧､繝医↓濶ｲ繧帝←逕ｨ縺吶ｋ
	if (ImGui::ColorEdit4("Color", my_color)) {
		for (auto& sprite : sprites) {
			sprite->SetColor({ my_color[0], my_color[1], my_color[2], my_color[3] });
		}
	}

	ImGui::Separator();

	// std::vector 縺ｮ蜈ｨ隕∫ｴ縺ｫ蟇ｾ縺励※蜃ｦ逅・
	for (int i = 0; i < sprites.size(); ++i)
	{
		// ID繧偵・繝・す繝･・医％繧後′辟｡縺・→蜈ｨ驛ｨ縺ｮ繧ｹ繝励Λ繧､繝医′蜷梧凾縺ｫ蜍輔＞縺ｦ縺励∪縺・ｼ・
		ImGui::PushID(i);
		ImGui::Text("Sprite %d", i); // 逡ｪ蜿ｷ陦ｨ遉ｺ
		//蠎ｧ讓・
		Vector2 pos = sprites[i]->GetPosition();
		if (ImGui::DragFloat2("Pos", &pos.x, 1.0f)) {
			sprites[i]->SetPosition(pos);
		}//蝗櫁ｻ｢
		float rot = sprites[i]->GetRotation();
		if (ImGui::DragFloat("Rot", &rot, 0.01f)) {
			sprites[i]->SetRotation(rot);
		}//繧ｵ繧､繧ｺ
		Vector2 size = sprites[i]->GetSize();
		if (ImGui::DragFloat2("Size", &size.x, 1.0f)) {
			sprites[i]->SetSize(size);
		}//繧｢繝ｳ繧ｫ繝ｼ繝昴う繝ｳ繝・
		Vector2 anchor = sprites[i]->GetAnchorPoint();
		// 0.0・・.0 縺ｮ遽・峇縺ｧ蜍輔°縺・
		if (ImGui::DragFloat2("Anchor", &anchor.x, 0.01f, 0.0f, 1.0f)) {
			sprites[i]->SetAnchorPoint(anchor);
		}//蟾ｦ蜿ｳ蜿崎ｻ｢
		bool isFlipX = sprites[i]->GetIsFlipX();
		if (ImGui::Checkbox("isFlipX", &isFlipX)) {
			sprites[i]->SetIsFlipX(isFlipX);
		}
		//荳贋ｸ句渚霆｢
		bool isFlipY = sprites[i]->GetIsFlipY();
		if (ImGui::Checkbox("isFlipY", &isFlipY)) {
			sprites[i]->SetIsFlipY(isFlipY);
		}//蟾ｦ荳雁ｺｧ讓・
		Vector2 texBase = sprites[i]->GetTextureLeftTop();
		if (ImGui::DragFloat2("TexLeftTop", &texBase.x, 1.0f)) {
			sprites[i]->SetTextureLeftTop(texBase);
		}//蛻・ｊ蜃ｺ縺励し繧､繧ｺ
		Vector2 texSize = sprites[i]->GetTextureSize();
		if (ImGui::DragFloat2("TexSize", &texSize.x, 1.0f)) {
			sprites[i]->SetTextureSize(texSize);
		}

		ImGui::Separator();
		ImGui::PopID(); // ID繧偵・繝・・
	}

	if (!embedded) {
		ImGui::End();
	}
#endif
}

void ImGuiManager::ModelWindow(const std::vector<std::unique_ptr<Object3d>>& normalObjects, const std::vector<std::unique_ptr<Object3d>>& animationObjects, Object3d::DirectionalLight& light,Object3d::PointLight& pointLight,Object3d::SpotLight& spotLight, bool embedded)
{
#ifdef USE_IMGUI

	static int selectedNormalIndex = 0;
	static int selectedAnimationIndex = 0;
	static bool useMonsterBall = false;//png蜈･繧梧崛縺・
	if (!embedded && !showModelWindow_) {
		return;
	}
	if (!embedded && !ImGui::Begin("Editing Object", &showModelWindow_)) {
		ImGui::End();
		return;
	}

	auto drawObjectEditor = [](const char* label, const std::vector<std::unique_ptr<Object3d>>& objects, int& selectedIndex) {
		if (objects.empty()) {
			ImGui::TextDisabled("No %s objects.", label);
			return;
		}
		if (selectedIndex >= static_cast<int>(objects.size())) {
			selectedIndex = 0;
		}

		std::string preview = std::string(label) + " " + std::to_string(selectedIndex);
		if (ImGui::BeginCombo("Target", preview.c_str())) {
			for (int index = 0; index < static_cast<int>(objects.size()); ++index) {
				std::string item = std::string(label) + " " + std::to_string(index);
				if (ImGui::Selectable(item.c_str(), selectedIndex == index)) {
					selectedIndex = index;
				}
			}
			ImGui::EndCombo();
		}

		Object3d* targetObject = objects[selectedIndex].get();
		ImGui::PushID(targetObject);

		Transform& transform = targetObject->GetTransform();
		ImGui::DragFloat3("Translate", &transform.translate.x, 0.01f);
		ImGui::DragFloat3("Rotate", &transform.rotate.x, 0.01f);
		ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);

		float environmentCoefficient = targetObject->GetEnvironmentCoefficient();
		if (ImGui::SliderFloat("Environment Reflection", &environmentCoefficient, 0.0f, 1.0f)) {
			targetObject->SetEnvironmentCoefficient(environmentCoefficient);
		}
		ImGui::PopID();
	};

	if (ImGui::BeginTabBar("ModelInspectorTabs")) {
		if (ImGui::BeginTabItem("Model")) {
			drawObjectEditor("Model", normalObjects, selectedNormalIndex);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Animation Model")) {
			ImGui::Checkbox("Skeleton Debug", &showSkeletonDebugDraw_);
			drawObjectEditor("Animation", animationObjects, selectedAnimationIndex);
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
			{
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
	std::string result = ""; // 菴輔ｂ謚ｼ縺輔ｌ縺ｦ縺・↑縺代ｌ縺ｰ遨ｺ譁・ｭ励ｒ霑斐☆
	// 繧ｦ繧｣繝ｳ繝峨え菴懈・
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
		result = "Circle"; // Circle縺梧款縺輔ｌ縺溘→蝣ｱ蜻・
	}
	ImGui::SameLine();

	if (ImGui::Button("Plane Texture"))
	{
		result = "Plane"; // Plane縺梧款縺輔ｌ縺溘→蝣ｱ蜻・
	}

	ImGui::Separator();
	ImGui::Text("Emitter Transform");
	ImGui::DragFloat3("Emitter Translate", &emitterTransform.translate.x, 0.01f);
	ImGui::DragFloat3("Emitter Rotate", &emitterTransform.rotate.x, 0.01f);
	ImGui::DragFloat3("Emitter Scale", &emitterTransform.scale.x, 0.01f, 0.01f, 10.0f);

	ImGui::Separator();
	ParticleManager* particleManager = ParticleManager::GetInstance();
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

	// 繧ｦ繧｣繝ｳ繝峨え菴懈・
	if (!embedded && !showCameraWindow_) {
		return;
	}
	if (!embedded && !ImGui::Begin("Camera Control", &showCameraWindow_)) {
		ImGui::End();
		return;
	}
	ImGui::Separator();

	//繧ｫ繝｡繝ｩ
	Camera* activeCamera = cameraManager->GetActiveCamera();
	if (activeCamera)
	{//菴咲ｽｮ
		Vector3 cameraPos = activeCamera->GetTranslate();
		if (ImGui::DragFloat3("CameraTranslate", &cameraPos.x, 0.01f))
		{
			activeCamera->SetTranslate(cameraPos);
		}//隗貞ｺｦ
		Vector3 cameraRot = activeCamera->GetRotate();
		if (ImGui::DragFloat3("CameraRotate", &cameraRot.x, 0.01f))
		{
			activeCamera->SetRotate(cameraRot);
		}
	}

	ImGui::Separator();

	if (ImGui::Button("Use MainCamera"))
	{//繝｡繧､繝ｳ繧ｫ繝｡繝ｩ
		cameraManager->SetActiveCamera("MainCamera");
	}
	ImGui::SameLine();
	if (ImGui::Button("Use UpCamera"))
	{//荳顔ｩｺ繧ｫ繝｡繝ｩ
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

// 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ縺ｮ繧ｹ繧ｱ繝ｫ繝医Φ繧偵ョ繝舌ャ繧ｰ陦ｨ遉ｺ縺吶ｋ縲・
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

	// Game View縺ｮ螟門・縺ｸ繝・ヰ繝・げ邱壹′縺ｯ縺ｿ蜃ｺ縺輔↑縺・ｈ縺・↓縺吶ｋ縲・
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

	//繝・せ繧ｯ繝ｪ繝励ち繝偵・繝励・驟榊・繧偵そ繝・ヨ縺吶ｋ繧ｳ繝槭Φ繝・
	SrvManager* srvManager = SrvManager::GetInstance();
	ID3D12DescriptorHeap* ppHeaps[] = { srvManager->GetDescriptorHeap() };

	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	//謠冗判繧ｳ繝槭Φ繝峨ｒ逋ｺ陦・
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}

void ImGuiManager::Finalize()
{
#ifdef USE_IMGUI

	//ImGui縺ｮ邨ゆｺ・・逅・
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}
