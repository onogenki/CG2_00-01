#pragma once
#include "MyMath.h"
#include "Transform.h"
#include "Model.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "Camera.h"
#include <vector>
#include <string>
#include <d3d12.h>
#include <wrl.h>

class Object3dCommon;

//3Dオブジェクト
class Object3d
{
public:

	//座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};

	// 平行光源データ
	struct DirectionalLight {
		Vector4 color;
		Vector3 direction;//ハイライトの位置
		float intensity;//明るさの強さ
	};

	struct PointLight
	{
		Vector4 color;//ライトの色
		Vector3 position;//ライトの位置
		float intensity;//輝度
		float radius;//ライトの届く最大距離
		float decay;//減衰率
		float padding[2];
	};

	struct SpotLight
	{
		Vector4 color;//ライトの色
		Vector3 position;//ライトの位置
		float intensity;//輝度
		Vector3 direction;//スポットライトの方向
		float distance;//ライトの届く最大距離
		float decay;//減衰率
		float cosAngle;//スポットライトの余弦
		float cosFalloffStart;
		float padding[1];
	};

	struct CameraForGPU
	{
		Vector3 worldPosition;
	};

	void Initialize(Object3dCommon* object3dCommon);

	void Update();

	void Draw();

	// モデルをポインタでセットする 
	void SetModel(Model* model) { this->model_ = model; }

	// モデルをファイルパス（文字列）でセットする
	void SetModel(const std::string& filePath, bool isAnimation = false);

	//スケルトンを取得
	Model::Skeleton& GetSkeleton() { return skeleton_; }

	/// setter
	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }
	void SetCamera(Camera* camera) { this->camera = camera; }
	//構造体(color,direction,intensity)全部入ってるsetter
	void SetDirectionalLight(const DirectionalLight& light) {
		*directionalLightData = light;
	};
	void SetPointLight(const PointLight& light) {
		*pointLightData = light;
	}
	void SetSpotLight(const SpotLight& light) {
		*spotLightData = light;
	}

	void PlayAnimation(const Model::Animation& animation);

	//getter
	const Vector3& GetScale()const { return transform.scale; }
	const Vector3& GetRotate()const { return transform.rotate; }
	const Vector3& GetTranslate()const { return transform.translate; }
	const DirectionalLight& GetDirectionalLight()const { return *directionalLightData; }

	//外部からアニメーションモデルかどうか判定
	bool IsSkeletal() const { return isSkeletal_; }

	// モデル
	Transform& GetTransform() { return transform; }

private:
	Object3dCommon* object3dCommon = nullptr;

	Model* model_ = nullptr;

	Camera* camera = nullptr;

	//3Dオブジェクト自身のトランスフォーム
	Transform transform{};

	//座標変換リソース(ConstantBuffer)
	void CreateTransformationMatrixData();
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	TransformationMatrix* transformationMatrixData = nullptr;

	//平行光源作成
	void CreateDirectionalLightData();
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	DirectionalLight* directionalLightData = nullptr;

	// カメラデータ作成関数
	void CreateCameraData();
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource;
	CameraForGPU* cameraData = nullptr;

	// カメラデータ作成関数
	void CreatePointLightData();
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource;
	PointLight* pointLightData = nullptr;

	// スポットライト作成関数
	void CreateSpotLightData();
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource;
	SpotLight* spotLightData = nullptr;

	// リソース作成のヘルパー関数 (Spriteから移植)
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);

	//アニメーション
	Model::Animation currentAnimation_;//アニメーション読み込み
	float animationTime_ = 0.0f;// アニメーションの再生時間を管理
	bool isAnimating_ = false;

	Model::Skeleton skeleton_; // このオブジェクト専用の骨
	bool isSkeletal_ = false;  // スケルトンを持っているかどうか

	Model::SkinCluster skinCluster_;
};

