#include "Player.h"

#include "DirectXCommon.h"
#include "Collision.h"
#include "Input.h"
#include "MyMath.h"
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

void Player::Update(float deltaTime, const std::vector<OBB>& solidObbs)
{
	//WASDの入力から、床と平行な移動方向を作る
	Vector3 moveDirection{};
	Input* input = Input::GetInstance();
	if (input->PushKey(DIK_W)) {
		moveDirection.z += 1.0f;
	}
	if (input->PushKey(DIK_S)) {
		moveDirection.z -= 1.0f;
	}
	if (input->PushKey(DIK_A)) {
		moveDirection.x -= 1.0f;
	}
	if (input->PushKey(DIK_D)) {
		moveDirection.x += 1.0f;
	}

	//斜め移動だけ速くならないよう、方向だけを正規化する
	if (Length(moveDirection) > 0.0f) {
		moveDirection = Normalize(moveDirection);
		position_.x += moveDirection.x * moveSpeed_ * deltaTime;
		position_.z += moveDirection.z * moveSpeed_ * deltaTime;

		//移動方向からY軸の回転角を作り、球の模様が進行方向を向くようにする
		facingYaw_ = std::atan2(moveDirection.x, moveDirection.z);
		object_.SetRotate({ 0.0f, facingYaw_, 0.0f });
	}

	//床に立っているときだけ、Spaceで上向きの速さを与える
	if (isGrounded_ && input->TriggerKey(DIK_SPACE)) {
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
	object_.SetTranslate(position_);
}
