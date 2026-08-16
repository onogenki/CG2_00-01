#include "CarryableMirror.h"

#include "Collision.h"
#include "MyMath.h"
#include "Object3dCommon.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

void CarryableMirror::Initialize(
	Object3dCommon* object3dCommon,
	const std::string& modelName,
	const Vector3& startPosition,
	float width,
	float height)
{
	width_ = (std::max)(width, 0.1f);
	height_ = (std::max)(height, 0.1f);
	object_.Initialize(object3dCommon);
	object_.SetModel(modelName);
	// 小型鏡は景色を映さず、銀色に近い明るい板として表示します。
	object_.SetTextureOverride("resources/white.png");
	ApplyTransform(startPosition, 3.14159265f);
}

void CarryableMirror::Update(
	float deltaTime,
	const Vector3& playerPosition,
	float playerFacingYaw,
	bool interactPressed,
	bool isAiming,
	float horizontalAimInput)
{
	if (interactPressed) {
		if (isCarried_) {
			isCarried_ = false;
		} else {
			const Vector3 difference{
				mirror_.GetCenter().x - playerPosition.x,
				mirror_.GetCenter().y - playerPosition.y,
				mirror_.GetCenter().z - playerPosition.z,
			};
			if (Length(difference) <= pickupDistance_) {
				isCarried_ = true;
				aimYawOffset_ = 0.0f;
			}
		}
	}

	if (isCarried_) {
		if (isAiming) {
			// Mouseを右へ動かすとMirrorもPlayerの右前方へ動きます。
			aimYawOffset_ += horizontalAimInput * aimSensitivity_;
			aimYawOffset_ = std::clamp(aimYawOffset_, -maxAimYawOffset_, maxAimYawOffset_);
		} else {
			// 構える操作を終えると、MirrorはPlayerの正面へ滑らかに戻ります。
			const float returnRate = 1.0f - std::exp(-holdTurnFollowSpeed_ * (std::max)(deltaTime, 0.0f));
			aimYawOffset_ += (0.0f - aimYawOffset_) * returnRate;
		}

		const float heldYaw = playerFacingYaw + aimYawOffset_;
		const Vector3 playerForward{
			std::sin(heldYaw),
			0.0f,
			std::cos(heldYaw),
		};
		const Vector3 heldPosition{
			playerPosition.x + playerForward.x * holdDistance_,
			playerPosition.y + holdHeight_,
			playerPosition.z + playerForward.z * holdDistance_,
		};
		// 板の表側がPlayerと反対方向を向くよう、Playerの正面へ180度足します。
		const float targetYaw = heldYaw + 3.14159265f;
		// 角度の差を-π～πへ収めると、359度から0度へ回る時も遠回りしません。
		const float yawDifference = std::remainder(targetYaw - yaw_, 2.0f * 3.14159265f);
		// 指数補間で、フレームレートに依存せず自然に鏡の向きだけを追従させます。
		const float followSpeed = isAiming ? holdTurnFollowSpeed_ * 2.5f : holdTurnFollowSpeed_;
		const float followRate = 1.0f - std::exp(-followSpeed * (std::max)(deltaTime, 0.0f));
		const float smoothedYaw = yaw_ + yawDifference * followRate;
		ApplyTransform(heldPosition, smoothedYaw);
	} else {
		// 落とした後もColliderを現在のTransformへ追従させます。
		collider_ = Collision::MakeOBB(object_.GetTransform(), colliderLocalHalfSize_);
	}
}

void CarryableMirror::ApplyTransform(const Vector3& position, float yaw)
{
	yaw_ = yaw;
	object_.SetTranslate(position);
	object_.SetRotate({ 0.0f, yaw_, 0.0f });
	object_.SetScale({ width_ * 0.5f, height_ * 0.5f, 1.0f });

	mirror_.SetCenter(position);
	mirror_.SetSize(width_, height_);
	mirror_.SetNormal({ std::sin(yaw_), 0.0f, std::cos(yaw_) });
	collider_ = Collision::MakeOBB(object_.GetTransform(), colliderLocalHalfSize_);
}
