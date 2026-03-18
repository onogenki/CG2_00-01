#pragma once
#include <vector>
#include <string>
#include <wrl.h>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "ModelCommon.h"
#include "MyMath.h"

//見た目のモデル
class Model
{
public:

	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
		float shininess;
	};

	struct DirectionalLight {
		Vector4 color;
		Vector3 direction;
		float intensity;
	};

	struct MaterialData {
		std::string textureFilePath;
		//uint32_t textureIndex = 0;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		MaterialData material;
	};

	void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);

	void Draw();
private:

	//ModelCommonのポインタ
	ModelCommon* modelCommon_ = nullptr;
	//OBJファイルのデータ
	ModelData modelData;
	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	VertexData* vertexData = nullptr;
	//マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;



	void LoadObjFile(const std::string& directoryPath, const std::string& filename);
	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	void CreateVertexData();
	void CreateMaterialData();
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResources(ID3D12Device* device, size_t sizeInBytes);

};