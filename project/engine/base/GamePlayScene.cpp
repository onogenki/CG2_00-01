#include "GamePlayScene.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "PostEffect.h"
#include "ImGuiManager.h"
#include "Input.h"
#include"SceneManager.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
#include <algorithm>
#include <dinput.h>
using namespace MyMath;

void GamePlayScene::Initialize()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);

	//3Dオブジェクト共通部の初期化
	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);

	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(dxCommon);

	//カメラマネージャ
	cameraManager = std::make_unique<CameraManager>();


	ParticleManager::GetInstance()->SetCameraManager(cameraManager.get());

	//メインカメラ
	mainCamera = std::make_unique<Camera>();
	mainCamera->SetTranslate({ 0.0f,0.0f,-10.0f });
	cameraManager->AddCamera("MainCamera", mainCamera.get());

	//上アングルカメラ
	upCamera = std::make_unique<Camera>();
	upCamera->SetRotate({ 0.785f,0.0f,0.0f });
	upCamera->SetTranslate({ 0.0f,5.0f,-5.0f });
	cameraManager->AddCamera("UpCamera", upCamera.get());

	//MainCameraをアクティブ
	cameraManager->SetActiveCamera("MainCamera");
	//共通部にはマネージャのアクティブカメラを渡す
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");//1枚目
	TextureManager::GetInstance()->LoadTexture("Resources/monsterBall.png");//2枚目
	TextureManager::GetInstance()->LoadTexture("Resources/grass.png");//terrainのpng
	TextureManager::GetInstance()->LoadTexture("Resources/circle.png");
	TextureManager::GetInstance()->LoadTexture("Resources/circle2.png");
	TextureManager::GetInstance()->LoadTexture("Resources/gradationLine.png");

	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "Resources/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Plane", "Resources/uvChecker.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Hit", "Resources/circle2.png");
	ParticleManager::GetInstance()->CreateRingParticleGroup("Ring", "Resources/gradationLine.png");
	ParticleManager::GetInstance()->CreateCylinderParticleGroup("Cylinder", "Resources/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("PillarSparkle", "Resources/circle2.png");
	ParticleManager::GetInstance()->CreateParticleGroup("LightCore", "Resources/circle2.png");
	ParticleManager::GetInstance()->CreateParticleGroup("LightRain", "Resources/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("LightSpiral", "Resources/circle2.png");
	
	//.objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("AnimatedCube.gltf");//アニメーションありのモデル
	ModelManager::GetInstance()->LoadModel("walk.gltf");//アニメーションのみだが必要
	ModelManager::GetInstance()->LoadModel("sneakWalk.gltf");//アニメーションのみだが必要
	ModelManager::GetInstance()->LoadModel("human.gltf");//持ってきたもの
	ModelManager::GetInstance()->LoadModel("playerCloudAnimation.gltf");//持ってきたアニメーション

	//アニメーションの読み込み
	walkAnimation_ = Model::LoadAnimationFile("./resources", "walk.gltf");//アニメーションのみ
	sneakWalkAnimation_ = Model::LoadAnimationFile("./resources", "sneakWalk.gltf");//アニメーションのみ
	humanAnimation_ = Model::LoadAnimationFile("./resources", "human.gltf");//持ってきたもの
	hissatu_ = Model::LoadAnimationFile("./resources", "playerCloudAnimation.gltf");//持ってきたもの

	// skyBoxの背景
	TextureManager::GetInstance()->LoadTexture("Resources/qwantani_moon_noon_puresky_1k.dds");

	// skyBoxによってモデルの反射が映り込む
	object3dCommon->SetEnvironmentTexturePath("Resources/qwantani_moon_noon_puresky_1k.dds");

	//Skybox
	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(dxCommon, cameraManager->GetActiveCamera());
	// 添付されていたDDSテクスチャのパスを指定する
	skyBox_->SetTexture("Resources/qwantani_moon_noon_puresky_1k.dds");

	//音声読み込み
	Audio::GetInstance()->LoadFile("Resources/Alarm01.wav");
	//音声再生
	Audio::GetInstance()->PlayWave("Resources/Alarm01.wav");

	///
	/// 3Dオブジェクト生成
	/// normalとanimationに分けてるのは処理を軽くするため
	
	// 一時的に unique_ptr を作り、初期化してから vector に move する
	auto objPlane = std::make_unique<Object3d>();
	objPlane->Initialize(object3dCommon);
	objPlane->SetModel("terrain.obj");
	objPlane->GetTransform().translate = { 1.0f, -2.0f, 10.0f };
	objectPlane = objPlane.get();           // 中身を指すだけのポインタを保存
	normalObjects.push_back(std::move(objPlane));//通常(obj)モデル入れる

	auto objAxis = std::make_unique<Object3d>();
	objAxis->Initialize(object3dCommon);
	objAxis->SetModel("human.gltf");//アニメーションモデル読み込み
	objAxis->InitializeAnimation();//skinClusterが1回だけ作られてisSkeletal_がtrueになる
	objAxis->SetEnvironmentCoefficient(0.3f);//このモデルの反射の強さ
	objAxis->GetTransform().translate = { 2.0f, 0.0f, 0.0f };
	objAxis->GetTransform().rotate = { 0.0f,0.0f,0.0f };
	objAxis->GetTransform().scale = { 0.2f,0.2f,0.2f };

	objAxis->PlayAnimation(humanAnimation_);//アニメーション再生

	//1ループ再生させる場合(ループや時間指定させたい場合は削除する)
	objAxis->SetIsLoop(false);
	//時間指定したいなら利用(アニメーション時間で演出などができる)
	//objAxis->SetMaxPlayTime(6.0f);

	objectAxis = objAxis.get();//外部保存用に記録

	animationObjects.push_back(std::move(objAxis));//アニメーションモデル専用に登録
	
	for (uint32_t i = 0; i < 1; ++i)
	{
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon, "Resources/monsterBall.png");

		if (i % 2 == 0) {
			// 偶数にモンスターボールpng
			sprite->SetTexture("Resources/uvChecker.png");
		}
		Vector2 pos = { 0.0f + i * 0.0f, 0.0f + i * 50.0f };
		sprite->SetPosition(pos);

		sprites.push_back(std::move(sprite));
	}

	// ライトの初期値を設定する
	// 平行光源はOFF (Intensity = 0.0f)
	directionalLight.direction = { 1.0f, -1.0f, 1.0f };
	directionalLight.intensity = 0.0f;
	directionalLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 点光源はON (初期位置 0, 2, 0)
	pointLight.position = { 0.0f, 2.0f, 0.0f };
	pointLight.intensity = 1.0f;
	pointLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLight.radius = 10.0f;
	pointLight.decay = 1.0f;

	//スポットライト
	spotLight.position = { 2.0f,1.25f,0.0f };
	spotLight.intensity = 4.0f;
	spotLight.color = { 1.0f,1.0f,1.0f,1.0f };
	spotLight.distance = 7.0f;
	spotLight.direction =
		Normalize({ -1.0f,-1.0f,0.0f });
	spotLight.decay = 2.0f;
	spotLight.cosAngle =
		std::cos(std::numbers::pi_v<float> / 3.0f);
	spotLight.cosFalloffStart = 1.0f;

	//パーティクル
	//座標、1回の発生数、発生頻度[秒]
	emitterTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	//Circleパーティクル
	emitterCircle = std::make_unique<ParticleEmitter>("Circle", emitterTransform, 1, 0.1f,false);
	//四角形のパーティクル(風に吹かれる方)
	emitterPlane = std::make_unique<ParticleEmitter>("Plane", emitterTransform, 1, 0.3f,true);

	//最初はcircleにする
	activeEmitter = emitterCircle.get();

	selectedUI = 0;

}

void GamePlayScene::UpdateGameViewCameraControl()
{
	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (!activeCamera) {
		return;
	}

	Input* input = Input::GetInstance();
	const Vector2 mouseScreen = input->GetMouseScreen();
	const bool isMouseOverGameView = ImGuiManager::GetInstance()->IsMouseOverGameView(mouseScreen.x, mouseScreen.y);
	const bool isMouseButtonDown =
		input->IsMouseButtonPressed(0) ||
		input->IsMouseButtonPressed(1) ||
		input->IsMouseButtonPressed(2);

	if (!isMouseButtonDown) {
		isGameViewCameraDragging_ = false;
	}
	if (isMouseOverGameView &&
		(input->TriggerMouseButton(0) || input->TriggerMouseButton(1) || input->TriggerMouseButton(2))) {
		isGameViewCameraDragging_ = true;
	}

	Vector3 cameraTranslate = activeCamera->GetTranslate();
	Vector3 cameraRotate = activeCamera->GetRotate();

	constexpr float moveSpeed = 0.01f;
	constexpr float rotateSpeed = 0.005f;
	constexpr float zoomSpeed = 0.01f;
	constexpr float minPitch = -1.45f;
	constexpr float maxPitch = 1.45f;

	if (isGameViewCameraDragging_ && input->IsMouseButtonPressed(0)) {
		cameraTranslate.x -= static_cast<float>(input->GetMouseX()) * moveSpeed;
		cameraTranslate.y += static_cast<float>(input->GetMouseY()) * moveSpeed;
	}
	if (isGameViewCameraDragging_ && (input->IsMouseButtonPressed(1) || input->IsMouseButtonPressed(2))) {
		cameraRotate.y += static_cast<float>(input->GetMouseX()) * rotateSpeed;
		cameraRotate.x += static_cast<float>(input->GetMouseY()) * rotateSpeed;
		cameraRotate.x = std::clamp(cameraRotate.x, minPitch, maxPitch);
	}
	if (isMouseOverGameView && input->GetMouseWheel() != 0) {
		cameraTranslate.z += static_cast<float>(input->GetMouseWheel()) * zoomSpeed;
	}

	activeCamera->SetTranslate(cameraTranslate);
	activeCamera->SetRotate(cameraRotate);
}

Vector3 GamePlayScene::GetParticleEffectPosition() const
{
	if (!objectAxis) {
		return { 0.0f, 0.0f, 0.0f };
	}

	Vector3 effectPosition = objectAxis->GetTransform().translate;
	effectPosition.x += 1.0f;
	effectPosition.y += 1.0f;
	return effectPosition;
}

void GamePlayScene::DrawParticleEffectImGui(bool embedded)
{
#ifdef USE_IMGUI
	ParticleManager* particleManager = ParticleManager::GetInstance();
	if (!embedded) {
		ImGui::Begin("Particle Effects");
	}
	ImGui::TextUnformatted("Emit position: same as P key");

	auto drawControl = [particleManager](const char* label, const char* groupName, ParticleEffectControl& control) {
		if (ImGui::TreeNode(label)) {
			ImGui::Checkbox("Enable", &control.enabled);
			ImGui::SliderInt("Emit Count", &control.emitCount, 1, 100);
			ImGui::SliderFloat("Scale", &control.scale, 0.1f, 5.0f, "%.2f");
			if (ImGui::Checkbox("Billboard", &control.billboard)) {
				particleManager->SetBillboardEnabled(groupName, control.billboard);
			}
			particleManager->SetBillboardEnabled(groupName, control.billboard);
			ImGui::TreePop();
		}
	};

	drawControl("Hit Slash", "Hit", hitEffect_);
	drawControl("Impact Ring", "Ring", ringEffect_);
	if (ImGui::TreeNode("Portal Cylinder")) {
		if (ImGui::Checkbox("Display", &cylinderEffect_.enabled)) {
			refreshCylinderEffect_ = true;
		}
		ImGui::SliderInt("Emit Count", &cylinderEffect_.emitCount, 1, 100);
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			refreshCylinderEffect_ = true;
		}
		ImGui::SliderFloat("Scale", &cylinderEffect_.scale, 0.1f, 5.0f, "%.2f");
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			refreshCylinderEffect_ = true;
		}
		if (ImGui::Checkbox("Billboard", &cylinderEffect_.billboard)) {
			particleManager->SetBillboardEnabled("Cylinder", cylinderEffect_.billboard);
			refreshCylinderEffect_ = true;
		}
		particleManager->SetBillboardEnabled("Cylinder", cylinderEffect_.billboard);
		ImGui::TreePop();
	}
	drawControl("Pillar Sparkle", "PillarSparkle", pillarSparkleEffect_);
	drawControl("Light Core", "LightCore", lightCoreEffect_);
	drawControl("Light Rain", "LightRain", lightRainEffect_);
	drawControl("Light Spiral", "LightSpiral", lightSpiralEffect_);

	if (ImGui::Button("Clear All Effects")) {
		particleManager->ClearParticles("Hit");
		particleManager->ClearParticles("Ring");
		particleManager->ClearParticles("Cylinder");
		particleManager->ClearParticles("PillarSparkle");
		particleManager->ClearParticles("LightCore");
		particleManager->ClearParticles("LightRain");
		particleManager->ClearParticles("LightSpiral");
		isCylinderEffectVisible_ = false;
		hitEffect_.enabled = false;
		ringEffect_.enabled = false;
		cylinderEffect_.enabled = false;
		pillarSparkleEffect_.enabled = false;
		lightCoreEffect_.enabled = false;
		lightRainEffect_.enabled = false;
		lightSpiralEffect_.enabled = false;
		lastCylinderEffectEnabled_ = false;
		lastCylinderEffectEmitCount_ = cylinderEffect_.emitCount;
		lastCylinderEffectScale_ = cylinderEffect_.scale;
		lastCylinderEffectBillboard_ = cylinderEffect_.billboard;
		refreshCylinderEffect_ = false;
	}
	if (!embedded) {
		ImGui::End();
	}
#else
	(void)embedded;
#endif
}

void GamePlayScene::DrawInspectorImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Inspector");
	if (ImGui::BeginTabBar("InspectorTabs")) {
		if (ImGui::BeginTabItem("Sprite")) {
			ImGuiManager::GetInstance()->SpriteWindow(sprites, true);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Model")) {
			ImGuiManager::GetInstance()->ModelWindow(normalObjects, animationObjects, directionalLight, pointLight, spotLight, true);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Particle")) {
			std::string particleRequest = ImGuiManager::GetInstance()->ParticleWindow(emitterTransform, true);
			if (particleRequest == "Circle") {
				activeEmitter = emitterCircle.get();
			} else if (particleRequest == "Plane") {
				activeEmitter = emitterPlane.get();
			}
			if (activeEmitter) {
				activeEmitter->SetTranslate(emitterTransform.translate);
			}

			auto drawEmitterControl = [](const char* label, ParticleEmitter* emitter) {
				if (!emitter || !ImGui::TreeNode(label)) {
					return;
				}

				int count = static_cast<int>(emitter->GetCount());
				if (ImGui::SliderInt("Emit Count", &count, 1, 100)) {
					emitter->SetCount(static_cast<uint32_t>(count));
				}

				float frequency = emitter->GetFrequency();
				if (ImGui::SliderFloat("Frequency", &frequency, 0.02f, 2.0f, "%.2f")) {
					emitter->SetFrequency(frequency);
				}

				Vector3 scale = emitter->GetTransform().scale;
				if (ImGui::SliderFloat("Scale", &scale.x, 0.1f, 5.0f, "%.2f")) {
					scale.y = scale.x;
					scale.z = scale.x;
					emitter->SetScale(scale);
				}

				bool receivesWind = emitter->GetReceivesWind();
				if (ImGui::Checkbox("Receives Wind", &receivesWind)) {
					emitter->SetReceivesWind(receivesWind);
				}
				ImGui::TreePop();
			};

			drawEmitterControl("Circle Particle", emitterCircle.get());
			drawEmitterControl("Plane Particle", emitterPlane.get());

			if (ImGui::Button("Rain Wind")) {
				ParticleManager::GetInstance()->SetWindEnabled(true);
				ParticleManager::GetInstance()->SetWindAcceleration({ 0.0f, -0.35f, 0.0f });
				emitterCircle->SetReceivesWind(true);
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Effect")) {
			DrawParticleEffectImGui(true);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Camera")) {
			ImGuiManager::GetInstance()->CameraWindow(cameraManager.get(), true);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
#endif
}

void GamePlayScene::UpdateParticleEffectEmission()
{
	ParticleManager* particleManager = ParticleManager::GetInstance();
	const Vector3 effectPosition = GetParticleEffectPosition();
	if (cylinderEffect_.enabled != lastCylinderEffectEnabled_ || (cylinderEffect_.enabled && refreshCylinderEffect_)) {
		if (cylinderEffect_.enabled) {
			particleManager->ClearParticles("Cylinder");
			particleManager->EmitCylinderEffect("Cylinder", static_cast<uint32_t>(cylinderEffect_.emitCount), effectPosition, cylinderEffect_.scale);
			isCylinderEffectVisible_ = true;
			lastCylinderEffectEmitCount_ = cylinderEffect_.emitCount;
			lastCylinderEffectScale_ = cylinderEffect_.scale;
			lastCylinderEffectBillboard_ = cylinderEffect_.billboard;
		} else {
			particleManager->ClearParticles("Cylinder");
			isCylinderEffectVisible_ = false;
		}
		lastCylinderEffectEnabled_ = cylinderEffect_.enabled;
		refreshCylinderEffect_ = false;
	}

	particleEffectEmitTimer_ += DirectXCommon::GetInstance()->GetDeltaTime();
	if (particleEffectEmitTimer_ < 0.12f) {
		return;
	}
	particleEffectEmitTimer_ = 0.0f;

	if (hitEffect_.enabled) {
		particleManager->EmitHitEffect("Hit", static_cast<uint32_t>(hitEffect_.emitCount), effectPosition, hitEffect_.scale);
	}
	if (ringEffect_.enabled) {
		particleManager->EmitRingEffect("Ring", static_cast<uint32_t>(ringEffect_.emitCount), effectPosition, ringEffect_.scale);
	}
	if (pillarSparkleEffect_.enabled) {
		particleManager->EmitPillarSparkle("PillarSparkle", static_cast<uint32_t>(pillarSparkleEffect_.emitCount), effectPosition, pillarSparkleEffect_.scale);
	}
	if (lightCoreEffect_.enabled) {
		particleManager->EmitLightCore("LightCore", static_cast<uint32_t>(lightCoreEffect_.emitCount), effectPosition, lightCoreEffect_.scale);
	}
	if (lightRainEffect_.enabled) {
		particleManager->EmitLightRain("LightRain", static_cast<uint32_t>(lightRainEffect_.emitCount), effectPosition, lightRainEffect_.scale);
	}
	if (lightSpiralEffect_.enabled) {
		particleManager->EmitLightSpiral("LightSpiral", static_cast<uint32_t>(lightSpiralEffect_.emitCount), effectPosition, lightSpiralEffect_.scale);
	}
}

void GamePlayScene::Update()
{
	UpdateGameViewCameraControl();

	//カメラの更新
	cameraManager->Update();

	//カメラのビュープロジェクション行列を渡して更新
	Matrix4x4 viewMatrix = cameraManager->GetActiveCamera()->GetViewMatrix();
	Matrix4x4 projectionMatrix = cameraManager->GetActiveCamera()->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

	// 安全のために Nullチェックを追加
	if (activeEmitter) {
		activeEmitter->Update();
	}
	//パーティクル全体の更新
	ParticleManager::GetInstance()->Update();

	//3Dオブジェクトの更新通常モデル
	for (auto& object3d : normalObjects) {
		//毎フレーム、マネージャから今のアクティブカメラをもらう
		object3d->SetCamera(cameraManager->GetActiveCamera());
		float length = Length(directionalLight.direction);
		if (length > 0.0f) {
			directionalLight.direction = Normalize(directionalLight.direction);
		} else {
			// 0の場合は適当な下向きにするなど、エラーを回避する
			directionalLight.direction = { 0.0f, -1.0f, 0.0f };
		}
		object3d->SetDirectionalLight(directionalLight);//光を他のモデルにも分け与える
		object3d->SetPointLight(pointLight);
		object3d->SetSpotLight(spotLight);
		object3d->Update();
	}

	// アニメーションの時間確認
	this->animationTime_ = objectAxis->GetAnimationTime();

	//3Dオブジェクトの更新アニメーションモデル
	for (auto& object3d : animationObjects) {
		//毎フレーム、マネージャから今のアクティブカメラをもらう
		object3d->SetCamera(cameraManager->GetActiveCamera());
		float length = Length(directionalLight.direction);
		if (length > 0.0f) {
			directionalLight.direction = Normalize(directionalLight.direction);
		} else {
			// 0の場合は適当な下向きにするなど、エラーを回避する
			directionalLight.direction = { 0.0f, -1.0f, 0.0f };
		}
		object3d->SetDirectionalLight(directionalLight);//光を他のモデルにも分け与える
		object3d->SetPointLight(pointLight);
		object3d->SetSpotLight(spotLight);
		object3d->Update();
	}

	//スプライトの更新
	for (auto& sprite : sprites)
	{
		sprite->Update();
	}
	skyBox_->Update();

	//ゲームの処理

	ImGuiManager::GetInstance()->Begin("GamePlay");
	DrawInspectorImGui();
	// スケルトンをGame Viewの映像座標へ合わせ、モデルの上へ重ねて表示する。
	if (ImGuiManager::GetInstance()->IsSkeletonDebugDrawEnabled() && !animationObjects.empty())
	{
		Object3d* animationObject = animationObjects[0].get();
		if (animationObject->IsSkeletal())
		{
			const Transform& animationTransform = animationObject->GetTransform();
			const Matrix4x4 animationWorldMatrix = MakeAffineMatrix(
				animationTransform.scale,
				animationTransform.rotate,
				animationTransform.translate);

			ImGuiManager::GetInstance()->SkeletonDebugDraw(
				animationObject->GetSkeleton(),
				animationWorldMatrix,
				cameraManager->GetActiveCamera()->GetViewProjectionMatrix());
		}
	}

	ImGuiManager::GetInstance()->End();

	UpdateParticleEffectEmission();


	//Cキーが押されたらCylinderエフェクトの表示を切り替える
	if (Input::GetInstance()->TriggerKey(DIK_0))
	{
		if (isCylinderEffectVisible_) {
			ParticleManager::GetInstance()->ClearParticles("Cylinder");
			isCylinderEffectVisible_ = false;
			cylinderEffect_.enabled = false;
			lastCylinderEffectEnabled_ = false;
		} else {
			OutputDebugStringA("Hit C\n");
			Vector3 hitPosition = GetParticleEffectPosition();
			ParticleManager::GetInstance()->EmitCylinderEffect("Cylinder", static_cast<uint32_t>(cylinderEffect_.emitCount), hitPosition, cylinderEffect_.scale);
			isCylinderEffectVisible_ = true;
			cylinderEffect_.enabled = true;
			lastCylinderEffectEnabled_ = true;
			lastCylinderEffectEmitCount_ = cylinderEffect_.emitCount;
			lastCylinderEffectScale_ = cylinderEffect_.scale;
			lastCylinderEffectBillboard_ = cylinderEffect_.billboard;
		}
	}

	//数字のPキーが押されていたら
	if (Input::GetInstance()->TriggerKey(DIK_P))
	{
		Vector3 hitPosition = objectAxis->GetTransform().translate;
		hitPosition.x += 1.0f;
		hitPosition.y += 1.0f;
		OutputDebugStringA("Hit p\n");
		ParticleManager::GetInstance()->EmitHitEffect("Hit", static_cast<uint32_t>(hitEffect_.emitCount), hitPosition, hitEffect_.scale);
		ParticleManager::GetInstance()->EmitRingEffect("Ring", static_cast<uint32_t>(ringEffect_.emitCount), hitPosition, ringEffect_.scale);
	}
}

void GamePlayScene::Draw()
{

	//描画前処理
	DirectXCommon::GetInstance()->PreDraw();
	SrvManager::GetInstance()->PreDraw();

	// 先に通常モデル用を描画(処理を軽くするため)
	object3dCommon->SetCommonDrawSetting();
	for (const auto& object3d : normalObjects) {
		if (!object3d->IsSkeletal()) {
			object3d->Draw();
		}
	}
	//skyBox描画
	if (skyBox_) {
		skyBox_->Draw();
	}

	// アニメーションモデル用の描画
	object3dCommon->SetSkinningCommonDrawSetting();
	for (const auto& object3d : animationObjects) {
		if (object3d->IsSkeletal()) {
			object3d->Draw();
		}
	}

	//パーティクル描画
	ParticleManager::GetInstance()->Draw();

	// スプライト描画
	//Spriteの描画準備Spriteの描画に共通のグラフィックスコマンドを積む
	spriteCommon->SetCommonDrawSetting();
	for (const auto& sprite : sprites)
	{
		sprite->Draw();
	}

	// SceneはRenderTextureへ描画済みなので、ImGuiの直前にSwapChainへ切り替える
	if (PostEffect::GetInstance()->IsEnabled()) {
		DirectXCommon::GetInstance()->PreDrawForPostEffectTexture();
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), true);
	}
	DirectXCommon::GetInstance()->PreDrawForSwapChain(PostEffect::GetInstance()->IsEnabled());
#ifndef USE_IMGUI
	// RenderTextureのSceneを全画面三角形でSwapChainへコピーする
	if (PostEffect::GetInstance()->IsEnabled()) {
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetPostEffectTextureSrvIndex(), false);
	} else {
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), false);
	}
#endif
	ImGuiManager::GetInstance()->Draw(DirectXCommon::GetInstance());

	DirectXCommon::GetInstance()->PostDraw();

}

void GamePlayScene::Finalize()
{
	//GPUの完了待ち
	DirectXCommon::GetInstance()->WaitForGPU();

	// 中途半端に生き残っている粒子が原因のアクセス違反を防ぐ
	ParticleManager::GetInstance()->SetCameraManager(nullptr);
	ParticleManager::GetInstance()->ClearAllGroups();

	activeEmitter = nullptr;
	objectPlane = nullptr;
	objectAxis = nullptr;
	emitterCircle.reset();
	emitterPlane.reset();
	skyBox_.reset();
	sprites.clear();
	normalObjects.clear();
	animationObjects.clear();
}

