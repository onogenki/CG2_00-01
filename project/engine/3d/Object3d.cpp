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
	//カメラデータ作成
	CreateCameraData();
	//平行光源データ作成
	CreateDirectionalLightData();
	//点光源データ作成
	CreatePointLightData();
	//スポットライトデータ作成
	CreateSpotLightData();
	//Transform変数を作る
	transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
}

void Object3d::Update()
{

	if (model_ && isAnimating_)
	{
		animationTime_ += 1.0f / 60.0f;//時刻を進める。1/60で固定してあるが、計算した時間を使って可変フレーム対応するほうが望ましい
		animationTime_ = std::fmod(animationTime_, currentAnimation_.duration);//最後まで行ったら最初からリポート再生

		if (isSkeletal_)//スケルトンがある場合
		{//全てのジョイントにアニメーションを適用
			model_->ApplyAnimation(skeleton_, currentAnimation_, animationTime_);
			//親子関係を計算してskeletonSpaceMatrixを更新
			model_->Update(skeleton_);
		} else {//RootNodeだけ動かす場合
			std::string rootName = model_->GetModelData().rootNode.name;
			if (currentAnimation_.nodeAnimations.count(rootName))
			{
				Model::NodeAnimation& rootNodeAnimation = currentAnimation_.nodeAnimations[rootName];//rootNodeのAnimationを取得

				Vector3 translate = Model::CalculateValue(rootNodeAnimation.translate.keyframes, animationTime_);//指定時刻の値を取得。関数の詳細は次ページ
				Quaternion rotate = Model::CalculateValue(rootNodeAnimation.rotate.keyframes, animationTime_);
				Vector3 scale = Model::CalculateValue(rootNodeAnimation.scale.keyframes, animationTime_);

				// モデルの rootNode の localMatrix を更新する
				model_->GetModelData().rootNode.localMatrix = MakeAffineMatrixQuaternion(scale, rotate, translate);
			}
		}
	}
//TransformからWorldMatrixを作る
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	if (model_)
	{
		if (!isSkeletal_)
		{
			Matrix4x4 rootMatrix = model_->GetModelData().rootNode.localMatrix;
			worldMatrix = Multiply(rootMatrix, worldMatrix);
		}
	}
	if (camera)
	{
		const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
		transformationMatrixData->WVP = Multiply(worldMatrix, viewProjectionMatrix);
		transformationMatrixData->World = worldMatrix;
		cameraData->worldPosition = camera->GetTranslate();
	}
	Matrix4x4 inverseMatrix = Inverse(worldMatrix);
	transformationMatrixData->WorldInverseTranspose = Transpose(inverseMatrix);
}

void Object3d::Draw()
{
	// コマンドリストを取得
	ID3D12GraphicsCommandList* commandList = object3dCommon->GetDxCommon()->GetCommandList();

	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(5, pointLightResource->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(6, spotLightResource->GetGPUVirtualAddress());

	if (model_)
	{
		model_->Draw();
	}
}

void Object3d::SetModel(const std::string& filePath)
{// モデルマネージャからモデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
	if (model_)
	{
		//モデルの階層構造からスケルトンを生成
		skeleton_ = model_->CreateSkeleton(model_->GetModelData().rootNode);
		//ジョイントがあればスケルトンモデルとして扱う
		isSkeletal_ = !skeleton_.joints.empty();
	}
}

void Object3d::PlayAnimation(const Model::Animation& animation)
{
	currentAnimation_ = animation;
	animationTime_ = 0.0f;
	isAnimating_ = true;
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

	// 向き: 斜めの光 (X, Y, Z)
	directionalLightData->direction = { 1.0f, -1.0f, 1.0f };

	directionalLightData->direction = Normalize(directionalLightData->direction);

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

void Object3d::CreatePointLightData()
{
	// 1. 点光源リソースを作る
	// (サイズは PointLight 構造体1つ分)
	pointLightResource = CreateBufferResources(object3dCommon->GetDxCommon()->GetDevice(), sizeof(PointLight));

	// 2. データを書き込むためのアドレスを取得して pointLightData に割り当てる
	pointLightResource->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData));

	// 3. デフォルト値を書き込んでおく
	// 色: 白 (R, G, B, A)
	pointLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 位置
	pointLightData->position = { 0.0f, 2.0f, 0.0f };
	// 強さ
	pointLightData->intensity = 1.0f;

	pointLightData->radius = 10.0f;//届く距離
	pointLightData->decay = 1.0f;//普通の線形減衰
}

void Object3d::CreateSpotLightData()
{
	spotLightResource = CreateBufferResources(object3dCommon->GetDxCommon()->GetDevice(), sizeof(SpotLight));
	spotLightResource->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData));

	// 資料通りの初期値
	spotLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	spotLightData->position = { 2.0f, 1.25f, 0.0f };
	spotLightData->distance = 7.0f;
	spotLightData->direction = Normalize({ -1.0f, -1.0f, 0.0f });
	spotLightData->intensity = 4.0f;
	spotLightData->decay = 2.0f;
	spotLightData->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);// 限界の角度 (60度)
	spotLightData->cosFalloffStart = 1.0f;//1.0 = 中心(0度)から減衰が始まる
}