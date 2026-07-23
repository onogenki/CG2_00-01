#include "CameraController.h"

#include "Camera.h"
#include "MyMath.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

void CameraController::Initialize(Camera* camera, const Vector3& targetPosition)
{
	// Controller は Camera を作らず、Scene から渡された Camera を操作する
	camera_ = camera;
	focus_ = {
		targetPosition.x + focusOffset_.x,
		targetPosition.y + focusOffset_.y,
		targetPosition.z + focusOffset_.z,
	};
	cameraPosition_ = CalculateTargetCameraPosition();
	ApplyCameraTransform();
}

void CameraController::Update(float deltaTime, const Vector3& targetPosition)
{
	if (!camera_) {
		return;
	}

	// Player の中心ではなく、少し上に置いた点を Focus の目標にする
	const Vector3 targetFocus{
		targetPosition.x + focusOffset_.x,
		targetPosition.y + focusOffset_.y,
		targetPosition.z + focusOffset_.z,
	};
	const Vector3 focusToTarget{
		targetFocus.x - focus_.x,
		targetFocus.y - focus_.y,
		targetFocus.z - focus_.z,
	};
	const float focusDistance = Length(focusToTarget);
	Vector3 desiredFocus = focus_;

	// 対象が Dead Zone の外へ出た時だけ、Focus の目標位置を作る
	if (focusDistance > deadZoneRadius_) {
		const Vector3 direction = Normalize(focusToTarget);
		desiredFocus = {
			targetFocus.x - direction.x * deadZoneRadius_,
			targetFocus.y - direction.y * deadZoneRadius_,
			targetFocus.z - direction.z * deadZoneRadius_,
		};
	}

	// フレームレートに左右されにくい割合で Focus をゆっくり移動させる
	const float focusT = 1.0f - std::exp(-focusFollowSpeed_ * deltaTime);
	focus_ = Lerp(focus_, desiredFocus, focusT);

	// Camera の理想位置は Focus 基準で決めるため、Player が向くだけでは移動しない
	const Vector3 targetCameraPosition = CalculateTargetCameraPosition();
	const float cameraT = 1.0f - std::exp(-cameraFollowSpeed_ * deltaTime);
	cameraPosition_ = Lerp(cameraPosition_, targetCameraPosition, cameraT);
	ApplyCameraTransform();
}

Vector3 CameraController::CalculateTargetCameraPosition() const
{
	return {
		focus_.x - std::sin(orbitYaw_) * distance_,
		focus_.y + height_,
		focus_.z - std::cos(orbitYaw_) * distance_,
	};
}

void CameraController::ApplyCameraTransform()
{
	if (!camera_) {
		return;
	}

	// Camera の位置から Focus へのベクトルを、X と Y の回転角へ変換する
	const Vector3 lookDirection = Normalize({
		focus_.x - cameraPosition_.x,
		focus_.y - cameraPosition_.y,
		focus_.z - cameraPosition_.z,
	});
	const float pitch = -std::asin(std::clamp(lookDirection.y, -1.0f, 1.0f));
	const float yaw = std::atan2(lookDirection.x, lookDirection.z);

	camera_->SetTranslate(cameraPosition_);
	camera_->SetRotate({ pitch, yaw, 0.0f });
}
