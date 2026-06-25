#include "ImGuiManager.h"
#include "SrvManager.h"
#include "Sprite.h"
#include "ImGuiManager.h"
#include "Object3d.h"
#include "ParticleEmitter.h"
#include "CameraManager.h"
#include "ModelManager.h"
#include "SceneManager.h"
#include "PostEffect.h"
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
			*out_cpu_handle = srvManager->GetCPUDescriptorHandle(index);
			*out_gpu_handle = srvManager->GetGPUDescriptorHandle(index);
		};

	//SRV解放用関数の設定
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
		{
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
		// 1フレーム目でウィンドウを登録し、2フレーム目で配置を確定する。
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
			ImGui::MenuItem("Sprite Editor", nullptr, &showSpriteWindow_);
			ImGui::MenuItem("Model Editor", nullptr, &showModelWindow_);
			ImGui::MenuItem("Particle Editor", nullptr, &showParticleWindow_);
			ImGui::MenuItem("Camera Control", nullptr, &showCameraWindow_);
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
		ImGui::EndMenuBar();
	}

	const ImGuiID dockspaceId = ImGui::GetID("MainDockspaceV2");
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
	const ImGuiID bottomId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.22f, nullptr, &centerId);

	ImGui::DockBuilderDockWindow("Game View", centerId);
	ImGui::DockBuilderDockWindow("Scene", leftId);
	ImGui::DockBuilderDockWindow("FPS", leftId);
	ImGui::DockBuilderDockWindow("Post Effect", leftId);
	ImGui::DockBuilderDockWindow("Editing Object", rightId);
	ImGui::DockBuilderDockWindow("Camera Control", rightId);
	ImGui::DockBuilderDockWindow("Editing UVTranslate ( Sprite )", bottomId);
	ImGui::DockBuilderDockWindow("Editing Particle", bottomId);
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
			// 映像全体が必ず収まる倍率を選び、余った部分をレターボックスにする。
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

	// Factoryへ登録したシーンを一覧表示し、選択時に切り替えを予約する。
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

	ImGui::Spacing();
	ImGui::TextDisabled("Drag any debug window by its tab to rearrange it.");
	ImGui::TextDisabled("Use Layout > Reset to Default to restore the layout.");
	ImGui::End();
}
#endif

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
	ImGui::Begin("FPS", &showFpsWindow_);

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
void ImGuiManager::SpriteWindow(const std::vector<std::unique_ptr<Sprite>>& sprites)
{
#ifdef USE_IMGUI

	static float my_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // カラーピッカーの色

	// ウィンドウ作成
	if (!showSpriteWindow_) {
		return;
	}
	ImGui::Begin("Editing UVTranslate ( Sprite )", &showSpriteWindow_);
	ImGui::Separator();

	// ここで色を変えたら、配列内の全スプライトに色を適用する
	if (ImGui::ColorEdit4("Color", my_color)) {
		for (auto& sprite : sprites) {
			sprite->SetColor({ my_color[0], my_color[1], my_color[2], my_color[3] });
		}
	}

	ImGui::Separator();

	// std::vector の全要素に対して処理
	for (int i = 0; i < sprites.size(); ++i)
	{
		// IDをプッシュ（これが無いと全部のスプライトが同時に動いてしまう）
		ImGui::PushID(i);
		ImGui::Text("Sprite %d", i); // 番号表示
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
		ImGui::PopID(); // IDをポップ
	}

	ImGui::End();
#endif
}

void ImGuiManager::ModelWindow(const std::vector<std::unique_ptr<Object3d>>& normalObjects, const std::vector<std::unique_ptr<Object3d>>& animationObjects, Object3d::DirectionalLight& light,Object3d::PointLight& pointLight,Object3d::SpotLight& spotLight)
{
#ifdef USE_IMGUI

	static int targetArray = 0;// 0:アニメーションなし 1:アニメーションあり
	static int selectedIndex = 0; //選択されている番号
	static bool useMonsterBall = false;//png入れ替え
	// ウィンドウ作成
	if (!showModelWindow_) {
		return;
	}
	ImGui::Begin("Editing Object", &showModelWindow_);

	if (ImGui::Button("Select NormalModel")) {
		targetArray = 0;
		selectedIndex = 0;
	}
	ImGui::SameLine();
	if (ImGui::Button("Select AnimarionModel")) {
		targetArray = 1;
		selectedIndex = 0;
	}
	ImGui::SameLine();
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

	ImGui::Separator();

	//選択しているオブジェクトのポインタを取得
	Object3d* targetObject = nullptr;
	// 配列の範囲外エラーを防ぐ
	if (targetArray == 0 && selectedIndex < normalObjects.size()) {
		targetObject = normalObjects[selectedIndex].get();
		ImGui::Text("Editing: Normal Object [%d]", selectedIndex);
	}else if (targetArray == 1 && selectedIndex < animationObjects.size()) {
		targetObject = animationObjects[selectedIndex].get();
		ImGui::Text("Editing: Animation Object [%d]", selectedIndex);
	}

	// --- 対象が存在すれば Transform をいじる ---
	if (targetObject) {
		// IDを分けて干渉を防ぐ（配列番号×100 + インデックス）
		ImGui::PushID(targetArray * 100 + selectedIndex);

		Transform& transform = targetObject->GetTransform();

		// 各オブジェクトのTransformを取得して操作
		ImGui::DragFloat3("Translate", &transform.translate.x, 0.01f);
		ImGui::DragFloat3("Rotate", &transform.rotate.x, 0.01f);
		ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);

		float environmentCoefficient = targetObject->GetEnvironmentCoefficient();
		if (ImGui::SliderFloat("Environment Reflection", &environmentCoefficient, 0.0f, 1.0f)) {
			targetObject->SetEnvironmentCoefficient(environmentCoefficient);
		}
		ImGui::PopID();
} else {
	ImGui::TextColored(ImVec4(1, 0, 0, 1), "Selected object does not exist!");
}

		ImGui::Separator();

		ImGui::Text("Light Settings");
		ImGui::DragFloat3("DirectoinalLight:direction", &light.direction.x, 0.01f);//ハイライトの位置
		ImGui::DragFloat("DirectoinalLight:intensity", &light.intensity, 0.01f);//全体の明るさ
		ImGui::DragFloat3("DirectoinalLight:color", &light.color.x, 0.01f);

		ImGui::Separator();

		ImGui::Text("Point Light");
		ImGui::DragFloat3("PointLight:position", &pointLight.position.x, 0.01f);
		ImGui::DragFloat("PointLight:intensity", &pointLight.intensity, 0.01f, 0.0f, 10.0f);
		ImGui::ColorEdit3("PointLight:color", & pointLight.color.x);
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
	
	ImGui::End();
#endif
}

std::string ImGuiManager::ParticleWindow(Transform& emitterTransform)
{
#ifdef USE_IMGUI
	std::string result = ""; // 何も押されていなければ空文字を返す
	// ウィンドウ作成
	if (!showParticleWindow_) {
		return "";
	}
	ImGui::Begin("Editing Particle", &showParticleWindow_);
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

	ImGui::End();
	return result;
#else
	return "";
#endif
}

void ImGuiManager::CameraWindow(CameraManager* cameraManager)
{
#ifdef USE_IMGUI

	// ウィンドウ作成
	if (!showCameraWindow_) {
		return;
	}
	ImGui::Begin("Camera Control", &showCameraWindow_);
	ImGui::Separator();

	//カメラ
	Camera* activeCamera = cameraManager->GetActiveCamera();
	if (activeCamera)
	{//位置
		Vector3 cameraPos = activeCamera->GetTranslate();
		if (ImGui::DragFloat3("CameraTranslate", &cameraPos.x, 0.01f))
		{
			activeCamera->SetTranslate(cameraPos);
		}//角度
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

	ImGui::End();
#endif
}

void ImGuiManager::PostEffectWindow()
{
#ifdef USE_IMGUI
	if (!showPostEffectWindow_) {
		return;
	}
	ImGui::Begin("Post Effect", &showPostEffectWindow_);
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

// アニメーションのスケルトンをデバッグ表示する。
void ImGuiManager::SkeletonDebugDraw(const Model::Skeleton& skeleton, const Matrix4x4& worldMatrix, const Matrix4x4& viewProjectionMatrix)
{
#ifdef USE_IMGUI
	ImDrawList* drawList = gameViewDrawList_;
	ImVec2 imageMin = gameViewImageMin_;
	ImVec2 imageSize = gameViewImageSize_;
	if (drawList == nullptr || imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
		drawList = ImGui::GetBackgroundDrawList();
		imageMin = ImVec2(0.0f, 0.0f);
		imageSize = ImGui::GetIO().DisplaySize;
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

	// Game Viewの外側へデバッグ線がはみ出さないようにする。
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
