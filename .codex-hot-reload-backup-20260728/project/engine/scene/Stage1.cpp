#include "Stage1.h"

#include "DirectXCommon.h"
#include "Collision.h"
#include "Input.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "PostEffect.h"
#include "SrvManager.h"
#include <algorithm>
#include <cmath>
#include <dinput.h>
#include <numbers>

using namespace MyMath;

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
}

void Stage1::Finalize()
{
	// unique_ptr がオブジェクトを自動的に解放します。
	// SceneManager がこの関数の前に GPU の処理完了を待機します。
	sceneObjects_.clear();
	floor_ = nullptr;
	mirrorVisual_.reset();
	cameraController_.reset();
	reflectionCamera_.reset();
	player_.reset();
}

void Stage1::Update()
{
	// ---------- プレイヤーの移動と重力 ----------
	if (player_ && floor_ && mirrorVisual_) {
		//floor.objの大きさとTransformから、見た目と一致するOBBを作る
		floorObb_ = Collision::MakeOBB(floor_->GetTransform(), floorLocalHalfSize_);
		mirrorObb_ = Collision::MakeOBB(mirrorVisual_->GetTransform(), mirrorLocalHalfSize_);

		//床と鏡を、プレイヤーがぶつかれるOBBとしてまとめる
		const std::vector<MyMath::OBB> solidObbs{
			floorObb_,
			mirrorObb_,
		};
		player_->Update(DirectXCommon::GetInstance()->GetDeltaTime(), solidObbs);
	}

	// ---------- カメラとデバッグ UI の更新 ----------
	UpdateMainCamera();
	cameraManager->Update();
	UpdateReflectionCamera();
	ImGuiManager::GetInstance()->Begin("Stage1");
	DrawMirrorDebugUi();
	DrawCollisionDebugUi();
	ImGuiManager::GetInstance()->End();

	// ---------- 3D オブジェクトの更新 ----------
	for (const auto& object : sceneObjects_) {
		UpdateObject(*object);
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
}
