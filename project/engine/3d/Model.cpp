#include "Model.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <fstream>
#include <sstream>
#include <cassert>

using namespace MyMath;

Model::Skeleton Model::CreateSkeleton(const Node& rootNode)
{
	Skeleton skeleton;
	skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

	//名前とindexのマッピングを行いアクセスしやすくなる
	for (const Joint& joint : skeleton.joints)
	{
		skeleton.jointMap.emplace(joint.name, joint.index);
	}

	//作成直後にも一度更新をかけておくと初期姿勢が正しく計算される
	Update(skeleton);

	return skeleton;
}

int32_t Model::CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints)
{
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonSpaceMatrix = MakeIdentity4x4();
	joint.transform = node.transform;
	joint.index = uint32_t(joints.size());//現在登録されてる数をindexに
	joint.parent = parent;

	joints.push_back(joint);//skeletonのJoint列に追加

	for (const Node& child : node.children)
	{
		//子Jointを作成し、そのIndexを登録
		int32_t childIndex = CreateJoint(child, joint.index, joints);
		joints[joint.index].children.push_back(childIndex);//push_backなどで再確保が走るとアドレス変わるためindex
	}
	//自身のIndexを返す
	return joint.index;
}

Model::Animation Model::LoadAnimationFile(const std::string& directoryPath, const std::string& filename)
{
	//アニメーション
	Animation animation;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);

	assert(scene->mNumAnimations != 0);//アニメーションがない
	aiAnimation* animationAssimp = scene->mAnimations[0];  //周波数における長さ/周波数

	animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond);//時間の単位を秒に変換

	//NodeAnimation解析する
	for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex)
	{
		aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];

		NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex)
		{
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);//秒に変換
			keyframe.value = { -keyAssimp.mValue.x,keyAssimp.mValue.y,keyAssimp.mValue.z };//右手->左手
			nodeAnimation.translate.keyframes.push_back(keyframe);
		}
		//RotateはmNumRotationKeys/mRotationKeys,ScaleはmNumScalingKeys/mScalingKeysで取得できるので同様に行う

		//Rotate
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex)
		{
			aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
			KeyframeQuaternion keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // 秒に変換
			// 右手->左手 (Quaternionの場合は y と z を反転)
			//RotateはQuaternionで右手->左手に変換するために、yとzを反転させる必要がある。
			keyframe.value = { keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w };

			nodeAnimation.rotate.keyframes.push_back(keyframe);
		}

		//Scale
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex)
		{
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // 秒に変換
			// Scale は変換不要　scaleはそのままでよい。
			keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };

			nodeAnimation.scale.keyframes.push_back(keyframe);
		}
	}
	//解析完了
	return animation;
}

Vector3 Model::CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time)
{
	assert(!keyframes.empty());//キーがないものは返す値がわからないので駄目
	if (keyframes.size() == 1 || time <= keyframes[0].time)//キーが1つか時刻がキーフレーム前なら最初の値とする
	{
		return keyframes[0].value;
	}
	for (size_t index = 0; index < keyframes.size() - 1; ++index)
	{
		size_t nextIndex = index + 1;
		//indexとnextIndexの2つのkeyframeを取得して範囲内に時刻があるかを判定
		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time)
		{
			//範囲内を補間する
			float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
			return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
		}
	}
	//ここまできた場合は一番後の時刻よりも後ろなので最後の値を返すことにする
	return (*keyframes.rbegin()).value;
}

Quaternion Model::CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time)
{
	assert(!keyframes.empty());
	if (keyframes.size() == 1 || time <= keyframes[0].time) {
		return keyframes[0].value;
	}
	for (size_t index = 0; index < keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;
		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
			float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
			return Slerp(keyframes[index].value, keyframes[nextIndex].value, t); // ここは Slerp！
		}
	}
	return (*keyframes.rbegin()).value;
}

Model::Node Model::ReadNode(aiNode* node)
{
	Node result;
	aiVector3D scale, translate;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale, rotate, translate);//assimpの行列からSRTを抽出する関数を利用
	result.transform.scale = { scale.x,scale.y,scale.z };//scaleはそのまま
	result.transform.rotate = { rotate.x,-rotate.y,-rotate.z,rotate.w };//X軸を反転、さらに回転方向が逆なので軸を反転させる
	result.transform.translate = { -translate.x,translate.y,translate.z };//X軸を反転
	result.localMatrix = MakeAffineMatrixQuaternion(result.transform.scale, result.transform.rotate, result.transform.translate);

	result.name = node->mName.C_Str();//Node名を格納
	result.children.resize(node->mNumChildren);//子供の数だけ確保
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
	{
		//再帰的に読んで階層構造体を作っていく
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}

	return result;
}

void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename)
{

	//ModelCommonのポインタを引数からメンバ変数に記録する
	modelCommon_ = modelCommon;
	//モデル読み込み
	LoadModelFile(directoryPath, filename);
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

void Model::Update(Skeleton& skeleton)
{
	//すべてのJointを更新 親が若いので通常ループで処理可能になっている
	for (Joint& joint : skeleton.joints)
	{
		joint.localMatrix = MakeAffineMatrixQuaternion(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
		if (joint.parent)//親がいれば親の行列をかける
		{
			joint.skeletonSpaceMatrix = Multiply(joint.localMatrix, skeleton.joints[*joint.parent].skeletonSpaceMatrix);
		} else {//親がいないのでlocalMatrixとskeletonSpaceMatrixは一致する
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
	}
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

//アニメーション適用
void Model::ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime)
{
	for (Joint& joint : skeleton.joints)
	{
		//対象のJointのAnimationがあれば値の適用を行う。初期化付きif文
		if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end())
		{
			const NodeAnimation& rootNodeAnimation = (*it).second;
			joint.transform.translate = CalculateValue(rootNodeAnimation.translate.keyframes, animationTime);
			joint.transform.rotate = CalculateValue(rootNodeAnimation.rotate.keyframes, animationTime);
			joint.transform.scale = CalculateValue(rootNodeAnimation.scale.keyframes, animationTime);
		}
	}
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

void Model::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
	// メンバ変数をクリアしておく
	modelData.vertices.clear();

	//assimpでobjを読む
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;

	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate);
	assert(scene->HasMeshes());//メッシュがないのは対応しない

	modelData.rootNode = ReadNode(scene->mRootNode);

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
}