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

	// ---------- プレイヤーの作成 ----------
	//球をプレイヤーとして使用し、床の上から開始します。
	player_ = std::make_unique<Player>();
	player_->Initialize(object3dCommon, "sphere.obj", { 0.0f, -0.8f, 5.0f }, 1.2f);
	// Camera 本体とは別の Controller に、Player を追従するルールを任せる
	cameraController_ = std::make_unique<CameraController>();
	cameraController_->Initialize(mainCamera.get(), player_->GetPosition());
	// 通常Cameraの向きはPlayerの移動方向へ勝手に回さず、矢印キーで選んだ位置を保つ
	cameraController_->SetAutoRecenterEnabled(false);

	// ---------- 持てる小型鏡とレーザーの作成 ----------
	carryableMirror_ = std::make_unique<CarryableMirror>();
	carryableMirror_->Initialize(
		object3dCommon,
		"plane.obj",
		{ -2.5f, -0.8f, 4.5f },
		3.0f,
		3.0f);
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
		laserRenderer_->SetBeamWidth(laserCollisionRadius_ * 2.0f);
	}

	// 外部ファイルを最初に読み、以降は保存された時だけ再読込する
	stageMapHotReload_.SetFilePath(kStageMapFilePath);
	ReloadStageMap();
	stageMapHotReload_.Synchronize();
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
	fixedMirrors_.clear();
	carryableMirror_.reset();
	laserRenderer_.reset();
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
	if (player_ && floor_) {
		//floor.objの大きさとTransformから、見た目と一致するOBBを作る
		floorObb_ = Collision::MakeOBB(
			floor_->GetTransform(),
			floorColliderLocalCenter_,
			floorLocalHalfSize_);
		// 床・鏡・JSONで追加したオブジェクトを、PlayerとCameraが使うOBBとしてまとめる
		stageSolidObbs_ = { floorObb_ };
		for (const auto& fixedMirror : fixedMirrors_) {
			if (fixedMirror) {
				stageSolidObbs_.push_back(fixedMirror->GetCollider());
			}
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
		}
		if (gameplaySmokeEnabled_) {
			// 自動検証ではCameraに影響されない世界+X方向へ歩かせます。
			Player::ControlInput smokeControl{};
			smokeControl.right = 1.0f;
			player_->UpdateWithControl(
				DirectXCommon::GetInstance()->GetDeltaTime(),
				stageSolidObbs_,
				{ 0.0f, 0.0f, 1.0f },
				smokeControl);
		} else if (ImGuiManager::GetInstance()->IsGameViewActive()) {
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
	UpdateMirrorGameplay();

	// ---------- カメラとデバッグ UI の更新 ----------
	if (ImGuiManager::GetInstance()->IsGameViewActive()) {
		UpdateMainCamera();
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
			isPlayerHitByLaser_);
	}
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
		fixedMirror->DrawSurface();
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
	return object;
}

void Stage1::UpdateObject(Object3d& object)
{
	// 毎フレームのカメラ位置、照明、行列を Object3d へ反映します。
	object.SetCamera(cameraManager->GetActiveCamera());
	object.SetDirectionalLight(directionalLight_);
	object.SetPointLight(pointLight_);
	object.Update();
}

void Stage1::DrawFixedMirrorReflections()
{
	if (fixedMirrors_.empty()) {
		return;
	}

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	// 鏡が増えても反射Sceneの再描画は一フレームに一枚だけ行います。
	const size_t updateCount = fixedMirrors_.size();
	for (size_t attempt = 0; attempt < updateCount; ++attempt) {
		const size_t mirrorIndex = reflectionUpdateCursor_ % updateCount;
		reflectionUpdateCursor_ = (reflectionUpdateCursor_ + 1) % updateCount;
		FixedMirror* fixedMirror = fixedMirrors_[mirrorIndex].get();
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

		fixedMirror->EndReflection();
		RestoreSceneCameraMatrices();
		break;
	}
}

void Stage1::RestoreSceneCameraMatrices()
{
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
}

void Stage1::UpdateMirrorGameplay()
{
	if (!player_ || !carryableMirror_) {
		return;
	}

	const bool interactPressed =
		ImGuiManager::GetInstance()->IsGameViewActive() &&
		Input::GetInstance()->TriggerKey(DIK_E);
	carryableMirror_->Update(
		player_->GetPosition(),
		player_->GetFacingYaw(),
		interactPressed);

	// レーザー側は鏡の種類を知らず、共通のMirror面として二種類を扱います。
	std::vector<const Mirror*> laserMirrors;
	laserMirrors.reserve(fixedMirrors_.size() + 1);
	for (const auto& fixedMirror : fixedMirrors_) {
		if (fixedMirror) {
			laserMirrors.push_back(&fixedMirror->GetMirror());
		}
	}
	laserMirrors.push_back(&carryableMirror_->GetMirror());
	laser_.SetOrigin(laserOrigin_);
	laser_.SetDirection(laserDirection_);
	laser_.Update(laserMirrors);

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

void Stage1::InitializeGameplaySmoke()
{
	gameplaySmokeEnabled_ = IsEnvironmentEnabled("CG2_STAGE1_GAMEPLAY_SMOKE");
	if (!gameplaySmokeEnabled_ || !player_ || !carryableMirror_) {
		return;
	}

	gameplaySmokeStartY_ = player_->GetPosition().y;
	const Vector3 mirrorPosition = carryableMirror_->GetMirror().GetCenter();

	// 鏡の中心にPlayerがいる条件を渡し、Eキーと同じ拾う処理を直接確認します。
	carryableMirror_->Update(mirrorPosition, 0.0f, true);
	gameplaySmokePickedUpMirror_ = carryableMirror_->IsCarried();
	// 持った状態のままPlayer正面へ移動させてから、反射判定を行います。
	carryableMirror_->Update(mirrorPosition, 0.0f, false);

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
	carryableMirror_->Update(mirrorPosition, 0.0f, true);
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
	constexpr float testPitch = 0.58f;
	constexpr float testDistance = 11.5f;
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

	// 左右は中心から二段、遠近は近・中・遠の三段で止まることを確認します。
	const bool orbitReachedPositiveLimit =
		testController.TryStepOrbit(1, {}) &&
		!testController.TryStepOrbit(1, {}) &&
		testController.GetOrbitStepIndex() == 2;
	const bool orbitReachedNegativeLimit =
		testController.TryStepOrbit(-1, {}) &&
		testController.TryStepOrbit(-1, {}) &&
		testController.TryStepOrbit(-1, {}) &&
		testController.TryStepOrbit(-1, {}) &&
		!testController.TryStepOrbit(-1, {}) &&
		testController.GetOrbitStepIndex() == -2;
	const bool distanceReachedFarLimit =
		testController.TryStepDistance(1, {}) &&
		!testController.TryStepDistance(1, {}) &&
		testController.GetDistanceStepIndex() == 2;
	const bool distanceReachedNearLimit =
		testController.TryStepDistance(-1, {}) &&
		testController.TryStepDistance(-1, {}) &&
		!testController.TryStepDistance(-1, {}) &&
		testController.GetDistanceStepIndex() == 0;
	gameplaySmokeCameraSteps_ =
		orbitReachedPositiveLimit &&
		orbitReachedNegativeLimit &&
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
		gameplaySmokeCarriedMirrorBlocksPlayer_ &&
		gameplaySmokeLaserHitsPlayer_ &&
		gameplaySmokeMirrorBlocksPlayer_ &&
		gameplaySmokeFixedMirrorReflection_ &&
		gameplaySmokeCameraSteps_ &&
		gameplaySmokeCameraWallBlock_ &&
		gameplaySmokeCameraSmooth_ &&
		gameplaySmokeSawGrounded_ &&
		gameplaySmokeLeftFloor_ &&
		gameplaySmokeFell_;
	std::ofstream log("logs/stage1_gameplay_smoke.log", std::ios::trunc);
	if (log) {
		log << (success ? "SUCCESS" : "FAILURE")
			<< ": picked=" << gameplaySmokePickedUpMirror_
			<< " dropped=" << gameplaySmokeDroppedMirror_
			<< " carryLaser=" << gameplaySmokeCarryMirrorReflectedLaser_
			<< " carriedMirrorBlocksPlayer=" << gameplaySmokeCarriedMirrorBlocksPlayer_
			<< " laserHitsPlayer=" << gameplaySmokeLaserHitsPlayer_
			<< " mirrorBlocksPlayer=" << gameplaySmokeMirrorBlocksPlayer_
			<< " fixedMirrorReflection=" << gameplaySmokeFixedMirrorReflection_
			<< " cameraSteps=" << gameplaySmokeCameraSteps_
			<< " cameraWall=" << gameplaySmokeCameraWallBlock_
			<< " cameraSmooth=" << gameplaySmokeCameraSmooth_
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
	if (!stageMapData_ || !floor_) {
		return false;
	}

	const LevelLoader::ObjectData* floorData = nullptr;
	std::vector<const LevelLoader::ObjectData*> mirrorDataList;
	std::vector<const LevelLoader::ObjectData*> additionalObjects;
	std::vector<const LevelLoader::ObjectData*> eventTriggerDataList;
	std::vector<const LevelLoader::ObjectData*> eventCameraDataList;
	std::vector<const LevelLoader::ObjectData*> cameraAreaDataList;
	std::function<void(const std::vector<LevelLoader::ObjectData>&)> collectObjects;
	collectObjects = [&](const std::vector<LevelLoader::ObjectData>& objects)
	{
		for (const LevelLoader::ObjectData& objectData : objects) {
			if (objectData.tag == "Floor") {
				floorData = &objectData;
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

	// Stage1で必須の床と鏡がなければ、何も変更しない
	if (!floorData || mirrorDataList.empty()) {
		return false;
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
