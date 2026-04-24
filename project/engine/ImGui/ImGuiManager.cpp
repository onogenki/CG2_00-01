#include "ImGuiManager.h"
#include "SrvManager.h"
#include "Sprite.h"
#include "ImGuiManager.h"
#include "Object3d.h"
#include "ParticleEmitter.h"
#include "CameraManager.h"
#include "ModelManager.h"

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]]DirectXCommon* directXCommon, [[maybe_unused]]SrvManager* srvManager)
{
#ifdef USE_IMGUI

	//ImGuiのコンテキストを生成
	ImGui::CreateContext();
	//ImGuiのスタイルを設定
	ImGui::StyleColorsDark();

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

void ImGuiManager::Begin()
{
#ifdef USE_IMGUI
	//ゲームの処理
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void ImGuiManager::End()
{
#ifdef USE_IMGUI
	//ImGuiの内部コマンドを生成する
	ImGui::Render();
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
	ImGui::Begin("FPS");

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
	ImGui::Begin("Editing UVTranslate ( Sprite )");
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

void ImGuiManager::ModelWindow(const std::vector<std::unique_ptr<Object3d>>& objects, Object3d::DirectionalLight& light,Object3d::PointLight& pointLight,Object3d::SpotLight& spotLight)
{
#ifdef USE_IMGUI

	static int selectedIndex = 0; //選択されている番号
	static bool useMonsterBall = false;//png入れ替え
	// ウィンドウ作成
	ImGui::Begin("Editing Object");

	if (ImGui::Button("Select Plane")) {
		selectedIndex = 0;
	}
	ImGui::SameLine();
	if (ImGui::Button("Select Axis")) {
		selectedIndex = 1;
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

	// 配列の範囲外エラーを防ぐ
	if (selectedIndex >= 0 && selectedIndex < objects.size()) {
		ImGui::PushID(selectedIndex); // IDを分けて干渉を防ぐ

		if (selectedIndex == 0) {
			ImGui::Text("Plane");
		} else if (selectedIndex == 1) {
			ImGui::Text("Axis");
		}

		Transform& transform = objects[selectedIndex]->GetTransform();
		ImGui::Separator();

		// 各オブジェクトのTransformを取得して操作
		ImGui::DragFloat3("Translate", &transform.translate.x, 0.01f);
		ImGui::DragFloat3("Rotate", &transform.rotate.x, 0.01f);
		ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);

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

		ImGui::PopID();
	}
	ImGui::End();
#endif
}

std::string ImGuiManager::ParticleWindow(Transform& emitterTransform)
{
#ifdef USE_IMGUI
	std::string result = ""; // 何も押されていなければ空文字を返す
	// ウィンドウ作成
	ImGui::Begin("Editing Particle");
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
	ImGui::Begin("Camera Control");
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