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
		const Vector3& playerPosition,
		float playerFacingYaw,
		bool interactPressed);

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
	bool isCarried_ = false;
};
