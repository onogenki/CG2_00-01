#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include"MyMath.h"

class SpriteCommon;

//スプライト1枚分
class Sprite
{
public:

	//頂点データ
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	//マテリアルデータ
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

	Sprite() = default;
	~Sprite() = default;

	void Initialize(SpriteCommon* spriteCommon);

	void Update();

	void Draw();

private:

	SpriteCommon* spriteCommon = nullptr;

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource>vertexResource;
	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	//バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;


	Microsoft::WRL::ComPtr<ID3D12Resource>indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	uint32_t* indexData = nullptr;

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource>materialResource;
	//バッファリソース内のデータを指すポインタ
	Material* materialData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource>transformationMatrixResource;
	//バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData = nullptr;

	// 変形情報
	MyMath::Transform transform;

};

