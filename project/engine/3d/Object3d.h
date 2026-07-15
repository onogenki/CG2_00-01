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

	~Object3d();

	void Initialize(Object3dCommon* object3dCommon);

	void Update();

	void Draw();

	// モデルをポインタでセットする 
	void SetModel(Model* model) { this->model_ = model; }

	// モデルをファイルパス（文字列）でセットする
	void SetModel(const std::string& filePath);

	//アニメーションの初期化と生成専用の関数
	void InitializeAnimation();

	void PlayAnimation(const Model::Animation& animation);

	//ループするかどうかの関数(falseで1ループのみ)
	void SetIsLoop(bool isLoop) { isLoop_ = isLoop; }

	//最大で何秒まで再生するか設定
	void SetMaxPlayTime(float maxTime) { maxPlayTime_ = maxTime; }

	//今何秒か教える関数
	float GetAnimationTime() const { return animationTime_; }
	//シーンからアニメーションの時間をいじる関数
	void SetAnimationTime(float time) { this->animationTime_ = time; }

	//スケルトン(アニメーションするか)を取得
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
	void SetEnvironmentCoefficient(float coefficient);

	//getter
	const Vector3& GetScale()const { return transform.scale; }
	const Vector3& GetRotate()const { return transform.rotate; }
	const Vector3& GetTranslate()const { return transform.translate; }
	const DirectionalLight& GetDirectionalLight()const { return *directionalLightData; }
	float GetEnvironmentCoefficient() const;
	Model* GetModel() const { return model_; }
	const std::string& GetModelName() const { return modelName_; }

	//外部からアニメーションモデルかどうか判定
	bool IsSkeletal() const { return isSkeletal_; }

	// モデル
	Transform& GetTransform() { return transform; }
	const Transform& GetTransform() const { return transform; }

private:
	void ReleaseSkinClusterDescriptors();

	Object3dCommon* object3dCommon = nullptr;

	Model* model_ = nullptr;
	std::string modelName_;

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

	//アニメーション
	Model::Animation currentAnimation_;//アニメーション読み込み
	float animationTime_ = 0.0f;// アニメーションの再生時間を管理
	bool isAnimating_ = false;
	bool isLoop_ = true;//デフォルトはループ
	float maxPlayTime_ = 0.0f;

	Model::Skeleton skeleton_; // このオブジェクト専用の骨
	bool isSkeletal_ = false;  // スケルトンを持っているかどうか
	Model::SkinCluster skinCluster_;//骨に合わせて動く体
};

