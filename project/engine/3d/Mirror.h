#pragma once

#include "MyMath.h"

// 反射描画とレーザーの当たり判定で使用する、平面鏡の情報です。
// このクラスは鏡のデータだけを持ちます。鏡を画面に描画する役割は
// Stage1（将来的には専用の描画クラス）が担当します。
class Mirror {
public:
	Mirror() = default;
	Mirror(const Vector3& center, const Vector3& normal, float width, float height) {
		SetCenter(center);
		SetNormal(normal);
		SetSize(width, height);
	}

	void SetCenter(const Vector3& center) { center_ = center; }
	void SetNormal(const Vector3& normal) {
		const Vector3 normalized = MyMath::Normalize(normal);
		// 長さが 0 の法線には向きがないため、最後に設定した正常な向きを保ちます。
		if (MyMath::Length(normalized) > 0.0f) {
			normal_ = normalized;
		}
	}
	void SetSize(float width, float height) {
		width_ = (width > 0.01f) ? width : 0.01f;
		height_ = (height > 0.01f) ? height : 0.01f;
	}

	const Vector3& GetCenter() const { return center_; }
	const Vector3& GetNormal() const { return normal_; }
	float GetWidth() const { return width_; }
	float GetHeight() const { return height_; }

	// この鏡の面を基準に反転した位置を返します。
	// 次の段階で、反射カメラの位置を求めるために使用します。
	Vector3 ReflectPoint(const Vector3& point) const {
		const Vector3 fromCenter{
			point.x - center_.x,
			point.y - center_.y,
			point.z - center_.z,
		};
		const float distanceToPlane = MyMath::Dot(fromCenter, normal_);
		const Vector3 correction = MyMath::Multiply(2.0f * distanceToPlane, normal_);
		return {
			point.x - correction.x,
			point.y - correction.y,
			point.z - correction.z,
		};
	}

	// この鏡の面で反射した方向を返します。
	// 後でレーザーの反射方向を求めるときにも、同じ計算を使用します。
	Vector3 ReflectDirection(const Vector3& direction) const {
		const float towardNormal = MyMath::Dot(direction, normal_);
		const Vector3 correction = MyMath::Multiply(2.0f * towardNormal, normal_);
		return {
			direction.x - correction.x,
			direction.y - correction.y,
			direction.z - correction.z,
		};
	}

private:
	Vector3 center_{ 0.0f, 0.0f, 0.0f };
	Vector3 normal_{ 0.0f, 0.0f, 1.0f };
	float width_ = 4.0f;
	float height_ = 4.0f;
};
