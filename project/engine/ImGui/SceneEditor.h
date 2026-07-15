#pragma once
#include "Object3d.h"
#include "Sprite.h"
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class SceneEditor
{
public:
	struct ShelfEntry {
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
		std::vector<ShelfEntry> entries;
		std::string selectedEntry;
		std::string message;
	};

	struct ShelfCallbacks {
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

	static void ScanResourceShelf(ShelfState& state);
	static void DrawModelShelf(ShelfState& state, const ShelfCallbacks& callbacks);
	static void HandleShelfDropOnGameView(ShelfState& state, const ShelfCallbacks& callbacks);
	static void DrawInspector(const InspectorOptions& options);
};
