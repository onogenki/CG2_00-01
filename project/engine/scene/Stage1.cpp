#include "Stage1.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "Collision.h"
#include "Input.h"
#include "ImGuiManager.h"
#include "LevelLoader.h"
#include "ModelManager.h"
#include "PostEffect.h"
#include "SrvManager.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <dinput.h>
#include <fstream>
#include <functional>
#include <numbers>

using namespace MyMath;

namespace
{
	constexpr const char* kStageMapFilePath = "resources/levels/stage1.json";
	constexpr const char* kStageMapFileName = "stage1";

	bool IsEnvironmentEnabled(const char* name)
	{
		char* value = nullptr;
		size_t size = 0;
		if (_dupenv_s(&value, &size, name) != 0 || !value) {
			return false;
		}
		const bool enabled = std::string(value) == "1";
		std::free(value);
		return enabled;
	}

	float CatmullRomValue(float p0, float p1, float p2, float p3, float t)
	{
		const float t2 = t * t;
		const float t3 = t2 * t;
		return 0.5f * (
			2.0f * p1 +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
	}

	Vector3 EvaluateControlPointPath(
		const std::vector<Vector3>& controlPoints,
		float progress,
		bool loop)
	{
		if (controlPoints.empty()) {
			return {};
		}
		if (controlPoints.size() == 1) {
			return controlPoints.front();
		}

		const int pointCount = static_cast<int>(controlPoints.size());
		const int segmentCount = loop ? pointCount : pointCount - 1;
		float pathPosition = progress;
		if (loop) {
			pathPosition = std::fmod(pathPosition, static_cast<float>(segmentCount));
			if (pathPosition < 0.0f) {
				pathPosition += static_cast<float>(segmentCount);
			}
		} else {
			pathPosition = std::clamp(
				pathPosition,
				0.0f,
				static_cast<float>(segmentCount));
		}

		int segmentIndex = static_cast<int>(std::floor(pathPosition));
		float segmentT = pathPosition - static_cast<float>(segmentIndex);
		if (!loop && segmentIndex >= segmentCount) {
			segmentIndex = segmentCount - 1;
			segmentT = 1.0f;
		}

		auto getPoint = [&](int index) -> const Vector3&
		{
			if (loop) {
				index %= pointCount;
				if (index < 0) {
					index += pointCount;
				}
			} else {
				index = std::clamp(index, 0, pointCount - 1);
			}
			return controlPoints[index];
		};

		const Vector3& p0 = getPoint(segmentIndex - 1);
		const Vector3& p1 = getPoint(segmentIndex);
		const Vector3& p2 = getPoint(segmentIndex + 1);
		const Vector3& p3 = getPoint(segmentIndex + 2);
		return {
			CatmullRomValue(p0.x, p1.x, p2.x, p3.x, segmentT),
			CatmullRomValue(p0.y, p1.y, p2.y, p3.y, segmentT),
			CatmullRomValue(p0.z, p1.z, p2.z, p3.z, segmentT),
		};
	}
}

void Stage1::Initialize()
{
	// ---------- 描画の共通設定 ----------
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);

	// ---------- 通常カメラの作成 ----------
	// プレイヤーが部屋を見るときに使うカメラです。
	cameraManager = std::make_unique<CameraManager>();
	mainCamera = std::make_unique<Camera>();
	mainCamera->SetTranslate({ 0.0f, 1.0f, -12.0f });
	cameraManager->AddCamera("MainCamera", mainCamera.get());

	cameraManager->SetActiveCamera("MainCamera");

	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());

	// ---------- 照明の設定 ----------
	directionalLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLight_.direction = Normalize({ 0.5f, -1.0f, 0.5f });
	directionalLight_.intensity = 0.3f;
	directionalLight_.ambientColor = { 1.0f, 1.0f, 1.0f };
	directionalLight_.ambientIntensity = 0.0f;
	pointLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLight_.position = { 0.0f, 3.0f, -2.0f };
	pointLight_.intensity = 5.0f;
	pointLight_.radius = 20.0f;
	pointLight_.decay = 1.0f;

	// ---------- テスト用モデルの読み込み ----------
	// 最初のテストでは、プロジェクトに元からあるモデルだけを使用します。
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("floor.obj");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	if (Model* sphereModel = ModelManager::GetInstance()->FindModel("sphere.obj")) {
		sphereModel->SetTexture("Resources/monsterBall.png");
	}

	// ---------- 床の作成 ----------
	// 床を置くと、鏡がどの位置にあるかを確認しやすくなります。
	auto floor = CreateObject("floor.obj");
	floor->SetScale({ 1.0f, 1.0f, 1.0f });
	floor->SetRotate({ 0.0f, 0.0f, 0.0f });
	//floor.objの高さは3.0なので、上面がY=-2.0になる中心位置に置く
	floor->SetTranslate({ 0.0f, -3.500001f, 5.0f });
	floor_ = floor.get();
	sceneObjects_.push_back(std::move(floor));

	// ---------- 持てる小型鏡とレーザーの作成 ----------
	carryableMirror_ = std::make_unique<CarryableMirror>();
	carryableMirror_->Initialize(
		object3dCommon,
		"plane.obj",
		{ -2.5f, -0.8f, 4.5f },
		3.6f,
		3.6f);

	// ---------- 鏡床の作成 ----------
	// 通常床とは別に、Playerが持てない正方形の鏡床ギミックを配置します。
	mirrorFloor_ = std::make_unique<FixedMirror>();
	if (mirrorFloor_->Initialize(
		object3dCommon,
		dxCommon,
		SrvManager::GetInstance(),
		"plane.obj",
		mirrorFloorPosition_,
		0.0f,
		mirrorFloorWidth_,
		mirrorFloorHeight_,
		256)) {
		// plane.objを寝かせると、上側から床の反射Textureを表示できます。
		mirrorFloor_->SetPitch(-1.57079633f);
		mirrorFloor_->SyncVisualAndCollider();
		// 鏡床は上下どちらから来たLightも反射する特殊ギミックです。
		mirrorFloor_->GetMirror().SetReflectBackface(true);
		mirrorFloor_->GetObject().SetDirectionalLight(directionalLight_);
		mirrorFloor_->GetObject().SetPointLight(pointLight_);
	} else {
		mirrorFloor_.reset();
	}
	// 白い小球をLaserの発射装置として置き、光がどこから出るか見えるようにする
	auto laserEmitter = CreateObject("sphere.obj");
	laserEmitter->SetTranslate(laserOrigin_);
	laserEmitter->SetScale({ 0.9f, 0.9f, 0.9f });
	laserEmitter->SetTextureOverride("resources/white.png");
	laserEmitter_ = laserEmitter.get();
	sceneObjects_.push_back(std::move(laserEmitter));
	laser_.SetOrigin(laserOrigin_);
	laser_.SetDirection(laserDirection_);
	laser_.SetMaxDistance(30.0f);
	laser_.SetMaxReflectionCount(8);
	laserRenderer_ = std::make_unique<LaserRenderer>();
	if (!laserRenderer_->Initialize(dxCommon, 32)) {
		laserRenderer_.reset();
	} else {
		laserRenderer_->SetBeamWidth(laserVisualWidth_);
		laserRenderer_->SetColor({ 0.05f, 0.95f, 1.00f, 1.0f });
	}
	// Door Laserは大型Mirrorが90度回転した後の反射先を、オレンジ色で見せます。
	auto doorLaserEmitter = CreateObject("sphere.obj");
	doorLaserEmitter->SetTranslate(doorLaserOrigin_);
	doorLaserEmitter->SetScale({ 0.65f, 0.65f, 0.65f });
	doorLaserEmitter->SetTextureOverride("resources/white.png");
	doorLaserEmitter_ = doorLaserEmitter.get();
	sceneObjects_.push_back(std::move(doorLaserEmitter));
	doorLaser_.SetOrigin(doorLaserOrigin_);
	doorLaser_.SetDirection(doorLaserDirection_);
	doorLaser_.SetMaxDistance(20.0f);
	doorLaser_.SetMaxReflectionCount(2);
	doorLaserRenderer_ = std::make_unique<LaserRenderer>();
	if (!doorLaserRenderer_->Initialize(dxCommon, 8)) {
		doorLaserRenderer_.reset();
	} else {
		doorLaserRenderer_->SetBeamWidth(laserVisualWidth_);
		doorLaserRenderer_->SetColor({ 1.00f, 0.38f, 0.05f, 1.0f });
	}

	// ---------- 時間制御で動く危険Lightの描画準備 ----------
	// すべてLaserRendererを使うことで、反射PuzzleのLaserと同じ3D空間の太い光として描画します。
	auto createHazardLightRenderer = [dxCommon](
		std::unique_ptr<LaserRenderer>& renderer,
		const Vector4& color,
		float beamWidth,
		size_t maximumSegmentCount) {
		renderer = std::make_unique<LaserRenderer>();
		if (!renderer->Initialize(dxCommon, maximumSegmentCount)) {
			renderer.reset();
			return;
		}
		renderer->SetColor(color);
		renderer->SetBeamWidth(beamWidth);
	};
	// 危険Lightは判定半径を変えず、まず視認性確認用に半透明ビームだけを太くします。
	createHazardLightRenderer(ceilingSweepLightRenderer_, { 0.95f, 0.20f, 1.00f, 1.0f }, 0.70f, 4);
	createHazardLightRenderer(horizontalMoveLightRenderer_, { 1.00f, 0.82f, 0.10f, 1.0f }, 0.68f, 4);
	createHazardLightRenderer(bottomPulseLightRenderer_, { 0.15f, 0.55f, 1.00f, 1.0f }, 0.78f, 4);
	createHazardLightRenderer(bottomPulseWarningRenderer_, { 1.00f, 0.05f, 0.05f, 0.80f }, 2.40f, 1);
	createHazardLightRenderer(orbitLightRenderer_, { 0.20f, 1.00f, 0.35f, 1.0f }, 0.74f, 12);

	// ---------- 反射Laserで動く充電SwitchとDoorの作成 ----------
	// 二つのSwitchはSphere、Doorは厚みのあるfloor.objを縮小して表現します。
	auto chargeSwitch = CreateObject("sphere.obj");
	chargeSwitch->SetTranslate(chargeSwitchPosition_);
	chargeSwitch->SetScale({
		chargeSwitchRadius_ * 2.0f,
		chargeSwitchRadius_ * 2.0f,
		chargeSwitchRadius_ * 2.0f,
	});
	chargeSwitch->SetTextureOverride("resources/white.png");
	chargeSwitch_ = chargeSwitch.get();
	sceneObjects_.push_back(std::move(chargeSwitch));

	auto doorSwitch = CreateObject("sphere.obj");
	doorSwitch->SetTranslate(doorSwitchPosition_);
	doorSwitch->SetScale({ doorSwitchRadius_ * 2.0f, doorSwitchRadius_ * 2.0f, doorSwitchRadius_ * 2.0f });
	doorSwitch->SetTextureOverride("resources/white.png");
	doorSwitch_ = doorSwitch.get();
	sceneObjects_.push_back(std::move(doorSwitch));

	auto lightDoor = CreateObject("floor.obj");
	lightDoor->SetTranslate(doorClosedPosition_);
	lightDoor->SetScale({ 0.20f, 1.00f, 0.05f });
	lightDoor_ = lightDoor.get();
	doorCollider_ = Collision::MakeOBB(
		lightDoor_->GetTransform(),
		doorColliderLocalHalfSize_);
	sceneObjects_.push_back(std::move(lightDoor));

	// 外部ファイルを最初に読み、以降は保存された時だけ再読込する
	stageMapHotReload_.SetFilePath(kStageMapFilePath);
	ReloadStageMap();
	stageMapHotReload_.Synchronize();

	// ---------- プレイヤーと通常Cameraの作成 ----------
	// stage1.jsonのPlayerStartを使い、開始用の床の上へPlayerを置きます。
	const Vector3 playerStartPosition = hasStagePlayerStart_
		? stagePlayerStartPosition_
		: Vector3{ 0.0f, -0.8f, 5.0f };
	player_ = std::make_unique<Player>();
	player_->Initialize(object3dCommon, "sphere.obj", playerStartPosition, 1.2f);
	// Camera 本体とは別の Controller に、Player を追従するルールを任せます。
	cameraController_ = std::make_unique<CameraController>();
	cameraController_->Initialize(mainCamera.get(), player_->GetPosition());
	// 通常Cameraの向きはPlayerの移動方向へ勝手に回さず、矢印キーで選んだ位置を保ちます。
	cameraController_->SetAutoRecenterEnabled(false);
	// 通常Cameraの完成位置を保存してから、開始演出用の前上方Cameraへ切り替えます。
	stageStart_ = std::make_unique<StageStart>();
	stageStart_->Begin(
		*player_,
		*mainCamera,
		playerStartPosition,
		mainCamera->GetTranslate(),
		mainCamera->GetRotate(),
		stageStartSettings_);
	SceneEditor::ScanResourceShelf(stageShelfState_);
	InitializeGameplaySmoke();
}

void Stage1::Finalize()
{
	// unique_ptr がオブジェクトを自動的に解放します。
	// SceneManager がこの関数の前に GPU の処理完了を待機します。
	if (cameraManager) {
		cameraManager->SetActiveCamera("MainCamera");
		for (const StageEventCamera& eventCamera : stageEventCameras_) {
			cameraManager->RemoveCamera(eventCamera.sourceName);
		}
	}
	stageEventCameras_.clear();
	stageEventTriggers_.clear();
	stageCameraAreas_.clear();
	activeEventCameraName_.clear();
	activeCameraAreaName_.clear();
	sceneObjects_.clear();
	stageMapRuntimeObjects_.clear();
	stageMapData_.reset();
	floor_ = nullptr;
	laserEmitter_ = nullptr;
	doorLaserEmitter_ = nullptr;
	chargeSwitch_ = nullptr;
	doorSwitch_ = nullptr;
	lightDoor_ = nullptr;
	fixedMirrors_.clear();
	mirrorFloor_.reset();
	carryableMirror_.reset();
	laserRenderer_.reset();
	doorLaserRenderer_.reset();
	ceilingSweepLightRenderer_.reset();
	horizontalMoveLightRenderer_.reset();
	bottomPulseLightRenderer_.reset();
	bottomPulseWarningRenderer_.reset();
	orbitLightRenderer_.reset();
	stageStart_.reset();
	cameraController_.reset();
	player_.reset();
}

void Stage1::Update()
{
	// ---------- Stage1マップのホットリロード ----------
	UpdateStageMapHotReload();
	if (ImGuiManager::GetInstance()->IsGameViewActive()) {
		UpdateStageMapPaths(DirectXCommon::GetInstance()->GetDeltaTime());
	}

	// ---------- プレイヤーの移動と重力 ----------
	bool isStageStartPlaying = false;
	if (player_ && floor_) {
		//floor.objの大きさとTransformから、見た目と一致するOBBを作る
		floorObb_ = Collision::MakeOBB(
			floor_->GetTransform(),
			floorColliderLocalCenter_,
			floorLocalHalfSize_);
		// 床・鏡・JSONで追加したオブジェクトを、PlayerとCameraが使うOBBとしてまとめる
		stageSolidObbs_ = { floorObb_ };
		// Light用はMirrorを除き、床・Door・壁だけを遮るOBBとして別にまとめます。
		stageLightBlockingObbs_ = { floorObb_ };
		for (const auto& fixedMirror : fixedMirrors_) {
			if (fixedMirror) {
				stageSolidObbs_.push_back(fixedMirror->GetCollider());
			}
		}
		// Doorが開き切るまでは、Playerが通れない壁としてOBBへ加えます。
		if (lightDoor_ && doorOpenAmount_ < 0.95f) {
			stageSolidObbs_.push_back(doorCollider_);
			stageLightBlockingObbs_.push_back(doorCollider_);
		}
		// 持っている間はPlayer自身へ当たらないよう、小型鏡を衝突一覧から外します。
		if (carryableMirror_ && !carryableMirror_->IsCarried()) {
			stageSolidObbs_.push_back(carryableMirror_->GetCollider());
		}
		for (StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
			if (!runtimeObject.visual || !runtimeObject.hasBoxCollider) {
				continue;
			}
			runtimeObject.collider = Collision::MakeOBB(
				runtimeObject.visual->GetTransform(),
				runtimeObject.colliderLocalCenter,
				runtimeObject.colliderLocalHalfSize);
			stageSolidObbs_.push_back(runtimeObject.collider);
			stageLightBlockingObbs_.push_back(runtimeObject.collider);
		}
		const bool isGameViewActive = ImGuiManager::GetInstance()->IsGameViewActive();
		isStageStartPlaying =
			isGameViewActive &&
			stageStart_ &&
			stageStart_->Update(
				DirectXCommon::GetInstance()->GetDeltaTime(),
				*player_,
				*mainCamera);
		if (isStageStartPlaying) {
			// 開始演出中はPlayer入力・Jump・Mirror操作を受け付けません。
		} else if (gameplaySmokeEnabled_) {
			// 自動検証ではCameraに影響されない世界+X方向へ歩かせます。
			Player::ControlInput smokeControl{};
			smokeControl.right = 1.0f;
			player_->UpdateWithControl(
				DirectXCommon::GetInstance()->GetDeltaTime(),
				stageSolidObbs_,
				{ 0.0f, 0.0f, 1.0f },
				smokeControl);
		} else if (ImGuiManager::GetInstance()->IsGameViewActive()) {
			// Mouse左右クリックで携帯Mirrorを構えている間は、Playerの移動・Jumpを弱めます。
			const bool isMirrorGuarding =
				carryableMirror_ &&
				carryableMirror_->IsCarried() &&
				(Input::GetInstance()->IsMouseButtonPressed(0) ||
					Input::GetInstance()->IsMouseButtonPressed(1));
			player_->SetMirrorGuardMode(isMirrorGuarding);
			// 現在画面に映しているCameraの正面を渡し、WASDを画面基準の移動へ変換する
			Vector3 cameraForward{ 0.0f, 0.0f, 1.0f };
			if (Camera* activeCamera = cameraManager->GetActiveCamera()) {
				cameraForward = GetCameraForward(*activeCamera);
			}
			player_->Update(
				DirectXCommon::GetInstance()->GetDeltaTime(),
				stageSolidObbs_,
				cameraForward);
		}
	}
	UpdateGameplaySmoke(DirectXCommon::GetInstance()->GetDeltaTime());
	if (!isStageStartPlaying) {
		UpdateMirrorGameplay();
	}
	UpdateLightPuzzle(DirectXCommon::GetInstance()->GetDeltaTime());
	UpdateHazardLights(DirectXCommon::GetInstance()->GetDeltaTime());
	// 見た目だけの線ではなく、反射後の経路も周囲を照らすSpotLightへ反映します。
	UpdateLaserSpotLights();

	// ---------- カメラとデバッグ UI の更新 ----------
	if (ImGuiManager::GetInstance()->IsGameViewActive() && !isStageStartPlaying) {
		UpdateMainCamera();
		if (stageStart_ && stageStart_->IsCameraHandoffPlaying()) {
			// 通常Cameraの壁回避・追従結果へ、開始演出Cameraを滑らかに近づけます。
			stageStart_->UpdateCameraHandoff(
				DirectXCommon::GetInstance()->GetDeltaTime(),
				*mainCamera);
		}
		UpdateStageEvents();
		UpdateEventManualCamera();
	} else if (!activeEventCameraName_.empty()) {
		// Edit Viewではイベントカメラへ自動切替せず、編集用のMainCameraを維持する。
		cameraManager->SetActiveCamera("MainCamera");
		activeEventCameraName_.clear();
	}
	cameraManager->Update();
	UpdateReflectionCameras();
	ImGuiManager::GetInstance()->Begin("Stage1");
	// Gameplay中もPlayerの球Colliderを表示し、Laser接触を黄色で確認できるようにする
	if (player_) {
		ImGuiManager::GetInstance()->DrawPlayerCollisionDebug(
			player_->GetCollider(),
			cameraManager->GetActiveCamera(),
			player_->IsColliding(),
			isPlayerHitByLaser_ || isPlayerHitByHazardLight_);
	}
	DrawLightPuzzleDebugUi();
	if (ImGuiManager::GetInstance()->IsEditViewActive()) {
		DrawMirrorDebugUi();
		DrawCollisionDebugUi();
		const LevelEditorResult levelEditorResult =
			ImGuiManager::GetInstance()->LevelHotReloadWindow(
		autoStageMapReload_,
		stageMapHotReload_.GetFilePath(),
		stageMapReloadStatus_,
		stageMapData_.get(),
		selectedStageMapObjectIndex_);
		if (levelEditorResult.reloadRequested) {
			ReloadStageMap();
			stageMapHotReload_.Synchronize();
		} else {
			if (levelEditorResult.dataChanged) {
				if (ApplyStageMapData(false)) {
					stageMapReloadStatus_ = "Edited in memory. Press Save Map to keep it.";
				}
			}
			if (levelEditorResult.addSphereRequested) {
				if (AddStageMapSphere()) {
					SaveStageMap();
				}
			}
			if (levelEditorResult.addEventPairRequested) {
				if (AddStageMapEventPair()) {
					SaveStageMap();
				}
			}
			if (levelEditorResult.addCameraAreaRequested) {
				if (AddStageMapCameraArea()) {
					SaveStageMap();
				}
			}
			if (levelEditorResult.addPathSphereRequested) {
				if (AddStageMapPathSphere()) {
					SaveStageMap();
				}
			}
			if (levelEditorResult.removeSelectedRequested) {
				if (RemoveSelectedStageMapObject()) {
					SaveStageMap();
				}
			}
		if (levelEditorResult.saveRequested) {
				SaveStageMap();
			}
		}
		if (stageMapData_ && ImGuiManager::GetInstance()->StageLightingWindow(stageMapData_->lighting)) {
			// UIが変えたLevelDataを現在の共有Lightへ反映し、Save MapでJSONへ残せる状態にします。
			stageMapData_->hasLighting = true;
			if (ApplyStageMapData(false)) {
				stageMapReloadStatus_ = "Lighting edited in memory. Press Save Map to keep it.";
			}
		}
		DrawStageEditViewport();
		DrawStageModelShelf();
		HandleStageShelfDropOnEditView();
	}
	ImGuiManager::GetInstance()->End();

	// ---------- 3D オブジェクトの更新 ----------
	for (const auto& object : sceneObjects_) {
		UpdateObject(*object);
	}
	for (StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
		if (runtimeObject.visual) {
			UpdateObject(*runtimeObject.visual);
		}
	}
	for (const auto& fixedMirror : fixedMirrors_) {
		if (fixedMirror) {
			UpdateObject(fixedMirror->GetObject());
		}
	}
	if (mirrorFloor_) {
		UpdateObject(mirrorFloor_->GetObject());
	}
	if (carryableMirror_) {
		UpdateObject(carryableMirror_->GetObject());
	}
	if (player_) {
		UpdateObject(player_->GetObject());
	}
}

void Stage1::Draw()
{
	// ---------- 固定鏡の反射Textureを先に作成 ----------
	SrvManager::GetInstance()->PreDraw();
	DrawFixedMirrorReflections();

	// ---------- ゲーム画面への描画 ----------
	DirectXCommon::GetInstance()->PreDraw();

	object3dCommon->SetCommonDrawSetting();
	for (const auto& object : sceneObjects_) {
		object->Draw();
	}
	for (const StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
		if (runtimeObject.visual) {
			runtimeObject.visual->Draw();
		}
	}
	for (const auto& fixedMirror : fixedMirrors_) {
		if (!fixedMirror) {
			continue;
		}
		fixedMirror->DrawSurface(*cameraManager->GetActiveCamera());
		object3dCommon->SetCommonDrawSetting();
	}
	if (mirrorFloor_) {
		mirrorFloor_->DrawSurface(*cameraManager->GetActiveCamera());
		object3dCommon->SetCommonDrawSetting();
	}
	if (carryableMirror_) {
		carryableMirror_->GetObject().Draw();
	}
	if (player_) {
		player_->GetObject().Draw();
	}
	if (laserRenderer_ && cameraManager && cameraManager->GetActiveCamera()) {
		laserRenderer_->Draw(laser_.GetSegments(), *cameraManager->GetActiveCamera());
	}
	if (doorLaserRenderer_ && cameraManager && cameraManager->GetActiveCamera()) {
		doorLaserRenderer_->Draw(doorLaser_.GetSegments(), *cameraManager->GetActiveCamera());
	}
	if (ceilingSweepLightRenderer_ && cameraManager && cameraManager->GetActiveCamera()) {
		ceilingSweepLightRenderer_->Draw(ceilingSweepLightSegments_, *cameraManager->GetActiveCamera());
	}
	if (horizontalMoveLightRenderer_ && cameraManager && cameraManager->GetActiveCamera()) {
		horizontalMoveLightRenderer_->Draw(horizontalMoveLightSegments_, *cameraManager->GetActiveCamera());
	}
	if (bottomPulseLightRenderer_ && cameraManager && cameraManager->GetActiveCamera()) {
		bottomPulseLightRenderer_->Draw(bottomPulseLightSegments_, *cameraManager->GetActiveCamera());
	}
	if (bottomPulseWarningRenderer_ && cameraManager && cameraManager->GetActiveCamera()) {
		bottomPulseWarningRenderer_->Draw(bottomPulseWarningSegments_, *cameraManager->GetActiveCamera());
	}
	if (orbitLightRenderer_ && cameraManager && cameraManager->GetActiveCamera()) {
		orbitLightRenderer_->Draw(orbitLightSegments_, *cameraManager->GetActiveCamera());
	}

	//Post Effectが有効なときは、SceneのRenderTextureへ効果を適用してからGame Viewへ表示する
	const bool isPostEffectEnabled = PostEffect::GetInstance()->IsEnabled();
	if (isPostEffectEnabled) {
		if (PostEffect::GetInstance()->IsGaussianFilter()) {
			DirectXCommon::GetInstance()->PreDrawForGaussianHorizontalTexture();
			PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), true);
			DirectXCommon::GetInstance()->PreDrawForGaussianVerticalTexture();
			PostEffect::GetInstance()->DrawGaussianVertical(DirectXCommon::GetInstance()->GetGaussianBlurTextureSrvIndex());
		} else if (PostEffect::GetInstance()->IsDepthBasedOutline()) {
			PostEffect::GetInstance()->SetProjectionInverse(Inverse(cameraManager->GetActiveCamera()->GetProjectionMatrix()));
			DirectXCommon::GetInstance()->PreDrawForDepthBasedOutlineTexture();
			PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), true);
		} else {
			DirectXCommon::GetInstance()->PreDrawForPostEffectTexture();
			PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), true);
		}
	}

	DirectXCommon::GetInstance()->PreDrawForSwapChain(isPostEffectEnabled);
#ifndef USE_IMGUI
	// ImGuiを含まない構成では、RenderTextureのSceneを全画面三角形でSwapChainへコピーする。
	if (isPostEffectEnabled) {
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetPostEffectTextureSrvIndex(), false);
	} else {
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), false);
	}
#endif
	ImGuiManager::GetInstance()->Draw(DirectXCommon::GetInstance());
	DirectXCommon::GetInstance()->PostDraw();
}

std::unique_ptr<Object3d> Stage1::CreateObject(const std::string& modelName)
{
	// Object3d はモデル・カメラ・照明を受け取って、画面へ描画できるようになります。
	auto object = std::make_unique<Object3d>();
	object->Initialize(object3dCommon);
	object->SetModel(modelName);
	object->SetDirectionalLight(directionalLight_);
	object->SetPointLight(pointLight_);
	object->SetSpotLights(spotLights_);
	return object;
}

void Stage1::UpdateObject(Object3d& object)
{
	// Object3dは自分でSceneを知らないため、Stage1が現在のCameraと共有Lightを毎フレーム渡します。
	// 毎フレームのカメラ位置、照明、行列を Object3d へ反映します。
	object.SetCamera(cameraManager->GetActiveCamera());
	object.SetDirectionalLight(directionalLight_);
	object.SetPointLight(pointLight_);
	object.SetSpotLights(spotLights_);
	object.Update();
}

void Stage1::DrawFixedMirrorReflections()
{
	// 鏡は通常Cameraでそのまま描くのではなく、鏡ごとの反射Cameraで一度Textureへ描きます。
	std::vector<FixedMirror*> reflectionMirrors;
	reflectionMirrors.reserve(fixedMirrors_.size() + 1);
	for (const auto& fixedMirror : fixedMirrors_) {
		if (fixedMirror) {
			reflectionMirrors.push_back(fixedMirror.get());
		}
	}
	if (mirrorFloor_) {
		reflectionMirrors.push_back(mirrorFloor_.get());
	}
	if (reflectionMirrors.empty()) {
		return;
	}

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	// 鏡が増えても反射Sceneの再描画は一フレームに一枚だけ行います。
	const size_t updateCount = reflectionMirrors.size();
	for (size_t attempt = 0; attempt < updateCount; ++attempt) {
		const size_t mirrorIndex = reflectionUpdateCursor_ % updateCount;
		reflectionUpdateCursor_ = (reflectionUpdateCursor_ + 1) % updateCount;
		FixedMirror* fixedMirror = reflectionMirrors[mirrorIndex];
		if (!fixedMirror || !fixedMirror->IsReady()) {
			continue;
		}

		Camera& reflectionCamera = fixedMirror->GetReflectionCamera();
		fixedMirror->BeginReflection(dxCommon->GetDepthStencilViewHandle());
		object3dCommon->SetCommonDrawSetting();

		// 鏡面同士の無限反射は行わず、部屋・小型鏡・Playerだけを一度描画します。
		for (const auto& object : sceneObjects_) {
			object->UpdateCameraForDraw(&reflectionCamera);
			object->Draw();
		}
		for (StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
			if (!runtimeObject.visual) {
				continue;
			}
			runtimeObject.visual->UpdateCameraForDraw(&reflectionCamera);
			runtimeObject.visual->Draw();
		}
		if (carryableMirror_) {
			carryableMirror_->GetObject().UpdateCameraForDraw(&reflectionCamera);
			carryableMirror_->GetObject().Draw();
		}
		if (player_) {
			player_->GetObject().UpdateCameraForDraw(&reflectionCamera);
			player_->GetObject().Draw();
		}
		if (laserRenderer_) {
			laserRenderer_->Draw(laser_.GetSegments(), reflectionCamera);
		}
		if (doorLaserRenderer_) {
			doorLaserRenderer_->Draw(doorLaser_.GetSegments(), reflectionCamera);
		}
		if (ceilingSweepLightRenderer_) {
			ceilingSweepLightRenderer_->Draw(ceilingSweepLightSegments_, reflectionCamera);
		}
		if (horizontalMoveLightRenderer_) {
			horizontalMoveLightRenderer_->Draw(horizontalMoveLightSegments_, reflectionCamera);
		}
		if (bottomPulseLightRenderer_) {
			bottomPulseLightRenderer_->Draw(bottomPulseLightSegments_, reflectionCamera);
		}
		if (bottomPulseWarningRenderer_) {
			bottomPulseWarningRenderer_->Draw(bottomPulseWarningSegments_, reflectionCamera);
		}
		if (orbitLightRenderer_) {
			orbitLightRenderer_->Draw(orbitLightSegments_, reflectionCamera);
		}

		fixedMirror->EndReflection();
		RestoreSceneCameraMatrices();
		break;
	}
}

void Stage1::RestoreSceneCameraMatrices()
{
	// 反射Camera用に書き換えたObject3dの行列を、通常Game Camera用へ戻します。
	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (!activeCamera) {
		return;
	}
	for (const auto& object : sceneObjects_) {
		object->UpdateCameraForDraw(activeCamera);
	}
	for (StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
		if (runtimeObject.visual) {
			runtimeObject.visual->UpdateCameraForDraw(activeCamera);
		}
	}
	for (const auto& fixedMirror : fixedMirrors_) {
		if (fixedMirror) {
			fixedMirror->GetObject().UpdateCameraForDraw(activeCamera);
		}
	}
	if (mirrorFloor_) {
		mirrorFloor_->GetObject().UpdateCameraForDraw(activeCamera);
	}
	if (carryableMirror_) {
		carryableMirror_->GetObject().UpdateCameraForDraw(activeCamera);
	}
	if (player_) {
		player_->GetObject().UpdateCameraForDraw(activeCamera);
	}
}

void Stage1::UpdateReflectionCameras()
{
	Camera* sourceCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (!sourceCamera) {
		return;
	}

	const Vector3 sourceForward = GetCameraForward(*sourceCamera);
	for (const auto& fixedMirror : fixedMirrors_) {
		if (fixedMirror) {
			fixedMirror->UpdateReflectionCamera(*sourceCamera, sourceForward);
		}
	}
	if (mirrorFloor_) {
		mirrorFloor_->UpdateReflectionCamera(*sourceCamera, sourceForward);
	}
}

void Stage1::UpdateMirrorGameplay()
{
	// Eキー・Mouse入力をCarryableMirrorへ渡し、鏡のTransformとLaser反射経路を同じフレームで更新します。
	if (!player_ || !carryableMirror_) {
		return;
	}

	Input* input = Input::GetInstance();
	const Vector2 mouseScreen = input->GetMouseScreen();
	// ImGuiのInspectorやSliderを右クリックしても、Game View外ならMirror操作へ渡しません。
	const bool isMouseOverGameView =
		ImGuiManager::GetInstance()->IsGameViewActive() &&
		ImGuiManager::GetInstance()->IsMouseOverGameView(mouseScreen.x, mouseScreen.y);
	const bool interactPressed =
		ImGuiManager::GetInstance()->IsGameViewActive() &&
		input->TriggerKey(DIK_E);
	// 左クリックは縦向きの盾、右クリックの短押しは水平Mirrorの表裏切替に使用します。
	const bool isMirrorAiming =
		isMouseOverGameView &&
		carryableMirror_->IsCarried() &&
		input->IsMouseButtonPressed(0);
	const bool isHorizontalMirrorHeld =
		isMouseOverGameView &&
		carryableMirror_->IsCarried() &&
		input->IsMouseButtonPressed(1);
	const float mirrorAimMouseX = (isMirrorAiming || isHorizontalMirrorHeld)
		? static_cast<float>(input->GetMouseX())
		: 0.0f;
	// 右クリック中はMouse横移動で左右へ向け、Mouse縦移動で水平Mirrorを前後へ傾けます。
	const float mirrorAimMouseY = isHorizontalMirrorHeld
		? static_cast<float>(input->GetMouseY())
		: 0.0f;
	carryableMirror_->Update(
		DirectXCommon::GetInstance()->GetDeltaTime(),
		player_->GetPosition(),
		player_->GetFacingYaw(),
		interactPressed,
		isMirrorAiming,
		mirrorAimMouseX,
		isHorizontalMirrorHeld,
		mirrorAimMouseY);

	// Charge Laserも固定Mirror・携帯Mirror・鏡床の全てへ当たれば反射します。
	const std::vector<const Mirror*> chargeLaserMirrors = GetLaserReflectors();
	laser_.SetOrigin(laserOrigin_);
	laser_.SetDirection(laserDirection_);
	// Mirrorより先に床・壁・Doorへ当たったら、その地点でLightを止める。
	laser_.Update(
		chargeLaserMirrors,
		stageLightBlockingObbs_,
		laserVisualWidth_ * 0.5f);
	if (laserEmitter_) {
		// ImGuiで動かした発射装置の見た目も、計算に使用するOriginと同じ位置へ置く。
		laserEmitter_->SetTranslate(laserOrigin_);
	}

	// 鏡で分割された各Laser線分と、Playerの球Colliderを3D空間で判定する
	isPlayerHitByLaser_ = false;
	const Sphere playerSphere = player_->GetCollider();
	for (const LaserSegment& segment : laser_.GetSegments()) {
		if (Collision::SegmentSphere(
			segment.start,
			segment.end,
			playerSphere,
			laserCollisionRadius_).isHit) {
			isPlayerHitByLaser_ = true;
			break;
		}
	}
}

std::vector<const Mirror*> Stage1::GetLaserReflectors() const
{
	std::vector<const Mirror*> mirrors;
	mirrors.reserve(fixedMirrors_.size() + 2);
	for (const auto& fixedMirror : fixedMirrors_) {
		if (fixedMirror) {
			mirrors.push_back(&fixedMirror->GetMirror());
		}
	}
	if (mirrorFloor_) {
		mirrors.push_back(&mirrorFloor_->GetMirror());
	}
	if (carryableMirror_) {
		mirrors.push_back(&carryableMirror_->GetMirror());
	}
	return mirrors;
}

std::vector<LaserSegment> Stage1::ReflectHazardLightSegments(
	const std::vector<LaserSegment>& sourceSegments) const
{
	std::vector<LaserSegment> reflectedSegments;
	const std::vector<const Mirror*> mirrors = GetLaserReflectors();
	for (const LaserSegment& sourceSegment : sourceSegments) {
		const Vector3 direction{
			sourceSegment.end.x - sourceSegment.start.x,
			sourceSegment.end.y - sourceSegment.start.y,
			sourceSegment.end.z - sourceSegment.start.z,
		};
		const float distance = Length(direction);
		if (distance <= 0.0001f) {
			continue;
		}

		// 危険Lightも通常Laserと同じ経路計算を使い、最大3回まで反射させます。
		Laser reflectedLight;
		reflectedLight.SetOrigin(sourceSegment.start);
		reflectedLight.SetDirection(direction);
		reflectedLight.SetMaxDistance(distance);
		reflectedLight.SetMaxReflectionCount(3);
		// 危険Lightも、反射先を求める前に床・壁・Doorで遮られます。
		reflectedLight.Update(mirrors, stageLightBlockingObbs_, 0.06f);
		const std::vector<LaserSegment>& segments = reflectedLight.GetSegments();
		reflectedSegments.insert(reflectedSegments.end(), segments.begin(), segments.end());
	}
	return reflectedSegments;
}

void Stage1::UpdateLightPuzzle(float deltaTime)
{
	// LaserがSwitchへ当たっている時間だけ充電し、0〜1の進行度をDoorと大型Mirrorへ使います。
	if (!carryableMirror_ || !chargeSwitch_ || !doorSwitch_ || !lightDoor_ ||
		fixedMirrors_.empty() || !fixedMirrors_.front()) {
		return;
	}

	const std::vector<LaserSegment>& segments = laser_.GetSegments();
	// ---------- 第1段階: 携帯鏡の反射光で大型Mirrorを充電 ----------
	isChargeSwitchReceivingLight_ = false;
	const Sphere chargeSwitchSphere{ chargeSwitchPosition_, chargeSwitchRadius_ };
	for (const LaserSegment& segment : segments) {
		// Switchは直射光でも反射光でも、光線が届けば反応します。
		if (Collision::SegmentSphere(
			segment.start,
			segment.end,
			chargeSwitchSphere,
			laserCollisionRadius_).isHit) {
			isChargeSwitchReceivingLight_ = true;
			break;
		}
	}
	if (!isLargeMirrorCharged_) {
		// 反射光を当て続けると蓄積し、外れると徐々に減る充電式Switchです。
		const float chargeTarget = isChargeSwitchReceivingLight_ ? 1.0f : 0.0f;
		const float chargeSpeed = isChargeSwitchReceivingLight_ ? 6.0f : 2.0f;
		const float chargeRate = (std::min)(chargeSpeed * (std::max)(deltaTime, 0.0f), 1.0f);
		mirrorCharge_ += (chargeTarget - mirrorCharge_) * chargeRate;
		if (mirrorCharge_ >= 0.98f) {
			mirrorCharge_ = 1.0f;
			isLargeMirrorCharged_ = true;
		}
	}

	// 充電完了後、大型MirrorをY軸へ横方向に振り、反射先をDoor Switchへ変えます。
	const float mirrorRotationTarget = isLargeMirrorCharged_ ? 1.0f : 0.0f;
	const float mirrorRotationRate = (std::min)(1.8f * (std::max)(deltaTime, 0.0f), 1.0f);
	largeMirrorRotationAmount_ +=
		(mirrorRotationTarget - largeMirrorRotationAmount_) * mirrorRotationRate;
	FixedMirror& largeMirror = *fixedMirrors_.front();
	largeMirror.SetPitch(0.0f);
	largeMirror.GetYawForEdit() =
		largeMirrorBaseYaw_ +
		largeMirrorTargetYawOffset_ * largeMirrorRotationAmount_;
	largeMirror.SyncVisualAndCollider();

	// ---------- 第2段階: 横向き大型Mirrorの反射光でDoorを開閉 ----------
	isDoorSwitchReceivingLight_ = false;
	// Door Laserも、他のLightと同じ全Mirrorへ反射するルールを使います。
	const std::vector<const Mirror*> doorLaserMirrors = GetLaserReflectors();
	doorLaser_.SetOrigin(doorLaserOrigin_);
	doorLaser_.SetDirection(doorLaserDirection_);
	// Door用Lightも、壁越しにSwitchへ届かないよう同じ遮蔽判定を使う。
	doorLaser_.Update(
		doorLaserMirrors,
		stageLightBlockingObbs_,
		laserVisualWidth_ * 0.5f);
	if (doorLaserEmitter_) {
		doorLaserEmitter_->SetTranslate(doorLaserOrigin_);
	}
	const std::vector<LaserSegment>& doorSegments = doorLaser_.GetSegments();
	// Door用Lightも通常Laserと同じ危険物なので、反射後の線分を含めてPlayerへ判定します。
	if (player_) {
		const Sphere playerSphere = player_->GetCollider();
		for (const LaserSegment& segment : doorSegments) {
			if (Collision::SegmentSphere(
				segment.start,
				segment.end,
				playerSphere,
				laserCollisionRadius_).isHit) {
				isPlayerHitByLaser_ = true;
				break;
			}
		}
	}
	const Sphere doorSwitchSphere{ doorSwitchPosition_, doorSwitchRadius_ };
	for (const LaserSegment& segment : doorSegments) {
		// Door Switchも、直射光・大型Mirrorでの反射光のどちらでも反応します。
		if (Collision::SegmentSphere(
			segment.start,
			segment.end,
			doorSwitchSphere,
			laserCollisionRadius_).isHit) {
			isDoorSwitchReceivingLight_ = true;
			break;
		}
	}

	// Doorは大型Mirrorの反射光が当たっている間だけ、滑らかに開きます。
	const float doorTarget = isDoorSwitchReceivingLight_ ? 1.0f : 0.0f;
	const float doorRate = (std::min)(3.0f * (std::max)(deltaTime, 0.0f), 1.0f);
	doorOpenAmount_ += (doorTarget - doorOpenAmount_) * doorRate;
	lightDoor_->SetTranslate({
		doorClosedPosition_.x,
		doorClosedPosition_.y + doorOpenHeight_ * doorOpenAmount_,
		doorClosedPosition_.z,
	});
	doorCollider_ = Collision::MakeOBB(
		lightDoor_->GetTransform(),
		doorColliderLocalHalfSize_);

	// 二つのSwitchは、充電量または受光中に少し大きくして画面上でも見分けられるようにする。
	const float chargeSwitchScale = chargeSwitchRadius_ * 2.0f * (1.0f + mirrorCharge_ * 0.35f);
	chargeSwitch_->SetTranslate(chargeSwitchPosition_);
	chargeSwitch_->SetScale({ chargeSwitchScale, chargeSwitchScale, chargeSwitchScale });
	const float doorSwitchScale = doorSwitchRadius_ * 2.0f * (isDoorSwitchReceivingLight_ ? 1.20f : 1.0f);
	doorSwitch_->SetTranslate(doorSwitchPosition_);
	doorSwitch_->SetScale({ doorSwitchScale, doorSwitchScale, doorSwitchScale });
}

void Stage1::UpdateHazardLights(float deltaTime)
{
	// 危険Lightの位置は毎フレーム時刻から計算します。経路を保存せず、同じ時刻なら同じ位置になります。
	const float safeDeltaTime = (std::max)(deltaTime, 0.0f);
	hazardLightTime_ += safeDeltaTime;
	const auto easeInOutSine = [](float progress) {
		const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
		return -(std::cos(clampedProgress * std::numbers::pi_v<float>) - 1.0f) * 0.5f;
	};
	const auto easeOutSine = [](float progress) {
		const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
		return std::sin(clampedProgress * std::numbers::pi_v<float> * 0.5f);
	};
	const auto easeInSine = [](float progress) {
		const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
		return 1.0f - std::cos(clampedProgress * std::numbers::pi_v<float> * 0.5f);
	};

	// ---------- 1. 上の始点を固定し、床へ当たる先端だけを左右へ振るLight ----------
	const float ceilingEndOffset = std::sin(hazardLightTime_ * 1.30f) * ceilingSweepDistance_;
	ceilingSweepLightSegments_ = {
		{
			ceilingSweepStart_,
			{ ceilingSweepStart_.x + ceilingEndOffset, -4.00f, ceilingSweepStart_.z },
			false,
			0,
		},
	};

	// ---------- 2. 横一直線のLightを、三秒停止してから奥・手前へ往復させる ----------
	const float horizontalCycleTime = std::fmod(hazardLightTime_, 10.0f);
	float horizontalMoveProgress = 0.0f;
	if (horizontalCycleTime < 3.0f) {
		horizontalMoveProgress = 0.0f;
	} else if (horizontalCycleTime < 5.0f) {
		horizontalMoveProgress = easeInOutSine((horizontalCycleTime - 3.0f) / 2.0f);
	} else if (horizontalCycleTime < 8.0f) {
		horizontalMoveProgress = 1.0f;
	} else {
		horizontalMoveProgress = 1.0f - easeInOutSine((horizontalCycleTime - 8.0f) / 2.0f);
	}
	const Vector3 horizontalStart{
		horizontalMoveNearStart_.x + (horizontalMoveFarStart_.x - horizontalMoveNearStart_.x) * horizontalMoveProgress,
		horizontalMoveNearStart_.y + (horizontalMoveFarStart_.y - horizontalMoveNearStart_.y) * horizontalMoveProgress,
		horizontalMoveNearStart_.z + (horizontalMoveFarStart_.z - horizontalMoveNearStart_.z) * horizontalMoveProgress,
	};
	horizontalMoveLightSegments_ = {
		{
			horizontalStart,
			{ horizontalStart.x - 14.0f, horizontalStart.y, horizontalStart.z },
			false,
			0,
		},
	};

	// ---------- 3. 下から出るLightは五秒表示・十秒停止、出現三秒前だけ床を赤く予告 ----------
	const float bottomPulseCycleTime = std::fmod(hazardLightTime_, 15.0f);
	const bool isBottomPulseActive = bottomPulseCycleTime < 5.0f;
	const bool isBottomPulseWarning = bottomPulseCycleTime >= 12.0f;
	bottomPulseLightSegments_.clear();
	bottomPulseWarningSegments_.clear();
	if (isBottomPulseActive) {
		bottomPulseLightSegments_.push_back({
			bottomPulsePosition_,
			{ bottomPulsePosition_.x, 4.50f, bottomPulsePosition_.z },
			false,
			0,
		});
	}
	if (isBottomPulseWarning) {
		// 床上の太い赤線を、Lightが出る危険範囲として三秒間だけ表示します。
		bottomPulseWarningSegments_.push_back({
			{ bottomPulsePosition_.x - 1.40f, -1.96f, bottomPulsePosition_.z },
			{ bottomPulsePosition_.x + 1.40f, -1.96f, bottomPulsePosition_.z },
			false,
			0,
		});
	}

	// ---------- 4. 三本の上向きLightを、回転しながら広げたり閉じたりさせる ----------
	const float orbitCycleProgress = std::fmod(hazardLightTime_, 4.0f) / 4.0f;
	const bool isOrbitClosing = orbitCycleProgress >= 0.5f;
	const float orbitRadiusProgress = isOrbitClosing
		? 1.0f - easeInSine((orbitCycleProgress - 0.5f) * 2.0f)
		: easeOutSine(orbitCycleProgress * 2.0f);
	const float orbitRadius = 1.20f + (5.00f - 1.20f) * orbitRadiusProgress;
	// 回転Lightもほかの危険Lightと同じ太さにし、閉じる時だけ少し細くします。
	const float orbitBeamWidth = isOrbitClosing
		? 0.38f + (0.74f - 0.38f) * orbitRadiusProgress
		: 0.74f;
	if (orbitLightRenderer_) {
		// 閉じるほど細くして、Light全体が小さくなったように見せます。
		orbitLightRenderer_->SetBeamWidth(orbitBeamWidth);
	}
	orbitLightSegments_.clear();
	const float orbitBaseAngle = hazardLightTime_ * 1.80f;
	for (int lightIndex = 0; lightIndex < 3; ++lightIndex) {
		const float angle = orbitBaseAngle + std::numbers::pi_v<float> * 2.0f * static_cast<float>(lightIndex) / 3.0f;
		const Vector3 orbitPosition{
			orbitLightCenter_.x + std::cos(angle) * orbitRadius,
			4.50f,
			orbitLightCenter_.z + std::sin(angle) * orbitRadius,
		};
		orbitLightSegments_.push_back({
			orbitPosition,
			{ orbitPosition.x, -4.00f, orbitPosition.z },
			false,
			0,
		});
	}

	// 4色の危険Lightも、固定Mirror・持てるMirror・鏡床で反射する経路へ変換します。
	ceilingSweepLightSegments_ = ReflectHazardLightSegments(ceilingSweepLightSegments_);
	horizontalMoveLightSegments_ = ReflectHazardLightSegments(horizontalMoveLightSegments_);
	bottomPulseLightSegments_ = ReflectHazardLightSegments(bottomPulseLightSegments_);
	orbitLightSegments_ = ReflectHazardLightSegments(orbitLightSegments_);

	// 有効な危険LightだけPlayerの球Colliderと判定し、既存の赤いHit Debug表示へ渡します。
	isPlayerHitByHazardLight_ = false;
	if (player_) {
		const Sphere playerSphere = player_->GetCollider();
		const auto isHitBySegments = [&](const std::vector<LaserSegment>& segments) {
			for (const LaserSegment& segment : segments) {
				if (Collision::SegmentSphere(segment.start, segment.end, playerSphere, 0.16f).isHit) {
					return true;
				}
			}
			return false;
		};
		isPlayerHitByHazardLight_ =
			isHitBySegments(ceilingSweepLightSegments_) ||
			isHitBySegments(horizontalMoveLightSegments_) ||
			isHitBySegments(bottomPulseLightSegments_) ||
			isHitBySegments(orbitLightSegments_);
	}
}

void Stage1::UpdateLaserSpotLights()
{
	// 見えるLaser線分と別にSpotLightを作り、光線が近くの壁・床を実際に照らすようにします。
	// 前フレームのLightが残らないよう、まず全要素を無効化します。
	spotLights_.fill({});
	size_t lightIndex = 0;
	// 最初にJSONで決めたキー・フィル・バックライトを入れ、残りの枠をLaser用に使います。
	for (const Object3d::SpotLight& stageSpotLight : stageSpotLights_) {
		if (lightIndex >= spotLights_.size()) {
			break;
		}
		spotLights_[lightIndex++] = stageSpotLight;
	}
	const auto addSegmentsAsSpotLights =
		[this, &lightIndex](
			const std::vector<LaserSegment>& segments,
			const Vector4& color,
			float intensity)
	{
		for (const LaserSegment& segment : segments) {
			if (lightIndex >= spotLights_.size()) {
				return;
			}
			const Vector3 segmentDirection{
				segment.end.x - segment.start.x,
				segment.end.y - segment.start.y,
				segment.end.z - segment.start.z,
			};
			const float segmentLength = Length(segmentDirection);
			if (segmentLength <= 0.05f) {
				continue;
			}

			// Laserの始点・方向・遮蔽物までの長さを、そのまま円錐Lightへ使います。
			Object3d::SpotLight& spotLight = spotLights_[lightIndex++];
			spotLight.color = color;
			spotLight.position = segment.start;
			spotLight.direction = Normalize(segmentDirection);
			spotLight.intensity = intensity;
			spotLight.distance = segmentLength + 0.35f;
			spotLight.decay = 1.20f;
			spotLight.cosAngle = 0.82f;
			spotLight.cosFalloffStart = 0.96f;
		}
	};

	// 通常のPuzzle Light、Door Light、四種類の危険Lightを同じ仕組みで照明へ反映します。
	addSegmentsAsSpotLights(laser_.GetSegments(), { 0.05f, 0.95f, 1.00f, 1.0f }, 3.5f);
	addSegmentsAsSpotLights(doorLaser_.GetSegments(), { 1.00f, 0.38f, 0.05f, 1.0f }, 3.5f);
	addSegmentsAsSpotLights(ceilingSweepLightSegments_, { 0.95f, 0.20f, 1.00f, 1.0f }, 3.2f);
	addSegmentsAsSpotLights(horizontalMoveLightSegments_, { 1.00f, 0.82f, 0.10f, 1.0f }, 3.0f);
	addSegmentsAsSpotLights(bottomPulseLightSegments_, { 0.15f, 0.55f, 1.00f, 1.0f }, 3.4f);
	addSegmentsAsSpotLights(orbitLightSegments_, { 0.20f, 1.00f, 0.35f, 1.0f }, 2.8f);
}

void Stage1::InitializeGameplaySmoke()
{
	gameplaySmokeEnabled_ = IsEnvironmentEnabled("CG2_STAGE1_GAMEPLAY_SMOKE");
	if (!gameplaySmokeEnabled_ || !player_ || !carryableMirror_) {
		return;
	}

	gameplaySmokeStartY_ = player_->GetPosition().y;
	const Vector3 mirrorPosition = carryableMirror_->GetMirror().GetCenter();

	// 鏡の中心にPlayerがいる条件を渡し、Eキーと同じ拾う処理を直接確認します。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, true);
	gameplaySmokePickedUpMirror_ = carryableMirror_->IsCarried();
	// 持った状態のままPlayer正面へ移動させてから、反射判定を行います。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, false);

	// 携帯中の鏡へ正面から光を当て、衝突後に反射線分が作られることを確認します。
	const Mirror& carryMirror = carryableMirror_->GetMirror();
	const Vector3 mirrorNormal = carryMirror.GetNormal();
	Laser carryMirrorProbe;
	carryMirrorProbe.SetOrigin({
		carryMirror.GetCenter().x + mirrorNormal.x * 3.0f,
		carryMirror.GetCenter().y + mirrorNormal.y * 3.0f,
		carryMirror.GetCenter().z + mirrorNormal.z * 3.0f,
	});
	carryMirrorProbe.SetDirection({ -mirrorNormal.x, -mirrorNormal.y, -mirrorNormal.z });
	carryMirrorProbe.SetMaxDistance(8.0f);
	carryMirrorProbe.SetMaxReflectionCount(1);
	carryMirrorProbe.Update({ &carryMirror });
	const std::vector<LaserSegment>& probeSegments = carryMirrorProbe.GetSegments();
	gameplaySmokeCarryMirrorReflectedLaser_ =
		probeSegments.size() >= 2 && probeSegments.front().hitMirror;

	// 表側とは逆から当てたLaserは、携帯Mirrorでは反射しないことを確認します。
	Laser carryMirrorBackfaceProbe;
	carryMirrorBackfaceProbe.SetOrigin({
		carryMirror.GetCenter().x - mirrorNormal.x * 3.0f,
		carryMirror.GetCenter().y - mirrorNormal.y * 3.0f,
		carryMirror.GetCenter().z - mirrorNormal.z * 3.0f,
	});
	carryMirrorBackfaceProbe.SetDirection(mirrorNormal);
	carryMirrorBackfaceProbe.SetMaxDistance(8.0f);
	carryMirrorBackfaceProbe.SetMaxReflectionCount(1);
	carryMirrorBackfaceProbe.Update({ &carryMirror });
	gameplaySmokeCarryMirrorBackfaceIgnored_ =
		carryMirrorBackfaceProbe.GetSegments().size() == 1 &&
		!carryMirrorBackfaceProbe.GetSegments().front().hitMirror &&
		Length({
			carryMirrorBackfaceProbe.GetSegments().front().end.x - carryMirrorBackfaceProbe.GetSegments().front().start.x,
			carryMirrorBackfaceProbe.GetSegments().front().end.y - carryMirrorBackfaceProbe.GetSegments().front().start.y,
			carryMirrorBackfaceProbe.GetSegments().front().end.z - carryMirrorBackfaceProbe.GetSegments().front().start.z,
		}) < 3.10f;

	// 短い右クリックを離す操作を二回行い、上向き・下向きの水平Mirrorになることを確認します。
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, true, 0.0f);
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	const bool isHorizontalMirrorFacingUp = carryableMirror_->GetMirror().GetNormal().y > 0.90f;
	const Vector3 horizontalMirrorOffset{
		carryableMirror_->GetMirror().GetCenter().x - mirrorPosition.x,
		carryableMirror_->GetMirror().GetCenter().y - mirrorPosition.y,
		carryableMirror_->GetMirror().GetCenter().z - mirrorPosition.z,
	};
	const bool isHorizontalMirrorFurtherForward =
		std::sqrt(horizontalMirrorOffset.x * horizontalMirrorOffset.x +
			horizontalMirrorOffset.z * horizontalMirrorOffset.z) > 2.50f;
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, true, 0.0f);
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	const bool isHorizontalMirrorFacingDown = carryableMirror_->GetMirror().GetNormal().y < -0.90f;
	// 下向きの水平Mirrorを前方へ傾けると、真下からのLaserがPlayer正面へ進むことを確認します。
	carryableMirror_->Update(0.50f, mirrorPosition, 0.0f, false, false, 0.0f, true, 100.0f);
	Laser carryMirrorTiltProbe;
	const Mirror& tiltedCarryMirror = carryableMirror_->GetMirror();
	carryMirrorTiltProbe.SetOrigin({
		tiltedCarryMirror.GetCenter().x,
		tiltedCarryMirror.GetCenter().y - 3.0f,
		tiltedCarryMirror.GetCenter().z,
	});
	carryMirrorTiltProbe.SetDirection({ 0.0f, 1.0f, 0.0f });
	carryMirrorTiltProbe.SetMaxDistance(8.0f);
	carryMirrorTiltProbe.SetMaxReflectionCount(1);
	carryMirrorTiltProbe.Update({ &tiltedCarryMirror });
	const std::vector<LaserSegment>& tiltedProbeSegments = carryMirrorTiltProbe.GetSegments();
	gameplaySmokeCarryMirrorTiltRedirectsLaser_ =
		tiltedProbeSegments.size() >= 2 &&
		tiltedProbeSegments.front().hitMirror &&
		tiltedProbeSegments[1].end.z - tiltedProbeSegments[1].start.z > 0.50f;
	// 長押しを離しても、短押し扱いになって表裏が切り替わらないことを確認します。
	carryableMirror_->Update(0.01f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	// 左クリックと同じ操作で、水平状態から通常の縦向きMirrorへ戻します。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, false, true, 0.0f);
	// 長押しは鏡を反転させず、構えるための操作だけとして扱います。
	carryableMirror_->Update(0.30f, mirrorPosition, 0.0f, false, false, 0.0f, true, 0.0f);
	carryableMirror_->Update(0.01f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	const bool doesLongRightMouseHoldKeepVertical =
		std::abs(carryableMirror_->GetMirror().GetNormal().y) < 0.01f;
	gameplaySmokeCarryMirrorHorizontalControl_ =
		isHorizontalMirrorFacingUp &&
		isHorizontalMirrorFacingDown &&
		isHorizontalMirrorFurtherForward &&
		doesLongRightMouseHoldKeepVertical;

	// 鏡床は上から下へ向かうLightを、下から上へ反射することを確認します。
	if (mirrorFloor_) {
		Laser mirrorFloorProbe;
		const Mirror& mirrorFloor = mirrorFloor_->GetMirror();
		mirrorFloorProbe.SetOrigin({
			mirrorFloor.GetCenter().x,
			mirrorFloor.GetCenter().y + 3.0f,
			mirrorFloor.GetCenter().z,
		});
		mirrorFloorProbe.SetDirection({ 0.0f, -1.0f, 0.0f });
		mirrorFloorProbe.SetMaxDistance(8.0f);
		mirrorFloorProbe.SetMaxReflectionCount(1);
		mirrorFloorProbe.Update({ &mirrorFloor });
		Laser mirrorFloorBackfaceProbe;
		mirrorFloorBackfaceProbe.SetOrigin({
			mirrorFloor.GetCenter().x,
			mirrorFloor.GetCenter().y - 3.0f,
			mirrorFloor.GetCenter().z,
		});
		mirrorFloorBackfaceProbe.SetDirection({ 0.0f, 1.0f, 0.0f });
		mirrorFloorBackfaceProbe.SetMaxDistance(8.0f);
		mirrorFloorBackfaceProbe.SetMaxReflectionCount(1);
		mirrorFloorBackfaceProbe.Update({ &mirrorFloor });
		gameplaySmokeMirrorFloorReflectsLaser_ =
			mirrorFloorProbe.GetSegments().size() >= 2 &&
			mirrorFloorProbe.GetSegments().front().hitMirror &&
			mirrorFloorProbe.GetSegments()[1].end.y > mirrorFloorProbe.GetSegments()[1].start.y &&
			mirrorFloorBackfaceProbe.GetSegments().size() >= 2 &&
			mirrorFloorBackfaceProbe.GetSegments().front().hitMirror &&
			mirrorFloorBackfaceProbe.GetSegments()[1].end.y < mirrorFloorBackfaceProbe.GetSegments()[1].start.y;
	}

	// 斜め回転した大型MirrorがDoor Switchへ光を送れることを確認します。
	if (!fixedMirrors_.empty() && fixedMirrors_.front()) {
		const Mirror diagonalDoorMirror(
			fixedMirrors_.front()->GetMirror().GetCenter(),
			{ std::sin(largeMirrorBaseYaw_ + largeMirrorTargetYawOffset_), 0.0f,
				std::cos(largeMirrorBaseYaw_ + largeMirrorTargetYawOffset_) },
			fixedMirrors_.front()->GetMirror().GetWidth(),
			fixedMirrors_.front()->GetMirror().GetHeight());
		Laser diagonalDoorLaser;
		diagonalDoorLaser.SetOrigin(doorLaserOrigin_);
		diagonalDoorLaser.SetDirection(doorLaserDirection_);
		diagonalDoorLaser.SetMaxDistance(20.0f);
		diagonalDoorLaser.SetMaxReflectionCount(1);
		diagonalDoorLaser.Update({ &diagonalDoorMirror });
		const Sphere diagonalDoorSwitch{ doorSwitchPosition_, doorSwitchRadius_ };
		gameplaySmokeDoorDiagonalReflection_ = std::any_of(
			diagonalDoorLaser.GetSegments().begin(),
			diagonalDoorLaser.GetSegments().end(),
			[&](const LaserSegment& segment)
			{
				return Collision::SegmentSphere(
					segment.start,
					segment.end,
					diagonalDoorSwitch,
					laserCollisionRadius_).isHit;
			});
	}

	const auto laserHitsSphere = [&](const Laser& testLaser, const Sphere& sphere) {
		return std::any_of(
			testLaser.GetSegments().begin(),
			testLaser.GetSegments().end(),
			[&](const LaserSegment& segment)
			{
				return Collision::SegmentSphere(
					segment.start,
					segment.end,
					sphere,
					laserCollisionRadius_).isHit;
			});
	};
	// 実際に持っている鏡をPlayer正面へ構え、鏡なしなら当たる光が遮られることを確認する。
	const Sphere carriedPlayerSphere{ mirrorPosition, 1.2f };
	const Vector3 carriedTestOrigin{
		mirrorPosition.x,
		mirrorPosition.y + 1.8f,
		mirrorPosition.z + 2.5f,
	};
	const Vector3 carriedTestDirection{ 0.0f, -1.8f, -2.5f };
	Laser carriedUnblockedLaser;
	carriedUnblockedLaser.SetOrigin(carriedTestOrigin);
	carriedUnblockedLaser.SetDirection(carriedTestDirection);
	carriedUnblockedLaser.SetMaxDistance(12.0f);
	carriedUnblockedLaser.Update({});
	Laser carriedBlockedLaser;
	carriedBlockedLaser.SetOrigin(carriedTestOrigin);
	carriedBlockedLaser.SetDirection(carriedTestDirection);
	carriedBlockedLaser.SetMaxDistance(12.0f);
	carriedBlockedLaser.SetMaxReflectionCount(1);
	carriedBlockedLaser.Update({ &carryMirror });
	gameplaySmokeCarriedMirrorBlocksPlayer_ =
		laserHitsSphere(carriedUnblockedLaser, carriedPlayerSphere) &&
		carriedBlockedLaser.GetSegments().size() >= 2 &&
		!laserHitsSphere(carriedBlockedLaser, carriedPlayerSphere);
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, true);
	gameplaySmokeDroppedMirror_ = !carryableMirror_->IsCarried();

	// 鏡がない時はPlayerへ届き、途中に鏡がある時は入射線が鏡で止まることを確認します。
	const Sphere testPlayerSphere{ { 0.0f, -0.8f, 5.0f }, 1.2f };
	Laser unblockedLaser;
	unblockedLaser.SetOrigin(laserOrigin_);
	unblockedLaser.SetDirection(laserDirection_);
	unblockedLaser.SetMaxDistance(30.0f);
	unblockedLaser.Update({});
	gameplaySmokeLaserHitsPlayer_ = laserHitsSphere(unblockedLaser, testPlayerSphere);

	const Vector3 normalizedLaserDirection = Normalize(laserDirection_);
	const Mirror shieldMirror(
		{
			laserOrigin_.x + normalizedLaserDirection.x * 1.2f,
			laserOrigin_.y + normalizedLaserDirection.y * 1.2f,
			laserOrigin_.z + normalizedLaserDirection.z * 1.2f,
		},
		Multiply(-1.0f, normalizedLaserDirection),
		3.0f,
		3.0f);
	Laser blockedLaser;
	blockedLaser.SetOrigin(laserOrigin_);
	blockedLaser.SetDirection(laserDirection_);
	blockedLaser.SetMaxDistance(30.0f);
	blockedLaser.SetMaxReflectionCount(1);
	blockedLaser.Update({ &shieldMirror });
	gameplaySmokeMirrorBlocksPlayer_ =
		blockedLaser.GetSegments().size() >= 2 &&
		!laserHitsSphere(blockedLaser, testPlayerSphere);

	// 描画用Cameraとは別の小さなControllerを作り、段階操作と壁制限を確認します。
	Camera testCamera;
	CameraController testController;
	testController.Initialize(&testCamera, { 0.0f, 0.0f, 0.0f });
	testController.SetAutoRecenterEnabled(false);

	// 左一段目のCamera候補へ壁を置き、壁側へ回れないことを確認します。
	constexpr float testYaw = 0.52359878f;
	const float testPitch = testController.GetOrbitPitch();
	const float testDistance = testController.GetDistance();
	const Vector3 testFocus = testController.GetFocus();
	const float horizontalDistance = testDistance * std::cos(testPitch);
	const Vector3 blockedCameraPosition{
		testFocus.x - std::sin(testYaw) * horizontalDistance,
		testFocus.y + std::sin(testPitch) * testDistance,
		testFocus.z - std::cos(testYaw) * horizontalDistance,
	};
	OBB testWall{};
	testWall.center = Lerp(testFocus, blockedCameraPosition, 0.5f);
	testWall.orientations[0] = { 1.0f, 0.0f, 0.0f };
	testWall.orientations[1] = { 0.0f, 1.0f, 0.0f };
	testWall.orientations[2] = { 0.0f, 0.0f, 1.0f };
	testWall.size = { 0.75f, 0.75f, 0.75f };
	gameplaySmokeCameraWallBlock_ =
		!testController.TryStepOrbit(1, { testWall }) &&
		testController.GetOrbitStepIndex() == 0;

	// 一段目の直後も現在角度が目標へ瞬間移動せず、途中にあることを確認します。
	const float yawBeforeStep = testController.GetOrbitYaw();
	const bool firstOrbitStep = testController.TryStepOrbit(1, {});
	testController.Update(
		1.0f / 60.0f,
		{ 0.0f, 0.0f, 0.0f },
		{},
		true,
		{});
	const float yawAfterOneFrame = testController.GetOrbitYaw();
	gameplaySmokeCameraSmooth_ =
		firstOrbitStep &&
		yawAfterOneFrame > yawBeforeStep + 0.0001f &&
		yawAfterOneFrame < testYaw - 0.0001f;

	// PlayerがCamera側へ後退しても、Dead Zoneを越えたFocusがすぐ追いつくことを確認します。
	Camera backwardTestCamera;
	CameraController backwardTestController;
	backwardTestController.Initialize(&backwardTestCamera, { 0.0f, 0.0f, 0.0f });
	backwardTestController.SetAutoRecenterEnabled(false);
	backwardTestController.Update(
		0.25f,
		{ 0.0f, 0.0f, -8.0f },
		{ 0.0f, 0.0f, -1.0f },
		false,
		{});
	gameplaySmokeCameraBacktracks_ = backwardTestController.GetFocus().z < -5.0f;

	// 左右を十二回押すと、一回30度のまま360度回り切れることを確認します。
	Camera fullOrbitCamera;
	CameraController fullOrbitController;
	fullOrbitController.Initialize(&fullOrbitCamera, { 0.0f, 0.0f, 0.0f });
	fullOrbitController.SetAutoRecenterEnabled(false);
	bool canCompleteFullOrbit = true;
	for (int stepIndex = 0; stepIndex < 12; ++stepIndex) {
		canCompleteFullOrbit &= fullOrbitController.TryStepOrbit(1, {});
	}
	const bool canReturnFromFullOrbit =
		fullOrbitController.TryStepOrbit(-1, {}) &&
		fullOrbitController.GetOrbitStepIndex() == 11;
	const bool distanceReachedFarLimit =
		testController.TryStepDistance(1, {}) &&
		!testController.TryStepDistance(1, {}) &&
		testController.GetDistanceStepIndex() == 3;
	const bool distanceReachedNearLimit =
		testController.TryStepDistance(-1, {}) &&
		testController.TryStepDistance(-1, {}) &&
		testController.TryStepDistance(-1, {}) &&
		!testController.TryStepDistance(-1, {}) &&
		testController.GetDistanceStepIndex() == 0;
	gameplaySmokeCameraSteps_ =
		canCompleteFullOrbit &&
		canReturnFromFullOrbit &&
		distanceReachedFarLimit &&
		distanceReachedNearLimit;
}

void Stage1::UpdateGameplaySmoke(float deltaTime)
{
	if (!gameplaySmokeEnabled_ || !player_) {
		return;
	}

	++gameplaySmokeFrame_;
	gameplaySmokeElapsedTime_ += (std::max)(deltaTime, 0.0f);
	if (player_->IsGrounded()) {
		gameplaySmokeSawGrounded_ = true;
	}
	for (const auto& fixedMirror : fixedMirrors_) {
		if (fixedMirror && fixedMirror->HasReflectionCapture()) {
			gameplaySmokeFixedMirrorReflection_ = true;
			break;
		}
	}
	if (mirrorFloor_ && mirrorFloor_->HasReflectionCapture()) {
		gameplaySmokeMirrorFloorReflection_ = true;
	}
	const auto hasReflectedSegment = [](const std::vector<LaserSegment>& segments) {
		return std::any_of(
			segments.begin(),
			segments.end(),
			[](const LaserSegment& segment) { return segment.hitMirror; });
	};
	gameplaySmokeHazardLightReflection_ =
		gameplaySmokeHazardLightReflection_ ||
		hasReflectedSegment(ceilingSweepLightSegments_) ||
		hasReflectedSegment(horizontalMoveLightSegments_) ||
		hasReflectedSegment(bottomPulseLightSegments_) ||
		hasReflectedSegment(orbitLightSegments_);
	gameplaySmokeDoorStartsClosed_ =
		gameplaySmokeDoorStartsClosed_ ||
		(!isDoorSwitchReceivingLight_ && doorOpenAmount_ <= 0.01f);
	if (gameplaySmokeSawGrounded_ &&
		!player_->IsGrounded() &&
		player_->GetPosition().x > 10.0f) {
		gameplaySmokeLeftFloor_ = true;
	}
	if (gameplaySmokeLeftFloor_ &&
		player_->GetPosition().y < gameplaySmokeStartY_ - 1.0f) {
		gameplaySmokeFell_ = true;
	}

	const bool finishedSuccessfully =
		gameplaySmokeFell_ && gameplaySmokeElapsedTime_ >= 3.0f;
	const bool timedOut = gameplaySmokeElapsedTime_ >= 8.0f || gameplaySmokeFrame_ >= 1200;
	if (!finishedSuccessfully && !timedOut) {
		return;
	}

	const bool success =
		gameplaySmokePickedUpMirror_ &&
		gameplaySmokeDroppedMirror_ &&
		gameplaySmokeCarryMirrorReflectedLaser_ &&
		gameplaySmokeCarryMirrorBackfaceIgnored_ &&
		gameplaySmokeCarryMirrorHorizontalControl_ &&
		gameplaySmokeCarryMirrorTiltRedirectsLaser_ &&
		gameplaySmokeCarriedMirrorBlocksPlayer_ &&
		gameplaySmokeLaserHitsPlayer_ &&
		gameplaySmokeMirrorBlocksPlayer_ &&
		gameplaySmokeFixedMirrorReflection_ &&
		gameplaySmokeMirrorFloorReflectsLaser_ &&
		gameplaySmokeMirrorFloorReflection_ &&
		gameplaySmokeHazardLightReflection_ &&
		gameplaySmokeDoorStartsClosed_ &&
		gameplaySmokeDoorDiagonalReflection_ &&
		gameplaySmokeCameraSteps_ &&
		gameplaySmokeCameraWallBlock_ &&
		gameplaySmokeCameraSmooth_ &&
		gameplaySmokeCameraBacktracks_ &&
		gameplaySmokeSawGrounded_ &&
		gameplaySmokeLeftFloor_ &&
		gameplaySmokeFell_;
	std::ofstream log("logs/stage1_gameplay_smoke.log", std::ios::trunc);
	if (log) {
		log << (success ? "SUCCESS" : "FAILURE")
			<< ": picked=" << gameplaySmokePickedUpMirror_
			<< " dropped=" << gameplaySmokeDroppedMirror_
			<< " carryLaser=" << gameplaySmokeCarryMirrorReflectedLaser_
			<< " carryBackface=" << gameplaySmokeCarryMirrorBackfaceIgnored_
			<< " carryHorizontal=" << gameplaySmokeCarryMirrorHorizontalControl_
			<< " carryTiltLaser=" << gameplaySmokeCarryMirrorTiltRedirectsLaser_
			<< " carriedMirrorBlocksPlayer=" << gameplaySmokeCarriedMirrorBlocksPlayer_
			<< " laserHitsPlayer=" << gameplaySmokeLaserHitsPlayer_
			<< " mirrorBlocksPlayer=" << gameplaySmokeMirrorBlocksPlayer_
			<< " fixedMirrorReflection=" << gameplaySmokeFixedMirrorReflection_
			<< " mirrorFloorLaser=" << gameplaySmokeMirrorFloorReflectsLaser_
			<< " mirrorFloorReflection=" << gameplaySmokeMirrorFloorReflection_
			<< " hazardReflection=" << gameplaySmokeHazardLightReflection_
			<< " doorStartsClosed=" << gameplaySmokeDoorStartsClosed_
			<< " doorDiagonal=" << gameplaySmokeDoorDiagonalReflection_
			<< " cameraSteps=" << gameplaySmokeCameraSteps_
			<< " cameraWall=" << gameplaySmokeCameraWallBlock_
			<< " cameraSmooth=" << gameplaySmokeCameraSmooth_
			<< " cameraBacktracks=" << gameplaySmokeCameraBacktracks_
			<< " grounded=" << gameplaySmokeSawGrounded_
			<< " leftFloor=" << gameplaySmokeLeftFloor_
			<< " fell=" << gameplaySmokeFell_
			<< " position=" << player_->GetPosition().x
			<< ',' << player_->GetPosition().y
			<< ',' << player_->GetPosition().z
			<< " seconds=" << gameplaySmokeElapsedTime_
			<< '\n';
	}
	gameplaySmokeEnabled_ = false;
	PostQuitMessage(success ? 0 : 1);
}

void Stage1::UpdateMainCamera()
{
	if (!player_ || !cameraController_) {
		return;
	}
	UpdateCameraAreas();

	// 通常時のCameraは、矢印キー一回につき一段階だけ位置を変更する
	Input* input = Input::GetInstance();
	bool isCameraStepInput = false;
	if (input->TriggerKey(DIK_LEFT)) {
		isCameraStepInput |= cameraController_->TryStepOrbit(1, stageSolidObbs_);
	} else if (input->TriggerKey(DIK_RIGHT)) {
		isCameraStepInput |= cameraController_->TryStepOrbit(-1, stageSolidObbs_);
	}
	if (input->TriggerKey(DIK_UP)) {
		isCameraStepInput |= cameraController_->TryStepDistance(1, stageSolidObbs_);
	} else if (input->TriggerKey(DIK_DOWN)) {
		isCameraStepInput |= cameraController_->TryStepDistance(-1, stageSolidObbs_);
	}

	// Rを押すと、Playerが最後に向いた方向の後ろへCameraをゆっくり戻す
	if (input->TriggerKey(DIK_R)) {
		cameraController_->ResetBehindTarget(player_->GetFacingYaw());
	}

	// Playerの進行方向だけを渡し、先読みと自動リセンターに使用する
	cameraController_->Update(
		DirectXCommon::GetInstance()->GetDeltaTime(),
		player_->GetPosition(),
		player_->GetMoveDirection(),
		isCameraStepInput,
		stageSolidObbs_);
}

Vector3 Stage1::GetCameraForward(const Camera& camera) const
{
	// このプロジェクトでは、カメラのローカル座標 +Z が正面です。
	const Matrix4x4& worldMatrix = camera.GetWorldMatrix();
	return Normalize({ worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2] });
}

void Stage1::DrawMirrorDebugUi()
{
	if (fixedMirrors_.empty() || !fixedMirrors_.front()) {
		return;
	}

	FixedMirror& fixedMirror = *fixedMirrors_.front();
	Mirror& mirror = fixedMirror.GetMirror();
	// ImGui の詳細は ImGuiManager へまとめ、Stage1 は変更結果だけを受け取ります。
	if (ImGuiManager::GetInstance()->MirrorDebugWindow(
		mirror,
		fixedMirror.GetYawForEdit(),
		fixedMirror.GetReflectionCamera(),
		fixedMirror.HasReflectionCapture())) {
		fixedMirror.SyncVisualAndCollider();

		// 従来の鏡Inspectorで編集した値も、Save MapできるLevelDataへ同期する
		if (stageMapData_) {
			std::function<bool(std::vector<LevelLoader::ObjectData>&)> syncMirrorData;
			syncMirrorData = [&](std::vector<LevelLoader::ObjectData>& objects)
			{
				for (LevelLoader::ObjectData& objectData : objects) {
					if (objectData.tag == "Mirror") {
						objectData.translation = mirror.GetCenter();
						objectData.rotation.y = fixedMirror.GetYaw();
						objectData.scaling.x = mirror.GetWidth() * 0.5f;
						objectData.scaling.y = mirror.GetHeight() * 0.5f;
						stageMapReloadStatus_ =
							"Mirror edited in memory. Press Save Map to keep it.";
						return true;
					}
					if (syncMirrorData(objectData.children)) {
						return true;
					}
				}
				return false;
			};
			syncMirrorData(stageMapData_->objects);
		}
	}
}

void Stage1::DrawLightPuzzleDebugUi()
{
	if (!carryableMirror_ || !chargeSwitch_ || !doorSwitch_ || !lightDoor_) {
		return;
	}

	// ImGui の描画と入力は ImGuiManager に集約し、Stage1 はゲーム用の値だけを渡します。
	if (ImGuiManager::GetInstance()->LightPuzzleDebugWindow(
		laserOrigin_,
		laserDirection_,
		doorLaserOrigin_,
		doorLaserDirection_,
		laserVisualWidth_,
		chargeSwitchPosition_,
		doorSwitchPosition_,
		largeMirrorTargetYawOffset_,
		carryableMirror_->IsCarried(),
		isChargeSwitchReceivingLight_,
		mirrorCharge_,
		isLargeMirrorCharged_,
		largeMirrorRotationAmount_,
		isDoorSwitchReceivingLight_,
		doorOpenAmount_)) {
		// Laserは方向ベクトルの長さではなく向きだけを使うため、編集後に正規化します。
		if (Length(laserDirection_) > 0.0001f) {
			laserDirection_ = Normalize(laserDirection_);
		} else {
			// 全て0にした場合は、前フレームの代わりに安全な正面方向へ戻します。
			laserDirection_ = { 0.0f, 0.0f, -1.0f };
		}
		if (Length(doorLaserDirection_) > 0.0001f) {
			doorLaserDirection_ = Normalize(doorLaserDirection_);
		} else {
			doorLaserDirection_ = { 1.0f, 0.0f, 0.0f };
		}
		if (laserRenderer_) {
			laserRenderer_->SetBeamWidth(laserVisualWidth_);
		}
		if (doorLaserRenderer_) {
			doorLaserRenderer_->SetBeamWidth(laserVisualWidth_);
		}
	}
}

void Stage1::DrawCollisionDebugUi()
{
	//床と鏡のどちらかが未作成なら、当たり判定のデバッグ表示を行わない
	if (!player_ || !floor_) {
		return;
	}

	//同じ球に対して、床と鏡をそれぞれ個別に判定する
	const Sphere playerSphere = player_->GetCollider();
	const Collision::CollisionInfo floorCollision =
		Collision::SphereOBB(playerSphere, floorObb_);

	//床のOBBを、当たっているときは赤、当たっていないときは青で表示する
	ImGuiManager::GetInstance()->DrawObbCollisionDebug(
		floorObb_,
		playerSphere,
		cameraManager->GetActiveCamera(),
		floorCollision.isCollision);

	//全ての固定鏡を、当たっているときは赤、当たっていないときは青で表示する
	for (const auto& fixedMirror : fixedMirrors_) {
		if (!fixedMirror) {
			continue;
		}
		const Collision::CollisionInfo mirrorCollision =
			Collision::SphereOBB(playerSphere, fixedMirror->GetCollider());
		ImGuiManager::GetInstance()->DrawObbCollisionDebug(
			fixedMirror->GetCollider(),
			playerSphere,
			cameraManager->GetActiveCamera(),
			mirrorCollision.isCollision);
	}

	// 持てる小型鏡は、置かれている時だけPlayerとの衝突状態を表示します。
	if (carryableMirror_ && !carryableMirror_->IsCarried()) {
		const Collision::CollisionInfo carryableCollision =
			Collision::SphereOBB(playerSphere, carryableMirror_->GetCollider());
		ImGuiManager::GetInstance()->DrawObbCollisionDebug(
			carryableMirror_->GetCollider(),
			playerSphere,
			cameraManager->GetActiveCamera(),
			carryableCollision.isCollision);
	}

	// JSONから追加したBOXコライダーも、同じ赤・青のワイヤーで確認する
	for (const StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
		if (!runtimeObject.hasBoxCollider) {
			if (!runtimeObject.controlPoints.empty()) {
				ImGuiManager::GetInstance()->DrawControlPointPathDebug(
					runtimeObject.pathBasePosition,
					runtimeObject.controlPoints,
					cameraManager->GetActiveCamera());
			}
			continue;
		}
		const Collision::CollisionInfo collision =
			Collision::SphereOBB(playerSphere, runtimeObject.collider);
		ImGuiManager::GetInstance()->DrawObbCollisionDebug(
			runtimeObject.collider,
			playerSphere,
			cameraManager->GetActiveCamera(),
			collision.isCollision);
		if (!runtimeObject.controlPoints.empty()) {
			ImGuiManager::GetInstance()->DrawControlPointPathDebug(
				runtimeObject.pathBasePosition,
				runtimeObject.controlPoints,
				cameraManager->GetActiveCamera());
		}
	}

	// Event Triggerは物理的に押し返さず、侵入中かどうかだけ色で表示する
	for (const StageEventTrigger& eventTrigger : stageEventTriggers_) {
		ImGuiManager::GetInstance()->DrawObbCollisionDebug(
			eventTrigger.collider,
			playerSphere,
			cameraManager->GetActiveCamera(),
			eventTrigger.isPlayerInside);
	}

	// Event Cameraの位置は小さなBOXとして表示する
	for (const StageEventCamera& eventCamera : stageEventCameras_) {
		if (!eventCamera.camera) {
			continue;
		}
		const Transform cameraTransform{
			{ 1.0f, 1.0f, 1.0f },
			eventCamera.camera->GetRotate(),
			eventCamera.camera->GetTranslate(),
		};
		const OBB cameraDebugObb =
			Collision::MakeOBB(cameraTransform, { 0.25f, 0.25f, 0.25f });
		ImGuiManager::GetInstance()->DrawObbCollisionDebug(
			cameraDebugObb,
			playerSphere,
			cameraManager->GetActiveCamera(),
			eventCamera.sourceName == activeEventCameraName_);
	}
}

void Stage1::DrawStageEditViewport()
{
#ifdef USE_IMGUI
	if (!stageMapData_) {
		return;
	}

	SceneEditor::ViewportOptions options{};
	options.camera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	std::vector<std::string> sourceNames;
	const auto addObject = [&](const std::string& sourceName, Object3d* object) {
		if (!object) {
			return;
		}
		sourceNames.push_back(sourceName);
		options.objects.push_back({ sourceName, object });
	};

	size_t fixedMirrorIndex = 0;
	for (const LevelLoader::ObjectData& objectData : stageMapData_->objects) {
		if (objectData.tag == "Floor") {
			addObject(objectData.name, floor_);
		} else if (objectData.tag == "Mirror") {
			if (fixedMirrorIndex < fixedMirrors_.size() && fixedMirrors_[fixedMirrorIndex]) {
				addObject(objectData.name, &fixedMirrors_[fixedMirrorIndex]->GetObject());
			}
			++fixedMirrorIndex;
		}
	}
	for (StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
		addObject(runtimeObject.sourceName, runtimeObject.visual.get());
	}

	viewportEditorState_.selectedIndex = -1;
	if (selectedStageMapObjectIndex_ >= 0 &&
		selectedStageMapObjectIndex_ < static_cast<int>(stageMapData_->objects.size())) {
		const std::string& selectedName = stageMapData_->objects[selectedStageMapObjectIndex_].name;
		const auto found = std::find(sourceNames.begin(), sourceNames.end(), selectedName);
		if (found != sourceNames.end()) {
			viewportEditorState_.selectedIndex = static_cast<int>(std::distance(sourceNames.begin(), found));
		}
	}

	options.onSelectionChanged = [this, &sourceNames](int viewportIndex) {
		if (viewportIndex < 0 || viewportIndex >= static_cast<int>(sourceNames.size()) || !stageMapData_) {
			return;
		}
		const std::string& selectedName = sourceNames[viewportIndex];
		for (int objectIndex = 0; objectIndex < static_cast<int>(stageMapData_->objects.size()); ++objectIndex) {
			if (stageMapData_->objects[objectIndex].name == selectedName) {
				selectedStageMapObjectIndex_ = objectIndex;
				return;
			}
		}
	};
	options.onTransformChanged = [this, &sourceNames](int viewportIndex, const Transform& transform) {
		if (viewportIndex < 0 || viewportIndex >= static_cast<int>(sourceNames.size()) || !stageMapData_) {
			return;
		}
		const std::string& selectedName = sourceNames[viewportIndex];
		for (LevelLoader::ObjectData& objectData : stageMapData_->objects) {
			if (objectData.name != selectedName) {
				continue;
			}
			objectData.translation = transform.translate;
			objectData.rotation = transform.rotate;
			objectData.scaling = transform.scale;
			if (ApplyStageMapData(false)) {
				stageMapReloadStatus_ = "Edited in Edit View. Press Save Map to keep it.";
			}
			return;
		}
	};
	SceneEditor::DrawViewportEditor(viewportEditorState_, options);
#endif
}

void Stage1::UpdateStageMapHotReload()
{
	if (!autoStageMapReload_) {
		return;
	}

	// 保存を検出したフレームだけJSONを再読込する
	if (stageMapHotReload_.ConsumeChange()) {
		ReloadStageMap();
	}
}

bool Stage1::ReloadStageMap()
{
	// 読込に失敗した時は、現在表示中のマップを変更しない
	std::unique_ptr<LevelLoader::LevelData> loadedData =
		LevelLoader::Load(kStageMapFileName);
	if (!loadedData) {
		stageMapReloadStatus_ = "Reload failed. Current stage was kept.";
		return false;
	}

	// 新しいLevelDataを仮に設定し、全オブジェクトを作れた場合だけ確定する
	std::unique_ptr<LevelLoader::LevelData> previousData = std::move(stageMapData_);
	stageMapData_ = std::move(loadedData);
	if (!ApplyStageMapData(true)) {
		stageMapData_ = std::move(previousData);
		stageMapReloadStatus_ = "Reload failed. Current stage was kept.";
		return false;
	}

	if (stageMapData_->objects.empty()) {
		selectedStageMapObjectIndex_ = -1;
	} else {
		selectedStageMapObjectIndex_ = std::clamp(
			selectedStageMapObjectIndex_,
			0,
			static_cast<int>(stageMapData_->objects.size()) - 1);
	}

	stageMapReloadStatus_ =
		"Reloaded stage1.json: " +
		std::to_string(stageMapData_->objects.size()) +
		" objects.";
	return true;
}

bool Stage1::SaveStageMap()
{
	if (!stageMapData_) {
		stageMapReloadStatus_ = "Save failed. No map data is loaded.";
		return false;
	}

	stageMapData_->coordinateSystem = "engine";
	if (!LevelLoader::Save(kStageMapFileName, *stageMapData_)) {
		stageMapReloadStatus_ = "Save failed. stage1.json was not changed.";
		return false;
	}

	// 自分で保存した変更を、次のフレームに外部変更として再読込しないよう同期する
	stageMapHotReload_.Synchronize();
	stageMapReloadStatus_ = "Saved stage1.json.";
	return true;
}

bool Stage1::ApplyStageMapData(bool rebuildRuntimeObjects)
{
	// JSONのObjectDataを分類し、既存の床・鏡・Camera・追加モデルへ反映する中心処理です。
	if (!stageMapData_ || !floor_) {
		return false;
	}

	const LevelLoader::ObjectData* floorData = nullptr;
	const LevelLoader::ObjectData* playerStartData = nullptr;
	const LevelLoader::ObjectData* carryableMirrorData = nullptr;
	std::vector<const LevelLoader::ObjectData*> mirrorDataList;
	std::vector<const LevelLoader::ObjectData*> additionalObjects;
	std::vector<const LevelLoader::ObjectData*> eventTriggerDataList;
	std::vector<const LevelLoader::ObjectData*> eventCameraDataList;
	std::vector<const LevelLoader::ObjectData*> cameraAreaDataList;
	std::function<void(const std::vector<LevelLoader::ObjectData>&)> collectObjects;
	collectObjects = [&](const std::vector<LevelLoader::ObjectData>& objects)
	{
		// childrenも同じ扱いにするため、JSONツリーを再帰的に最後まで調べます。
		for (const LevelLoader::ObjectData& objectData : objects) {
			if (objectData.tag == "Floor") {
				floorData = &objectData;
			} else if (objectData.tag == "PlayerStart") {
				playerStartData = &objectData;
			} else if (objectData.objectType == "CARRYABLE_MIRROR") {
				carryableMirrorData = &objectData;
			} else if (objectData.tag == "Mirror") {
				mirrorDataList.push_back(&objectData);
			} else if (objectData.objectType == "EVENT_TRIGGER") {
				eventTriggerDataList.push_back(&objectData);
			} else if (objectData.objectType == "EVENT_CAMERA") {
				eventCameraDataList.push_back(&objectData);
			} else if (objectData.objectType == "CAMERA_AREA" && objectData.hasCameraArea) {
				cameraAreaDataList.push_back(&objectData);
			} else if (objectData.type == "MESH" && !objectData.fileName.empty()) {
				additionalObjects.push_back(&objectData);
			}
			collectObjects(objectData.children);
		}
	};
	collectObjects(stageMapData_->objects);

	// Stage1で必須の床と鏡がなければ、途中まで生成した状態を残さず何も変更しない
	if (!floorData || mirrorDataList.empty()) {
		return false;
	}

	// PlayerStartは見えない目印で、マップ読込時に開始座標だけを記録します。
	// ホットリロード時にはPlayerを移動させないため、実際の配置はInitializeで一度だけ行います。
	hasStagePlayerStart_ = playerStartData != nullptr;
	if (playerStartData) {
		stagePlayerStartPosition_ = playerStartData->translation;
	}
	if (carryableMirrorData && carryableMirror_) {
		// 持てるMirrorの置き場所は、ほかのステージ物と同じくJSONで変更します。
		carryableMirror_->SetDroppedPosition(carryableMirrorData->translation);
	}
	if (stageMapData_->hasStageStart) {
		stageStartSettings_.duration = stageMapData_->stageStart.duration;
		stageStartSettings_.playerAirHeight = stageMapData_->stageStart.playerAirHeight;
		stageStartSettings_.cameraFrontDistance = stageMapData_->stageStart.cameraFrontDistance;
		stageStartSettings_.cameraFrontHeight = stageMapData_->stageStart.cameraFrontHeight;
		stageStartSettings_.cameraOrbitAngle = stageMapData_->stageStart.cameraOrbitAngle;
		stageStartSettings_.cameraHandoffDuration = stageMapData_->stageStart.cameraHandoffDuration;
	}
	stageSpotLights_.clear();
	if (stageMapData_->hasLighting) {
		const LevelLoader::LightingData& lighting = stageMapData_->lighting;
		directionalLight_.color = {
			lighting.directionalColor.x,
			lighting.directionalColor.y,
			lighting.directionalColor.z,
			1.0f,
		};
		directionalLight_.direction = Normalize(lighting.directionalDirection);
		directionalLight_.intensity = lighting.directionalIntensity;
		directionalLight_.ambientColor = lighting.ambientColor;
		directionalLight_.ambientIntensity = lighting.ambientIntensity;
		pointLight_.color = {
			lighting.pointColor.x,
			lighting.pointColor.y,
			lighting.pointColor.z,
			1.0f,
		};
		pointLight_.position = lighting.pointPosition;
		pointLight_.intensity = lighting.pointIntensity;
		pointLight_.radius = lighting.pointRadius;
		pointLight_.decay = lighting.pointDecay;
		for (const LevelLoader::SpotLightData& spotLightData : lighting.spotLights) {
			if (stageSpotLights_.size() >= kStageLightingSpotLightCount) {
				break;
			}
			if (spotLightData.intensity <= 0.0f || spotLightData.distance <= 0.0f) {
				continue;
			}

			Object3d::SpotLight spotLight{};
			spotLight.color = {
				spotLightData.color.x,
				spotLightData.color.y,
				spotLightData.color.z,
				1.0f,
			};
			spotLight.position = spotLightData.position;
			spotLight.intensity = spotLightData.intensity;
			spotLight.direction = Normalize(spotLightData.direction);
			spotLight.distance = spotLightData.distance;
			spotLight.decay = spotLightData.decay;
			spotLight.cosAngle = spotLightData.cosAngle;
			spotLight.cosFalloffStart = spotLightData.cosFalloffStart;
			stageSpotLights_.push_back(spotLight);
		}
	}

	// Mirrorタグの数だけ固定鏡を作るため、JSONへMirrorを追加すれば複数配置できます。
	const bool rebuildFixedMirrors =
		rebuildRuntimeObjects || fixedMirrors_.size() != mirrorDataList.size();
	std::vector<std::unique_ptr<FixedMirror>> rebuiltFixedMirrors;
	if (rebuildFixedMirrors) {
		rebuiltFixedMirrors.reserve(mirrorDataList.size());
		for (const LevelLoader::ObjectData* mirrorData : mirrorDataList) {
			auto fixedMirror = std::make_unique<FixedMirror>();
			if (!fixedMirror->Initialize(
				object3dCommon,
				DirectXCommon::GetInstance(),
				SrvManager::GetInstance(),
				mirrorData->fileName.empty() ? "plane.obj" : mirrorData->fileName,
				mirrorData->translation,
				mirrorData->rotation.y,
				std::abs(mirrorData->scaling.x) * 2.0f,
				std::abs(mirrorData->scaling.y) * 2.0f,
				512)) {
				return false;
			}
			fixedMirror->GetObject().SetDirectionalLight(directionalLight_);
			fixedMirror->GetObject().SetPointLight(pointLight_);
			if (mirrorData->hasCollider && mirrorData->collider.type == "BOX") {
				fixedMirror->SetColliderShape(
					mirrorData->collider.center,
					{
						std::abs(mirrorData->collider.size.x) * 0.5f,
						std::abs(mirrorData->collider.size.y) * 0.5f,
						std::abs(mirrorData->collider.size.z) * 0.5f,
					});
			}
			rebuiltFixedMirrors.push_back(std::move(fixedMirror));
		}
	}

	// モデル追加・削除時だけObject3dの一覧を作り直す
	std::vector<StageMapRuntimeObject> rebuiltObjects;
	std::vector<StageEventTrigger> rebuiltEventTriggers;
	std::vector<StageEventCamera> rebuiltEventCameras;
	std::vector<StageCameraArea> rebuiltCameraAreas;
	if (rebuildRuntimeObjects) {
		rebuiltObjects.reserve(additionalObjects.size());
		for (const LevelLoader::ObjectData* objectData : additionalObjects) {
			if (!ModelManager::GetInstance()->LoadModel(objectData->fileName)) {
				return false;
			}

			StageMapRuntimeObject runtimeObject{};
			runtimeObject.sourceName = objectData->name;
			runtimeObject.visual = CreateObject(objectData->fileName);
			runtimeObject.visual->SetTranslate(objectData->translation);
			runtimeObject.visual->SetRotate(objectData->rotation);
			runtimeObject.visual->SetScale(objectData->scaling);
			runtimeObject.pathBasePosition = objectData->translation;
			runtimeObject.controlPoints = objectData->controlPoints;
			runtimeObject.pathSpeed = objectData->pathSpeed;
			runtimeObject.pathLoop = objectData->pathLoop;
			runtimeObject.hasBoxCollider =
				objectData->hasCollider && objectData->collider.type == "BOX";
			if (runtimeObject.hasBoxCollider) {
				runtimeObject.colliderLocalCenter = objectData->collider.center;
				runtimeObject.colliderLocalHalfSize = {
					std::abs(objectData->collider.size.x) * 0.5f,
					std::abs(objectData->collider.size.y) * 0.5f,
					std::abs(objectData->collider.size.z) * 0.5f,
				};
			}
			rebuiltObjects.push_back(std::move(runtimeObject));
		}

		rebuiltEventTriggers.reserve(eventTriggerDataList.size());
		for (const LevelLoader::ObjectData* objectData : eventTriggerDataList) {
			StageEventTrigger eventTrigger{};
			eventTrigger.sourceName = objectData->name;
			eventTrigger.eventId = objectData->eventId;
			eventTrigger.eventCameraName = objectData->eventCameraName;
			eventTrigger.transform = {
				objectData->scaling,
				objectData->rotation,
				objectData->translation,
			};
			if (objectData->hasCollider && objectData->collider.type == "BOX") {
				eventTrigger.colliderLocalCenter = objectData->collider.center;
				eventTrigger.colliderLocalHalfSize = {
					std::abs(objectData->collider.size.x) * 0.5f,
					std::abs(objectData->collider.size.y) * 0.5f,
					std::abs(objectData->collider.size.z) * 0.5f,
				};
			}
			rebuiltEventTriggers.push_back(std::move(eventTrigger));
		}

		rebuiltEventCameras.reserve(eventCameraDataList.size());
		for (const LevelLoader::ObjectData* objectData : eventCameraDataList) {
			StageEventCamera eventCamera{};
			eventCamera.sourceName = objectData->name;
			eventCamera.camera = std::make_unique<Camera>();
			UpdateStageEventCamera(eventCamera, *objectData);
			rebuiltEventCameras.push_back(std::move(eventCamera));
		}

		rebuiltCameraAreas.reserve(cameraAreaDataList.size());
		for (const LevelLoader::ObjectData* objectData : cameraAreaDataList) {
			StageCameraArea cameraArea{};
			cameraArea.sourceName = objectData->name;
			cameraArea.transform = {
				objectData->scaling,
				objectData->rotation,
				objectData->translation,
			};
			if (objectData->hasCollider && objectData->collider.type == "BOX") {
				cameraArea.colliderLocalCenter = objectData->collider.center;
				cameraArea.colliderLocalHalfSize = {
					std::abs(objectData->collider.size.x) * 0.5f,
					std::abs(objectData->collider.size.y) * 0.5f,
					std::abs(objectData->collider.size.z) * 0.5f,
				};
			}
			cameraArea.settings = {
				objectData->cameraArea.distance,
				objectData->cameraArea.pitch,
				objectData->cameraArea.fovY,
			};
			rebuiltCameraAreas.push_back(std::move(cameraArea));
		}
	}

	// ---------- 床データの反映 ----------
	floor_->SetTranslate(floorData->translation);
	floor_->SetRotate(floorData->rotation);
	floor_->SetScale(floorData->scaling);
	if (floorData->hasCollider && floorData->collider.type == "BOX") {
		floorColliderLocalCenter_ = floorData->collider.center;
		floorLocalHalfSize_ = {
			std::abs(floorData->collider.size.x) * 0.5f,
			std::abs(floorData->collider.size.y) * 0.5f,
			std::abs(floorData->collider.size.z) * 0.5f,
		};
	}

	// ---------- 鏡データの反映 ----------
	if (rebuildFixedMirrors) {
		fixedMirrors_ = std::move(rebuiltFixedMirrors);
	} else {
		for (size_t index = 0; index < mirrorDataList.size(); ++index) {
			const LevelLoader::ObjectData& mirrorData = *mirrorDataList[index];
			FixedMirror& fixedMirror = *fixedMirrors_[index];
			fixedMirror.GetYawForEdit() = mirrorData.rotation.y;
			fixedMirror.GetMirror().SetCenter(mirrorData.translation);
			fixedMirror.GetMirror().SetSize(
				std::abs(mirrorData.scaling.x) * 2.0f,
				std::abs(mirrorData.scaling.y) * 2.0f);
			if (mirrorData.hasCollider && mirrorData.collider.type == "BOX") {
				fixedMirror.SetColliderShape(
					mirrorData.collider.center,
					{
						std::abs(mirrorData.collider.size.x) * 0.5f,
						std::abs(mirrorData.collider.size.y) * 0.5f,
						std::abs(mirrorData.collider.size.z) * 0.5f,
					});
			} else {
				fixedMirror.SyncVisualAndCollider();
			}
		}
	}

	if (rebuildRuntimeObjects) {
		stageMapRuntimeObjects_ = std::move(rebuiltObjects);
		if (cameraManager) {
			// CameraManagerに古いポインタを残さず、新しいEvent Cameraへ差し替える
			cameraManager->SetActiveCamera("MainCamera");
			for (const StageEventCamera& eventCamera : stageEventCameras_) {
				cameraManager->RemoveCamera(eventCamera.sourceName);
			}
		}
		stageEventCameras_ = std::move(rebuiltEventCameras);
		stageEventTriggers_ = std::move(rebuiltEventTriggers);
		stageCameraAreas_ = std::move(rebuiltCameraAreas);
		activeEventCameraName_.clear();
		activeCameraAreaName_.clear();
		if (cameraController_) {
			cameraController_->ClearAreaSettings();
		}
		if (cameraManager) {
			for (const StageEventCamera& eventCamera : stageEventCameras_) {
				cameraManager->AddCamera(
					eventCamera.sourceName,
					eventCamera.camera.get());
			}
		}
	} else {
		// Transform編集中は既存Object3dを再生成せず、値だけ即時反映する
		for (StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
			const auto found = std::find_if(
				additionalObjects.begin(),
				additionalObjects.end(),
				[&](const LevelLoader::ObjectData* objectData)
				{
					return objectData->name == runtimeObject.sourceName;
				});
			if (found == additionalObjects.end() || !runtimeObject.visual) {
				continue;
			}

			const LevelLoader::ObjectData& objectData = **found;
			runtimeObject.visual->SetTranslate(objectData.translation);
			runtimeObject.visual->SetRotate(objectData.rotation);
			runtimeObject.visual->SetScale(objectData.scaling);
			runtimeObject.pathBasePosition = objectData.translation;
			runtimeObject.controlPoints = objectData.controlPoints;
			runtimeObject.pathSpeed = objectData.pathSpeed;
			runtimeObject.pathLoop = objectData.pathLoop;
			runtimeObject.hasBoxCollider =
				objectData.hasCollider && objectData.collider.type == "BOX";
			if (runtimeObject.hasBoxCollider) {
				runtimeObject.colliderLocalCenter = objectData.collider.center;
				runtimeObject.colliderLocalHalfSize = {
					std::abs(objectData.collider.size.x) * 0.5f,
					std::abs(objectData.collider.size.y) * 0.5f,
					std::abs(objectData.collider.size.z) * 0.5f,
				};
			}
		}

		// Event TriggerとEvent Cameraも、Object3dを作り直さず編集値だけ反映する
		for (StageEventTrigger& eventTrigger : stageEventTriggers_) {
			const auto found = std::find_if(
				eventTriggerDataList.begin(),
				eventTriggerDataList.end(),
				[&](const LevelLoader::ObjectData* objectData)
				{
					return objectData->name == eventTrigger.sourceName;
				});
			if (found == eventTriggerDataList.end()) {
				continue;
			}
			const LevelLoader::ObjectData& objectData = **found;
			eventTrigger.eventId = objectData.eventId;
			eventTrigger.eventCameraName = objectData.eventCameraName;
			eventTrigger.transform = {
				objectData.scaling,
				objectData.rotation,
				objectData.translation,
			};
			if (objectData.hasCollider && objectData.collider.type == "BOX") {
				eventTrigger.colliderLocalCenter = objectData.collider.center;
				eventTrigger.colliderLocalHalfSize = {
					std::abs(objectData.collider.size.x) * 0.5f,
					std::abs(objectData.collider.size.y) * 0.5f,
					std::abs(objectData.collider.size.z) * 0.5f,
				};
			}
		}
		for (StageEventCamera& eventCamera : stageEventCameras_) {
			const auto found = std::find_if(
				eventCameraDataList.begin(),
				eventCameraDataList.end(),
				[&](const LevelLoader::ObjectData* objectData)
				{
					return objectData->name == eventCamera.sourceName;
				});
			if (found != eventCameraDataList.end()) {
				UpdateStageEventCamera(eventCamera, **found);
			}
		}
		for (StageCameraArea& cameraArea : stageCameraAreas_) {
			const auto found = std::find_if(
				cameraAreaDataList.begin(),
				cameraAreaDataList.end(),
				[&](const LevelLoader::ObjectData* objectData)
				{
					return objectData->name == cameraArea.sourceName;
				});
			if (found == cameraAreaDataList.end()) {
				continue;
			}
			const LevelLoader::ObjectData& objectData = **found;
			cameraArea.transform = {
				objectData.scaling,
				objectData.rotation,
				objectData.translation,
			};
			if (objectData.hasCollider && objectData.collider.type == "BOX") {
				cameraArea.colliderLocalCenter = objectData.collider.center;
				cameraArea.colliderLocalHalfSize = {
					std::abs(objectData.collider.size.x) * 0.5f,
					std::abs(objectData.collider.size.y) * 0.5f,
					std::abs(objectData.collider.size.z) * 0.5f,
				};
			}
			cameraArea.settings = {
				objectData.cameraArea.distance,
				objectData.cameraArea.pitch,
				objectData.cameraArea.fovY,
			};
		}
	}

	return true;
}

void Stage1::DrawStageModelShelf()
{
#ifdef USE_IMGUI
	SceneEditor::ShelfCallbacks callbacks{};
	callbacks.sceneLabel = "Stage1 Edit View";
	if (stageMapData_) {
		callbacks.addedModelCount = static_cast<size_t>(std::count_if(
			stageMapData_->objects.begin(),
			stageMapData_->objects.end(),
			[](const LevelLoader::ObjectData& objectData) { return objectData.tag == "EditorAdded"; }));
	}
	callbacks.addModel = [this](const std::string& fileName) {
		const bool added = AddStageMapModel(fileName);
		if (added) {
			SaveStageMap();
		}
		return added;
	};
	callbacks.addTexture = [](const std::string&) { return false; };
	callbacks.clearAdded = [this]() { ClearStageMapEditorAddedObjects(); };
	SceneEditor::DrawModelShelf(stageShelfState_, callbacks);
#endif
}

void Stage1::HandleStageShelfDropOnEditView()
{
#ifdef USE_IMGUI
	SceneEditor::ShelfCallbacks callbacks{};
	callbacks.sceneLabel = "Stage1 Edit View";
	callbacks.addModel = [this](const std::string& fileName) {
		const bool added = AddStageMapModel(fileName);
		if (added) {
			SaveStageMap();
		}
		return added;
	};
	callbacks.addTexture = [](const std::string&) { return false; };
	SceneEditor::HandleShelfDropOnEditView(stageShelfState_, callbacks);
#endif
}

bool Stage1::AddStageMapModel(const std::string& fileName)
{
	if (!stageMapData_ || fileName.empty()) {
		stageMapReloadStatus_ = "Add failed. No map data or model name.";
		return false;
	}

	int number = 1;
	std::string objectName;
	do {
		objectName = "EditorModel" + std::to_string(number++);
	} while (std::any_of(
		stageMapData_->objects.begin(),
		stageMapData_->objects.end(),
		[&](const LevelLoader::ObjectData& objectData) { return objectData.name == objectName; }));

	LevelLoader::ObjectData objectData{};
	objectData.type = "MESH";
	objectData.name = objectName;
	objectData.tag = "EditorAdded";
	objectData.objectType = "STATIC";
	objectData.fileName = fileName;
	objectData.translation = player_ ? player_->GetPosition() : Vector3{};
	objectData.translation.y += 1.0f;
	objectData.translation.z += 3.0f;
	objectData.scaling = { 1.0f, 1.0f, 1.0f };

	stageMapData_->objects.push_back(std::move(objectData));
	selectedStageMapObjectIndex_ = static_cast<int>(stageMapData_->objects.size()) - 1;
	if (!ApplyStageMapData(true)) {
		stageMapData_->objects.pop_back();
		stageMapReloadStatus_ = "Add failed. Model could not be loaded: " + fileName;
		return false;
	}
	stageMapReloadStatus_ = "Added model from Edit View: " + fileName;
	return true;
}

void Stage1::ClearStageMapEditorAddedObjects()
{
	if (!stageMapData_) {
		return;
	}
	std::vector<LevelLoader::ObjectData>& objects = stageMapData_->objects;
	objects.erase(
		std::remove_if(
			objects.begin(),
			objects.end(),
			[](const LevelLoader::ObjectData& objectData) { return objectData.tag == "EditorAdded"; }),
		objects.end());
	selectedStageMapObjectIndex_ = objects.empty()
		? -1
		: std::clamp(selectedStageMapObjectIndex_, 0, static_cast<int>(objects.size()) - 1);
	ApplyStageMapData(true);
	SaveStageMap();
	stageMapReloadStatus_ = "Cleared models added from Edit View.";
}

bool Stage1::AddStageMapSphere()
{
	if (!stageMapData_) {
		stageMapReloadStatus_ = "Add failed. No map data is loaded.";
		return false;
	}

	// 既存名と重ならない連番の名前を作る
	int number = 1;
	std::string objectName;
	do {
		objectName = "MapSphere" + std::to_string(number++);
	} while (std::any_of(
		stageMapData_->objects.begin(),
		stageMapData_->objects.end(),
		[&](const LevelLoader::ObjectData& objectData)
		{
			return objectData.name == objectName;
		}));

	LevelLoader::ObjectData objectData{};
	objectData.type = "MESH";
	objectData.name = objectName;
	objectData.tag = "MapObject";
	objectData.objectType = "STATIC";
	objectData.fileName = "sphere.obj";
	objectData.translation = player_ ? player_->GetPosition() : Vector3{};
	objectData.translation.y += 1.5f;
	objectData.translation.z += 3.0f;
	objectData.scaling = { 1.0f, 1.0f, 1.0f };
	objectData.hasCollider = true;
	objectData.collider.type = "BOX";
	objectData.collider.center = { 0.0f, 0.0f, 0.0f };
	objectData.collider.size = { 1.0f, 1.0f, 1.0f };

	stageMapData_->objects.push_back(objectData);
	selectedStageMapObjectIndex_ =
		static_cast<int>(stageMapData_->objects.size()) - 1;
	if (!ApplyStageMapData(true)) {
		stageMapData_->objects.pop_back();
		stageMapReloadStatus_ = "Add failed. sphere.obj could not be created.";
		return false;
	}
	stageMapReloadStatus_ = "Added sphere object.";
	return true;
}

bool Stage1::RemoveSelectedStageMapObject()
{
	if (!stageMapData_ ||
		selectedStageMapObjectIndex_ < 0 ||
		selectedStageMapObjectIndex_ >=
			static_cast<int>(stageMapData_->objects.size())) {
		return false;
	}

	const LevelLoader::ObjectData& selectedObject =
		stageMapData_->objects[selectedStageMapObjectIndex_];
	if (selectedObject.tag == "Floor" || selectedObject.tag == "Mirror") {
		return false;
	}

	stageMapData_->objects.erase(
		stageMapData_->objects.begin() + selectedStageMapObjectIndex_);
	if (stageMapData_->objects.empty()) {
		selectedStageMapObjectIndex_ = -1;
	} else {
		selectedStageMapObjectIndex_ = std::clamp(
			selectedStageMapObjectIndex_,
			0,
			static_cast<int>(stageMapData_->objects.size()) - 1);
	}
	ApplyStageMapData(true);
	stageMapReloadStatus_ = "Removed selected object.";
	return true;
}

bool Stage1::AddStageMapEventPair()
{
	if (!stageMapData_) {
		stageMapReloadStatus_ = "Add failed. No map data is loaded.";
		return false;
	}

	// EventCameraとEventTriggerの両方で使える、重複しない番号を探す
	int number = 1;
	std::string cameraName;
	std::string triggerName;
	bool nameExists = false;
	do {
		cameraName = "EventCamera" + std::to_string(number);
		triggerName = "EventTrigger" + std::to_string(number);
		++number;
		nameExists = std::any_of(
			stageMapData_->objects.begin(),
			stageMapData_->objects.end(),
			[&](const LevelLoader::ObjectData& objectData)
			{
				return objectData.name == cameraName ||
					objectData.name == triggerName;
			});
	} while (nameExists);

	const Vector3 playerPosition = player_ ? player_->GetPosition() : Vector3{};
	const Vector3 triggerPosition{
		playerPosition.x,
		playerPosition.y,
		playerPosition.z + 6.0f,
	};

	// ---------- イベント時に使用するCamera ----------
	LevelLoader::ObjectData cameraData{};
	cameraData.type = "CAMERA";
	cameraData.name = cameraName;
	cameraData.tag = "EventCamera";
	cameraData.objectType = "EVENT_CAMERA";
	cameraData.translation = {
		triggerPosition.x + 6.0f,
		triggerPosition.y + 4.0f,
		triggerPosition.z - 8.0f,
	};
	cameraData.scaling = { 1.0f, 1.0f, 1.0f };
	cameraData.hasCameraFocus = true;
	cameraData.cameraFocus = {
		triggerPosition.x,
		triggerPosition.y + 1.0f,
		triggerPosition.z,
	};

	// ---------- Playerの侵入を検出するTrigger ----------
	LevelLoader::ObjectData triggerData{};
	triggerData.type = "EMPTY";
	triggerData.name = triggerName;
	triggerData.tag = "EventTrigger";
	triggerData.objectType = "EVENT_TRIGGER";
	triggerData.eventId = "Event" + std::to_string(number - 1);
	triggerData.eventCameraName = cameraName;
	triggerData.translation = triggerPosition;
	triggerData.scaling = { 1.0f, 1.0f, 1.0f };
	triggerData.hasCollider = true;
	triggerData.collider.type = "BOX";
	triggerData.collider.center = { 0.0f, 0.0f, 0.0f };
	triggerData.collider.size = { 4.0f, 3.0f, 4.0f };

	const size_t previousObjectCount = stageMapData_->objects.size();
	stageMapData_->objects.push_back(std::move(cameraData));
	stageMapData_->objects.push_back(std::move(triggerData));
	// 追加直後は視点を調整するEvent Cameraを選び、すぐ位置と注視点を編集できるようにする
	selectedStageMapObjectIndex_ =
		static_cast<int>(previousObjectCount);

	if (!ApplyStageMapData(true)) {
		stageMapData_->objects.resize(previousObjectCount);
		stageMapReloadStatus_ = "Add failed. Event pair could not be created.";
		return false;
	}

	stageMapReloadStatus_ = "Added Event Trigger and Event Camera.";
	return true;
}

bool Stage1::AddStageMapCameraArea()
{
	if (!stageMapData_) {
		stageMapReloadStatus_ = "Add failed. No map data is loaded.";
		return false;
	}

	// 既存のArea名と重ならない連番の名前を作る
	int number = 1;
	std::string areaName;
	do {
		areaName = "CameraArea" + std::to_string(number++);
	} while (std::any_of(
		stageMapData_->objects.begin(),
		stageMapData_->objects.end(),
		[&](const LevelLoader::ObjectData& objectData)
		{
			return objectData.name == areaName;
		}));

	const Vector3 playerPosition = player_ ? player_->GetPosition() : Vector3{};
	LevelLoader::ObjectData areaData{};
	areaData.type = "EMPTY";
	areaData.name = areaName;
	areaData.tag = "CameraArea";
	areaData.objectType = "CAMERA_AREA";
	areaData.translation = playerPosition;
	areaData.scaling = { 1.0f, 1.0f, 1.0f };
	areaData.hasCollider = true;
	areaData.collider.type = "BOX";
	areaData.collider.center = { 0.0f, 0.0f, 0.0f };
	areaData.collider.size = { 8.0f, 6.0f, 8.0f };
	areaData.hasCameraArea = true;
	areaData.cameraArea.distance = 11.5f;
	areaData.cameraArea.pitch = 0.58f;
	areaData.cameraArea.fovY = 0.48f;

	stageMapData_->objects.push_back(std::move(areaData));
	selectedStageMapObjectIndex_ = static_cast<int>(stageMapData_->objects.size()) - 1;
	if (!ApplyStageMapData(true)) {
		stageMapData_->objects.pop_back();
		stageMapReloadStatus_ = "Add failed. Camera Area could not be created.";
		return false;
	}

	stageMapReloadStatus_ = "Added Camera Area. Adjust it in the Inspector.";
	return true;
}

void Stage1::UpdateStageEvents()
{
	// Playerの球Colliderと各Trigger OBBを調べ、今いる場所に対応したEvent Cameraを選びます。
	if (!player_ || !cameraManager) {
		return;
	}

	const Sphere playerSphere = player_->GetCollider();
	std::string requestedCameraName;

	for (StageEventTrigger& eventTrigger : stageEventTriggers_) {
		eventTrigger.collider = Collision::MakeOBB(
			eventTrigger.transform,
			eventTrigger.colliderLocalCenter,
			eventTrigger.colliderLocalHalfSize);
		eventTrigger.isPlayerInside =
			Collision::SphereOBB(playerSphere, eventTrigger.collider).isCollision;
		if (!eventTrigger.isPlayerInside || !requestedCameraName.empty()) {
			continue;
		}

		// JSONで接続されたEvent Cameraが存在するTriggerだけを起動する
		const auto cameraFound = std::find_if(
			stageEventCameras_.begin(),
			stageEventCameras_.end(),
			[&](const StageEventCamera& eventCamera)
			{
				return eventCamera.sourceName == eventTrigger.eventCameraName;
			});
		if (cameraFound != stageEventCameras_.end()) {
			requestedCameraName = cameraFound->sourceName;
		}
	}

	if (!requestedCameraName.empty()) {
		if (activeEventCameraName_ != requestedCameraName) {
			const auto cameraFound = std::find_if(
				stageEventCameras_.begin(),
				stageEventCameras_.end(),
				[&](const StageEventCamera& eventCamera)
				{
					return eventCamera.sourceName == requestedCameraName;
				});
			if (cameraFound == stageEventCameras_.end() || !cameraFound->camera) {
				return;
			}

			// JSONで置いたEvent Cameraの位置を、Player周囲を回る手動Cameraの初期位置にする
			const Vector3 initialFocus{
				player_->GetPosition().x,
				player_->GetPosition().y + 1.0f,
				player_->GetPosition().z,
			};
			// JSONのCamera座標を、CameraControllerが使う距離・yaw・pitchへ変換します。
			const Vector3 cameraOffset{
				cameraFound->camera->GetTranslate().x - initialFocus.x,
				cameraFound->camera->GetTranslate().y - initialFocus.y,
				cameraFound->camera->GetTranslate().z - initialFocus.z,
			};
			const float distance = (std::max)(Length(cameraOffset), 3.0f);
			const float yaw = std::atan2(-cameraOffset.x, -cameraOffset.z);
			const float pitch = std::asin(std::clamp(cameraOffset.y / distance, -1.0f, 1.0f));
			cameraFound->manualController = std::make_unique<CameraController>();
			cameraFound->manualController->Initialize(
				cameraFound->camera.get(),
				player_->GetPosition(),
				distance,
				yaw,
				pitch);
			// Event CameraゾーンではPlayerが自由にCameraを操作するため、自動リセンターを止める
			cameraFound->manualController->SetAutoRecenterEnabled(false);

			cameraManager->SetActiveCamera(requestedCameraName);
			activeEventCameraName_ = requestedCameraName;
			stageMapReloadStatus_ =
				"Event Manual Camera active: " + requestedCameraName;
		}
	} else if (!activeEventCameraName_.empty()) {
		// すべてのTriggerから出たら、通常の追従Cameraへ戻す
		const auto activeCameraFound = std::find_if(
			stageEventCameras_.begin(),
			stageEventCameras_.end(),
			[this](const StageEventCamera& eventCamera)
			{
				return eventCamera.sourceName == activeEventCameraName_;
			});
		if (activeCameraFound != stageEventCameras_.end()) {
			activeCameraFound->manualController.reset();
		}
		cameraManager->SetActiveCamera("MainCamera");
		activeEventCameraName_.clear();
		stageMapReloadStatus_ = "Event finished. MainCamera restored.";
	}
}

void Stage1::UpdateEventManualCamera()
{
	// Event中だけは通常Cameraではなく、このEvent Camera専用ControllerへMouse入力を渡します。
	if (activeEventCameraName_.empty() || !player_) {
		return;
	}

	const auto activeCameraFound = std::find_if(
		stageEventCameras_.begin(),
		stageEventCameras_.end(),
		[this](const StageEventCamera& eventCamera)
		{
			return eventCamera.sourceName == activeEventCameraName_;
		});
	if (activeCameraFound == stageEventCameras_.end() || !activeCameraFound->manualController) {
		return;
	}

	// Event Cameraゾーン内だけ、右マウス操作でPlayerの周囲を自由に回せる
	Input* input = Input::GetInstance();
	const bool isOrbitInput = input->IsMouseButtonPressed(1);
	if (isOrbitInput) {
		activeCameraFound->manualController->AddOrbitYaw(
			-static_cast<float>(input->GetMouseX()) * 0.005f);
		activeCameraFound->manualController->AddOrbitPitch(
			-static_cast<float>(input->GetMouseY()) * 0.003f);
	}
	const float distance = std::clamp(
		activeCameraFound->manualController->GetDistance() -
			static_cast<float>(input->GetMouseWheel()) * 0.005f,
		3.0f,
		20.0f);
	activeCameraFound->manualController->SetDistance(distance);
	activeCameraFound->manualController->Update(
		DirectXCommon::GetInstance()->GetDeltaTime(),
		player_->GetPosition(),
		player_->GetMoveDirection(),
		isOrbitInput,
		stageSolidObbs_);
}

void Stage1::UpdateCameraAreas()
{
	// Camera Areaは見えないOBBです。Playerが入った最初のAreaだけ通常Camera設定へ反映します。
	if (!player_ || !cameraController_) {
		return;
	}

	const Sphere playerSphere = player_->GetCollider();
	StageCameraArea* requestedArea = nullptr;
	for (StageCameraArea& cameraArea : stageCameraAreas_) {
		cameraArea.collider = Collision::MakeOBB(
			cameraArea.transform,
			cameraArea.colliderLocalCenter,
			cameraArea.colliderLocalHalfSize);
		cameraArea.isPlayerInside =
			Collision::SphereOBB(playerSphere, cameraArea.collider).isCollision;
		if (cameraArea.isPlayerInside && !requestedArea) {
			requestedArea = &cameraArea;
		}
	}

	if (requestedArea) {
		if (activeCameraAreaName_ != requestedArea->sourceName) {
			cameraController_->SetAreaSettings(requestedArea->settings);
			activeCameraAreaName_ = requestedArea->sourceName;
			stageMapReloadStatus_ = "Camera Area active: " + activeCameraAreaName_;
		}
	} else if (!activeCameraAreaName_.empty()) {
		cameraController_->ClearAreaSettings();
		activeCameraAreaName_.clear();
		stageMapReloadStatus_ = "Camera Area finished. Automatic camera restored.";
	}
}

void Stage1::UpdateStageEventCamera(
	StageEventCamera& eventCamera,
	const LevelLoader::ObjectData& objectData)
{
	// Event Cameraは、JSONにrotationを書くか、focusを見るかの二つの書き方を選べます。
	if (!eventCamera.camera) {
		return;
	}

	eventCamera.camera->SetTranslate(objectData.translation);
	eventCamera.hasFocus = objectData.hasCameraFocus;
	eventCamera.focus = objectData.cameraFocus;

	if (!eventCamera.hasFocus) {
		eventCamera.camera->SetRotate(objectData.rotation);
		return;
	}

	// Cameraの位置からFocusを見る方向を、X軸・Y軸の回転角へ変換する
	const Vector3 lookDirection = Normalize({
		eventCamera.focus.x - objectData.translation.x,
		eventCamera.focus.y - objectData.translation.y,
		eventCamera.focus.z - objectData.translation.z,
	});
	const float pitch =
		-std::asin(std::clamp(lookDirection.y, -1.0f, 1.0f));
	const float yaw = std::atan2(lookDirection.x, lookDirection.z);
	eventCamera.camera->SetRotate({ pitch, yaw, 0.0f });
}

void Stage1::UpdateStageMapPaths(float deltaTime)
{
	// JSONのcontrol_pointsを持つObjectだけ、経過時間から曲線上の座標を計算して移動します。
	for (StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
		if (!runtimeObject.visual || runtimeObject.controlPoints.size() < 2) {
			continue;
		}

		runtimeObject.pathProgress +=
			(std::max)(runtimeObject.pathSpeed, 0.0f) * deltaTime;
		const int segmentCount = runtimeObject.pathLoop
			? static_cast<int>(runtimeObject.controlPoints.size())
			: static_cast<int>(runtimeObject.controlPoints.size()) - 1;
		if (!runtimeObject.pathLoop) {
			runtimeObject.pathProgress = (std::min)(
				runtimeObject.pathProgress,
				static_cast<float>(segmentCount));
		}

		// 制御点はObjectの基準位置からの相対座標として扱う
		const Vector3 pathOffset = EvaluateControlPointPath(
			runtimeObject.controlPoints,
			runtimeObject.pathProgress,
			runtimeObject.pathLoop);
		runtimeObject.visual->SetTranslate({
			runtimeObject.pathBasePosition.x + pathOffset.x,
			runtimeObject.pathBasePosition.y + pathOffset.y,
			runtimeObject.pathBasePosition.z + pathOffset.z,
		});
	}
}

bool Stage1::AddStageMapPathSphere()
{
	if (!stageMapData_) {
		stageMapReloadStatus_ = "Add failed. No map data is loaded.";
		return false;
	}

	int number = 1;
	std::string objectName;
	do {
		objectName = "PathSphere" + std::to_string(number++);
	} while (std::any_of(
		stageMapData_->objects.begin(),
		stageMapData_->objects.end(),
		[&](const LevelLoader::ObjectData& objectData)
		{
			return objectData.name == objectName;
		}));

	LevelLoader::ObjectData objectData{};
	objectData.type = "MESH";
	objectData.name = objectName;
	objectData.tag = "MapObject";
	objectData.objectType = "PATH_OBJECT";
	objectData.fileName = "sphere.obj";
	objectData.translation = player_ ? player_->GetPosition() : Vector3{};
	objectData.translation.x -= 4.0f;
	objectData.translation.y += 1.5f;
	objectData.scaling = { 1.0f, 1.0f, 1.0f };
	objectData.controlPoints = {
		{ 0.0f, 0.0f, 0.0f },
		{ 2.0f, 1.0f, 2.0f },
		{ -2.0f, 2.0f, 4.0f },
		{ 0.0f, 0.0f, 6.0f },
	};
	objectData.pathSpeed = 1.0f;
	objectData.pathLoop = true;
	objectData.hasCollider = true;
	objectData.collider.type = "BOX";
	objectData.collider.center = { 0.0f, 0.0f, 0.0f };
	objectData.collider.size = { 1.0f, 1.0f, 1.0f };

	stageMapData_->objects.push_back(std::move(objectData));
	selectedStageMapObjectIndex_ =
		static_cast<int>(stageMapData_->objects.size()) - 1;
	if (!ApplyStageMapData(true)) {
		stageMapData_->objects.pop_back();
		stageMapReloadStatus_ = "Add failed. Path sphere could not be created.";
		return false;
	}

	stageMapReloadStatus_ = "Added control point path sphere.";
	return true;
}
