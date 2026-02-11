#include "Object3d.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include "MyMath.h"
using namespace MyMath;

void Object3d::Initialize(Object3dCommon* object3dCommon)
{//引数で受け取ってメンバ変数に記録する

	this->object3dCommon = object3dCommon;
	
	//モデル読み込み
	LoadObjFile("resources", "plane.obj");

	//頂点データ作成
	CreateVertexData();
	//マテリアルデータ作成
	CreateMaterialData();
	//座標変換行列データ作成
	CreateTransformationMatrixData();
	//平行光源データ作成
	CreateDirectionalLightData();

	//Transform変数を作る
	transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	cameraTransform = { { 1.0f,1.0f,1.0f }, { 0.3f,0.0f,0.0f }, { 0.0f,4.0f,-10.0f } };

	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	//読み込んだテクスチャの番号を取得
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData.material.textureFilePath);

}

void Object3d::Update()
{
	//transform.rotate.y += 3.14f / 180.0f;
	// 1. TransformからWorldMatrixを作る
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);
	Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData->WVP = worldViewProjectionMatrix;
	transformationMatrixData->World = worldMatrix;
}

void Object3d::Draw()
{
	// コマンドリストを取得
	ID3D12GraphicsCommandList* commandList = object3dCommon->GetDxCommon()->GetCommandList();

	// 1. 頂点バッファビューを設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

	// 2. 形状を設定 (三角形リスト)
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 3. マテリアルCBufferの場所を設定 (RootParameter 0)
	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

	// 4. 座標変換行列CBufferの場所を設定 (RootParameter 1)
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());

	// 5. SRVのDescriptorTableの先頭を設定 (RootParameter 2)
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle =
		TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureIndex);
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

	// 6. 平行光源CBufferの場所を設定 (RootParameter 3)
	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

	// 7. 描画 (DrawCall)
	commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Object3d::CreateBufferResources(ID3D12Device* device, size_t sizeInBytes)
{
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う

	// 頂点リソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際にリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

void Object3d::CreateVertexData()
{
	// 書き込むデータのサイズ（頂点データ全体のバイト数）
	size_t sizeInBytes = sizeof(VertexData) * modelData.vertices.size();

	// 1. 頂点リソースを作る (Spriteから持ってきたヘルパー関数を使用)
	vertexResource = CreateBufferResources(object3dCommon->GetDxCommon()->GetDevice(), sizeInBytes);

	// 2. 頂点バッファビューを作成
	// リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズ
	vertexBufferView.SizeInBytes = static_cast<UINT>(sizeInBytes);
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 3. データを書き込む
	// 書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// 読み込んだモデルデータ(std::vector)の中身を、マップしたメモリ(vertexData)へ一括コピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeInBytes);
}

void Object3d::CreateMaterialData()
{
	// 1. マテリアルリソースを作る (サイズは Material 構造体1つ分)
	materialResource = CreateBufferResources(object3dCommon->GetDxCommon()->GetDevice(), sizeof(Material));

	// 2. データを書き込むためのアドレスを取得して materialData に割り当てる
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	// 3. マテリアルデータの初期値を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	materialData->enableLighting = false;

	materialData->uvTransform = MakeIdentity4x4();
}

void Object3d::CreateTransformationMatrixData()
{
	// 1. 座標変換行列リソースを作る
	// (サイズは TransformationMatrix 構造体1つ分)
	transformationMatrixResource = CreateBufferResources(object3dCommon->GetDxCommon()->GetDevice(), sizeof(TransformationMatrix));

	// 2. データを書き込むためのアドレスを取得して transformationMatrixData に割り当てる
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));

	// 3. 単位行列を書き込んでおく
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();
}

void Object3d::CreateDirectionalLightData()
{
	// 1. 平行光源リソースを作る
	// (サイズは DirectionalLight 構造体1つ分)
	directionalLightResource = CreateBufferResources(object3dCommon->GetDxCommon()->GetDevice(), sizeof(DirectionalLight));

	// 2. データを書き込むためのアドレスを取得して directionalLightData に割り当てる
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// 3. デフォルト値を書き込んでおく
	// 色: 白 (R, G, B, A)
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 向き: 真上から下へ照らす (X, Y, Z)
	// ※必要に応じて {0.0f, -1.0f, 1.0f} など斜めにすると立体感が出やすいです
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };

	// 強さ: 1.0 (標準)
	directionalLightData->intensity = 1.0f;
}

Object3d::MaterialData Object3d::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	MaterialData materialData;//構築するMaterialData
	std::string line;//ファイルから読んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//とりあえず開けなかったら止める
	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);//文字列を分解しながら読むためのクラス
		s >> identifier;//先頭の識別子を読む

		if (identifier == "map_Kd")
		{
			std::string textureFilename;
			s >> textureFilename;
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

void Object3d::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	// メンバ変数をクリアしておく
	modelData.vertices.clear();

	std::vector<Vector4>positions;//位置
	std::vector<Vector3>normals;//法線
	std::vector<Vector2>texcoords;//テクスチャ座標
	std::string line;//ファイルから読んだ1行を格納する

	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//とりあえず開けなかったら止める

	VertexData triangle[3];
	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);//文字列を分解しながら読むためのクラス
		s >> identifier;//先頭の識別子を読む

		if (identifier == "v")//頂点位置
		{
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt")//頂点テクスチャ座標
		{
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn")//頂点法線
		{
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f")
		{//面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex)
			{
				std::string vertexDefinition;
				s >> vertexDefinition;
				//頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3] = { 0,0,0 };
				for (int32_t element = 0; element < 3; ++element)
				{
					std::string index;
					std::getline(v, index, '/');//区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				//要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				VertexData vertex = { position, texcoord, normal };

				// メンバ変数へ追加
				modelData.vertices.push_back(vertex);
				triangle[faceVertex] = { position,texcoord,normal };
			}
			//頂点を逆順で登録することで、回り順を逆にする
			//modelData.vertices.push_back(triangle[2]);
			//modelData.vertices.push_back(triangle[1]);
			//modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib")
		{
			std::string materialFilename;
			s >> materialFilename;
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
}