#pragma once

#include "Mirror.h"
#include "MyMath.h"
#include "Object3d.h"
#include <string>

class Object3dCommon;

// Playerが拾って運べる、小型のレーザー反射用Mirrorです。
class CarryableMirror
{
public:
	void Initialize(
		Object3dCommon* object3dCommon,
		const std::string& modelName,
		const Vector3& startPosition,
		float width,
		float height);
	// interactPressedはEキーなどの「押した瞬間」だけtrueを渡します。
	void Update(
		float deltaTime,
		const Vector3& playerPosition,
		float playerFacingYaw,
		bool interactPressed,
		bool isAiming = false,
		float horizontalAimInput = 0.0f);

	Object3d& GetObject() { return object_; }
	const Object3d& GetObject() const { return object_; }
	const Mirror& GetMirror() const { return mirror_; }
	const MyMath::OBB& GetCollider() const { return collider_; }
	bool IsCarried() const { return isCarried_; }

private:
	void ApplyTransform(const Vector3& position, float yaw);

	Object3d object_;
	Mirror mirror_;
	MyMath::OBB collider_{};
	Vector3 colliderLocalHalfSize_{ 1.0f, 1.0f, 0.05f };
	float width_ = 1.4f;
	float height_ = 1.0f;
	float yaw_ = 0.0f;
	float pickupDistance_ = 2.5f;
	float holdDistance_ = 1.8f;
	float holdHeight_ = 0.5f;
	// Playerが旋回した時に、鏡が目標方向へ追従する速さです。
	float holdTurnFollowSpeed_ = 9.0f;
	// 左クリック中にMouseを動かした時、MirrorをPlayerの前方で横へ回す速さです。
	float aimSensitivity_ = 0.012f;
	// MirrorはPlayerの真横までは回らないよう、前方を中心とした範囲に制限します。
	float maxAimYawOffset_ = 1.00f;
	// Playerの正面から見たMirrorの左右角度です。左クリックを離すと正面へ戻ります。
	float aimYawOffset_ = 0.0f;
	bool isCarried_ = false;
};
