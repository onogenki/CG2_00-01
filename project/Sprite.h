#pragma once
#include "main.cpp"


class SpriteCommon;

//スプライト1枚分
class Sprite
{

	//頂点データ
	struct VertexData {
		Vector4 posotion;
		Vector2 texcoord;
		Vector3 normal;
	};

	//マテリアルデータ
	struct Material
	{
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTrasnform;
	};

	//座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	void Initialize();

	void Updata();

	void Draw();

	SpriteCommon* spriteCommon = nullptr;

	void Initialize(SpriteCommon* spriteCommon);

	//バッファリソース
	void VertexResource(VertexBuffer);
	void IndexResource(IndexBuffer);
		//バッファリソース内のデータを指すポインタ
		VertexData* vertexData = nullptr;
	uint32_t* indexData = nullptr;
	//バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	//バッファリソース
	void MaterialResource(ConstantBuffer);
		//バッファリソース内のデータを指すポインタ
		Material* materialData = nullptr;

	//バッファリソース内のデータを指すポインタ
		TransformationMatrix* transformationMatrixData;


};

