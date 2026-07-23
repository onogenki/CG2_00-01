#include "Collision.h"

#include <cmath>
#include <limits>

using namespace MyMath;

namespace
{
	Vector3 Add(const Vector3& left, const Vector3& right)
	{
		return { left.x + right.x, left.y + right.y, left.z + right.z };
	}

	Vector3 Subtract(const Vector3& left, const Vector3& right)
	{
		return { left.x - right.x, left.y - right.y, left.z - right.z };
	}

	float Clamp(float value, float minimum, float maximum)
	{
		if (value < minimum) {
			return minimum;
		}
		if (value > maximum) {
			return maximum;
		}
		return value;
	}

	Vector3 Cross(const Vector3& left, const Vector3& right)
	{
		return {
			left.y * right.z - left.z * right.y,
			left.z * right.x - left.x * right.z,
			left.x * right.y - left.y * right.x,
		};
	}

	float GetHalfSize(const OBB& obb, int axisIndex)
	{
		return axisIndex == 0 ? obb.size.x : (axisIndex == 1 ? obb.size.y : obb.size.z);
	}

	float ProjectObbRadius(const OBB& obb, const Vector3& axis)
	{
		float radius = 0.0f;
		for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
			radius += GetHalfSize(obb, axisIndex) * std::abs(Dot(axis, obb.orientations[axisIndex]));
		}
		return radius;
	}

	bool TestSeparatingAxis(const OBB& first, const OBB& second, const Vector3& candidateAxis, Collision::CollisionInfo& result)
	{
		//平行な辺同士の外積は長さ0になるため、分離軸として使わない
		const float axisLength = Length(candidateAxis);
		if (axisLength <= 0.0001f) {
			return true;
		}

		const Vector3 axis = Multiply(1.0f / axisLength, candidateAxis);
		const float firstRadius = ProjectObbRadius(first, axis);
		const float secondRadius = ProjectObbRadius(second, axis);
		const Vector3 centerDifference = Subtract(second.center, first.center);
		const float centerDistance = Dot(centerDifference, axis);
		const float overlap = firstRadius + secondRadius - std::abs(centerDistance);
		if (overlap < 0.0f) {
			return false;
		}

		//最も短く押し出せる軸を、衝突の法線と押し戻し距離に採用する
		if (overlap < result.penetrationDepth) {
			result.penetrationDepth = overlap;
			result.normal = centerDistance >= 0.0f ? Multiply(-1.0f, axis) : axis;
		}
		return true;
	}
}

MyMath::OBB Collision::MakeOBB(const Transform& transform, const Vector3& localHalfSize)
{
	//拡大率を除いた回転行列から、OBBの3本の軸を取り出す
	const Matrix4x4 rotationMatrix = MakeAffineMatrix(
		{ 1.0f, 1.0f, 1.0f }, transform.rotate, { 0.0f, 0.0f, 0.0f });

	OBB obb{};
	obb.center = transform.translate;
	for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
		obb.orientations[axisIndex] = Normalize({
			rotationMatrix.m[axisIndex][0],
			rotationMatrix.m[axisIndex][1],
			rotationMatrix.m[axisIndex][2],
		});
	}

	//OBBのsizeは、中心から各面までの半分の長さを表す
	obb.size = {
		localHalfSize.x * std::abs(transform.scale.x),
		localHalfSize.y * std::abs(transform.scale.y),
		localHalfSize.z * std::abs(transform.scale.z),
	};
	return obb;
}

Collision::CollisionInfo Collision::SphereOBB(const Sphere& sphere, const OBB& obb)
{
	//球の中心を、OBBの3本の軸を使ってOBBのローカル位置へ変換する
	const Vector3 fromBoxCenter = Subtract(sphere.center, obb.center);
	float localPosition[3]{};
	Vector3 closestPoint = obb.center;

	for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
		localPosition[axisIndex] = Dot(fromBoxCenter, obb.orientations[axisIndex]);
		const float halfSize = axisIndex == 0 ? obb.size.x : (axisIndex == 1 ? obb.size.y : obb.size.z);
		const float clampedPosition = Clamp(localPosition[axisIndex], -halfSize, halfSize);
		closestPoint = Add(closestPoint, Multiply(clampedPosition, obb.orientations[axisIndex]));
	}

	//OBB上の最近接点と球の中心の距離が、球の半径より短ければ衝突している
	const Vector3 fromClosestPoint = Subtract(sphere.center, closestPoint);
	const float distance = Length(fromClosestPoint);
	//接触しただけの状態も衝突として扱い、床の上で接地状態を安定させる
	if (distance > sphere.radius + 0.001f) {
		return {};
	}

	CollisionInfo result{};
	result.isCollision = true;
	if (distance > 0.0001f) {
		//球の中心がOBBの外側にある通常の場合
		result.normal = Multiply(1.0f / distance, fromClosestPoint);
		result.penetrationDepth = (std::max)(0.0f, sphere.radius - distance);
		return result;
	}

	//球の中心がOBBの内側にある場合は、最も近い面を選んで外へ押し出す
	int nearestAxisIndex = 0;
	float nearestFaceDistance = obb.size.x - std::abs(localPosition[0]);
	for (int axisIndex = 1; axisIndex < 3; ++axisIndex) {
		const float halfSize = axisIndex == 1 ? obb.size.y : obb.size.z;
		const float faceDistance = halfSize - std::abs(localPosition[axisIndex]);
		if (faceDistance < nearestFaceDistance) {
			nearestFaceDistance = faceDistance;
			nearestAxisIndex = axisIndex;
		}
	}

	const float sign = localPosition[nearestAxisIndex] >= 0.0f ? 1.0f : -1.0f;
	result.normal = Multiply(sign, obb.orientations[nearestAxisIndex]);
	result.penetrationDepth = sphere.radius + nearestFaceDistance;
	return result;
}

Collision::CollisionInfo Collision::ObbObb(const OBB& first, const OBB& second)
{
	//OBB同士の分離軸候補は、面法線6本と辺同士の外積9本の合計15本
	CollisionInfo result{};
	result.penetrationDepth = (std::numeric_limits<float>::max)();

	for (int firstAxisIndex = 0; firstAxisIndex < 3; ++firstAxisIndex) {
		if (!TestSeparatingAxis(first, second, first.orientations[firstAxisIndex], result)) {
			return {};
		}
	}
	for (int secondAxisIndex = 0; secondAxisIndex < 3; ++secondAxisIndex) {
		if (!TestSeparatingAxis(first, second, second.orientations[secondAxisIndex], result)) {
			return {};
		}
	}
	for (int firstAxisIndex = 0; firstAxisIndex < 3; ++firstAxisIndex) {
		for (int secondAxisIndex = 0; secondAxisIndex < 3; ++secondAxisIndex) {
			const Vector3 crossAxis = Cross(first.orientations[firstAxisIndex], second.orientations[secondAxisIndex]);
			if (!TestSeparatingAxis(first, second, crossAxis, result)) {
				return {};
			}
		}
	}

	result.isCollision = true;
	return result;
}
