#include "Stage1.h"

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
#include <dinput.h>
#include <functional>
#include <numbers>

using namespace MyMath;

namespace
{
	constexpr const char* kStageMapFilePath = "resources/levels/stage1.json";
	constexpr const char* kStageMapFileName = "stage1";

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

	// 鏡の映像を描画するために使う、二台目のカメラです。
	// この段階では画面へ描画せず、位置と向きだけを更新します。
	reflectionCamera_ = std::make_unique<Camera>();
	cameraManager->AddCamera("ReflectionCamera", reflectionCamera_.get());
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

	// ---------- 鏡のデータと見た目の作成 ----------
	// Mirror は「中心・法線・幅・高さ」のデータを持ちます。
	// 見た目の板は別の Object3d として用意します。
	mirror_ = Mirror({ 0.0f, 1.0f, 8.0f }, { 0.0f, 0.0f, -1.0f }, 6.0f, 6.0f);
	mirrorVisual_ = CreateObject("plane.obj");
	mirrorVisual_->SetScale({});
	SyncMirrorVisual();

	// 外部ファイルを最初に読み、以降は保存された時だけ再読込する
	stageMapHotReload_.SetFilePath(kStageMapFilePath);
	ReloadStageMap();
	stageMapHotReload_.Synchronize();
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
	activeEventCameraName_.clear();
	sceneObjects_.clear();
	stageMapRuntimeObjects_.clear();
	stageMapData_.reset();
	floor_ = nullptr;
	mirrorVisual_.reset();
	cameraController_.reset();
	reflectionCamera_.reset();
	player_.reset();
}

void Stage1::Update()
{
	// ---------- Stage1マップのホットリロード ----------
	UpdateStageMapHotReload();
	UpdateStageMapPaths(DirectXCommon::GetInstance()->GetDeltaTime());

	// ---------- プレイヤーの移動と重力 ----------
	if (player_ && floor_ && mirrorVisual_) {
		//floor.objの大きさとTransformから、見た目と一致するOBBを作る
		floorObb_ = Collision::MakeOBB(
			floor_->GetTransform(),
			floorColliderLocalCenter_,
			floorLocalHalfSize_);
		mirrorObb_ = Collision::MakeOBB(
			mirrorVisual_->GetTransform(),
			mirrorColliderLocalCenter_,
			mirrorLocalHalfSize_);

		//床・鏡・JSONで追加したオブジェクトを、ぶつかれるOBBとしてまとめる
		std::vector<MyMath::OBB> solidObbs{ floorObb_, mirrorObb_ };
		for (StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
			if (!runtimeObject.visual || !runtimeObject.hasBoxCollider) {
				continue;
			}
			runtimeObject.collider = Collision::MakeOBB(
				runtimeObject.visual->GetTransform(),
				runtimeObject.colliderLocalCenter,
				runtimeObject.colliderLocalHalfSize);
			solidObbs.push_back(runtimeObject.collider);
		}
		player_->Update(DirectXCommon::GetInstance()->GetDeltaTime(), solidObbs);
	}

	// ---------- カメラとデバッグ UI の更新 ----------
	UpdateMainCamera();
	UpdateStageEvents();
	cameraManager->Update();
	UpdateReflectionCamera();
	ImGuiManager::GetInstance()->Begin("Stage1");
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
	if (mirrorVisual_) {
		UpdateObject(*mirrorVisual_);
	}
	if (player_) {
		UpdateObject(player_->GetObject());
	}
}

void Stage1::Draw()
{
	// ---------- ゲーム画面への描画 ----------
	DirectXCommon::GetInstance()->PreDraw();
	SrvManager::GetInstance()->PreDraw();

	object3dCommon->SetCommonDrawSetting();
	for (const auto& object : sceneObjects_) {
		object->Draw();
	}
	for (const StageMapRuntimeObject& runtimeObject : stageMapRuntimeObjects_) {
		if (runtimeObject.visual) {
			runtimeObject.visual->Draw();
		}
	}
	if (mirrorVisual_) {
		mirrorVisual_->Draw();
	}
	if (player_) {
		player_->GetObject().Draw();
	}

	//Post Effectが有効なときは、SceneのRenderTextureへ効果を適用してからGame Viewへ表示する
	const bool isPostEffectEnabled = PostEffect::GetInstance()->IsEnabled();
	if (isPostEffectEnabled) {
		DirectXCommon::GetInstance()->PreDrawForPostEffectTexture();
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), true);
	}

	DirectXCommon::GetInstance()->PreDrawForSwapChain(isPostEffectEnabled);
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

void Stage1::SyncMirrorVisual()
{
	if (!mirrorVisual_) {
		return;
	}

	// plane.obj は -1 ～ +1 の大きさなので、幅の半分を Scale に設定すると
	// 実際の幅が mirror_.GetWidth() と一致します。
	mirrorVisual_->SetTranslate(mirror_.GetCenter());
	mirrorVisual_->SetScale({ mirror_.GetWidth() * 0.5f, mirror_.GetHeight() * 0.5f, 1.0f });
	mirrorVisual_->SetRotate({ 0.0f, mirrorYaw_, 0.0f });
}

void Stage1::UpdateReflectionCamera()
{
	if (!mainCamera || !reflectionCamera_) {
		return;
	}

	// 通常カメラの位置を鏡面の反対側へ移します。
	const Vector3 reflectionPosition = mirror_.ReflectPoint(mainCamera->GetTranslate());

	// 通常カメラが見る方向も、鏡で跳ね返した方向へ変更します。
	const Vector3 reflectionForward = mirror_.ReflectDirection(GetCameraForward(*mainCamera));
	const float clampedY = std::clamp(reflectionForward.y, -1.0f, 1.0f);
	const float reflectionPitch = -std::asin(clampedY);
	const float reflectionYaw = std::atan2(reflectionForward.x, reflectionForward.z);

	reflectionCamera_->SetTranslate(reflectionPosition);
	reflectionCamera_->SetRotate({ reflectionPitch, reflectionYaw, 0.0f });
	reflectionCamera_->Update();
}

void Stage1::UpdateMainCamera()
{
	if (!player_ || !cameraController_) {
		return;
	}

	//右マウスドラッグで高さを調整し、ホイールでプレイヤーとの距離を調整する
	Input* input = Input::GetInstance();
	if (input->IsMouseButtonPressed(1)) {
		const float height = std::clamp(
			cameraController_->GetHeight() - static_cast<float>(input->GetMouseY()) * 0.02f,
			1.0f,
			12.0f);
		cameraController_->SetHeight(height);
	}
	const float distance = std::clamp(
		cameraController_->GetDistance() - static_cast<float>(input->GetMouseWheel()) * 0.005f,
		3.0f,
		20.0f);
	cameraController_->SetDistance(distance);

	// Player の向きではなく、Player の位置だけを渡して CameraController が追従させる
	cameraController_->Update(
		DirectXCommon::GetInstance()->GetDeltaTime(),
		player_->GetPosition());
}

Vector3 Stage1::GetCameraForward(const Camera& camera) const
{
	// このプロジェクトでは、カメラのローカル座標 +Z が正面です。
	const Matrix4x4& worldMatrix = camera.GetWorldMatrix();
	return Normalize({ worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2] });
}

void Stage1::DrawMirrorDebugUi()
{
	// ImGui の詳細は ImGuiManager へまとめ、Stage1 は変更結果だけを受け取ります。
	if (ImGuiManager::GetInstance()->MirrorDebugWindow(mirror_, mirrorYaw_, *reflectionCamera_)) {
		SyncMirrorVisual();

		// 従来の鏡Inspectorで編集した値も、Save MapできるLevelDataへ同期する
		if (stageMapData_) {
			std::function<bool(std::vector<LevelLoader::ObjectData>&)> syncMirrorData;
			syncMirrorData = [&](std::vector<LevelLoader::ObjectData>& objects)
			{
				for (LevelLoader::ObjectData& objectData : objects) {
					if (objectData.tag == "Mirror") {
						objectData.translation = mirror_.GetCenter();
						objectData.rotation.y = mirrorYaw_;
						objectData.scaling.x = mirror_.GetWidth() * 0.5f;
						objectData.scaling.y = mirror_.GetHeight() * 0.5f;
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
	if (!player_ || !floor_ || !mirrorVisual_) {
		return;
	}

	//同じ球に対して、床と鏡をそれぞれ個別に判定する
	const Sphere playerSphere = player_->GetCollider();
	const Collision::CollisionInfo floorCollision =
		Collision::SphereOBB(playerSphere, floorObb_);
	const Collision::CollisionInfo mirrorCollision =
		Collision::SphereOBB(playerSphere, mirrorObb_);

	//床のOBBを、当たっているときは赤、当たっていないときは青で表示する
	ImGuiManager::GetInstance()->DrawObbCollisionDebug(
		floorObb_,
		playerSphere,
		cameraManager->GetActiveCamera(),
		floorCollision.isCollision);

	//鏡のOBBを、当たっているときは赤、当たっていないときは青で表示する
	ImGuiManager::GetInstance()->DrawObbCollisionDebug(
		mirrorObb_,
		playerSphere,
		cameraManager->GetActiveCamera(),
		mirrorCollision.isCollision);

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
	if (!stageMapData_ || !floor_ || !mirrorVisual_) {
		return false;
	}

	const LevelLoader::ObjectData* floorData = nullptr;
	const LevelLoader::ObjectData* mirrorData = nullptr;
	std::vector<const LevelLoader::ObjectData*> additionalObjects;
	std::vector<const LevelLoader::ObjectData*> eventTriggerDataList;
	std::vector<const LevelLoader::ObjectData*> eventCameraDataList;
	std::function<void(const std::vector<LevelLoader::ObjectData>&)> collectObjects;
	collectObjects = [&](const std::vector<LevelLoader::ObjectData>& objects)
	{
		for (const LevelLoader::ObjectData& objectData : objects) {
			if (objectData.tag == "Floor") {
				floorData = &objectData;
			} else if (objectData.tag == "Mirror") {
				mirrorData = &objectData;
			} else if (objectData.objectType == "EVENT_TRIGGER") {
				eventTriggerDataList.push_back(&objectData);
			} else if (objectData.objectType == "EVENT_CAMERA") {
				eventCameraDataList.push_back(&objectData);
			} else if (objectData.type == "MESH" && !objectData.fileName.empty()) {
				additionalObjects.push_back(&objectData);
			}
			collectObjects(objectData.children);
		}
	};
	collectObjects(stageMapData_->objects);

	// Stage1で必須の床と鏡がなければ、何も変更しない
	if (!floorData || !mirrorData) {
		return false;
	}

	// モデル追加・削除時だけObject3dの一覧を作り直す
	std::vector<StageMapRuntimeObject> rebuiltObjects;
	std::vector<StageEventTrigger> rebuiltEventTriggers;
	std::vector<StageEventCamera> rebuiltEventCameras;
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
	mirrorYaw_ = mirrorData->rotation.y;
	mirror_.SetCenter(mirrorData->translation);
	mirror_.SetSize(
		std::abs(mirrorData->scaling.x) * 2.0f,
		std::abs(mirrorData->scaling.y) * 2.0f);
	mirror_.SetNormal({ std::sin(mirrorYaw_), 0.0f, std::cos(mirrorYaw_) });
	if (mirrorData->hasCollider && mirrorData->collider.type == "BOX") {
		mirrorColliderLocalCenter_ = mirrorData->collider.center;
		mirrorLocalHalfSize_ = {
			std::abs(mirrorData->collider.size.x) * 0.5f,
			std::abs(mirrorData->collider.size.y) * 0.5f,
			std::abs(mirrorData->collider.size.z) * 0.5f,
		};
	}
	SyncMirrorVisual();

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
		activeEventCameraName_.clear();
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
	}

	return true;
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
			cameraManager->SetActiveCamera(requestedCameraName);
			activeEventCameraName_ = requestedCameraName;
			stageMapReloadStatus_ =
				"Event Camera active: " + requestedCameraName;
		}
	} else if (!activeEventCameraName_.empty()) {
		// すべてのTriggerから出たら、通常の追従Cameraへ戻す
		cameraManager->SetActiveCamera("MainCamera");
		activeEventCameraName_.clear();
		stageMapReloadStatus_ = "Event finished. MainCamera restored.";
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
