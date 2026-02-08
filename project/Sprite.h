#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Transform.h"
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

	// 座標変換行列データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	Sprite() = default;
	~Sprite() = default;

	void Initialize(SpriteCommon* spriteCommon);

	void Update();

	void Draw();

	//getter
	const Vector2& GetPosition() const { return position; }//座標
	float GetRotation() const { return rotation; }//回転
	const Vector4& GetColor()const { return materialData->color; }//色
	const Vector2& GetSize() const { return size; }//サイズ

	//setter
	void SetPosition(const Vector2& position) { this->position = position; }//座標
	void SetRotation(float rotation) { this->rotation = rotation; }//回転
	void SetColor(const Vector4& color) { materialData->color = color; }//色
	void SetSize(const Vector2& size) { this->size = size; }//サイズ

	void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) { textureHandle_ = textureHandle; }
	Transform& GetTransform() { return transform; }
private:

	// デバイスを受け取ってバッファを作る関数
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResources(ID3D12Device* device, size_t sizeInBytes);

	SpriteCommon* spriteCommon = nullptr;

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource>vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>indexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>materialResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>transformationMatrixResource;

	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	

	//バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	uint32_t* indexData = nullptr;
	Material* materialData = nullptr;
	TransformationMatrix* transformationMatrixData = nullptr;

	Vector2 position = { 0.0f,0.0f };

	float rotation = 0.0f;

	Vector2 size = { 640.0f,360.0f };

	Transform transform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_;

	Transform uvTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

};

