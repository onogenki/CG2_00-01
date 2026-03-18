#include "Object3d.h"
#include "Object3dCommon.h"
#include "ModelManager.h"
#include <cassert>
#include "MyMath.h"
using namespace MyMath;

void Object3d::Initialize(Object3dCommon* object3dCommon)
{//引数で受け取ってメンバ変数に記録する

	this->object3dCommon = object3dCommon;

	//デフォルトカメラを自分にセット
	this->camera = object3dCommon->GetDefaultCamera();

	//座標変換行列データ作成
	CreateTransformationMatrixData();
	//平行光源データ作成
	CreateDirectionalLightData();
	//カメラデータ作成
	CreateCameraData();

	//Transform変数を作る
	transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
}

void Object3d::Update()
{
	//transform.rotate.y += 3.14f / 180.0f;
	// 1. TransformからWorldMatrixを作る
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 worldViewProjectionMatrix;
	if (camera)
	{
		const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
		cameraData->worldPosition = camera->GetTranslate();
	} else
	{
		worldViewProjectionMatrix = worldMatrix;
	}
	transformationMatrixData->WVP = worldViewProjectionMatrix;
	transformationMatrixData->World = worldMatrix;
}

void Object3d::Draw()
{
	// コマンドリストを取得
	ID3D12GraphicsCommandList* commandList = object3dCommon->GetDxCommon()->GetCommandList();

	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());

	if (model_)
	{
		model_->Draw();
	}
}

void Object3d::SetModel(const std::string& filePath)
{// モデルマネージャからモデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
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
	directionalLightData->direction = { 0.0f, 1.0f, 0.0f };

	// 強さ: 1.0 (標準)
	directionalLightData->intensity = 1.0f;
}

void Object3d::CreateCameraData()
{
	//カメラ
	cameraResource = CreateBufferResources(object3dCommon->GetDxCommon()->GetDevice(), sizeof(CameraForGPU));


	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));

	cameraData->worldPosition = { 0.0f, 0.0f, 10.0f };
}