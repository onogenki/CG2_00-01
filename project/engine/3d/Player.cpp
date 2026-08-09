#include "Player.h"

#include "DirectXCommon.h"
#include "Collision.h"
#include "Input.h"
#include "MyMath.h"
#include <algorithm>
#include <cmath>
#include <dinput.h>

using namespace MyMath;

void Player::Initialize(Object3dCommon* object3dCommon, const std::string& modelName, const Vector3& startPosition, float radius)
{
	//球モデルを画面に描画できる状態にする
	object_.Initialize(object3dCommon);
	object_.SetModel(modelName);

	//開始位置と大きさを保存する
	position_ = startPosition;
	radius_ = radius;

	//sphere.objは半径0.5のモデルなので、当たり判定の半径と見た目の半径を揃える
	constexpr float kSphereModelRadius = 0.5f;
	const float visualScale = radius_ / kSphereModelRadius;
	object_.SetScale({ visualScale, visualScale, visualScale });
	object_.SetTranslate(position_);
}

void Player::Update(
	float deltaTime,
	const std::vector<OBB>& solidObbs,
	const Vector3& cameraForward)
{
	// Keyboard入力を機器に依存しない値へ変換してから、共通の移動処理へ渡します。
	ControlInput controlInput{};
	Input* input = Input::GetInstance();
	if (input->PushKey(DIK_W)) {
		controlInput.forward += 1.0f;
	}
	if (input->PushKey(DIK_S)) {
		controlInput.forward -= 1.0f;
	}
	if (input->PushKey(DIK_A)) {
		controlInput.right -= 1.0f;
	}
	if (input->PushKey(DIK_D)) {
		controlInput.right += 1.0f;
	}
	controlInput.jumpPressed = input->TriggerKey(DIK_SPACE);
	UpdateWithControl(deltaTime, solidObbs, cameraForward, controlInput);
}

void Player::UpdateWithControl(
	float deltaTime,
	const std::vector<OBB>& solidObbs,
	const Vector3& cameraForward,
	const ControlInput& controlInput)
{
	const float forwardInput = std::clamp(controlInput.forward, -1.0f, 1.0f);
	const float rightInput = std::clamp(controlInput.right, -1.0f, 1.0f);

	// Cameraが上下を向いていても、移動には床と平行なX-Z方向だけを使う
	Vector3 forwardOnGround{ cameraForward.x, 0.0f, cameraForward.z };
	if (Length(forwardOnGround) <= 0.0001f) {
		forwardOnGround = { 0.0f, 0.0f, 1.0f };
	}
	forwardOnGround = Normalize(forwardOnGround);
	const Vector3 rightOnGround{ forwardOnGround.z, 0.0f, -forwardOnGround.x };

	// Wなら画面奥、Dなら画面右へ向く世界座標の移動方向を作る
	moveDirection_ = {
		forwardOnGround.x * forwardInput + rightOnGround.x * rightInput,
		0.0f,
		forwardOnGround.z * forwardInput + rightOnGround.z * rightInput,
	};

	// 斜め移動だけ速くならないよう、方向だけを正規化する
	if (Length(moveDirection_) > 0.0f) {
		moveDirection_ = Normalize(moveDirection_);
		position_.x += moveDirection_.x * moveSpeed_ * deltaTime;
		position_.z += moveDirection_.z * moveSpeed_ * deltaTime;

		// 移動方向からY軸の回転角を作り、球の模様が進行方向を向くようにする
		facingYaw_ = std::atan2(moveDirection_.x, moveDirection_.z);
		object_.SetRotate({ 0.0f, facingYaw_, 0.0f });
	}

	//床に立っているときだけ、Spaceで上向きの速さを与える
	if (isGrounded_ && controlInput.jumpPressed) {
		velocity_.y = jumpSpeed_;
		isGrounded_ = false;
	}

	//重力で毎フレームのY方向の速さを変化させ、位置へ反映する
	velocity_.y += gravity_ * deltaTime;
	position_.y += velocity_.y * deltaTime;

	//球と複数のOBBを順番に判定し、めり込んだ分だけ球をOBBの外へ押し戻す
	isGrounded_ = false;
	isColliding_ = false;

	//solidObbsには、床や鏡などプレイヤーがぶつかる全てのOBBが入っている
	// 斜めのOBBや複数のOBBが重なった場面でも、球をすべての箱の外へ安定して押し戻す。
	constexpr int kCollisionSolveCount = 3;
	for (int solveIndex = 0; solveIndex < kCollisionSolveCount; ++solveIndex) {
		for (const OBB& solidObb : solidObbs)
		{
		const Sphere playerSphere{ position_,radius_ };
		const Collision::CollisionInfo collision =
			Collision::SphereOBB(playerSphere, solidObb);

		if (!collision.isCollision)
		{
			continue;
		}
		isColliding_ = true;

		position_.x += collision.normal.x * collision.penetrationDepth;
		position_.y += collision.normal.y * collision.penetrationDepth;
		position_.z += collision.normal.z * collision.penetrationDepth;

		//衝突面が上向きなら、プレイヤーは床の上に立っている
		if (collision.normal.y > 0.5f) {
			isGrounded_ = true;
		}

		//面へ向かっている速度成分だけを取り除き、着地後に落下し続けないようにする
		const float velocityTowardSurface = Dot(velocity_, collision.normal);
		if (velocityTowardSurface < 0.0f) {
			velocity_.x -= collision.normal.x * velocityTowardSurface;
			velocity_.y -= collision.normal.y * velocityTowardSurface;
			velocity_.z -= collision.normal.z * velocityTowardSurface;
		}
	}

	//計算した位置を、画面に描く球モデルへ反映する
	}

	object_.SetTranslate(position_);
}
