#pragma once

#include "MyMath.h"

namespace Collision
{
	struct CollisionInfo
	{
		bool isCollision = false;
		Vector3 normal{};
		float penetrationDepth = 0.0f;
	};

	//Object3dのTransformから、回転と拡大率を反映したOBBを作成する
	MyMath::OBB MakeOBB(const Transform& transform, const Vector3& localHalfSize);
	//球とOBBの衝突を判定し、球を外へ戻すための情報を返す
	CollisionInfo SphereOBB(const MyMath::Sphere& sphere, const MyMath::OBB& obb);
	//分離軸定理を使い、1つ目のOBBを2つ目のOBBから押し出す情報を返す
	CollisionInfo ObbObb(const MyMath::OBB& first, const MyMath::OBB& second);
}
