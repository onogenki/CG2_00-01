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
	// 小型Mirrorも表側だけLaserを反射します。
	mirror_.SetReflectBackface(false);
	ApplyTransform(startPosition, 0.0f, 3.14159265f);
}

void CarryableMirror::Update(
	float deltaTime,
	const Vector3& playerPosition,
	float playerFacingYaw,
	bool interactPressed,
	bool isAiming,
	float horizontalAimInput,
	bool isRightMouseHeld,
	float verticalAimInput)
{
	if (interactPressed) {
		if (isCarried_) {
			isCarried_ = false;
			wasRightMouseHeld_ = false;
			rightMouseHoldTime_ = 0.0f;
		} else {
			const Vector3 difference{
				mirror_.GetCenter().x - playerPosition.x,
				mirror_.GetCenter().y - playerPosition.y,
				mirror_.GetCenter().z - playerPosition.z,
			};
			if (Length(difference) <= pickupDistance_) {
				isCarried_ = true;
				aimYawOffset_ = 0.0f;
				isHorizontalHoldMode_ = false;
				horizontalTilt_ = 0.0f;
			}
		}
	}

	if (isCarried_) {
		// 左クリックは常にPlayer全体を守る、縦向きの通常Mirrorへ戻します。
		if (isAiming) {
			isHorizontalHoldMode_ = false;
		}
		if (isRightMouseHeld) {
			rightMouseHoldTime_ += (std::max)(deltaTime, 0.0f);
			if (isHorizontalHoldMode_) {
				// Mouseを下へ動かすと、下から来たLightをPlayer前方へ返す向きへ傾きます。
				horizontalTilt_ -= verticalAimInput * horizontalTiltMouseSensitivity_;
				horizontalTilt_ = std::clamp(horizontalTilt_, -maxHorizontalTilt_, maxHorizontalTilt_);
			}
		} else if (wasRightMouseHeld_ && rightMouseHoldTime_ <= rightMouseClickThreshold_) {
			// 短い右クリックを離した時だけ、上向き・下向きの水平Mirrorを切り替えます。
			isHorizontalHoldMode_ = true;
			isHorizontalFacingUp_ = !isHorizontalFacingUp_;
			horizontalTilt_ = 0.0f;
		}
		if (!isRightMouseHeld) {
			rightMouseHoldTime_ = 0.0f;
		}
		wasRightMouseHeld_ = isRightMouseHeld;

		const bool isRotatingHeldMirror = isAiming || isRightMouseHeld;
		if (isRotatingHeldMirror) {
			// Mouseを右へ動かすと、縦向き・水平MirrorともPlayerの右前方へ動きます。
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
		const float heldDistance = isHorizontalHoldMode_ ? horizontalHoldDistance_ : holdDistance_;
		const float heldHeight = isHorizontalHoldMode_ ? holdHeight_ * 0.70f : holdHeight_;
		const Vector3 heldPosition{
			playerPosition.x + playerForward.x * heldDistance,
			playerPosition.y + heldHeight,
			playerPosition.z + playerForward.z * heldDistance,
		};
		// Playerの正面から来るLaserを反射するため、板の表側もPlayerの正面へ向けます。
		const float targetYaw = heldYaw;
		// 角度の差を-π～πへ収めると、359度から0度へ回る時も遠回りしません。
		const float yawDifference = std::remainder(targetYaw - yaw_, 2.0f * 3.14159265f);
		// 指数補間で、フレームレートに依存せず自然に鏡の向きだけを追従させます。
		const float followSpeed = isRotatingHeldMirror ? holdTurnFollowSpeed_ * 2.5f : holdTurnFollowSpeed_;
		const float followRate = 1.0f - std::exp(-followSpeed * (std::max)(deltaTime, 0.0f));
		const float smoothedYaw = yaw_ + yawDifference * followRate;
		const float horizontalBasePitch = isHorizontalFacingUp_ ? -1.57079633f : 1.57079633f;
		const float targetPitch = isHorizontalHoldMode_ ? horizontalBasePitch + horizontalTilt_ : 0.0f;
		ApplyTransform(heldPosition, targetPitch, smoothedYaw);
	} else {
		// 落とした後もColliderを現在のTransformへ追従させます。
		wasRightMouseHeld_ = false;
		rightMouseHoldTime_ = 0.0f;
		collider_ = Collision::MakeOBB(object_.GetTransform(), colliderLocalHalfSize_);
	}
}

void CarryableMirror::SetDroppedPosition(const Vector3& position)
{
	if (isCarried_) {
		// 持っている最中のJSON再読込で、手元のMirrorを地面へ戻さないようにします。
		return;
	}

	ApplyTransform(position, 0.0f, 3.14159265f);
}

void CarryableMirror::ApplyTransform(const Vector3& position, float pitch, float yaw)
{
	yaw_ = yaw;
	object_.SetTranslate(position);
	object_.SetRotate({ pitch, yaw_, 0.0f });
	object_.SetScale({ width_ * 0.5f, height_ * 0.5f, 1.0f });

	mirror_.SetCenter(position);
	mirror_.SetSize(width_, height_);
	const Matrix4x4 rotationMatrix = MakeAffineMatrix(
		{ 1.0f, 1.0f, 1.0f },
		{ pitch, yaw_, 0.0f },
		{ 0.0f, 0.0f, 0.0f });
	mirror_.SetNormal({
		rotationMatrix.m[2][0],
		rotationMatrix.m[2][1],
		rotationMatrix.m[2][2],
	});
	collider_ = Collision::MakeOBB(object_.GetTransform(), colliderLocalHalfSize_);
}
