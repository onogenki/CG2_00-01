#pragma once

#include "Vector3.h"

class Camera;

// Player や敵などの対象を、少し遅れて追いかける三人称カメラ用の操作クラス
class CameraController
{
public:
	// 操作する Camera と、最初に追いかける対象の位置を登録する
	void Initialize(Camera* camera, const Vector3& targetPosition);
	// 対象の現在位置を受け取り、Focus と Camera を更新する
	void Update(float deltaTime, const Vector3& targetPosition);

	// カメラの高さ・距離・横方向の角度を設定する
	void SetHeight(float height) { height_ = height; }
	void SetDistance(float distance) { distance_ = distance; }
	void SetOrbitYaw(float orbitYaw) { orbitYaw_ = orbitYaw; }
	// 追従対象の中心から、カメラが見る Focus までのずれを設定する
	void SetFocusOffset(const Vector3& offset) { focusOffset_ = offset; }
	// Player がこの半径内にいる間は Focus を動かさない
	void SetDeadZoneRadius(float radius) { deadZoneRadius_ = radius; }
	// Focus と Camera が追従する速さを設定する
	void SetFocusFollowSpeed(float speed) { focusFollowSpeed_ = speed; }
	void SetCameraFollowSpeed(float speed) { cameraFollowSpeed_ = speed; }

	float GetHeight() const { return height_; }
	float GetDistance() const { return distance_; }
	float GetOrbitYaw() const { return orbitYaw_; }
	const Vector3& GetFocus() const { return focus_; }
	const Vector3& GetCameraPosition() const { return cameraPosition_; }

private:
	// Focus の後ろ・上にある、カメラが目指す位置を計算する
	Vector3 CalculateTargetCameraPosition() const;
	// 現在のカメラ位置から Focus を向く回転を計算して Camera に設定する
	void ApplyCameraTransform();

	// 実際に移動させる共通 Camera。本クラスは所有しない
	Camera* camera_ = nullptr;
	// Camera が見続ける中心点
	Vector3 focus_{};
	// カメラ自身の現在位置
	Vector3 cameraPosition_{};
	// Player の中心より少し上を見続けるためのずれ
	Vector3 focusOffset_{ 0.0f, 1.0f, 0.0f };
	// Focus の後ろ・上に置くカメラの高さと距離
	float height_ = 4.0f;
	float distance_ = 10.0f;
	// Player の向きとは独立した、カメラ配置用の Y 軸角度
	float orbitYaw_ = 0.0f;
	// Player が Focus の周辺にいる間、Focus を止める半径
	float deadZoneRadius_ = 1.5f;
	// Focus と Camera の追従速度
	float focusFollowSpeed_ = 6.0f;
	float cameraFollowSpeed_ = 3.0f;
};
