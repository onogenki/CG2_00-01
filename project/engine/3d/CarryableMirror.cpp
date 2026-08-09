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
	const Vector3& playerPosition,
	float playerFacingYaw,
	bool interactPressed)
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
			}
		}
	}

	if (isCarried_) {
		const Vector3 playerForward{
			std::sin(playerFacingYaw),
			0.0f,
			std::cos(playerFacingYaw),
		};
		const Vector3 heldPosition{
			playerPosition.x + playerForward.x * holdDistance_,
			playerPosition.y + holdHeight_,
			playerPosition.z + playerForward.z * holdDistance_,
		};
		// 板の表側がPlayerと反対方向を向くよう、Playerの正面へ180度足します。
		ApplyTransform(heldPosition, playerFacingYaw + 3.14159265f);
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
