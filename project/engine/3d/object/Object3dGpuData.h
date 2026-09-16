#pragma once

#include "Object3d.h"
#include <wrl.h>

class Camera;
class DirectXCommon;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;

// Object3dが描画時にGPUへ渡す行列・Light・Camera・ReflectionのConstant Bufferを管理します。
// ModelのAnimationやTransformそのものは持たず、Object3dから受け取った値をGPU用データへ変換するだけです。
class Object3dGpuData
{
public:
	// Object3d一体分のConstant Bufferを作成し、安全な初期値を書き込みます。
	void Initialize(DirectXCommon* dxCommon);
	// World行列とCameraを使い、通常描画用の行列・Camera位置を更新します。
	void UpdateTransform(const Matrix4x4& worldMatrix, const Camera* camera);
	// 通常モデル描画で使う各Constant BufferをRoot Parameterへ設定します。
	void BindForObjectDraw(ID3D12GraphicsCommandList* commandList) const;
	// 壁越しシルエット描画で使う行列と色をRoot Parameterへ設定します。
	void BindForOccludedSilhouetteDraw(
		ID3D12GraphicsCommandList* commandList,
		const Vector4& color) const;
	// 鏡描画で使う行列・ReflectionデータをRoot Parameterへ設定します。
	void BindForMirrorDraw(
		ID3D12GraphicsCommandList* commandList,
		const Matrix4x4& reflectionViewProjection,
		const Vector4& tint) const;

	void SetDirectionalLight(const Object3d::DirectionalLight& light) { *directionalLightData_ = light; }
	void SetPointLight(const Object3d::PointLight& light) { *pointLightData_ = light; }
	void SetSpotLight(const Object3d::SpotLight& light);
	void SetSpotLights(const std::array<Object3d::SpotLight, Object3d::kMaximumSpotLightCount>& lights);
	const Object3d::DirectionalLight& GetDirectionalLight() const { return *directionalLightData_; }

private:
	// 各種類のConstant Bufferを確保してCPUから書ける状態へします。
	void CreateTransformationMatrixData(DirectXCommon* dxCommon);
	void CreateDirectionalLightData(DirectXCommon* dxCommon);
	void CreateCameraData(DirectXCommon* dxCommon);
	void CreateOccludedSilhouetteData(DirectXCommon* dxCommon);
	void CreateReflectionData(DirectXCommon* dxCommon);
	void CreatePointLightData(DirectXCommon* dxCommon);
	void CreateSpotLightData(DirectXCommon* dxCommon);

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	Object3d::TransformationMatrix* transformationMatrixData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	Object3d::DirectionalLight* directionalLightData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	Object3d::CameraForGPU* cameraData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> occludedSilhouetteResource_;
	Object3d::OccludedSilhouetteData* occludedSilhouetteData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> reflectionDataResource_;
	Object3d::ReflectionData* reflectionData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
	Object3d::PointLight* pointLightData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
	Object3d::SpotLightSet* spotLightData_ = nullptr;
};
