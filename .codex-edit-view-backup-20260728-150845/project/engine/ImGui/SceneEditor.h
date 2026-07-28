#pragma once
#include "Object3d.h"
#include "Sprite.h"
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ImGui上でリソース棚とシーン内オブジェクトの編集UIを構築する。
// 実データの追加・削除はコールバックでGamePlaySceneへ委譲する。
class SceneEditor
{
public:
	struct ShelfEntry {
		// resources配下で見つけたモデルまたはテクスチャ1件の表示情報。
		std::string fileName;
		std::string displayName;
		std::string textureFilePath;
		bool hasMesh = false;
		bool hasAnimation = false;
		bool canLoad = false;
		bool isTexture = false;
		unsigned int textureSrvIndex = 0;
		Vector2 textureSize{};
		std::vector<std::pair<Vector3, Vector3>> thumbnailLines;
		std::vector<std::array<Vector3, 3>> thumbnailTriangles;
		Vector3 thumbnailCenter{};
		float thumbnailRadius = 1.0f;
	};

	struct ShelfState {
		// スキャン結果と、UI上で現在選択されている項目を保持する。
		std::vector<ShelfEntry> entries;
		std::string selectedEntry;
		std::string message;
	};

	struct ShelfCallbacks {
		// リソースを追加・プレビューするためにシーン側が提供する処理。
		std::string sceneLabel = "Scene";
		size_t addedModelCount = 0;
		size_t addedTextureCount = 0;
		std::function<bool(const std::string&)> addModel;
		std::function<bool(const std::string&)> addTexture;
		std::function<void()> clearAdded;
		std::function<bool(const ShelfEntry&)> previewEntry;
		std::function<void()> afterAdd;
		std::function<void()> drawExtraToolbar;
		std::function<void()> drawExtraStatus;
		bool previewOnDoubleClick = false;
	};

	struct InspectorOptions {
		// インスペクターが直接編集するシーンデータとUI制御用の設定。
		const char* description = nullptr;
		std::vector<std::unique_ptr<Sprite>>* sprites = nullptr;
		std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		Object3d::DirectionalLight* directionalLight = nullptr;
		Object3d::PointLight* pointLight = nullptr;
		Object3d::SpotLight* spotLight = nullptr;
		size_t addedSpriteCount = 0;
		size_t protectedSpriteCount = 0;
		size_t protectedNormalObjectCount = 0;
		size_t protectedAnimationObjectCount = 0;
		int forcedSpriteIndex = -1;
		int forcedNormalIndex = -1;
		int forcedAnimationIndex = -1;
		bool selectSpriteTab = false;
		bool selectModelTab = false;
		bool forceDock = false;
		std::function<void(size_t)> removeSprite;
		std::function<void(bool animationObject, size_t index)> onModelRemoved;
		std::function<void()> drawHeader;
		std::function<void()> drawExtraTabs;
	};

	// resources配下を調べ、棚へ表示するモデル・テクスチャを更新する。
	static void ScanResourceShelf(ShelfState& state);
	// モデル棚を描画し、追加・プレビュー操作をコールバックへ渡す。
	static void DrawModelShelf(ShelfState& state, const ShelfCallbacks& callbacks);
	// Game View上へのドラッグ&ドロップを処理する。
	static void HandleShelfDropOnGameView(ShelfState& state, const ShelfCallbacks& callbacks);
	// 選択済みスプライト、モデル、ライトを編集するインスペクターを描画する。
	static void DrawInspector(const InspectorOptions& options);
};
