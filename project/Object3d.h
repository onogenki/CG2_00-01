#pragma once
#include "MyMath.h"
#include "Transform.h"
#include "Model.h"
#include "ModelManager.h"
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
	};

	// 平行光源データ
	struct DirectionalLight {
		Vector4 color;
		Vector3 direction;
		float intensity;
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
	void SetModel(const std::string& filePath);

	/// setter
	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }
	void SetCamera(Camera* camera) { this->camera = camera; }

	//getter
	const Vector3& GetScale()const { return transform.scale; }
	const Vector3& GetRotate()const { return transform.rotate; }
	const Vector3& GetTranslate()const { return transform.translate; }


	// モデル
	Transform& GetTransform() { return transform; }

private:
	Object3dCommon* object3dCommon = nullptr;
	
	Model* model_ = nullptr;

	Camera* camera = nullptr;

	//3Dオブジェクト自身のトランスフォーム
	Transform transform;

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

	// リソース作成のヘルパー関数 (Spriteから移植)
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResources(ID3D12Device* device, size_t sizeInBytes);

};

