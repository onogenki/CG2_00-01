#include "TitleScene.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "PostEffect.h"
#include "ImGuiManager.h"
#include "Input.h"
#include"SceneManager.h"
#include <dinput.h>
using namespace MyMath;

void TitleScene::Initialize()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	cameraManager = std::make_unique<CameraManager>();
	mainCamera = std::make_unique<Camera>();

	// ゲームプレイシーンと同じ向きでSkyBoxを表示する
	// X軸は傾けず、Y軸だけを1.6ラジアン回転する
	mainCamera->SetRotate({ 0.0f, 1.6f, 0.0f });
	mainCamera->SetTranslate({ 0.0f, 0.0f, -10.0f });
	cameraManager->AddCamera("MainCamera", mainCamera.get());
	cameraManager->SetActiveCamera("MainCamera");
	// パーティクルをタイトルシーンのカメラで描画する
	ParticleManager::GetInstance()->SetCameraManager(cameraManager.get());

	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());

	ModelManager::GetInstance()->LoadModel("plane.obj");
	auto terrain = std::make_unique<Object3d>();

	terrain->Initialize(object3dCommon);
	terrain->SetModel("plane.obj");
	terrain->GetTransform().translate = { 1.0f, -2.0f, 10.0f };
	obj = terrain.get();
	normalObjects.push_back(std::move(terrain));

	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(dxCommon);

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("Resources/circle.png");
	TextureManager::GetInstance()->LoadTexture("Resources/circle2.png");
	TextureManager::GetInstance()->LoadTexture("Resources/gradationLine.png");

	// ゲームシーンで使用していたエフェクト用グループをタイトルシーンにも作成する
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "Resources/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Plane", "Resources/uvChecker.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Hit", "Resources/circle2.png");
	ParticleManager::GetInstance()->CreateParticleGroup("LightCore", "Resources/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("LightRain", "Resources/circle2.png");
	ParticleManager::GetInstance()->CreateParticleGroup("LightSpiral", "Resources/circle2.png");
	ParticleManager::GetInstance()->CreateParticleGroup("PillarSparkle", "Resources/circle2.png");
	ParticleManager::GetInstance()->CreateRingParticleGroup("Ring", "Resources/gradationLine.png");
	ParticleManager::GetInstance()->CreateCylinderParticleGroup("Cylinder", "Resources/gradationLine.png");

	// skyBoxの背景
	TextureManager::GetInstance()->LoadTexture("Resources/rostock_laage_airport_4k.dds");

	// skyBoxによってモデルの反射が映り込む
	object3dCommon->SetEnvironmentTexturePath("Resources/rostock_laage_airport_4k.dds");

	//Skybox
	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(dxCommon, cameraManager->GetActiveCamera());
	// 添付されていたDDSテクスチャのパスを指定する
	skyBox_->SetTexture("Resources/rostock_laage_airport_4k.dds");

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(spriteCommon, "Resources/uvChecker.png");
	sprite_->SetPosition({ 0.0f, 0.0f });

	//音声読み込み
	Audio::GetInstance()->LoadFile("Resources/Alarm01.wav");
	//音声再生
	//Audio::GetInstance()->PlayWave("Resources/Alarm01.wav");

	// Circleを初期状態のエフェクトとして一定間隔で発生させる
	emitterTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	emitterCircle = std::make_unique<ParticleEmitter>("Circle", emitterTransform, 1, 0.1f, false);
	emitterPlane = std::make_unique<ParticleEmitter>("Plane", emitterTransform, 1, 0.3f, true);
	// 動画ではキー操作なしで自動再生するため、最初は手動エミッターを選択しない
	activeEmitter = nullptr;

	isFinished_ = false;
}

void TitleScene::Update()
{
	//カメラの更新
	cameraManager->Update();

	// カメラを動かしても、エフェクトが画面正面へ発生するようにする
	UpdateEffectPosition();

	for (auto& object3d : normalObjects) {
		object3d->SetCamera(cameraManager->GetActiveCamera());
		obj->Update();
	}

	//カメラのビュープロジェクション行列を渡して更新
	Matrix4x4 viewMatrix = cameraManager->GetActiveCamera()->GetViewMatrix();
	Matrix4x4 projectionMatrix = cameraManager->GetActiveCamera()->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

	// キー操作なしで30秒間のエフェクトを自動再生する
	UpdateEffectTimeline();

	// ImGuiで手動選択したエミッターがある場合だけ追加で発生させる
	if (activeEmitter) {
		activeEmitter->Update();
	}
	ParticleManager::GetInstance()->Update();

	sprite_->Update();

	ImGuiManager::GetInstance()->Begin();
	// ImGuiから常時発生させるエフェクトをCircleまたはPlaneへ切り替える
	std::string particleRequest = ImGuiManager::GetInstance()->ParticleWindow(emitterTransform);
	if (particleRequest == "Circle") {
		activeEmitter = emitterCircle.get();
	} else if (particleRequest == "Plane") {
		activeEmitter = emitterPlane.get();
	}
	ImGuiManager::GetInstance()->End();

	const Vector3 effectPosition = emitterTransform.translate;

	// 0キーで光の柱として使用するCylinderを表示・非表示にする
	//if (Input::GetInstance()->TriggerKey(DIK_0)) {
	//	if (isCylinderEffectVisible_) {
	//		ParticleManager::GetInstance()->ClearParticles("Cylinder");
	//		isCylinderEffectVisible_ = false;
	//	} else {
	//		ParticleManager::GetInstance()->EmitCylinderEffect("Cylinder", 1, effectPosition);
	//		isCylinderEffectVisible_ = true;
	//	}
	//}

	skyBox_->Update();

	//// PキーでHitとRingを同じ位置に発生させ、複合エフェクトとして表示する
	//if (Input::GetInstance()->TriggerKey(DIK_P)) {
	//	ParticleManager::GetInstance()->EmitHitEffect("Hit", 8, effectPosition);
	//	ParticleManager::GetInstance()->EmitRingEffect("Ring", 3, effectPosition);
	//}

	// 今はゲームシーンに移動しない
	// spaceキーが押されていたら
	//if (Input::GetInstance()->TriggerKey(DIK_SPACE) || Input::GetInstance()->IsPadButtonPressed(0, 1))
	//{
	//	// シーン切り替え
	//	SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	//}
}

// カメラのワールド行列から前方向を取得し、共通の発生位置を正面へ置く
void TitleScene::UpdateEffectPosition()
{
	Camera* camera = cameraManager->GetActiveCamera();
	if (!camera) {
		return;
	}

	// このエンジンでは、カメラのワールド行列の3行目が前方向を表す
	const Matrix4x4& cameraWorldMatrix = camera->GetWorldMatrix();
	const Vector3 forward = Normalize({
		cameraWorldMatrix.m[2][0],
		cameraWorldMatrix.m[2][1],
		cameraWorldMatrix.m[2][2]
	});

	const Vector3 cameraPosition = camera->GetTranslate();
	const float distanceFromCamera = 8.0f;

	emitterTransform.translate = {
		cameraPosition.x + forward.x * distanceFromCamera,
		cameraPosition.y + forward.y * distanceFromCamera,
		cameraPosition.z + forward.z * distanceFromCamera
	};
}

// 30秒を5秒ごとの区間に分け、各区間のエフェクト関数を呼び出す
void TitleScene::UpdateEffectTimeline()
{
	// constは、この関数の途中で値を変更しないことを表す
	const float deltaTime = 1.0f / 60.0f;
	const float showDuration = 30.0f;
	const float phaseDuration = 5.0f;

	effectTimer_ += deltaTime;
	emitTimer_ += deltaTime;

	// 30秒経過したら、粒子を消して最初から再生する
	if (effectTimer_ >= showDuration) {
		effectTimer_ = 0.0f;
		emitTimer_ = phaseDuration;
		currentEffectPhase_ = -1;
		ParticleManager::GetInstance()->ClearAllParticles();
	}

	const int newPhase = static_cast<int>(effectTimer_ / phaseDuration);

	// 区間が変わった瞬間に前の演出を整理する
	if (newPhase != currentEffectPhase_) {
		ParticleManager::GetInstance()->ClearAllParticles();
		currentEffectPhase_ = newPhase;
		emitTimer_ = phaseDuration;
		isCylinderEffectVisible_ = false;
	}

	const Vector3 position = emitterTransform.translate;

	if (currentEffectPhase_ == 0) {
		PlayLightCoreEffect(position);
	} else if (currentEffectPhase_ == 1) {
		PlayLightBurstEffect(position);
	} else if (currentEffectPhase_ == 2) {
		PlayLightPillarEffect(position);
	} else if (currentEffectPhase_ == 3) {
		PlayLightSpiralEffect(position);
	} else if (currentEffectPhase_ == 4) {
		PlayLightRainEffect(position);
	} else {
		PlayLightFinaleEffect(position);
	}
}

// 光の粒とリングを重ね、光が生まれる場面を作る
void TitleScene::PlayLightCoreEffect(const Vector3& position)
{
	if (emitTimer_ >= 0.35f) {
		ParticleManager::GetInstance()->EmitLightCore("LightCore", 4, position);
		ParticleManager::GetInstance()->EmitRingEffect("Ring", 1, position);
		emitTimer_ = 0.0f;
	}
}

// Hitの放射光とRingの波紋を同時に出し、光の爆発を作る
void TitleScene::PlayLightBurstEffect(const Vector3& position)
{
	if (emitTimer_ >= 0.8f) {
		ParticleManager::GetInstance()->EmitHitEffect("Hit", 12, position);
		ParticleManager::GetInstance()->EmitRingEffect("Ring", 3, position);
		emitTimer_ = 0.0f;
	}
}

// 薄いCylinderを一度だけ出し、その周囲へ星形の光粒を続けて発生させる
void TitleScene::PlayLightPillarEffect(const Vector3& position)
{
	if (!isCylinderEffectVisible_) {
		ParticleManager::GetInstance()->EmitCylinderEffect("Cylinder", 1, position);
		isCylinderEffectVisible_ = true;
	}

	if (emitTimer_ >= 0.12f) {
		ParticleManager::GetInstance()->EmitPillarSparkle("PillarSparkle", 4, position);
		emitTimer_ = 0.0f;
	}
}

// 二本の螺旋光とリングを重ね、光が渦を巻く場面を作る
void TitleScene::PlayLightSpiralEffect(const Vector3& position)
{
	if (emitTimer_ >= 2.0f) {
		ParticleManager::GetInstance()->EmitLightSpiral("LightSpiral", 80, position);
		ParticleManager::GetInstance()->EmitRingEffect("Ring", 2, position);
		emitTimer_ = 0.0f;
	}
}

// 上から光の筋を降らせ、中心の放射光と重ねて光の雨を作る
void TitleScene::PlayLightRainEffect(const Vector3& position)
{
	if (emitTimer_ >= 0.25f) {
		ParticleManager::GetInstance()->EmitLightRain("LightRain", 8, position);
		ParticleManager::GetInstance()->EmitHitEffect("Hit", 2, position);
		emitTimer_ = 0.0f;
	}
}

// 螺旋、放射光、リング、光柱を重ねて最後の盛り上がりを作る
void TitleScene::PlayLightFinaleEffect(const Vector3& position)
{
	if (!isCylinderEffectVisible_) {
		ParticleManager::GetInstance()->EmitCylinderEffect("Cylinder", 1, position);
		isCylinderEffectVisible_ = true;
	}

	if (emitTimer_ >= 1.2f) {
		ParticleManager::GetInstance()->EmitPillarSparkle("PillarSparkle", 24, position);
		ParticleManager::GetInstance()->EmitLightCore("LightCore", 6, position);
		ParticleManager::GetInstance()->EmitLightRain("LightRain", 18, position);
		ParticleManager::GetInstance()->EmitLightSpiral("LightSpiral", 48, position);
		ParticleManager::GetInstance()->EmitHitEffect("Hit", 16, position);
		ParticleManager::GetInstance()->EmitRingEffect("Ring", 3, position);
		emitTimer_ = 0.0f;
	}
}

void TitleScene::Draw()
{
	//描画前処理
	DirectXCommon::GetInstance()->PreDraw();
	SrvManager::GetInstance()->PreDraw();

	object3dCommon->SetCommonDrawSetting();
	for (const auto& object : normalObjects) {
		if (!obj->IsSkeletal()) {
			obj->Draw();
		}
	}
	//skyBox描画
	if (skyBox_) {
		skyBox_->Draw();
	}
	spriteCommon->SetCommonDrawSetting();
	//sprite_->Draw();

	// 背景スプライトの後に描画し、エフェクトが背景に隠れないようにする
	ParticleManager::GetInstance()->Draw();

	// SceneはRenderTextureへ描画済みなので、ImGuiの直前にSwapChainへ切り替える
	DirectXCommon::GetInstance()->PreDrawForSwapChain();
	// RenderTextureのSceneを全画面三角形でSwapChainへコピーする
	PostEffect::GetInstance()->Draw();
	ImGuiManager::GetInstance()->Draw(DirectXCommon::GetInstance());

	DirectXCommon::GetInstance()->PostDraw();
	
}

void TitleScene::Finalize()
{
	//GPUの完了待ち
	DirectXCommon::GetInstance()->WaitForGPU();
	// 次の初期化時に同名のパーティクルグループが重複しないように破棄する
	ParticleManager::GetInstance()->ClearAllParticles();
	ParticleManager::GetInstance()->ClearAllGroups();
	activeEmitter = nullptr;
	emitterCircle.reset();
	emitterPlane.reset();

}
