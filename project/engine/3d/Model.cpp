#include "Model.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include<assimp/Importer.hpp>
#include<assimp/scene.h>
#include<assimp/postprocess.h>

using namespace MyMath;

void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename)
{

	//ModelCommonのポインタを引数からメンバ変数に記録する
	modelCommon_ = modelCommon;
	//モデル読み込み
	LoadObjFile(directoryPath, filename);
	//頂点データ作成
	CreateVertexData();
	//マテリアルデータ作成
	CreateMaterialData();
	//pngが空ならuvChecker.pngに変える
	if (modelData.material.textureFilePath.empty() ||
		modelData.material.textureFilePath.find("None") != std::string::npos)
	{
		modelData.material.textureFilePath = "Resources/uvChecker.png";
	}
	//テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
}

void Model::Draw()
{

	// コマンドリストを取得
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	// 1. 頂点バッファビューを設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

	// 2. 形状を設定 (三角形リスト)
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 3. マテリアルCBufferの場所を設定 (RootParameter 0)
	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

	// 5. SRVのDescriptorTableの先頭を設定 (RootParameter 2)
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle =
		TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureFilePath);
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

	// 7. 描画 (DrawCall)
	commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

}

void Model::SetTexture(const std::string& filePath)
{//新しいテクスチャを読み込んで
	TextureManager::GetInstance()->LoadTexture(filePath);
	//文字列を書き換える
	modelData.material.textureFilePath = filePath;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Model::CreateBufferResources(ID3D12Device* device, size_t sizeInBytes)
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

void Model::CreateVertexData()
{
	// 書き込むデータのサイズ（頂点データ全体のバイト数）
	size_t sizeInBytes = sizeof(VertexData) * modelData.vertices.size();

	// 1. 頂点リソースを作る (Spriteから持ってきたヘルパー関数を使用)
	vertexResource = CreateBufferResources(modelCommon_->GetDxCommon()->GetDevice(), sizeInBytes);

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

void Model::CreateMaterialData()
{
	// 1. マテリアルリソースを作る (サイズは Material 構造体1つ分)
	materialResource = CreateBufferResources(modelCommon_->GetDxCommon()->GetDevice(), sizeof(Material));

	// 2. データを書き込むためのアドレスを取得して materialData に割り当てる
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	// 3. マテリアルデータの初期値を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	materialData->enableLighting = true;

	materialData->shininess = 50.0f;

	materialData->uvTransform = MakeIdentity4x4();
}

Model::MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
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

void Model::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	// メンバ変数をクリアしておく
	modelData.vertices.clear();

	std::vector<Vector4>positions;//位置
	std::vector<Vector3>normals;//法線
	std::vector<Vector2>texcoords;//テクスチャ座標
	std::string line;//ファイルから読んだ1行を格納する

	//assimpでobjを読む
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	assert(scene->HasMeshes());//メッシュがないのは対応しない
	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//とりあえず開けなかったら止める

	VertexData triangle[3];
	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);//文字列を分解しながら読むためのクラス
		s >> identifier;//先頭の識別子を読む

		for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)//mesh解析
		{
			aiMesh* mesh = scene->mMeshes[meshIndex];
			assert(mesh->HasNormals());//法線がないMeshは今回は非対応
			assert(mesh->HasTextureCoords(0));//TexcoordがないMeshは今回は非対応
			for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)//face解析
			{
				aiFace& face = mesh->mFaces[faceIndex];
				assert(face.mNumIndices == 3);//三角形のみサポート
				for (uint32_t element = 0; element < face.mNumIndices; ++element)//vertex解析
				{
					uint32_t vertexIndex = face.mIndices[element];
					aiVector3D& position = mesh->mVertices[vertexIndex];
					aiVector3D& normal = mesh->mNormals[vertexIndex];
					aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
					VertexData vertex;
					vertex.position = { position.x,position.y,position.z,1.0f };
					vertex.normal = { normal.x,normal.y,normal.z };
					vertex.texcoord = { texcoord.x,texcoord.y };
					//aiProcess_MakeLeftHandedはz*=-1で右手->左手に変換するので手動で対処
					vertex.position.x *= -1.0f;
					vertex.normal.x *= -1.0f;
					modelData.vertices.push_back(vertex);
				}
			}
		}

		//material解析
		for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
		{
			aiMaterial* material = scene->mMaterials[materialIndex];
			if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0)
			{
				aiString textureFilePath;
				material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
				modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
			}
		}

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