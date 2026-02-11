#pragma once
#include "MyMath.h"
#include "Transform.h"
#include <vector>
#include <string>
#include <d3d12.h>
#include <wrl.h>

class Object3dCommon;

//3Dオブジェクト
class Object3d
{
public:

	//頂点データ
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct MaterialData
	{
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		MaterialData material;
	};

	struct Material
	{
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

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

	void Initialize(Object3dCommon* object3dCommon);

	void Update();

	void Draw();

	// .objファイルの読み取り
	void LoadObjFile(const std::string& directoryPath, const std::string& filename);

	// モデル
	Transform& GetTransform() { return transform; }


	// カメラ
	Transform& GetCameraTransform() { return cameraTransform; }

private:
	Object3dCommon* object3dCommon = nullptr;
	
	//3Dオブジェクト自身のトランスフォーム
	Transform transform;
	//カメラのトランスフォーム
	Transform cameraTransform;

	//OBJファイルのデータ
	ModelData modelData;


	//頂点データ作成(リソース作成、ビュー作成、データの書き込み)
	void CreateVertexData();
	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	VertexData* vertexData = nullptr;


	//インデックス
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	uint32_t indexCount = 0; // インデックスの数


	//マテリアル作成
	void CreateMaterialData();
	//マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;

	//座標変換リソース(ConstantBuffer)
	void CreateTransformationMatrixData();
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	TransformationMatrix* transformationMatrixData = nullptr;

	//平行光源作成
	void CreateDirectionalLightData();
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	DirectionalLight* directionalLightData = nullptr;


	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU{};
	//マテリアルリソース(ConstantBuffer)


	// リソース作成のヘルパー関数 (Spriteから移植)
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResources(ID3D12Device* device, size_t sizeInBytes);

	// .mtlファイルの読み取り
	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);


};

