#pragma once

#include "Camera.h"
#include "Mirror.h"
#include "MyMath.h"
#include "Object3d.h"
#include "PlanarReflectionTarget.h"
#include <string>

class DirectXCommon;
class Object3dCommon;
class SrvManager;

// 部屋やPlayerを映す、動かない大型Mirrorです。
// 一枚ごとに専用の反射Cameraと描画先Textureを持ちます。
class FixedMirror
{
public:
	bool Initialize(
		Object3dCommon* object3dCommon,
		DirectXCommon* dxCommon,
		SrvManager* srvManager,
		const std::string& modelName,
		const Vector3& center,
		float yaw,
		float width,
		float height,
		uint32_t reflectionTextureSize = 512);

	// 通常Cameraを鏡面の反対側へ移し、反射Cameraを更新します。
	void UpdateReflectionCamera(const Camera& sourceCamera, const Vector3& sourceForward);
	// Mirrorの中心・大きさ・角度を、見た目と当たり判定へ反映します。
	void SyncVisualAndCollider();
	// モデルとColliderの中心や厚みが異なる場合に、ローカル座標の形を設定します。
	void SetColliderShape(const Vector3& localCenter, const Vector3& localHalfSize);
	// 専用Textureへの反射Scene描画を開始・終了します。
	void BeginReflection(D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle);
	void EndReflection();
	// 専用Textureを板へ貼り、鏡面を描画します。
	void DrawSurface();

	Object3d& GetObject() { return object_; }
	const Object3d& GetObject() const { return object_; }
	Mirror& GetMirror() { return mirror_; }
	const Mirror& GetMirror() const { return mirror_; }
	Camera& GetReflectionCamera() { return reflectionCamera_; }
	const Camera& GetReflectionCamera() const { return reflectionCamera_; }
	const MyMath::OBB& GetCollider() const { return collider_; }
	float GetYaw() const { return yaw_; }
	float& GetYawForEdit() { return yaw_; }
	bool IsReady() const { return reflectionTarget_.IsInitialized(); }
	bool HasReflectionCapture() const { return hasReflectionCapture_; }

private:
	Object3d object_;
	Mirror mirror_;
	Camera reflectionCamera_;
	PlanarReflectionTarget reflectionTarget_;
	MyMath::OBB collider_{};
	Vector3 colliderLocalCenter_{};
	Vector3 colliderLocalHalfSize_{ 1.0f, 1.0f, 0.05f };
	float yaw_ = 0.0f;
	Matrix4x4 capturedViewProjection_{};
	bool hasReflectionCapture_ = false;
};
