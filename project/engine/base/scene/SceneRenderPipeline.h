#pragma once

#include <memory>
#include <vector>

class Camera;
class DirectXCommon;
class Object3d;
class Object3dCommon;
class Sprite;
class SpriteCommon;

// Scene共通の描画開始・PostEffect・ImGui出力の順番をまとめる部品です。
// 各Sceneはこの部品を使い、「何を描くか」だけをDraw関数に書きます。
class SceneRenderPipeline
{
public:
	// 3D一覧のうち、どの種類を描くかを選ぶ条件です。
	enum class ObjectDrawFilter
	{
		// Animationの有無を問わず全Object3dを描きます。
		kAll,
		// Skeletonを持つAnimation Objectだけを描きます。
		kSkeletalOnly,
		// Skeletonを持たない通常Objectだけを描きます。
		kNonSkeletalOnly,
	};

	// RenderTextureへの描画を始める前に、DirectXとSRVの共通設定を行います。
	static void Begin(DirectXCommon* directXCommon);
	// Object3dの共通Pipelineを設定してから、指定条件に合う一覧をまとめて描画します。
	static void DrawObjects(
		Object3dCommon* object3dCommon,
		const std::vector<std::unique_ptr<Object3d>>& objects,
		ObjectDrawFilter filter = ObjectDrawFilter::kAll);
	// Spriteの共通Pipelineを設定してから、空要素を飛ばして一覧を描画します。
	static void DrawSprites(
		SpriteCommon* spriteCommon,
		const std::vector<std::unique_ptr<Sprite>>& sprites);
	// Sceneのモデル描画後に、PostEffect・SwapChain・ImGui・Presentまでを順番に実行します。
	static void End(DirectXCommon* directXCommon, const Camera* activeCamera);
};
