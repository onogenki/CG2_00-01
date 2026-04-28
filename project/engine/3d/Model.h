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
#include<assimp/Importer.hpp>
#include<assimp/scene.h>
#include<assimp/postprocess.h>
#include <map>
#include<optional>

//見た目のモデル
class Model
{
public:

	//キーフレーム
	template<typename tValue>
	struct Keyframe
	{
		float time;//キーフレームの時刻
		tValue value;//キーフレームの幅
	};
	using KeyframeVector3 = Keyframe<Vector3>;
	using KeyframeQuaternion = Keyframe<Quaternion>;

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
	};

	struct Node
	{
		QuaternionTransform transform;
		Matrix4x4 localMatrix;
		std::string name;
		std::vector<Node> children;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		MaterialData material;
		Node rootNode;
	};

	template<typename tValue>
	struct AnimationCurve
	{
		std::vector<Keyframe<tValue>> keyframes;
	};

	struct NodeAnimation
	{
		AnimationCurve<Vector3> translate;
		AnimationCurve<Quaternion>rotate;
		AnimationCurve<Vector3> scale;
	};

	struct Animation
	{
		float duration;//アニメーション全体の尺(単位は秒)
		//NodeAnimationの集合。Node名でひけるようにしておく
		std::map<std::string, NodeAnimation>nodeAnimations;
	};

	struct Joint
	{
		QuaternionTransform transform;//Transform情報
		Matrix4x4 localMatrix;
		Matrix4x4 skeletonSpaceMatrix;//skeletonSpaceでの変換行列
		std::string name;
		std::vector<int32_t> children;//子JointのIndexのリスト。いなければ空
		int32_t index;
		std::optional<int32_t>parent;//親JointのIndex、いなければnull
	};

	struct Skeleton
	{
		int32_t root;//RootJointのIndex
		std::map<std::string, int32_t>jointMap;//Joint名とIndexとの辞書
		std::vector<Joint>joints;//所属しているジョイント
	};

	Skeleton CreateSkeleton(const Node& rootNode);

	int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);

	//アニメーションの解析
	static Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

	static Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
	static Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

	Node ReadNode(aiNode* node);

	void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);

	//skeletonの更新
	void Update(Skeleton& skeleton);

	void Draw();

	void SetTexture(const std::string& filePath);

	//アニメーション適用
	void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);

	ModelData& GetModelData() { return modelData; }
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



	void LoadModelFile(const std::string& directoryPath, const std::string& filename);
	void CreateVertexData();
	void CreateMaterialData();
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResources(ID3D12Device* device, size_t sizeInBytes);

};