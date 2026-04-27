#pragma once
#include"Vector2.h"
#include"Vector3.h"
#include"Vector4.h"
#include"Matrix4x4.h"
#include"Transform.h"
#include <cassert>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>


// 名前空間で囲む
namespace MyMath {

	// 定義
	struct Sphere {
		Vector3 center; // 中心点
		float radius;   // 半径
	};

	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	// --- ベクトル計算 ---

	// スカラー倍
	Vector3 Multiply(float scalar, const Vector3& v);

	// 内積
	float Dot(const Vector3& v1, const Vector3& v2);

	// 長さ(ノルム)
	float Length(const Vector3& v);

	// 正規化
	Vector3 Normalize(const Vector3& v);


	// --- 行列計算 ---

	// 単位行列の作成
	Matrix4x4 MakeIdentity4x4();

	// 行列の積
	Matrix4x4 Multiply(const Matrix4x4& matrix1, const Matrix4x4& matrix2);

	// 逆行列
	Matrix4x4 Inverse(const Matrix4x4& m);

	//4x4行列の転置
	Matrix4x4 Transpose(const Matrix4x4& m);

	// --- 変換行列作成 ---

	// 拡大縮小行列の作成
	Matrix4x4 MakeScaleMatrix(const Vector3& scale);

	// X軸回転行列の作成
	Matrix4x4 MakeRotateXMatrix(float theta);

	// Y軸回転行列の作成
	Matrix4x4 MakeRotateYMatrix(float theta);

	// Z軸回転行列の作成
	Matrix4x4 MakeRotateZMatrix(float theta);

	// 平行移動行列の作成
	Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	// アフィン変換行列の作成
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	//クォータニオン
	Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);
	Matrix4x4 MakeAffineMatrixQuaternion(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);

	Vector3 Lerp(const Vector3& v1, const Vector3& v2, float t);

	Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);

	// --- 座標変換・投影 ---

	// 透視投影行列
	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	// 正射影行列
	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	// ビューポート変換行列
	Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

	inline void MatrixScreenPrintf() {};

	inline constexpr int kRowHeight = 20;
	inline constexpr int kColumnWidth = 60;

	// cot 関数
	float Cot(float x);

	struct AABB
	{
		Vector3 min;
		Vector3 max;
	};

}