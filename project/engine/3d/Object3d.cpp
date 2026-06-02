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
	{// 実時間を 1/60 秒ずつ進める
		animationTime_ += 1.0f / 60.0f;

		// ループOFFなら、自動的にアニメーションの1本分の長さ（duration)にする
		if (!isLoop_ && maxPlayTime_ == 0.0f) {
			maxPlayTime_ = currentAnimation_.duration;
		}

		//時間指定されてる場合の止まる処理
		if (maxPlayTime_ > 0.0f)
		{
			if (animationTime_ >= maxPlayTime_)
			{
				animationTime_ = maxPlayTime_;//指定時間でタイマー固定
			}
		}

		// 実際にモデルを動かすループ時間を計算
		float finalRenderTime = std::fmod(animationTime_, currentAnimation_.duration);

		// もし制限時間に達して止まったなら、最後のポーズで固定する
		if (maxPlayTime_ > 0.0f && animationTime_ >= maxPlayTime_) {
			finalRenderTime = std::fmod(maxPlayTime_, currentAnimation_.duration);
		}
		//アニメーションを骨に適用して行列をアップデート
		model_->ApplyAnimation(skeleton_, currentAnimation_, finalRenderTime);
		//親子関係を計算してskeletonSpaceMatrixを更新
		model_->Update(skeleton_);
		model_->Update(skinCluster_, skeleton_);
	}
	//TransformからWorldMatrixを作る
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

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
		//アニメーションモデルならアドレスを渡す
		if (isSkeletal_)
		{
			model_->Draw(skinCluster_);
		} else
		{
			model_->Draw();
		}
	}
}

void Object3d::SetModel(const std::string& filePath)
{
	// モデルマネージャからモデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
}
// この関数を初期化時に1回だけ呼ぶ
void Object3d::InitializeAnimation()
{
	//既に同じモデルがセットされている場合は、再生成を防ぐために何もせず抜ける
	if (!model_)return;

	//モデルの階層構造からスケルトンを生成
	skeleton_ = model_->CreateSkeleton(model_->GetModelData().rootNode);
	//ジョイントがあればスケルトンモデルとして扱う
	isSkeletal_ = !skeleton_.joints.empty();

	if (isSkeletal_) {
		//スケルトンがあるならSkinClusterも作成する
		// （引数の descriptorHeap や device は DxCommon 等から引っ張ってくる）
		auto device = object3dCommon->GetDxCommon()->GetDevice();
		auto srvHeap = SrvManager::GetInstance()->GetDescriptorHeap();
		uint32_t descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		skinCluster_ = model_->CreateSkinCluster(device, skeleton_, model_->GetModelData(), srvHeap, descriptorSize);
	}
	else
	{
		isSkeletal_ = false;//アニメーションしないモデルはfalse
	}
}

void Object3d::PlayAnimation(const Model::Animation& animation)
{
	currentAnimation_ = animation;
	animationTime_ = 0.0f;
	isAnimating_ = true;
}

void Object3d::CreateTransformationMatrixData()
{
	// 1. 座標変換行列リソースを作る
	// (サイズは TransformationMatrix 構造体1つ分)
	transformationMatrixResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));

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
	directionalLightResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLight));

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
	cameraResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(CameraForGPU));


	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));

	cameraData->worldPosition = { 0.0f, 0.0f, 10.0f };
}

void Object3d::CreatePointLightData()
{
	// 1. 点光源リソースを作る
	// (サイズは PointLight 構造体1つ分)
	pointLightResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(PointLight));

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
	spotLightResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(SpotLight));
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