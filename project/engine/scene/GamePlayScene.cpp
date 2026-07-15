#include "GamePlayScene.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "PostEffect.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "LevelLoader.h"
#include"SceneManager.h"
#include <shlobj.h>
#include <shellapi.h>
#include <vfw.h>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <dinput.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>
#pragma comment(lib, "vfw32.lib")
using namespace MyMath;

namespace {
Vector3 TransformPoint(const Vector3& point, const Matrix4x4& matrix)
{
	return {
		point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0],
		point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1],
		point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2],
	};
}

bool TransformCoord(const Vector3& point, const Matrix4x4& matrix, Vector3& outPoint)
{
	const float x = point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0];
	const float y = point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1];
	const float z = point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2];
	const float w = point.x * matrix.m[0][3] + point.y * matrix.m[1][3] + point.z * matrix.m[2][3] + matrix.m[3][3];
	if (std::abs(w) <= 0.0001f) {
		return false;
	}
	outPoint = { x / w, y / w, z / w };
	return true;
}

bool IsAabbOverlapping(const AABB& lhs, const AABB& rhs)
{
	return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x &&
		lhs.min.y <= rhs.max.y && lhs.max.y >= rhs.min.y &&
		lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z;
}

#ifdef USE_IMGUI
float DistanceToSegment(const ImVec2& point, const ImVec2& start, const ImVec2& end)
{
	const ImVec2 segment(end.x - start.x, end.y - start.y);
	const ImVec2 toPoint(point.x - start.x, point.y - start.y);
	const float lengthSquared = segment.x * segment.x + segment.y * segment.y;
	if (lengthSquared <= 0.0001f) {
		const float dx = point.x - start.x;
		const float dy = point.y - start.y;
		return std::sqrt(dx * dx + dy * dy);
	}
	const float t = std::clamp((toPoint.x * segment.x + toPoint.y * segment.y) / lengthSquared, 0.0f, 1.0f);
	const ImVec2 closest(start.x + segment.x * t, start.y + segment.y * t);
	const float dx = point.x - closest.x;
	const float dy = point.y - closest.y;
	return std::sqrt(dx * dx + dy * dy);
}
#endif

bool OpenFolderInExplorer(const std::filesystem::path& directory)
{
	std::error_code errorCode;
	std::filesystem::create_directories(directory, errorCode);
	if (errorCode) {
		return false;
	}

	HINSTANCE result = ShellExecuteW(
		nullptr,
		L"open",
		directory.wstring().c_str(),
		nullptr,
		nullptr,
		SW_SHOWNORMAL);
	return reinterpret_cast<INT_PTR>(result) > 32;
}

bool OpenFileWithDefaultApp(const std::filesystem::path& filePath)
{
	std::error_code errorCode;
	if (!std::filesystem::exists(filePath, errorCode) || errorCode) {
		return false;
	}

	HINSTANCE result = ShellExecuteW(
		nullptr,
		L"open",
		filePath.wstring().c_str(),
		nullptr,
		nullptr,
		SW_SHOWNORMAL);
	return reinterpret_cast<INT_PTR>(result) > 32;
}

std::string GetEnvironmentString(const char* name)
{
	char* value = nullptr;
	size_t size = 0;
	if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
		return {};
	}

	std::string result(value);
	std::free(value);
	return result;
}
}

void GamePlayScene::Initialize()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);
	ParticleManager::GetInstance()->ClearAllGroups();

	//3Dオブジェクト�E通部の初期匁E
	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);

	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(dxCommon);

	//カメラマネージャ
	cameraManager = std::make_unique<CameraManager>();


	ParticleManager::GetInstance()->SetCameraManager(cameraManager.get());

	//メインカメラ
	mainCamera = std::make_unique<Camera>();
	mainCamera->SetTranslate({ 0.0f,0.0f,-10.0f });
	cameraManager->AddCamera("MainCamera", mainCamera.get());

	//上アングルカメラ
	upCamera = std::make_unique<Camera>();
	upCamera->SetRotate({ 0.785f,0.0f,0.0f });
	upCamera->SetTranslate({ 0.0f,5.0f,-5.0f });
	cameraManager->AddCamera("UpCamera", upCamera.get());

	//MainCameraをアクチE��チE
	cameraManager->SetActiveCamera("MainCamera");
	//共通部にはマネージャのアクチE��ブカメラを渡ぁE
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");//1枚目
	TextureManager::GetInstance()->LoadTexture("Resources/monsterBall.png");//2枚目
	TextureManager::GetInstance()->LoadTexture("Resources/grass.png");//terrainのpng
	TextureManager::GetInstance()->LoadTexture("Resources/circle.png");
	TextureManager::GetInstance()->LoadTexture("Resources/circle2.png");
	TextureManager::GetInstance()->LoadTexture("Resources/gradationLine.png");

	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "Resources/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Plane", "Resources/uvChecker.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Hit", "Resources/circle2.png");
	ParticleManager::GetInstance()->CreateRingParticleGroup("Ring", "Resources/gradationLine.png");
	ParticleManager::GetInstance()->CreateCylinderParticleGroup("Cylinder", "Resources/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("PillarSparkle", "Resources/circle2.png");
	ParticleManager::GetInstance()->CreateParticleGroup("LightCore", "Resources/circle2.png");
	ParticleManager::GetInstance()->CreateParticleGroup("LightRain", "Resources/gradationLine.png");
	ParticleManager::GetInstance()->CreateParticleGroup("LightSpiral", "Resources/circle2.png");
	
	//.objファイルからモチE��を読み込む
	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("AnimatedCube.gltf");//アニメーションありのモチE��
	ModelManager::GetInstance()->LoadModel("walk.gltf");//アニメーションのみだが忁E��E
	ModelManager::GetInstance()->LoadModel("sneakWalk.gltf");//アニメーションのみだが忁E��E
	ModelManager::GetInstance()->LoadModel("human.gltf");//持ってきたも�E
	ModelManager::GetInstance()->LoadModel("playerCloudAnimation.gltf");//持ってきたアニメーション

	//アニメーションの読み込み
	walkAnimation_ = Model::LoadAnimationFile("./resources", "walk.gltf");//アニメーションのみ
	sneakWalkAnimation_ = Model::LoadAnimationFile("./resources", "sneakWalk.gltf");//アニメーションのみ
	humanAnimation_ = Model::LoadAnimationFile("./resources", "human.gltf");//持ってきたも�E
	hissatu_ = Model::LoadAnimationFile("./resources", "playerCloudAnimation.gltf");//持ってきたも�E

	// skyBoxの背景
	TextureManager::GetInstance()->LoadTexture("Resources/qwantani_moon_noon_puresky_1k.dds");

	// skyBoxによってモチE��の反封E��映り込む
	object3dCommon->SetEnvironmentTexturePath("Resources/qwantani_moon_noon_puresky_1k.dds");

	//Skybox
	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(dxCommon, cameraManager->GetActiveCamera());
	// 添付されてぁE��DDSチE��スチャのパスを指定すめE
	skyBox_->SetTexture("Resources/qwantani_moon_noon_puresky_1k.dds");

	//音声読み込み
	Audio::GetInstance()->LoadFile("Resources/Alarm01.wav");
	//音声再生
	Audio::GetInstance()->PlayWave("Resources/Alarm01.wav");

	///
	/// 3Dオブジェクト生戁E
	/// normalとanimationに刁E��てる�Eは処琁E��軽くするためE
	
	// 一時的に unique_ptr を作り、�E期化してから vector に move する
	auto objPlane = std::make_unique<Object3d>();
	objPlane->Initialize(object3dCommon);
	objPlane->SetModel("terrain.obj");
	objPlane->GetTransform().translate = { 1.0f, -2.0f, 10.0f };
	objectPlane = objPlane.get();           // 中身を指すだけ�Eポインタを保孁E
	normalObjects.push_back(std::move(objPlane));//通常(obj)モチE��入れる

	auto objAxis = std::make_unique<Object3d>();
	objAxis->Initialize(object3dCommon);
	objAxis->SetModel("human.gltf");//アニメーションモチE��読み込み
	objAxis->InitializeAnimation();//skinClusterぁE回だけ作られてisSkeletal_がtrueになめE
	objAxis->SetEnvironmentCoefficient(0.3f);//こ�EモチE��の反封E�E強ぁE
	objAxis->GetTransform().translate = { 2.0f, 0.0f, 0.0f };
	objAxis->GetTransform().rotate = { 0.0f,0.0f,0.0f };
	objAxis->GetTransform().scale = { 0.2f,0.2f,0.2f };

	objAxis->PlayAnimation(humanAnimation_);//アニメーション再生

	//1ループ�E生させる場吁Eループや時間持E��させたぁE��合�E削除する)
	objAxis->SetIsLoop(false);
	//時間持E��したいなら利用(アニメーション時間で演�Eなどができる)
	//objAxis->SetMaxPlayTime(6.0f);

	objectAxis = objAxis.get();//外部保存用に記録

	animationObjects.push_back(std::move(objAxis));//アニメーションモチE��専用に登録

	// レベルチE�Eタからオブジェクトを生�E、E�E置
	std::unique_ptr<LevelLoader::LevelData> levelData(LevelLoader::Load("scene"));
	auto createLevelObjects = [&](auto&& self, const std::vector<LevelLoader::ObjectData>& objects) -> void
	{
		for (const LevelLoader::ObjectData& objectData : objects)
		{
			if (objectData.type == "MESH" && !objectData.fileName.empty())
			{
				const std::string modelPath = "resources/" + objectData.fileName;
				std::ifstream modelFile(modelPath);
				if (!modelFile.fail())
				{
					if (!ModelManager::GetInstance()->LoadModel(objectData.fileName)) {
						self(self, objectData.children);
						continue;
					}

					auto newObject = std::make_unique<Object3d>();
					newObject->Initialize(object3dCommon);
					newObject->SetModel(objectData.fileName);
					newObject->SetTranslate(objectData.translation);
					newObject->SetRotate(objectData.rotation);
					newObject->SetScale(objectData.scaling);

					normalObjects.push_back(std::move(newObject));
				}
			}

			self(self, objectData.children);
		}
	};
	if (levelData) {
		createLevelObjects(createLevelObjects, levelData->objects);
	}
	
	for (uint32_t i = 0; i < 1; ++i)
	{
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon, "Resources/monsterBall.png");

		if (i % 2 == 0) {
			// 偶数にモンスターボ�Eルpng
			sprite->SetTexture("Resources/uvChecker.png");
		}
		Vector2 pos = { 0.0f + i * 0.0f, 0.0f + i * 50.0f };
		sprite->SetPosition(pos);

		sprites.push_back(std::move(sprite));
	}

	// ライト�E初期値を設定すめE
	// 平行�E源�EOFF (Intensity = 0.0f)
	directionalLight.direction = { 1.0f, -1.0f, 1.0f };
	directionalLight.intensity = 0.0f;
	directionalLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 点光源�EON (初期位置 0, 2, 0)
	pointLight.position = { 0.0f, 2.0f, 0.0f };
	pointLight.intensity = 1.0f;
	pointLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLight.radius = 10.0f;
	pointLight.decay = 1.0f;

	//スポットライチE
	spotLight.position = { 2.0f,1.25f,0.0f };
	spotLight.intensity = 4.0f;
	spotLight.color = { 1.0f,1.0f,1.0f,1.0f };
	spotLight.distance = 7.0f;
	spotLight.direction =
		Normalize({ -1.0f,-1.0f,0.0f });
	spotLight.decay = 2.0f;
	spotLight.cosAngle =
		std::cos(std::numbers::pi_v<float> / 3.0f);
	spotLight.cosFalloffStart = 1.0f;

	//パ�EチE��クル
	//座標、E回�E発生数、発生頻度[秒]
	emitterTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	//Circleパ�EチE��クル
	emitterCircle = std::make_unique<ParticleEmitter>("Circle", emitterTransform, 1, 0.1f,false);
	//四角形のパ�EチE��クル(風に吹かれる方)
	emitterPlane = std::make_unique<ParticleEmitter>("Plane", emitterTransform, 1, 0.3f,true);

	//最初�Ecircleにする
	activeEmitter = emitterCircle.get();

	selectedUI = 0;
	inspectorForceDockFrames_ = 120;
	baseNormalObjectCount_ = normalObjects.size();
	baseAnimationObjectCount_ = animationObjects.size();
	baseSpriteCount_ = sprites.size();
	ScanResourceModels();
	InitializeUiSmokeFromEnvironment();

}

void GamePlayScene::ScanResourceModels()
{
	SceneEditor::ShelfState state{};
	state.entries = std::move(modelLibrary_);
	state.selectedEntry = selectedLibraryModel_;
	SceneEditor::ScanResourceShelf(state);
	modelLibrary_ = std::move(state.entries);
	selectedLibraryModel_ = state.selectedEntry;

	if (isModelPreviewMode_) {
		const bool previewStillLoadable = std::any_of(modelLibrary_.begin(), modelLibrary_.end(), [&](const ResourceModelEntry& entry) {
			return (!isTexturePreviewMode_ && entry.fileName == previewModelFile_ && entry.canLoad) ||
				(isTexturePreviewMode_ && entry.textureFilePath == previewTextureFilePath_ && entry.isTexture);
		});
		if (!previewStillLoadable) {
			ExitModelPreview();
		}
	}
}
std::unique_ptr<Object3d> GamePlayScene::CreateObjectFromModel(const std::string& fileName, bool playAnimation)
{
	auto modelIt = std::find_if(modelLibrary_.begin(), modelLibrary_.end(), [&](const ResourceModelEntry& entry) {
		return entry.fileName == fileName;
	});
	if (modelIt == modelLibrary_.end() || !modelIt->canLoad) {
		return nullptr;
	}

	if (!ModelManager::GetInstance()->LoadModel(fileName)) {
		return nullptr;
	}

	auto object = std::make_unique<Object3d>();
	object->Initialize(object3dCommon);
	object->SetModel(fileName);
	object->InitializeAnimation();
	object->SetCamera(cameraManager ? cameraManager->GetActiveCamera() : nullptr);
	object->SetDirectionalLight(directionalLight);
	object->SetPointLight(pointLight);
	object->SetSpotLight(spotLight);

	if (object->IsSkeletal() && modelIt->hasAnimation && playAnimation) {
		Model::Animation animation = Model::LoadAnimationFile("./resources", fileName);
		if (animation.duration > 0.0f) {
			object->PlayAnimation(animation);
			object->SetIsLoop(true);
		}
	}

	return object;
}

bool GamePlayScene::AddModelToScene(const std::string& fileName)
{
	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	Vector3 spawnPosition = { 0.0f, 0.0f, 4.0f };
	if (activeCamera) {
		spawnPosition = activeCamera->GetTranslate();
		spawnPosition.z += 6.0f;
	}
	return AddModelToScene(fileName, spawnPosition);
}

bool GamePlayScene::AddModelToScene(const std::string& fileName, const Vector3& spawnPosition)
{
	auto object = CreateObjectFromModel(fileName, true);
	if (!object) {
		return false;
	}

	object->SetTranslate(spawnPosition);

	if (object->IsSkeletal()) {
		animationObjects.push_back(std::move(object));
		SelectSceneObject(true, animationObjects.size() - 1);
	} else {
		normalObjects.push_back(std::move(object));
		SelectSceneObject(false, normalObjects.size() - 1);
	}
	return true;
}

bool GamePlayScene::AddTextureToScene(const std::string& textureFilePath)
{
	const float clientWidth = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
	return AddTextureToScene(textureFilePath, { clientWidth * 0.5f, clientHeight * 0.5f });
}

bool GamePlayScene::AddTextureToScene(const std::string& textureFilePath, const Vector2& position)
{
	if (textureFilePath.empty()) {
		return false;
	}
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon, textureFilePath);
	sprite->SetAnchorPoint({ 0.5f, 0.5f });
	const Vector2 originalSize = sprite->GetSize();
	const float maxPreviewSize = 220.0f;
	const float largestSide = (std::max)(originalSize.x, originalSize.y);
	if (largestSide > maxPreviewSize && largestSide > 0.0f) {
		const float scale = maxPreviewSize / largestSide;
		sprite->SetSize({ originalSize.x * scale, originalSize.y * scale });
	}
	sprite->SetPosition(position);
	sprites.push_back(std::move(sprite));
	SelectSceneSprite(sprites.size() - 1);
	return true;
}

void GamePlayScene::ClearAddedSceneModels()
{
	DirectXCommon::GetInstance()->WaitForGPU();
	if (normalObjects.size() > baseNormalObjectCount_) {
		normalObjects.resize(baseNormalObjectCount_);
	}
	if (animationObjects.size() > baseAnimationObjectCount_) {
		animationObjects.resize(baseAnimationObjectCount_);
	}
	if (sprites.size() > baseSpriteCount_) {
		sprites.resize(baseSpriteCount_);
	}
	ClearSceneObjectSelection();
	ClearSceneSpriteSelection();
}

Object3d* GamePlayScene::GetSelectedSceneObject()
{
	if (!hasSelectedSceneObject_) {
		return nullptr;
	}
	if (selectedSceneObjectIsAnimation_) {
		return selectedSceneObjectIndex_ < animationObjects.size() ? animationObjects[selectedSceneObjectIndex_].get() : nullptr;
	}
	return selectedSceneObjectIndex_ < normalObjects.size() ? normalObjects[selectedSceneObjectIndex_].get() : nullptr;
}

const Object3d* GamePlayScene::GetSelectedSceneObject() const
{
	if (!hasSelectedSceneObject_) {
		return nullptr;
	}
	if (selectedSceneObjectIsAnimation_) {
		return selectedSceneObjectIndex_ < animationObjects.size() ? animationObjects[selectedSceneObjectIndex_].get() : nullptr;
	}
	return selectedSceneObjectIndex_ < normalObjects.size() ? normalObjects[selectedSceneObjectIndex_].get() : nullptr;
}

void GamePlayScene::SelectSceneObject(bool animationObject, size_t index)
{
	if (animationObject) {
		if (index >= animationObjects.size()) {
			ClearSceneObjectSelection();
			return;
		}
	} else if (index >= normalObjects.size()) {
		ClearSceneObjectSelection();
		return;
	}
	selectedSceneObjectIsAnimation_ = animationObject;
	selectedSceneObjectIndex_ = index;
	hasSelectedSceneObject_ = true;
	inspectorAutoSelectModelFrames_ = 2;
	inspectorAutoSelectSpriteFrames_ = 0;
	ClearSceneSpriteSelection();
}

void GamePlayScene::ClearSceneObjectSelection()
{
	hasSelectedSceneObject_ = false;
	selectedSceneObjectIsAnimation_ = false;
	selectedSceneObjectIndex_ = 0;
	activeGizmoAxis_ = GizmoAxis::None;
	isDraggingGizmo_ = false;
	activeGizmoScreenDirectionX_ = 0.0f;
	activeGizmoScreenDirectionY_ = 0.0f;
	activeGizmoWorldDirection_ = { 1.0f, 0.0f, 0.0f };
	inspectorAutoSelectModelFrames_ = 0;
}

Sprite* GamePlayScene::GetSelectedSceneSprite()
{
	if (!hasSelectedSceneSprite_ || selectedSceneSpriteIndex_ >= sprites.size()) {
		return nullptr;
	}
	return sprites[selectedSceneSpriteIndex_].get();
}

const Sprite* GamePlayScene::GetSelectedSceneSprite() const
{
	if (!hasSelectedSceneSprite_ || selectedSceneSpriteIndex_ >= sprites.size()) {
		return nullptr;
	}
	return sprites[selectedSceneSpriteIndex_].get();
}

void GamePlayScene::SelectSceneSprite(size_t index)
{
	if (index >= sprites.size()) {
		ClearSceneSpriteSelection();
		return;
	}
	selectedSceneSpriteIndex_ = index;
	hasSelectedSceneSprite_ = true;
	inspectorAutoSelectSpriteFrames_ = 2;
	inspectorAutoSelectModelFrames_ = 0;
	ClearSceneObjectSelection();
}

void GamePlayScene::ClearSceneSpriteSelection()
{
	hasSelectedSceneSprite_ = false;
	selectedSceneSpriteIndex_ = 0;
	activeSpriteGizmoAxis_ = GizmoAxis::None;
	isDraggingSpriteGizmo_ = false;
	inspectorAutoSelectSpriteFrames_ = 0;
}

bool GamePlayScene::TryGetGameViewWorldPosition(float screenX, float screenY, Vector3& outPosition) const
{
	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (!activeCamera) {
		return false;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight) ||
		rectWidth <= 0.0f ||
		rectHeight <= 0.0f) {
		return false;
	}

	const float normalizedX = (screenX - rectX) / rectWidth;
	const float normalizedY = (screenY - rectY) / rectHeight;
	if (normalizedX < 0.0f || normalizedX > 1.0f || normalizedY < 0.0f || normalizedY > 1.0f) {
		return false;
	}

	const float ndcX = normalizedX * 2.0f - 1.0f;
	const float ndcY = 1.0f - normalizedY * 2.0f;
	const Matrix4x4 inverseViewProjection = Inverse(activeCamera->GetViewProjectionMatrix());

	Vector3 nearPoint{};
	Vector3 farPoint{};
	if (!TransformCoord({ ndcX, ndcY, 0.0f }, inverseViewProjection, nearPoint) ||
		!TransformCoord({ ndcX, ndcY, 1.0f }, inverseViewProjection, farPoint)) {
		return false;
	}

	const Vector3 rayDirection{
		farPoint.x - nearPoint.x,
		farPoint.y - nearPoint.y,
		farPoint.z - nearPoint.z
	};

	constexpr float placementHeight = 0.0f;
	if (std::abs(rayDirection.y) > 0.0001f) {
		const float t = (placementHeight - nearPoint.y) / rayDirection.y;
		if (t >= 0.0f) {
			outPosition = {
				nearPoint.x + rayDirection.x * t,
				placementHeight,
				nearPoint.z + rayDirection.z * t
			};
			return true;
		}
	}

	const float length = (std::max)(Length(rayDirection), 0.0001f);
	outPosition = {
		nearPoint.x + rayDirection.x / length * 6.0f,
		nearPoint.y + rayDirection.y / length * 6.0f,
		nearPoint.z + rayDirection.z / length * 6.0f
	};
	return true;
}

bool GamePlayScene::TryGetGameViewSpritePosition(float screenX, float screenY, Vector2& outPosition) const
{
	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight) ||
		rectWidth <= 0.0f ||
		rectHeight <= 0.0f) {
		return false;
	}

	const float normalizedX = (screenX - rectX) / rectWidth;
	const float normalizedY = (screenY - rectY) / rectHeight;
	if (normalizedX < 0.0f || normalizedX > 1.0f || normalizedY < 0.0f || normalizedY > 1.0f) {
		return false;
	}

	outPosition = {
		normalizedX * static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth()),
		normalizedY * static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight())
	};
	return true;
}

bool GamePlayScene::EnterModelPreview(const std::string& fileName)
{
	auto object = CreateObjectFromModel(fileName, true);
	if (!object) {
		return false;
	}

	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (activeCamera && !isModelPreviewMode_) {
		previewReturnCameraTranslate_ = activeCamera->GetTranslate();
		previewReturnCameraRotate_ = activeCamera->GetRotate();
	}

	Vector3 previewCenter{};
	float previewRadius = 1.0f;
	auto modelIt = std::find_if(modelLibrary_.begin(), modelLibrary_.end(), [&](const ResourceModelEntry& entry) {
		return entry.fileName == fileName;
	});
	if (modelIt != modelLibrary_.end()) {
		previewCenter = modelIt->thumbnailCenter;
		previewRadius = (std::max)(modelIt->thumbnailRadius, 0.1f);
	}

	object->SetTranslate({ -previewCenter.x, -previewCenter.y, -previewCenter.z });
	object->SetScale({ 1.0f, 1.0f, 1.0f });
	previewObject_ = std::move(object);
	previewModelFile_ = fileName;
	isModelPreviewMode_ = true;
	suppressPreviewExitUntilMouseRelease_ = true;
	previewCameraDefaultDistance_ = (std::max)(3.0f, previewRadius * 3.2f);
	ResetModelPreviewCamera();
	return true;
}

bool GamePlayScene::EnterTexturePreview(const std::string& textureFilePath)
{
	if (textureFilePath.empty()) {
		return false;
	}

	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (activeCamera && !isModelPreviewMode_) {
		previewReturnCameraTranslate_ = activeCamera->GetTranslate();
		previewReturnCameraRotate_ = activeCamera->GetRotate();
	}

	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon, textureFilePath);
	sprite->SetAnchorPoint({ 0.5f, 0.5f });

	previewObject_.reset();
	previewSprite_ = std::move(sprite);
	previewTextureFilePath_ = textureFilePath;
	previewModelFile_ = textureFilePath;
	isModelPreviewMode_ = true;
	isTexturePreviewMode_ = true;
	suppressPreviewExitUntilMouseRelease_ = true;
	ResetTexturePreviewView();
	return true;
}

void GamePlayScene::ExitModelPreview()
{
	if (!isModelPreviewMode_) {
		return;
	}

	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (activeCamera) {
		activeCamera->SetTranslate(previewReturnCameraTranslate_);
		activeCamera->SetRotate(previewReturnCameraRotate_);
	}
	previewObject_.reset();
	previewSprite_.reset();
	previewModelFile_.clear();
	previewTextureFilePath_.clear();
	isModelPreviewMode_ = false;
	isTexturePreviewMode_ = false;
	suppressPreviewExitUntilMouseRelease_ = false;
	previewCameraTarget_ = {};
	previewCameraDistance_ = 3.0f;
	previewCameraDefaultDistance_ = 3.0f;
	previewCameraYaw_ = 0.0f;
	previewCameraPitch_ = 0.0f;
}

void GamePlayScene::ResetTexturePreviewView()
{
	if (!previewSprite_) {
		return;
	}
	const float clientWidth = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
	const Vector2 originalSize = previewSprite_->GetTextureSize();
	const float maxWidth = (std::max)(160.0f, clientWidth * 0.72f);
	const float maxHeight = (std::max)(120.0f, clientHeight * 0.72f);
	const float scale = (std::min)(maxWidth / (std::max)(originalSize.x, 1.0f), maxHeight / (std::max)(originalSize.y, 1.0f));
	previewSprite_->SetSize({ originalSize.x * scale, originalSize.y * scale });
	previewSprite_->SetPosition({ clientWidth * 0.5f, clientHeight * 0.5f });
}

void GamePlayScene::ResetModelPreviewCamera()
{
	if (!isModelPreviewMode_) {
		return;
	}
	if (isTexturePreviewMode_) {
		ResetTexturePreviewView();
		return;
	}

	previewCameraTarget_ = { 0.0f, 0.0f, 0.0f };
	previewCameraYaw_ = 0.0f;
	previewCameraPitch_ = 0.0f;
	previewCameraDistance_ = previewCameraDefaultDistance_;

	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (activeCamera) {
		activeCamera->SetTranslate({ 0.0f, 0.0f, -previewCameraDistance_ });
		activeCamera->SetRotate({ 0.0f, 0.0f, 0.0f });
	}
}

void GamePlayScene::UpdateGameViewCameraControl()
{
	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (!activeCamera) {
		return;
	}
	if (isDraggingGizmo_ || isDraggingSpriteGizmo_) {
		isGameViewCameraDragging_ = false;
		return;
	}

	Input* input = Input::GetInstance();
	const Vector2 mouseScreen = input->GetMouseScreen();
	const bool isMouseOverGameView = ImGuiManager::GetInstance()->IsMouseOverGameView(mouseScreen.x, mouseScreen.y);
	const bool isMouseButtonDown =
		input->IsMouseButtonPressed(0) ||
		input->IsMouseButtonPressed(1) ||
		input->IsMouseButtonPressed(2);

	if (!isMouseButtonDown) {
		isGameViewCameraDragging_ = false;
	}
	const bool isLeftMouseOnGizmo =
		input->TriggerMouseButton(0) &&
		(IsMouseOverSelectedObjectGizmo(mouseScreen.x, mouseScreen.y) ||
			IsMouseOverSelectedSpriteGizmo(mouseScreen.x, mouseScreen.y));

	if (isMouseOverGameView &&
		!isLeftMouseOnGizmo &&
		(input->TriggerMouseButton(0) || input->TriggerMouseButton(1) || input->TriggerMouseButton(2))) {
		isGameViewCameraDragging_ = true;
	}

	constexpr float moveSpeed = 0.01f;
	constexpr float rotateSpeed = 0.005f;
	constexpr float zoomSpeed = 0.01f;
	constexpr float minPitch = -1.45f;
	constexpr float maxPitch = 1.45f;

	if (isModelPreviewMode_) {
		if (isMouseOverGameView && input->TriggerKey(DIK_R)) {
			ResetModelPreviewCamera();
		}

		if (isTexturePreviewMode_) {
			float rectX = 0.0f;
			float rectY = 0.0f;
			float rectWidth = 0.0f;
			float rectHeight = 0.0f;
			if (previewSprite_ &&
				ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight) &&
				rectWidth > 0.0f &&
				rectHeight > 0.0f) {
				const float clientWidth = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
				const float clientHeight = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
				if (isGameViewCameraDragging_ && input->IsMouseButtonPressed(0)) {
					Vector2 position = previewSprite_->GetPosition();
					position.x += static_cast<float>(input->GetMouseX()) * (clientWidth / rectWidth);
					position.y += static_cast<float>(input->GetMouseY()) * (clientHeight / rectHeight);
					previewSprite_->SetPosition(position);
				}
				if (isMouseOverGameView && input->GetMouseWheel() != 0) {
					Vector2 size = previewSprite_->GetSize();
					const float zoomFactor = std::clamp(1.0f + static_cast<float>(input->GetMouseWheel()) * 0.001f, 0.2f, 4.0f);
					size.x = std::clamp(size.x * zoomFactor, 8.0f, clientWidth * 4.0f);
					size.y = std::clamp(size.y * zoomFactor, 8.0f, clientHeight * 4.0f);
					previewSprite_->SetSize(size);
				}
				previewSprite_->Update();
			}
			return;
		}

		const float targetMoveSpeed = (std::max)(0.003f, previewCameraDistance_ * 0.0025f);
		if (isGameViewCameraDragging_ && input->IsMouseButtonPressed(0)) {
			const float yawCos = std::cos(previewCameraYaw_);
			const float yawSin = std::sin(previewCameraYaw_);
			const float pitchCos = std::cos(previewCameraPitch_);
			const float pitchSin = std::sin(previewCameraPitch_);
			const Vector3 right{ yawCos, 0.0f, -yawSin };
			const Vector3 up{ yawSin * pitchSin, pitchCos, yawCos * pitchSin };
			const float mouseX = static_cast<float>(input->GetMouseX());
			const float mouseY = static_cast<float>(input->GetMouseY());
			previewCameraTarget_.x -= right.x * mouseX * targetMoveSpeed;
			previewCameraTarget_.y -= right.y * mouseX * targetMoveSpeed;
			previewCameraTarget_.z -= right.z * mouseX * targetMoveSpeed;
			previewCameraTarget_.x += up.x * mouseY * targetMoveSpeed;
			previewCameraTarget_.y += up.y * mouseY * targetMoveSpeed;
			previewCameraTarget_.z += up.z * mouseY * targetMoveSpeed;
		}
		if (isGameViewCameraDragging_ && (input->IsMouseButtonPressed(1) || input->IsMouseButtonPressed(2))) {
			previewCameraYaw_ += static_cast<float>(input->GetMouseX()) * rotateSpeed;
			previewCameraPitch_ += static_cast<float>(input->GetMouseY()) * rotateSpeed;
			previewCameraPitch_ = std::clamp(previewCameraPitch_, minPitch, maxPitch);
		}
		if (isMouseOverGameView && input->GetMouseWheel() != 0) {
			previewCameraDistance_ -= static_cast<float>(input->GetMouseWheel()) * zoomSpeed;
			previewCameraDistance_ = (std::max)(0.5f, previewCameraDistance_);
		}

		const float yawCos = std::cos(previewCameraYaw_);
		const float yawSin = std::sin(previewCameraYaw_);
		const float pitchCos = std::cos(previewCameraPitch_);
		const float pitchSin = std::sin(previewCameraPitch_);
		const Vector3 forward{
			yawSin * pitchCos,
			-pitchSin,
			yawCos * pitchCos
		};
		activeCamera->SetTranslate({
			previewCameraTarget_.x - forward.x * previewCameraDistance_,
			previewCameraTarget_.y - forward.y * previewCameraDistance_,
			previewCameraTarget_.z - forward.z * previewCameraDistance_
		});
		activeCamera->SetRotate({ previewCameraPitch_, previewCameraYaw_, 0.0f });
		return;
	}

	Vector3 cameraTranslate = activeCamera->GetTranslate();
	Vector3 cameraRotate = activeCamera->GetRotate();

	if (isGameViewCameraDragging_ && input->IsMouseButtonPressed(0)) {
		cameraTranslate.x -= static_cast<float>(input->GetMouseX()) * moveSpeed;
		cameraTranslate.y += static_cast<float>(input->GetMouseY()) * moveSpeed;
	}
	if (isGameViewCameraDragging_ && (input->IsMouseButtonPressed(1) || input->IsMouseButtonPressed(2))) {
		cameraRotate.y += static_cast<float>(input->GetMouseX()) * rotateSpeed;
		cameraRotate.x += static_cast<float>(input->GetMouseY()) * rotateSpeed;
		cameraRotate.x = std::clamp(cameraRotate.x, minPitch, maxPitch);
	}
	if (isMouseOverGameView && input->GetMouseWheel() != 0) {
		cameraTranslate.z += static_cast<float>(input->GetMouseWheel()) * zoomSpeed;
	}

	activeCamera->SetTranslate(cameraTranslate);
	activeCamera->SetRotate(cameraRotate);
}

Vector3 GamePlayScene::GetParticleEffectPosition() const
{
	if (!objectAxis) {
		return { 0.0f, 0.0f, 0.0f };
	}

	Vector3 effectPosition = objectAxis->GetTransform().translate;
	effectPosition.x += 1.0f;
	effectPosition.y += 1.0f;
	return effectPosition;
}

void GamePlayScene::DrawParticleEffectImGui(bool embedded)
{
#ifdef USE_IMGUI
	ParticleManager* particleManager = ParticleManager::GetInstance();
	if (!embedded) {
		ImGui::Begin("Particle Effects");
	}
	ImGui::TextUnformatted("Emit position: same as P key");

	auto drawControl = [particleManager](const char* label, const char* groupName, ParticleEffectControl& control) {
		if (ImGui::TreeNode(label)) {
			ImGui::Checkbox("Enable", &control.enabled);
			ImGui::SliderInt("Emit Count", &control.emitCount, 1, 100);
			ImGui::SliderFloat("Scale", &control.scale, 0.1f, 5.0f, "%.2f");
			if (ImGui::Checkbox("Billboard", &control.billboard)) {
				particleManager->SetBillboardEnabled(groupName, control.billboard);
			}
			particleManager->SetBillboardEnabled(groupName, control.billboard);
			ImGui::TreePop();
		}
	};

	drawControl("Hit Slash", "Hit", hitEffect_);
	drawControl("Impact Ring", "Ring", ringEffect_);
	if (ImGui::TreeNode("Portal Cylinder")) {
		if (ImGui::Checkbox("Display", &cylinderEffect_.enabled)) {
			refreshCylinderEffect_ = true;
		}
		ImGui::SliderInt("Emit Count", &cylinderEffect_.emitCount, 1, 100);
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			refreshCylinderEffect_ = true;
		}
		ImGui::SliderFloat("Scale", &cylinderEffect_.scale, 0.1f, 5.0f, "%.2f");
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			refreshCylinderEffect_ = true;
		}
		if (ImGui::Checkbox("Billboard", &cylinderEffect_.billboard)) {
			particleManager->SetBillboardEnabled("Cylinder", cylinderEffect_.billboard);
			refreshCylinderEffect_ = true;
		}
		particleManager->SetBillboardEnabled("Cylinder", cylinderEffect_.billboard);
		ImGui::TreePop();
	}
	drawControl("Pillar Sparkle", "PillarSparkle", pillarSparkleEffect_);
	drawControl("Light Core", "LightCore", lightCoreEffect_);
	drawControl("Light Rain", "LightRain", lightRainEffect_);
	drawControl("Light Spiral", "LightSpiral", lightSpiralEffect_);

	if (ImGui::Button("Clear All Effects")) {
		particleManager->ClearParticles("Hit");
		particleManager->ClearParticles("Ring");
		particleManager->ClearParticles("Cylinder");
		particleManager->ClearParticles("PillarSparkle");
		particleManager->ClearParticles("LightCore");
		particleManager->ClearParticles("LightRain");
		particleManager->ClearParticles("LightSpiral");
		isCylinderEffectVisible_ = false;
		hitEffect_.enabled = false;
		ringEffect_.enabled = false;
		cylinderEffect_.enabled = false;
		pillarSparkleEffect_.enabled = false;
		lightCoreEffect_.enabled = false;
		lightRainEffect_.enabled = false;
		lightSpiralEffect_.enabled = false;
		lastCylinderEffectEnabled_ = false;
		lastCylinderEffectEmitCount_ = cylinderEffect_.emitCount;
		lastCylinderEffectScale_ = cylinderEffect_.scale;
		lastCylinderEffectBillboard_ = cylinderEffect_.billboard;
		refreshCylinderEffect_ = false;
	}
	if (!embedded) {
		ImGui::End();
	}
#else
	(void)embedded;
#endif
}

std::filesystem::path GamePlayScene::GetUserMediaDirectory(const char* folderName, const char* projectFolderName) const
{
	std::filesystem::path basePath = ".";
	const KNOWNFOLDERID* knownFolderId = nullptr;
	if (std::strcmp(folderName, "Pictures") == 0) {
		knownFolderId = &FOLDERID_Pictures;
	} else if (std::strcmp(folderName, "Videos") == 0) {
		knownFolderId = &FOLDERID_Videos;
	}

	if (knownFolderId) {
		PWSTR knownFolderPath = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(*knownFolderId, KF_FLAG_DEFAULT, nullptr, &knownFolderPath)) && knownFolderPath) {
			basePath = knownFolderPath;
			CoTaskMemFree(knownFolderPath);
			return basePath / projectFolderName;
		}
		if (knownFolderPath) {
			CoTaskMemFree(knownFolderPath);
		}
	}

	char* userProfile = nullptr;
	size_t userProfileLength = 0;
	if (_dupenv_s(&userProfile, &userProfileLength, "USERPROFILE") == 0 && userProfile) {
		basePath = userProfile;
		free(userProfile);
	}

	return basePath / folderName / projectFolderName;
}

std::string GamePlayScene::MakeTimestampString() const
{
	const auto now = std::chrono::system_clock::now();
	const std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
	std::tm localTime{};
	localtime_s(&localTime, &currentTime);
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::ostringstream stream;
	stream << std::put_time(&localTime, "%Y%m%d_%H%M%S")
		<< '_' << std::setw(3) << std::setfill('0') << milliseconds.count();
	return stream.str();
}

bool GamePlayScene::CaptureGameViewPixels(std::vector<unsigned char>& pixels, int& width, int& height)
{
#ifdef USE_IMGUI
	return DirectXCommon::GetInstance()->CaptureGameTexturePixels(
		pixels,
		width,
		height,
		PostEffect::GetInstance()->IsEnabled());
#else
	(void)pixels;
	(void)width;
	(void)height;
	return false;
#endif
}

bool GamePlayScene::SavePixelsAsBmp(const std::filesystem::path& filePath, const std::vector<unsigned char>& pixels, int width, int height)
{
	if (pixels.empty() || width <= 0 || height <= 0) {
		return false;
	}

	std::error_code errorCode;
	std::filesystem::create_directories(filePath.parent_path(), errorCode);
	if (errorCode) {
		return false;
	}

	const DWORD imageSize = static_cast<DWORD>(width * height * 4);
	BITMAPFILEHEADER fileHeader{};
	fileHeader.bfType = 0x4D42;
	fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	fileHeader.bfSize = fileHeader.bfOffBits + imageSize;

	BITMAPINFOHEADER bitmapHeader{};
	bitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapHeader.biWidth = width;
	bitmapHeader.biHeight = -height;
	bitmapHeader.biPlanes = 1;
	bitmapHeader.biBitCount = 32;
	bitmapHeader.biCompression = BI_RGB;

	std::ofstream file(filePath, std::ios::binary);
	if (!file) {
		return false;
	}

	file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
	file.write(reinterpret_cast<const char*>(&bitmapHeader), sizeof(bitmapHeader));
	file.write(reinterpret_cast<const char*>(pixels.data()), imageSize);
	return file.good();
}

bool GamePlayScene::BeginRecordingAvi(const std::filesystem::path& filePath, int width, int height)
{
	if (width <= 0 || height <= 0) {
		return false;
	}

	EndRecordingAvi();

	std::error_code errorCode;
	std::filesystem::create_directories(filePath.parent_path(), errorCode);
	if (errorCode) {
		return false;
	}

	AVIFileInit();

	PAVIFILE aviFile = nullptr;
	HRESULT result = AVIFileOpenW(&aviFile, filePath.wstring().c_str(), OF_WRITE | OF_CREATE, nullptr);
	if (FAILED(result)) {
		AVIFileExit();
		return false;
	}

	AVISTREAMINFOW streamInfo{};
	streamInfo.fccType = streamtypeVIDEO;
	streamInfo.dwScale = 1;
	streamInfo.dwRate = 10;
	streamInfo.dwSuggestedBufferSize = static_cast<DWORD>(width * height * 4);
	SetRect(&streamInfo.rcFrame, 0, 0, width, height);

	PAVISTREAM aviStream = nullptr;
	result = AVIFileCreateStreamW(aviFile, &aviStream, &streamInfo);
	if (FAILED(result)) {
		AVIFileRelease(aviFile);
		AVIFileExit();
		return false;
	}

	BITMAPINFOHEADER bitmapHeader{};
	bitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapHeader.biWidth = width;
	bitmapHeader.biHeight = height;
	bitmapHeader.biPlanes = 1;
	bitmapHeader.biBitCount = 32;
	bitmapHeader.biCompression = BI_RGB;
	bitmapHeader.biSizeImage = static_cast<DWORD>(width * height * 4);

	result = AVIStreamSetFormat(aviStream, 0, &bitmapHeader, sizeof(bitmapHeader));
	if (FAILED(result)) {
		AVIStreamRelease(aviStream);
		AVIFileRelease(aviFile);
		AVIFileExit();
		return false;
	}

	recordingAviFile_ = aviFile;
	recordingAviStream_ = aviStream;
	recordingVideoPath_ = filePath;
	recordingVideoWidth_ = width;
	recordingVideoHeight_ = height;
	return true;
}

bool GamePlayScene::AppendRecordingFrame(const std::vector<unsigned char>& pixels, int width, int height)
{
	if (!recordingAviStream_ ||
		pixels.empty() ||
		width != recordingVideoWidth_ ||
		height != recordingVideoHeight_) {
		return false;
	}

	const size_t rowSize = static_cast<size_t>(width) * 4;
	std::vector<unsigned char> bottomUpPixels(pixels.size());
	for (int y = 0; y < height; ++y) {
		const unsigned char* source = pixels.data() + static_cast<size_t>(height - 1 - y) * rowSize;
		unsigned char* destination = bottomUpPixels.data() + static_cast<size_t>(y) * rowSize;
		std::memcpy(destination, source, rowSize);
	}

	PAVISTREAM aviStream = static_cast<PAVISTREAM>(recordingAviStream_);
	const HRESULT result = AVIStreamWrite(
		aviStream,
		recordingFrameIndex_,
		1,
		bottomUpPixels.data(),
		static_cast<LONG>(bottomUpPixels.size()),
		AVIIF_KEYFRAME,
		nullptr,
		nullptr);
	return SUCCEEDED(result);
}

void GamePlayScene::EndRecordingAvi()
{
	if (recordingAviStream_) {
		AVIStreamRelease(static_cast<PAVISTREAM>(recordingAviStream_));
		recordingAviStream_ = nullptr;
	}
	if (recordingAviFile_) {
		AVIFileRelease(static_cast<PAVIFILE>(recordingAviFile_));
		recordingAviFile_ = nullptr;
		AVIFileExit();
	}
	recordingVideoWidth_ = 0;
	recordingVideoHeight_ = 0;
}

void GamePlayScene::DrawCaptureImGui()
{
#ifdef USE_IMGUI
	const std::filesystem::path screenshotDirectory = GetUserMediaDirectory("Pictures", "CG2Captures");
	const std::filesystem::path videoDirectory = GetUserMediaDirectory("Videos", "CG2Recordings");

	ImGui::TextUnformatted("Capture");
	ImGui::SameLine();
	ImGui::TextDisabled("Game View only");

	if (ImGui::Button("Photo")) {
		const std::filesystem::path capturePath = screenshotDirectory / ("CG2_" + MakeTimestampString() + ".bmp");
		std::vector<unsigned char> pixels;
		int width = 0;
		int height = 0;
		if (!CaptureGameViewPixels(pixels, width, height)) {
			lastCaptureMessage_ = "Screenshot failed: Game View pixels could not be captured.";
		} else if (!SavePixelsAsBmp(capturePath, pixels, width, height)) {
			lastCaptureMessage_ = "Screenshot failed: could not write BMP to " + capturePath.string();
		} else {
			lastScreenshotPath_ = capturePath;
			lastCaptureMessage_ = "Saved screenshot: " + capturePath.string();
		}
	}
	ImGui::SameLine();
	const char* recordingButtonLabel = isRecordingGameView_ ? "Stop" : "Record";
	if (ImGui::Button(recordingButtonLabel)) {
		if (isRecordingGameView_) {
			isRecordingGameView_ = false;
			EndRecordingAvi();
			lastVideoPath_ = recordingVideoPath_;
			lastCaptureMessage_ = "Saved video: " + recordingVideoPath_.string();
		} else {
			recordingDirectory_ = videoDirectory;
			recordingVideoPath_ = recordingDirectory_ / ("CG2_" + MakeTimestampString() + ".avi");
			std::vector<unsigned char> pixels;
			int width = 0;
			int height = 0;
			if (!CaptureGameViewPixels(pixels, width, height)) {
				lastCaptureMessage_ = "Recording failed: Game View pixels could not be captured.";
			} else if (BeginRecordingAvi(recordingVideoPath_, width, height)) {
				isRecordingGameView_ = true;
				recordingTime_ = 0.0f;
				recordingFrameTimer_ = 0.0f;
				recordingFrameIndex_ = 0;
				if (AppendRecordingFrame(pixels, width, height)) {
					++recordingFrameIndex_;
					lastCaptureMessage_ = "Recording video: " + recordingVideoPath_.string();
				} else {
					EndRecordingAvi();
					isRecordingGameView_ = false;
					lastCaptureMessage_ = "Recording failed to write first frame.";
				}
			} else {
				lastCaptureMessage_ = "Recording failed: could not create AVI at " + recordingVideoPath_.string();
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("open Photos File")) {
		lastCaptureMessage_ = OpenFolderInExplorer(screenshotDirectory)
			? "Opened screenshot folder: " + screenshotDirectory.string()
			: "Could not open screenshot folder: " + screenshotDirectory.string();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("open Videos File")) {
		lastCaptureMessage_ = OpenFolderInExplorer(videoDirectory)
			? "Opened video folder: " + videoDirectory.string()
			: "Could not open video folder: " + videoDirectory.string();
	}

	if (isRecordingGameView_) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "REC %.2fs  %d frames", recordingTime_, recordingFrameIndex_);
	}
	if (!lastCaptureMessage_.empty()) {
		ImGui::TextWrapped("%s", lastCaptureMessage_.c_str());
	}
#endif
}

void GamePlayScene::DrawTopToolsImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::Begin("Top Tools")) {
		ImGui::End();
		return;
	}

	const ImGuiTableFlags tableFlags =
	    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV;
	if (ImGui::BeginTable("TopToolsTable", 3, tableFlags, ImVec2(-1.0f, 0.0f))) {
		ImGui::TableSetupColumn("FPS", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Capture", ImGuiTableColumnFlags_WidthFixed, 400.0f);
		ImGui::TableSetupColumn("PostEffect", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted("FPS");
		static float fpsValues[90] = {};
		static int fpsValueOffset = 0;
		fpsValues[fpsValueOffset] = ImGui::GetIO().Framerate;
		fpsValueOffset = (fpsValueOffset + 1) % 90;
		char fpsOverlay[32];
		snprintf(fpsOverlay, sizeof(fpsOverlay), "%.1f FPS", ImGui::GetIO().Framerate);
		const float fpsGraphWidth = (std::max)(120.0f, ImGui::GetContentRegionAvail().x);
		ImGui::PlotLines("##TopToolsFPS", fpsValues, 90, fpsValueOffset, fpsOverlay, 0.0f, 120.0f, ImVec2(fpsGraphWidth, 58.0f));

		ImGui::TableSetColumnIndex(1);
		DrawCaptureImGui();

		ImGui::TableSetColumnIndex(2);
		ImGui::TextUnformatted("Post Effect");
		bool isGrayscale = PostEffect::GetInstance()->IsGrayscale();
		bool isSepia = PostEffect::GetInstance()->IsSepia();
		if (ImGui::Checkbox("Gray", &isGrayscale) && isGrayscale) {
			isSepia = false;
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Sepia", &isSepia) && isSepia) {
			isGrayscale = false;
		}
		PostEffect::GetInstance()->SetGrayscale(isGrayscale);
		PostEffect::GetInstance()->SetSepia(isSepia);
		ImGui::TextDisabled("Capture uses Game View.");

		ImGui::EndTable();
	}
	ImGui::End();
#endif
}

void GamePlayScene::UpdateRecordingCapture()
{
	if (!isRecordingGameView_) {
		return;
	}

	const float deltaTime = DirectXCommon::GetInstance()->GetDeltaTime();
	recordingTime_ += deltaTime;
	recordingFrameTimer_ += deltaTime;

	constexpr float captureInterval = 1.0f / 10.0f;
	if (recordingFrameIndex_ > 0 && recordingFrameTimer_ < captureInterval) {
		return;
	}
	while (recordingFrameTimer_ >= captureInterval) {
		recordingFrameTimer_ -= captureInterval;
	}

	std::vector<unsigned char> pixels;
	int width = 0;
	int height = 0;
	if (!CaptureGameViewPixels(pixels, width, height) || !AppendRecordingFrame(pixels, width, height)) {
		isRecordingGameView_ = false;
		EndRecordingAvi();
		lastCaptureMessage_ = "Recording stopped because AVI frame capture failed.";
		return;
	}

	++recordingFrameIndex_;
}

void GamePlayScene::DrawInspectorImGui()
{
#ifdef USE_IMGUI
	const bool autoSelectSpriteInspector = hasSelectedSceneSprite_ && inspectorAutoSelectSpriteFrames_ > 0;
	const bool autoSelectModelInspector = hasSelectedSceneObject_ && inspectorAutoSelectModelFrames_ > 0;
	SceneEditor::InspectorOptions options{};
	options.sprites = &sprites;
	options.normalObjects = &normalObjects;
	options.animationObjects = &animationObjects;
	options.directionalLight = &directionalLight;
	options.pointLight = &pointLight;
	options.spotLight = &spotLight;
	options.addedSpriteCount = sprites.size() > baseSpriteCount_ ? sprites.size() - baseSpriteCount_ : 0;
	options.protectedNormalObjectCount = baseNormalObjectCount_;
	options.protectedAnimationObjectCount = baseAnimationObjectCount_;
	options.protectedSpriteCount = baseSpriteCount_;
	options.forcedSpriteIndex = autoSelectSpriteInspector ? static_cast<int>(selectedSceneSpriteIndex_) : -1;
	options.forcedNormalIndex = autoSelectModelInspector && !selectedSceneObjectIsAnimation_ ? static_cast<int>(selectedSceneObjectIndex_) : -1;
	options.forcedAnimationIndex = autoSelectModelInspector && selectedSceneObjectIsAnimation_ ? static_cast<int>(selectedSceneObjectIndex_) : -1;
	options.selectSpriteTab = autoSelectSpriteInspector;
	options.selectModelTab = autoSelectModelInspector;
	options.forceDock = inspectorForceDockFrames_ > 0;
	options.removeSprite = [this](size_t index) {
		if (index < baseSpriteCount_ || index >= sprites.size()) {
			return;
		}
		DirectXCommon::GetInstance()->WaitForGPU();
		sprites.erase(sprites.begin() + static_cast<std::ptrdiff_t>(index));
		ClearSceneSpriteSelection();
		inspectorAutoSelectSpriteFrames_ = 0;
	};
	options.onModelRemoved = [this](bool, size_t) {
		ClearSceneObjectSelection();
		inspectorAutoSelectModelFrames_ = 0;
	};
	options.drawHeader = [this]() {
		if (isRecordingGameView_) {
			ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "Recording Game View: %.2f sec", recordingTime_);
			ImGui::Separator();
		}
	};
	options.drawExtraTabs = [this]() { DrawGamePlayInspectorTabs(); };
	SceneEditor::DrawInspector(options);
	if (inspectorForceDockFrames_ > 0) {
		--inspectorForceDockFrames_;
	}
	if (inspectorAutoSelectSpriteFrames_ > 0) {
		--inspectorAutoSelectSpriteFrames_;
	}
	if (inspectorAutoSelectModelFrames_ > 0) {
		--inspectorAutoSelectModelFrames_;
	}
#endif
}

void GamePlayScene::DrawGamePlayInspectorTabs()
{
#ifdef USE_IMGUI
	if (ImGui::BeginTabItem("Particle")) {
		std::string particleRequest = ImGuiManager::GetInstance()->ParticleWindow(emitterTransform, true);
		if (particleRequest == "Circle") {
			activeEmitter = emitterCircle.get();
		} else if (particleRequest == "Plane") {
			activeEmitter = emitterPlane.get();
		}
		if (activeEmitter) {
			activeEmitter->SetTranslate(emitterTransform.translate);
		}

		auto drawEmitterControl = [](const char* label, ParticleEmitter* emitter) {
			if (!emitter || !ImGui::TreeNode(label)) {
				return;
			}

			int count = static_cast<int>(emitter->GetCount());
			if (ImGui::SliderInt("Emit Count", &count, 1, 100)) {
				emitter->SetCount(static_cast<uint32_t>(count));
			}

			float frequency = emitter->GetFrequency();
			if (ImGui::SliderFloat("Frequency", &frequency, 0.02f, 2.0f, "%.2f")) {
				emitter->SetFrequency(frequency);
			}

			Vector3 scale = emitter->GetTransform().scale;
			if (ImGui::SliderFloat("Scale", &scale.x, 0.1f, 5.0f, "%.2f")) {
				scale.y = scale.x;
				scale.z = scale.x;
				emitter->SetScale(scale);
			}

			bool receivesWind = emitter->GetReceivesWind();
			if (ImGui::Checkbox("Receives Wind", &receivesWind)) {
				emitter->SetReceivesWind(receivesWind);
			}
			ImGui::TreePop();
		};

		drawEmitterControl("Circle Particle", emitterCircle.get());
		drawEmitterControl("Plane Particle", emitterPlane.get());

		if (ImGui::Button("Rain Wind")) {
			ParticleManager::GetInstance()->SetWindEnabled(true);
			ParticleManager::GetInstance()->SetWindAcceleration({ 0.0f, -0.35f, 0.0f });
			emitterCircle->SetReceivesWind(true);
		}
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Effect")) {
		DrawParticleEffectImGui(true);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Camera")) {
		ImGuiManager::GetInstance()->CameraWindow(cameraManager.get(), true);
		ImGui::EndTabItem();
	}
#endif
}

void GamePlayScene::DrawModelShelfImGui()
{
#ifdef USE_IMGUI
	SceneEditor::ShelfState state{};
	state.entries = std::move(modelLibrary_);
	state.selectedEntry = selectedLibraryModel_;
	state.message = lastModelShelfMessage_;

	const size_t addedNormalCount = normalObjects.size() > baseNormalObjectCount_ ? normalObjects.size() - baseNormalObjectCount_ : 0;
	const size_t addedAnimationCount = animationObjects.size() > baseAnimationObjectCount_ ? animationObjects.size() - baseAnimationObjectCount_ : 0;
	const size_t addedTextureCount = sprites.size() > baseSpriteCount_ ? sprites.size() - baseSpriteCount_ : 0;
	const std::filesystem::path resourceDirectory = std::filesystem::absolute("resources");

	SceneEditor::ShelfCallbacks callbacks{};
	callbacks.sceneLabel = "Game View";
	callbacks.addedModelCount = addedNormalCount + addedAnimationCount;
	callbacks.addedTextureCount = addedTextureCount;
	callbacks.addModel = [this](const std::string& fileName) { return AddModelToScene(fileName); };
	callbacks.addTexture = [this](const std::string& textureFilePath) { return AddTextureToScene(textureFilePath); };
	callbacks.clearAdded = [this]() { ClearAddedSceneModels(); };
	callbacks.afterAdd = [this]() { ExitModelPreview(); };
	callbacks.previewOnDoubleClick = true;
	callbacks.previewEntry = [this](const SceneEditor::ShelfEntry& entry) {
		return entry.isTexture ? EnterTexturePreview(entry.textureFilePath) : EnterModelPreview(entry.fileName);
	};
	callbacks.drawExtraToolbar = [this, &state, resourceDirectory]() {
		ImGui::SameLine();
		if (ImGui::SmallButton("Open Resources")) {
			state.message = OpenFolderInExplorer(resourceDirectory)
				? "Opened resources folder: " + resourceDirectory.string()
				: "Could not open resources folder: " + resourceDirectory.string();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Collision Wire", &showCollisionDebug_);
	};
	callbacks.drawExtraStatus = [this]() {
		if (!isModelPreviewMode_) {
			return;
		}
		ImGui::TextDisabled("Preview: %s", previewModelFile_.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Back to Game")) {
			ExitModelPreview();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset View")) {
			if (isTexturePreviewMode_) {
				ResetTexturePreviewView();
			} else {
				ResetModelPreviewCamera();
			}
		}
	};

	SceneEditor::DrawModelShelf(state, callbacks);

	modelLibrary_ = std::move(state.entries);
	selectedLibraryModel_ = state.selectedEntry;
	lastModelShelfMessage_ = state.message;
#endif
}
void GamePlayScene::HandleModelDropOnGameView()
{
#ifdef USE_IMGUI
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(x, y, width, height)) {
		return;
	}

	const ImRect gameViewRect(ImVec2(x, y), ImVec2(x + width, y + height));
	if (ImGui::BeginDragDropTargetCustom(gameViewRect, ImGui::GetID("GameViewModelDropTarget"))) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_FILE")) {
			std::string fileName(static_cast<const char*>(payload->Data), payload->DataSize);
			if (!fileName.empty() && fileName.back() == '\0') {
				fileName.pop_back();
			}
			Vector3 spawnPosition{};
			const ImVec2 mousePosition = ImGui::GetMousePos();
			if (TryGetGameViewWorldPosition(mousePosition.x, mousePosition.y, spawnPosition)) {
				lastModelShelfMessage_ = AddModelToScene(fileName, spawnPosition)
					? "Added model to Game View: " + fileName
					: "Could not add model to Game View: " + fileName;
			} else {
				lastModelShelfMessage_ = AddModelToScene(fileName)
					? "Added model to Game View: " + fileName
					: "Could not add model to Game View: " + fileName;
			}
			ExitModelPreview();
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_FILE")) {
			std::string textureFilePath(static_cast<const char*>(payload->Data), payload->DataSize);
			if (!textureFilePath.empty() && textureFilePath.back() == '\0') {
				textureFilePath.pop_back();
			}
			Vector2 spritePosition{};
			const ImVec2 mousePosition = ImGui::GetMousePos();
			if (TryGetGameViewSpritePosition(mousePosition.x, mousePosition.y, spritePosition)) {
				lastModelShelfMessage_ = AddTextureToScene(textureFilePath, spritePosition)
					? "Added 2D Texture to Game View: " + textureFilePath
					: "Could not add 2D Texture to Game View: " + textureFilePath;
			} else {
				lastModelShelfMessage_ = AddTextureToScene(textureFilePath)
					? "Added 2D Texture to Game View: " + textureFilePath
					: "Could not add 2D Texture to Game View: " + textureFilePath;
			}
			ExitModelPreview();
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::IsDragDropActive() && gameViewRect.Contains(ImGui::GetMousePos())) {
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->AddRect(gameViewRect.Min, gameViewRect.Max, IM_COL32(80, 180, 255, 255), 0.0f, 0, 4.0f);
		drawList->AddText(ImVec2(gameViewRect.Min.x + 16.0f, gameViewRect.Min.y + 16.0f), IM_COL32(180, 230, 255, 255), "Drop model here");
	}
#endif
}

void GamePlayScene::HandleGameViewSpriteSelection()
{
#ifdef USE_IMGUI
	if (isModelPreviewMode_ || ImGui::IsDragDropActive() || isDraggingSpriteGizmo_) {
		return;
	}
	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}
	const ImVec2 mouse = ImGui::GetMousePos();
	const ImRect gameViewRect(ImVec2(rectX, rectY), ImVec2(rectX + rectWidth, rectY + rectHeight));
	if (!gameViewRect.Contains(mouse) || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return;
	}

	const float clientWidth = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
	if (clientWidth <= 0.0f || clientHeight <= 0.0f) {
		return;
	}

	for (size_t reverseIndex = sprites.size(); reverseIndex > 0; --reverseIndex) {
		const size_t index = reverseIndex - 1;
		const Sprite* sprite = sprites[index].get();
		if (!sprite) {
			continue;
		}
		const Vector2 position = sprite->GetPosition();
		const Vector2 size = sprite->GetSize();
		const Vector2 anchor = sprite->GetAnchorPoint();
		const ImVec2 minPoint(
			rectX + ((position.x - size.x * anchor.x) / clientWidth) * rectWidth,
			rectY + ((position.y - size.y * anchor.y) / clientHeight) * rectHeight);
		const ImVec2 maxPoint(
			rectX + ((position.x + size.x * (1.0f - anchor.x)) / clientWidth) * rectWidth,
			rectY + ((position.y + size.y * (1.0f - anchor.y)) / clientHeight) * rectHeight);
		if (ImRect(minPoint, maxPoint).Contains(mouse)) {
			SelectSceneSprite(index);
			return;
		}
	}
	ClearSceneSpriteSelection();
#endif
}

void GamePlayScene::HandleGameViewObjectSelection()
{
#ifdef USE_IMGUI
	if (isModelPreviewMode_ || ImGui::IsDragDropActive() || isDraggingGizmo_) {
		return;
	}
	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}
	const ImVec2 mouse = ImGui::GetMousePos();
	const ImRect gameViewRect(ImVec2(rectX, rectY), ImVec2(rectX + rectWidth, rectY + rectHeight));
	if (!gameViewRect.Contains(mouse) || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return;
	}
	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (!activeCamera) {
		return;
	}

	const Matrix4x4& viewProjectionMatrix = activeCamera->GetViewProjectionMatrix();
	float bestPickScore = (std::numeric_limits<float>::max)();
	bool found = false;
	bool foundAnimation = false;
	size_t foundIndex = 0;

	const auto testObject = [&](const std::unique_ptr<Object3d>& object, bool animationObject, size_t index) {
		if (!object) {
			return;
		}
		MyMath::AABB aabb{};
		if (!BuildWorldAabb(*object, aabb)) {
			return;
		}
		const std::array<Vector3, 8> corners{ {
			{ aabb.min.x, aabb.min.y, aabb.min.z },
			{ aabb.max.x, aabb.min.y, aabb.min.z },
			{ aabb.min.x, aabb.max.y, aabb.min.z },
			{ aabb.max.x, aabb.max.y, aabb.min.z },
			{ aabb.min.x, aabb.min.y, aabb.max.z },
			{ aabb.max.x, aabb.min.y, aabb.max.z },
			{ aabb.min.x, aabb.max.y, aabb.max.z },
			{ aabb.max.x, aabb.max.y, aabb.max.z }
		} };

		ImVec2 screenMin((std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)());
		ImVec2 screenMax(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
		int visibleCorners = 0;
		for (const Vector3& corner : corners) {
			Vector3 ndc{};
			if (!TransformCoord(corner, viewProjectionMatrix, ndc)) {
				continue;
			}
			const ImVec2 screen(
				rectX + (ndc.x + 1.0f) * 0.5f * rectWidth,
				rectY + (1.0f - ndc.y) * 0.5f * rectHeight);
			screenMin.x = (std::min)(screenMin.x, screen.x);
			screenMin.y = (std::min)(screenMin.y, screen.y);
			screenMax.x = (std::max)(screenMax.x, screen.x);
			screenMax.y = (std::max)(screenMax.y, screen.y);
			++visibleCorners;
		}
		if (visibleCorners == 0) {
			return;
		}
		const ImRect objectRect(screenMin, screenMax);
		if (!objectRect.Contains(mouse)) {
			return;
		}
		const ImVec2 objectCenter((screenMin.x + screenMax.x) * 0.5f, (screenMin.y + screenMax.y) * 0.5f);
		const float centerDx = mouse.x - objectCenter.x;
		const float centerDy = mouse.y - objectCenter.y;
		const float area = (std::max)(1.0f, (screenMax.x - screenMin.x) * (screenMax.y - screenMin.y));
		float score = centerDx * centerDx + centerDy * centerDy + area * 0.01f;
		const bool initialSceneObject =
			(animationObject && index < baseAnimationObjectCount_) ||
			(!animationObject && index < baseNormalObjectCount_);
		if (initialSceneObject) {
			score += 250000.0f;
		}
		if (score < bestPickScore) {
			bestPickScore = score;
			found = true;
			foundAnimation = animationObject;
			foundIndex = index;
		}
	};

	for (size_t index = 0; index < normalObjects.size(); ++index) {
		testObject(normalObjects[index], false, index);
	}
	for (size_t index = 0; index < animationObjects.size(); ++index) {
		testObject(animationObjects[index], true, index);
	}

	if (found) {
		SelectSceneObject(foundAnimation, foundIndex);
	} else {
		ClearSceneObjectSelection();
	}
#endif
}

void GamePlayScene::DrawGameViewModelToolsOverlay()
{
#ifdef USE_IMGUI
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(x, y, width, height)) {
		return;
	}

	if (!isModelPreviewMode_ && !ImGui::IsDragDropActive()) {
		return;
	}

	const ImVec2 gameViewMin(x, y);
	const ImVec2 gameViewMax(x + width, y + height);
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(gameViewMin, gameViewMax, true);

	const ImVec2 panelMin(gameViewMin.x + 14.0f, gameViewMin.y + 14.0f);
	const ImVec2 panelMax(panelMin.x + 390.0f, panelMin.y + (isModelPreviewMode_ ? 88.0f : 54.0f));
	drawList->AddRectFilled(panelMin, panelMax, IM_COL32(8, 12, 18, 205), 8.0f);
	drawList->AddRect(panelMin, panelMax, IM_COL32(95, 170, 255, 210), 8.0f, 0, 1.5f);

	ImVec2 textPos(panelMin.x + 12.0f, panelMin.y + 10.0f);
	if (isModelPreviewMode_) {
		const std::string title = "Preview: " + previewModelFile_;
		drawList->AddText(textPos, IM_COL32(235, 245, 255, 255), title.c_str());
		textPos.y += 20.0f;
		if (isTexturePreviewMode_) {
			drawList->AddText(textPos, IM_COL32(190, 220, 255, 255), "Left drag: pan 2D Texture / Wheel: zoom / R: reset");
		} else {
			drawList->AddText(textPos, IM_COL32(190, 220, 255, 255), "Left drag: pan / Right drag: orbit / Wheel: zoom / R: reset");
		}
		textPos.y += 20.0f;
		drawList->AddText(textPos, IM_COL32(190, 220, 255, 255), "Click outside Game View or selected shelf card to return.");
		textPos.y += 20.0f;
		drawList->AddText(textPos, IM_COL32(255, 220, 120, 255), isTexturePreviewMode_
			? "Drag this texture card to Game View, or use Add Selected, to place it."
			: "Drag this shelf card to Game View to add it to the scene.");
	}
	else if (ImGui::IsDragDropActive()) {
		drawList->AddText(textPos, IM_COL32(235, 245, 255, 255), "Drop model here");
		textPos.y += 20.0f;
		drawList->AddText(textPos, IM_COL32(190, 220, 255, 255), "Release on Game View to add the model at the cursor.");
		drawList->AddRect(gameViewMin, gameViewMax, IM_COL32(80, 180, 255, 240), 0.0f, 0, 3.0f);
	}

	drawList->PopClipRect();
#endif
}

bool GamePlayScene::IsMouseOverSelectedObjectGizmo(float mouseScreenX, float mouseScreenY) const
{
#ifdef USE_IMGUI
	const Object3d* selectedObject = GetSelectedSceneObject();
	if (!selectedObject || isModelPreviewMode_) {
		return false;
	}
	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (!activeCamera) {
		return false;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return false;
	}

	const ImRect gameViewRect(ImVec2(rectX, rectY), ImVec2(rectX + rectWidth, rectY + rectHeight));
	const ImVec2 mouse(mouseScreenX, mouseScreenY);
	if (!gameViewRect.Contains(mouse)) {
		return false;
	}

	MyMath::AABB aabb{};
	if (!BuildWorldAabb(*selectedObject, aabb)) {
		return false;
	}
	const Vector3 center{
		(aabb.min.x + aabb.max.x) * 0.5f,
		(aabb.min.y + aabb.max.y) * 0.5f,
		(aabb.min.z + aabb.max.z) * 0.5f
	};
	const Vector3 size{
		aabb.max.x - aabb.min.x,
		aabb.max.y - aabb.min.y,
		aabb.max.z - aabb.min.z
	};
	const float axisLength = (std::max)({ size.x, size.y, size.z, 1.0f }) * 0.85f;
	const Matrix4x4& viewProjectionMatrix = activeCamera->GetViewProjectionMatrix();
	const auto projectPoint = [&](const Vector3& world, ImVec2& out) {
		Vector3 ndc{};
		if (!TransformCoord(world, viewProjectionMatrix, ndc)) {
			return false;
		}
		out = ImVec2(
			rectX + (ndc.x + 1.0f) * 0.5f * rectWidth,
			rectY + (1.0f - ndc.y) * 0.5f * rectHeight);
		return true;
	};

	ImVec2 centerScreen{};
	ImVec2 endScreen{};
	if (!projectPoint(center, centerScreen)) {
		return false;
	}

	constexpr float hitRadius = 12.0f;
	const std::array<Vector3, 6> axisEnds{ {
		{ center.x + axisLength, center.y, center.z },
		{ center.x - axisLength, center.y, center.z },
		{ center.x, center.y + axisLength, center.z },
		{ center.x, center.y - axisLength, center.z },
		{ center.x, center.y, center.z + axisLength },
		{ center.x, center.y, center.z - axisLength }
	} };

	for (const Vector3& axisEnd : axisEnds) {
		if (projectPoint(axisEnd, endScreen) &&
			DistanceToSegment(mouse, centerScreen, endScreen) <= hitRadius) {
			return true;
		}
	}
	return false;
#else
	(void)mouseScreenX;
	(void)mouseScreenY;
	return false;
#endif
}

bool GamePlayScene::IsMouseOverSelectedSpriteGizmo(float mouseScreenX, float mouseScreenY) const
{
#ifdef USE_IMGUI
	const Sprite* selectedSprite = GetSelectedSceneSprite();
	if (!selectedSprite || isModelPreviewMode_) {
		return false;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return false;
	}
	const float clientWidth = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
	if (clientWidth <= 0.0f || clientHeight <= 0.0f) {
		return false;
	}

	const Vector2 position = selectedSprite->GetPosition();
	const ImVec2 center(
		rectX + (position.x / clientWidth) * rectWidth,
		rectY + (position.y / clientHeight) * rectHeight);
	const float axisLength = 64.0f;
	const ImVec2 mouse(mouseScreenX, mouseScreenY);
	constexpr float hitRadius = 12.0f;
	return DistanceToSegment(mouse, center, ImVec2(center.x + axisLength, center.y)) <= hitRadius ||
		DistanceToSegment(mouse, center, ImVec2(center.x - axisLength, center.y)) <= hitRadius ||
		DistanceToSegment(mouse, center, ImVec2(center.x, center.y + axisLength)) <= hitRadius ||
		DistanceToSegment(mouse, center, ImVec2(center.x, center.y - axisLength)) <= hitRadius;
#else
	(void)mouseScreenX;
	(void)mouseScreenY;
	return false;
#endif
}

void GamePlayScene::DrawSelectedSpriteGizmo()
{
#ifdef USE_IMGUI
	Sprite* selectedSprite = GetSelectedSceneSprite();
	if (!selectedSprite || isModelPreviewMode_) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}
	const float clientWidth = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
	if (clientWidth <= 0.0f || clientHeight <= 0.0f) {
		return;
	}

	Vector2 position = selectedSprite->GetPosition();
	const ImVec2 center(
		rectX + (position.x / clientWidth) * rectWidth,
		rectY + (position.y / clientHeight) * rectHeight);
	const float axisLength = 64.0f;
	const ImVec2 right(center.x + axisLength, center.y);
	const ImVec2 left(center.x - axisLength, center.y);
	const ImVec2 down(center.x, center.y + axisLength);
	const ImVec2 up(center.x, center.y - axisLength);

	const ImVec2 mouse = ImGui::GetMousePos();
	constexpr float hitRadius = 12.0f;
	GizmoAxis hoveredAxis = GizmoAxis::None;
	Vector2 moveDirection{};
	float bestDistance = hitRadius;
	const auto testAxis = [&](const ImVec2& end, GizmoAxis axis, const Vector2& direction) {
		const float distance = DistanceToSegment(mouse, center, end);
		if (distance <= bestDistance) {
			bestDistance = distance;
			hoveredAxis = axis;
			moveDirection = direction;
		}
	};
	testAxis(right, GizmoAxis::X, { 1.0f, 0.0f });
	testAxis(left, GizmoAxis::X, { -1.0f, 0.0f });
	testAxis(down, GizmoAxis::Y, { 0.0f, 1.0f });
	testAxis(up, GizmoAxis::Y, { 0.0f, -1.0f });

	if (hoveredAxis != GizmoAxis::None) {
		isGameViewCameraDragging_ = false;
	}
	if (hoveredAxis != GizmoAxis::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		activeSpriteGizmoAxis_ = hoveredAxis;
		isDraggingSpriteGizmo_ = true;
		activeGizmoScreenDirectionX_ = moveDirection.x;
		activeGizmoScreenDirectionY_ = moveDirection.y;
		isGameViewCameraDragging_ = false;
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		activeSpriteGizmoAxis_ = GizmoAxis::None;
		isDraggingSpriteGizmo_ = false;
	}
	if (isDraggingSpriteGizmo_ && activeSpriteGizmoAxis_ != GizmoAxis::None) {
		const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
		const float spriteDeltaX = mouseDelta.x * (clientWidth / rectWidth);
		const float spriteDeltaY = mouseDelta.y * (clientHeight / rectHeight);
		if (activeSpriteGizmoAxis_ == GizmoAxis::X) {
			position.x += spriteDeltaX;
		}
		if (activeSpriteGizmoAxis_ == GizmoAxis::Y) {
			position.y += spriteDeltaY;
		}
		selectedSprite->SetPosition(position);
	}

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	const ImVec2 gameViewMin(rectX, rectY);
	const ImVec2 gameViewMax(rectX + rectWidth, rectY + rectHeight);
	drawList->PushClipRect(gameViewMin, gameViewMax, true);
	const auto axisColor = [&](GizmoAxis axis, ImU32 color) {
		if (activeSpriteGizmoAxis_ == axis) {
			return IM_COL32(255, 255, 255, 255);
		}
		if (hoveredAxis == axis) {
			return IM_COL32(255, 235, 120, 255);
		}
		return color;
	};
	const auto drawAxis = [&](const ImVec2& end, GizmoAxis axis, ImU32 color, const char* label) {
		const ImU32 finalColor = axisColor(axis, color);
		drawList->AddLine(center, end, finalColor, 4.0f);
		drawList->AddCircleFilled(end, 7.0f, finalColor);
		drawList->AddText(ImVec2(end.x + 8.0f, end.y - 8.0f), finalColor, label);
	};
	drawList->AddCircleFilled(center, 6.0f, IM_COL32(255, 255, 255, 230));
	drawAxis(right, GizmoAxis::X, IM_COL32(255, 80, 80, 255), "Right");
	drawAxis(left, GizmoAxis::X, IM_COL32(255, 80, 80, 255), "Left");
	drawAxis(down, GizmoAxis::Y, IM_COL32(80, 255, 120, 255), "Down");
	drawAxis(up, GizmoAxis::Y, IM_COL32(80, 255, 120, 255), "Up");
	drawList->AddText(ImVec2(center.x + 12.0f, center.y + 12.0f), IM_COL32(255, 255, 255, 230), "Selected 2D Texture");
	drawList->PopClipRect();
#endif
}

void GamePlayScene::DrawSelectedObjectGizmo()
{
#ifdef USE_IMGUI
	Object3d* selectedObject = GetSelectedSceneObject();
	if (!selectedObject || isModelPreviewMode_) {
		return;
	}
	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (!activeCamera) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}

	MyMath::AABB aabb{};
	if (!BuildWorldAabb(*selectedObject, aabb)) {
		return;
	}
	const Vector3 center{
		(aabb.min.x + aabb.max.x) * 0.5f,
		(aabb.min.y + aabb.max.y) * 0.5f,
		(aabb.min.z + aabb.max.z) * 0.5f
	};
	const Vector3 size{
		aabb.max.x - aabb.min.x,
		aabb.max.y - aabb.min.y,
		aabb.max.z - aabb.min.z
	};
	const float axisLength = (std::max)({ size.x, size.y, size.z, 1.0f }) * 0.85f;
	const Matrix4x4& viewProjectionMatrix = activeCamera->GetViewProjectionMatrix();
	const auto projectPoint = [&](const Vector3& world, ImVec2& out) {
		Vector3 ndc{};
		if (!TransformCoord(world, viewProjectionMatrix, ndc)) {
			return false;
		}
		out = ImVec2(
			rectX + (ndc.x + 1.0f) * 0.5f * rectWidth,
			rectY + (1.0f - ndc.y) * 0.5f * rectHeight);
		return true;
	};

	ImVec2 centerScreen{};
	ImVec2 xPlusScreen{};
	ImVec2 xMinusScreen{};
	ImVec2 yPlusScreen{};
	ImVec2 yMinusScreen{};
	ImVec2 zPlusScreen{};
	ImVec2 zMinusScreen{};
	if (!projectPoint(center, centerScreen) ||
		!projectPoint({ center.x + axisLength, center.y, center.z }, xPlusScreen) ||
		!projectPoint({ center.x - axisLength, center.y, center.z }, xMinusScreen) ||
		!projectPoint({ center.x, center.y + axisLength, center.z }, yPlusScreen) ||
		!projectPoint({ center.x, center.y - axisLength, center.z }, yMinusScreen) ||
		!projectPoint({ center.x, center.y, center.z + axisLength }, zPlusScreen) ||
		!projectPoint({ center.x, center.y, center.z - axisLength }, zMinusScreen)) {
		return;
	}

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	const ImVec2 gameViewMin(rectX, rectY);
	const ImVec2 gameViewMax(rectX + rectWidth, rectY + rectHeight);
	drawList->PushClipRect(gameViewMin, gameViewMax, true);

	const ImVec2 mouse = ImGui::GetMousePos();
	const float hitRadius = 10.0f;
	GizmoAxis hoveredAxis = GizmoAxis::None;
	ImVec2 hoveredScreenDirection{};
	Vector3 hoveredWorldDirection{};
	float bestAxisDistance = hitRadius;
	const auto normalizedScreenDirection = [&](const ImVec2& end) {
		const ImVec2 direction(end.x - centerScreen.x, end.y - centerScreen.y);
		const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
		if (length <= 0.0001f) {
			return ImVec2(0.0f, 0.0f);
		}
		return ImVec2(direction.x / length, direction.y / length);
	};
	const auto testAxis = [&](const ImVec2& end, GizmoAxis axis, const Vector3& worldDirection) {
		const float distance = DistanceToSegment(mouse, centerScreen, end);
		if (distance <= bestAxisDistance) {
			bestAxisDistance = distance;
			hoveredAxis = axis;
			hoveredScreenDirection = normalizedScreenDirection(end);
			hoveredWorldDirection = worldDirection;
		}
	};
	testAxis(xPlusScreen, GizmoAxis::X, { 1.0f, 0.0f, 0.0f });
	testAxis(xMinusScreen, GizmoAxis::X, { -1.0f, 0.0f, 0.0f });
	testAxis(yPlusScreen, GizmoAxis::Y, { 0.0f, 1.0f, 0.0f });
	testAxis(yMinusScreen, GizmoAxis::Y, { 0.0f, -1.0f, 0.0f });
	testAxis(zPlusScreen, GizmoAxis::Z, { 0.0f, 0.0f, 1.0f });
	testAxis(zMinusScreen, GizmoAxis::Z, { 0.0f, 0.0f, -1.0f });

	if (hoveredAxis != GizmoAxis::None) {
		isGameViewCameraDragging_ = false;
	}
	if (hoveredAxis != GizmoAxis::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		activeGizmoAxis_ = hoveredAxis;
		isDraggingGizmo_ = true;
		activeGizmoScreenDirectionX_ = hoveredScreenDirection.x;
		activeGizmoScreenDirectionY_ = hoveredScreenDirection.y;
		activeGizmoWorldDirection_ = hoveredWorldDirection;
		isGameViewCameraDragging_ = false;
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		activeGizmoAxis_ = GizmoAxis::None;
		isDraggingGizmo_ = false;
		activeGizmoScreenDirectionX_ = 0.0f;
		activeGizmoScreenDirectionY_ = 0.0f;
	}
	if (isDraggingGizmo_ && activeGizmoAxis_ != GizmoAxis::None) {
		const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
		const float moveScale = (std::max)(axisLength, 1.0f) * 0.006f;
		const float projectedDrag =
			mouseDelta.x * activeGizmoScreenDirectionX_ +
			mouseDelta.y * activeGizmoScreenDirectionY_;
		const float worldMove = projectedDrag * moveScale;
		Transform& transform = selectedObject->GetTransform();
		transform.translate.x += activeGizmoWorldDirection_.x * worldMove;
		transform.translate.y += activeGizmoWorldDirection_.y * worldMove;
		transform.translate.z += activeGizmoWorldDirection_.z * worldMove;
		selectedObject->SetTranslate(transform.translate);
	}

	const auto axisColor = [&](GizmoAxis axis, ImU32 normalColor) {
		if (activeGizmoAxis_ == axis) {
			return IM_COL32(255, 255, 255, 255);
		}
		if (hoveredAxis == axis) {
			return IM_COL32(255, 235, 120, 255);
		}
		return normalColor;
	};
	const auto drawAxis = [&](const ImVec2& end, GizmoAxis axis, ImU32 color, const char* label, bool primary) {
		const ImU32 finalColor = axisColor(axis, color);
		const ImU32 fadedColor = primary ? finalColor : (finalColor & IM_COL32(255, 255, 255, 120));
		drawList->AddLine(centerScreen, end, fadedColor, primary ? 4.0f : 2.0f);
		drawList->AddCircleFilled(end, primary ? 7.0f : 5.0f, fadedColor);
		drawList->AddText(ImVec2(end.x + 8.0f, end.y - 8.0f), fadedColor, label);
	};

	drawList->AddCircleFilled(centerScreen, 6.0f, IM_COL32(255, 255, 255, 230));
	drawAxis(xPlusScreen, GizmoAxis::X, IM_COL32(255, 80, 80, 255), "X", true);
	drawAxis(xMinusScreen, GizmoAxis::X, IM_COL32(255, 80, 80, 255), "-X", false);
	drawAxis(yPlusScreen, GizmoAxis::Y, IM_COL32(80, 255, 120, 255), "Y", true);
	drawAxis(yMinusScreen, GizmoAxis::Y, IM_COL32(80, 255, 120, 255), "-Y", false);
	drawAxis(zPlusScreen, GizmoAxis::Z, IM_COL32(80, 150, 255, 255), "Z", true);
	drawAxis(zMinusScreen, GizmoAxis::Z, IM_COL32(80, 150, 255, 255), "-Z", false);

	const std::string label = selectedSceneObjectIsAnimation_ ? "Selected Animation Model" : "Selected Model";
	drawList->AddText(ImVec2(centerScreen.x + 12.0f, centerScreen.y + 12.0f), IM_COL32(255, 255, 255, 230), label.c_str());
	drawList->PopClipRect();
#endif
}

bool GamePlayScene::BuildWorldAabb(const Object3d& object, MyMath::AABB& outAabb) const
{
	Model* model = object.GetModel();
	if (!model || model->GetModelData().vertices.empty()) {
		return false;
	}

	const Transform& transform = object.GetTransform();
	const Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	Vector3 minPoint{
		(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)()
	};
	Vector3 maxPoint{
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest()
	};

	for (const Model::VertexData& vertex : model->GetModelData().vertices) {
		const Vector3 localPosition{ vertex.position.x, vertex.position.y, vertex.position.z };
		const Vector3 worldPosition = TransformPoint(localPosition, worldMatrix);
		minPoint.x = (std::min)(minPoint.x, worldPosition.x);
		minPoint.y = (std::min)(minPoint.y, worldPosition.y);
		minPoint.z = (std::min)(minPoint.z, worldPosition.z);
		maxPoint.x = (std::max)(maxPoint.x, worldPosition.x);
		maxPoint.y = (std::max)(maxPoint.y, worldPosition.y);
		maxPoint.z = (std::max)(maxPoint.z, worldPosition.z);
	}

	outAabb.min = minPoint;
	outAabb.max = maxPoint;
	return true;
}

void GamePlayScene::DrawCollisionDebugOverlay()
{
#ifdef USE_IMGUI
	if (!showCollisionDebug_) {
		return;
	}

	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (!activeCamera) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}

	std::vector<CollisionDebugBox> boxes;
	auto appendObject = [&](const std::unique_ptr<Object3d>& object) {
		if (!object) {
			return;
		}
		CollisionDebugBox box{};
		if (BuildWorldAabb(*object, box.aabb)) {
			boxes.push_back(box);
		}
	};

	if (isModelPreviewMode_ && previewObject_) {
		appendObject(previewObject_);
	} else {
		for (const auto& object : normalObjects) {
			appendObject(object);
		}
		for (const auto& object : animationObjects) {
			appendObject(object);
		}
	}

	for (size_t i = 0; i < boxes.size(); ++i) {
		for (size_t j = i + 1; j < boxes.size(); ++j) {
			if (IsAabbOverlapping(boxes[i].aabb, boxes[j].aabb)) {
				boxes[i].overlaps = true;
				boxes[j].overlaps = true;
			}
		}
	}

	const Matrix4x4& viewProjectionMatrix = activeCamera->GetViewProjectionMatrix();
	const ImVec2 imageMin(rectX, rectY);
	const ImVec2 imageMax(rectX + rectWidth, rectY + rectHeight);
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(imageMin, imageMax, true);

	const auto projectToGameView = [&](const Vector3& position, ImVec2& screenPosition) {
		const float x = position.x * viewProjectionMatrix.m[0][0] + position.y * viewProjectionMatrix.m[1][0] + position.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0];
		const float y = position.x * viewProjectionMatrix.m[0][1] + position.y * viewProjectionMatrix.m[1][1] + position.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1];
		const float w = position.x * viewProjectionMatrix.m[0][3] + position.y * viewProjectionMatrix.m[1][3] + position.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];
		if (w <= 0.0f) {
			return false;
		}
		screenPosition = ImVec2(
			imageMin.x + (x / w + 1.0f) * 0.5f * rectWidth,
			imageMin.y + (1.0f - y / w) * 0.5f * rectHeight);
		return true;
	};

	constexpr std::array<std::pair<int, int>, 12> edges{ {
		{0, 1}, {1, 3}, {3, 2}, {2, 0},
		{4, 5}, {5, 7}, {7, 6}, {6, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}
	} };

	for (const CollisionDebugBox& box : boxes) {
		const std::array<Vector3, 8> corners{ {
			{ box.aabb.min.x, box.aabb.min.y, box.aabb.min.z },
			{ box.aabb.max.x, box.aabb.min.y, box.aabb.min.z },
			{ box.aabb.min.x, box.aabb.max.y, box.aabb.min.z },
			{ box.aabb.max.x, box.aabb.max.y, box.aabb.min.z },
			{ box.aabb.min.x, box.aabb.min.y, box.aabb.max.z },
			{ box.aabb.max.x, box.aabb.min.y, box.aabb.max.z },
			{ box.aabb.min.x, box.aabb.max.y, box.aabb.max.z },
			{ box.aabb.max.x, box.aabb.max.y, box.aabb.max.z }
		} };

		std::array<ImVec2, 8> projected{};
		std::array<bool, 8> visible{};
		for (size_t i = 0; i < corners.size(); ++i) {
			visible[i] = projectToGameView(corners[i], projected[i]);
		}

		const ImU32 color = box.overlaps ? IM_COL32(255, 60, 60, 255) : IM_COL32(70, 160, 255, 255);
		for (const auto& edge : edges) {
			if (visible[edge.first] && visible[edge.second]) {
				drawList->AddLine(projected[edge.first], projected[edge.second], color, 2.0f);
			}
		}
	}

	drawList->PopClipRect();
#endif
}

void GamePlayScene::InitializeUiSmokeFromEnvironment()
{
	if (GetEnvironmentString("CG2_GAMEPLAY_UI_SMOKE").empty()) {
		return;
	}

	uiSmokeEnabled_ = true;
	uiSmokeFinished_ = false;
	uiSmokePendingCapture_ = false;
	uiSmokeFrame_ = 0;
	uiSmokeStage_ = 0;
	uiSmokeModelFile_.clear();

	std::error_code errorCode;
	std::filesystem::create_directories("logs", errorCode);
	uiSmokeLogPath_ = std::filesystem::path("logs") / ("gameplay_ui_smoke_" + MakeTimestampString() + ".log");
}

void GamePlayScene::UpdateUiSmoke()
{
	if (!uiSmokeEnabled_ || uiSmokeFinished_) {
		return;
	}

	++uiSmokeFrame_;
	if (uiSmokeFrame_ < 6) {
		return;
	}

	if (uiSmokeStage_ == 0) {
		const auto modelIt = std::find_if(modelLibrary_.begin(), modelLibrary_.end(), [](const ResourceModelEntry& entry) {
			return entry.canLoad && !entry.hasAnimation && !entry.isTexture;
		});
		if (modelIt == modelLibrary_.end()) {
			FinishUiSmoke(false, "No loadable non-animation model was found in resources.");
			return;
		}

		uiSmokeModelFile_ = modelIt->fileName;
		if (!EnterModelPreview(uiSmokeModelFile_)) {
			FinishUiSmoke(false, "EnterModelPreview failed for " + uiSmokeModelFile_);
			return;
		}
		if (!isModelPreviewMode_ || !previewObject_) {
			FinishUiSmoke(false, "Preview mode did not become active.");
			return;
		}

		MyMath::AABB previewAabb{};
		if (!BuildWorldAabb(*previewObject_, previewAabb)) {
			FinishUiSmoke(false, "Preview AABB could not be built.");
			return;
		}

		uiSmokeStage_ = 1;
		uiSmokeFrame_ = 0;
		return;
	}

	if (uiSmokeStage_ == 1) {
		ResetModelPreviewCamera();
		ExitModelPreview();
		if (isModelPreviewMode_ || previewObject_) {
			FinishUiSmoke(false, "ExitModelPreview did not leave preview mode.");
			return;
		}

		const size_t normalBefore = normalObjects.size();
		const size_t animationBefore = animationObjects.size();
		if (!AddModelToScene(uiSmokeModelFile_)) {
			FinishUiSmoke(false, "AddModelToScene failed for " + uiSmokeModelFile_);
			return;
		}
		if (normalObjects.size() != normalBefore + 1 || animationObjects.size() != animationBefore) {
			FinishUiSmoke(false, "Non-animation model did not add to the normal model list.");
			return;
		}

		Object3d* addedObject = nullptr;
		std::string addedInspectorGroup;
		size_t addedInspectorIndex = 0;
		if (normalObjects.size() > normalBefore) {
			addedObject = normalObjects.back().get();
			addedInspectorGroup = "Model";
			addedInspectorIndex = normalObjects.size() - 1;
		}
		MyMath::AABB addedAabb{};
		if (!addedObject || !BuildWorldAabb(*addedObject, addedAabb)) {
			FinishUiSmoke(false, "Added model AABB could not be built.");
			return;
		}
		if (addedObject->GetModelName() != uiSmokeModelFile_) {
			FinishUiSmoke(false, "Added model name was not reflected on the inspector target.");
			return;
		}
		if ((addedInspectorGroup == "Model" && addedInspectorIndex < baseNormalObjectCount_) ||
			(addedInspectorGroup == "Animation Model" && addedInspectorIndex < baseAnimationObjectCount_)) {
			FinishUiSmoke(false, "Added model was not placed in the editable inspector range.");
			return;
		}
		if (!hasSelectedSceneObject_ ||
			selectedSceneObjectIsAnimation_ ||
			selectedSceneObjectIndex_ != addedInspectorIndex) {
			FinishUiSmoke(false, "Added normal model was not selected for inspector/gizmo editing.");
			return;
		}

		const auto textureIt = std::find_if(modelLibrary_.begin(), modelLibrary_.end(), [](const ResourceModelEntry& entry) {
			return entry.isTexture;
		});
		if (textureIt == modelLibrary_.end()) {
			FinishUiSmoke(false, "No 2D Texture was found in resources.");
			return;
		}
		if (!EnterTexturePreview(textureIt->textureFilePath)) {
			FinishUiSmoke(false, "EnterTexturePreview failed for " + textureIt->textureFilePath);
			return;
		}
		if (!isModelPreviewMode_ || !isTexturePreviewMode_ || !previewSprite_) {
			FinishUiSmoke(false, "2D Texture preview mode did not become active.");
			return;
		}
		ResetModelPreviewCamera();
		ExitModelPreview();
		if (isModelPreviewMode_ || isTexturePreviewMode_ || previewSprite_) {
			FinishUiSmoke(false, "ExitModelPreview did not leave 2D Texture preview mode.");
			return;
		}
		const size_t spriteBefore = sprites.size();
		if (!AddTextureToScene(textureIt->textureFilePath)) {
			FinishUiSmoke(false, "AddTextureToScene failed for " + textureIt->textureFilePath);
			return;
		}
		if (sprites.size() != spriteBefore + 1 ||
			!hasSelectedSceneSprite_ ||
			selectedSceneSpriteIndex_ != sprites.size() - 1 ||
			hasSelectedSceneObject_) {
			FinishUiSmoke(false, "Added 2D Texture was not selected for Sprite inspector/gizmo editing.");
			return;
		}

		ClearAddedSceneModels();
		if (normalObjects.size() != baseNormalObjectCount_ ||
			animationObjects.size() != baseAnimationObjectCount_ ||
			sprites.size() != baseSpriteCount_) {
			FinishUiSmoke(false, "ClearAddedSceneModels did not restore base counts.");
			return;
		}

		uiSmokePendingCapture_ = true;
		uiSmokeStage_ = 2;
		uiSmokeFrame_ = 0;
		return;
	}
}

void GamePlayScene::UpdateUiSmokeAfterDraw()
{
	if (!uiSmokeEnabled_ || uiSmokeFinished_ || !uiSmokePendingCapture_) {
		return;
	}

	std::vector<unsigned char> pixels;
	int width = 0;
	int height = 0;
	if (!CaptureGameViewPixels(pixels, width, height)) {
		FinishUiSmoke(false, "CaptureGameViewPixels failed.");
		return;
	}

	const std::filesystem::path screenshotPath =
		GetUserMediaDirectory("Pictures", "CG2Captures") / ("CG2_ui_smoke_" + MakeTimestampString() + ".bmp");
	if (!SavePixelsAsBmp(screenshotPath, pixels, width, height)) {
		FinishUiSmoke(false, "SavePixelsAsBmp failed: " + screenshotPath.string());
		return;
	}

	const std::filesystem::path videoPath =
		GetUserMediaDirectory("Videos", "CG2Recordings") / ("CG2_ui_smoke_" + MakeTimestampString() + ".avi");
	recordingFrameIndex_ = 0;
	if (!BeginRecordingAvi(videoPath, width, height)) {
		FinishUiSmoke(false, "BeginRecordingAvi failed: " + videoPath.string());
		return;
	}
	if (!AppendRecordingFrame(pixels, width, height)) {
		EndRecordingAvi();
		FinishUiSmoke(false, "AppendRecordingFrame failed.");
		return;
	}
	++recordingFrameIndex_;
	EndRecordingAvi();

	std::error_code errorCode;
	const bool screenshotExists = std::filesystem::exists(screenshotPath, errorCode) && !errorCode;
	errorCode.clear();
	const bool videoExists = std::filesystem::exists(videoPath, errorCode) && !errorCode;
	if (!screenshotExists || !videoExists) {
		FinishUiSmoke(false, "Capture files were not created.");
		return;
	}

	lastScreenshotPath_ = screenshotPath;
	lastVideoPath_ = videoPath;
	uiSmokePendingCapture_ = false;
	FinishUiSmoke(true, "OK model=" + uiSmokeModelFile_ +
		" inspector=editable" +
		" screenshot=" + screenshotPath.string() +
		" video=" + videoPath.string());
}

void GamePlayScene::FinishUiSmoke(bool success, const std::string& message)
{
	if (uiSmokeFinished_) {
		return;
	}

	uiSmokeFinished_ = true;
	std::ofstream log(uiSmokeLogPath_, std::ios::app);
	if (log) {
		log << (success ? "SUCCESS: " : "FAILURE: ") << message << '\n';
	}
	PostQuitMessage(success ? 0 : 1);
}

void GamePlayScene::UpdateParticleEffectEmission()
{
	ParticleManager* particleManager = ParticleManager::GetInstance();
	const Vector3 effectPosition = GetParticleEffectPosition();
	if (cylinderEffect_.enabled != lastCylinderEffectEnabled_ || (cylinderEffect_.enabled && refreshCylinderEffect_)) {
		if (cylinderEffect_.enabled) {
			particleManager->ClearParticles("Cylinder");
			particleManager->EmitCylinderEffect("Cylinder", static_cast<uint32_t>(cylinderEffect_.emitCount), effectPosition, cylinderEffect_.scale);
			isCylinderEffectVisible_ = true;
			lastCylinderEffectEmitCount_ = cylinderEffect_.emitCount;
			lastCylinderEffectScale_ = cylinderEffect_.scale;
			lastCylinderEffectBillboard_ = cylinderEffect_.billboard;
		} else {
			particleManager->ClearParticles("Cylinder");
			isCylinderEffectVisible_ = false;
		}
		lastCylinderEffectEnabled_ = cylinderEffect_.enabled;
		refreshCylinderEffect_ = false;
	}

	particleEffectEmitTimer_ += DirectXCommon::GetInstance()->GetDeltaTime();
	if (particleEffectEmitTimer_ < 0.12f) {
		return;
	}
	particleEffectEmitTimer_ = 0.0f;

	if (hitEffect_.enabled) {
		particleManager->EmitHitEffect("Hit", static_cast<uint32_t>(hitEffect_.emitCount), effectPosition, hitEffect_.scale);
	}
	if (ringEffect_.enabled) {
		particleManager->EmitRingEffect("Ring", static_cast<uint32_t>(ringEffect_.emitCount), effectPosition, ringEffect_.scale);
	}
	if (pillarSparkleEffect_.enabled) {
		particleManager->EmitPillarSparkle("PillarSparkle", static_cast<uint32_t>(pillarSparkleEffect_.emitCount), effectPosition, pillarSparkleEffect_.scale);
	}
	if (lightCoreEffect_.enabled) {
		particleManager->EmitLightCore("LightCore", static_cast<uint32_t>(lightCoreEffect_.emitCount), effectPosition, lightCoreEffect_.scale);
	}
	if (lightRainEffect_.enabled) {
		particleManager->EmitLightRain("LightRain", static_cast<uint32_t>(lightRainEffect_.emitCount), effectPosition, lightRainEffect_.scale);
	}
	if (lightSpiralEffect_.enabled) {
		particleManager->EmitLightSpiral("LightSpiral", static_cast<uint32_t>(lightSpiralEffect_.emitCount), effectPosition, lightSpiralEffect_.scale);
	}
}

void GamePlayScene::Update()
{
	UpdateGameViewCameraControl();

	//カメラの更新
	cameraManager->Update();

	//カメラのビュープロジェクション行�Eを渡して更新
	Matrix4x4 viewMatrix = cameraManager->GetActiveCamera()->GetViewMatrix();
	Matrix4x4 projectionMatrix = cameraManager->GetActiveCamera()->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

	// 安�Eのために NullチェチE��を追加
	if (activeEmitter) {
		activeEmitter->Update();
	}
	//パ�EチE��クル全体�E更新
	ParticleManager::GetInstance()->Update();

	//3Dオブジェクト�E更新通常モチE��
	for (auto& object3d : normalObjects) {
		//毎フレーム、�Eネ�Eジャから今�EアクチE��ブカメラをもらう
		object3d->SetCamera(cameraManager->GetActiveCamera());
		float length = Length(directionalLight.direction);
		if (length > 0.0f) {
			directionalLight.direction = Normalize(directionalLight.direction);
		} else {
			// 0の場合�E適当な下向きにするなど、エラーを回避する
			directionalLight.direction = { 0.0f, -1.0f, 0.0f };
		}
		object3d->SetDirectionalLight(directionalLight);//光を他�EモチE��にも�Eけ与えめE
		object3d->SetPointLight(pointLight);
		object3d->SetSpotLight(spotLight);
		object3d->Update();
	}

	// アニメーションの時間確誁E
	this->animationTime_ = objectAxis->GetAnimationTime();

	//3Dオブジェクト�E更新アニメーションモチE��
	for (auto& object3d : animationObjects) {
		//毎フレーム、�Eネ�Eジャから今�EアクチE��ブカメラをもらう
		object3d->SetCamera(cameraManager->GetActiveCamera());
		float length = Length(directionalLight.direction);
		if (length > 0.0f) {
			directionalLight.direction = Normalize(directionalLight.direction);
		} else {
			// 0の場合�E適当な下向きにするなど、エラーを回避する
			directionalLight.direction = { 0.0f, -1.0f, 0.0f };
		}
		object3d->SetDirectionalLight(directionalLight);//光を他�EモチE��にも�Eけ与えめE
		object3d->SetPointLight(pointLight);
		object3d->SetSpotLight(spotLight);
		object3d->Update();
	}

	//スプライト�E更新
	if (previewObject_) {
		previewObject_->SetCamera(cameraManager->GetActiveCamera());
		previewObject_->SetDirectionalLight(directionalLight);
		previewObject_->SetPointLight(pointLight);
		previewObject_->SetSpotLight(spotLight);
		previewObject_->Update();
	}
	if (previewSprite_) {
		previewSprite_->Update();
	}

	for (auto& sprite : sprites)
	{
		sprite->Update();
	}
	skyBox_->Update();

	//ゲームの処琁E

	ImGuiManager::GetInstance()->Begin("GamePlay");
	DrawTopToolsImGui();
	DrawInspectorImGui();
	DrawModelShelfImGui();
	HandleModelDropOnGameView();
	HandleGameViewObjectSelection();
	HandleGameViewSpriteSelection();
	DrawCollisionDebugOverlay();
	DrawSelectedObjectGizmo();
	DrawSelectedSpriteGizmo();
	DrawGameViewModelToolsOverlay();
#ifdef USE_IMGUI
	if (isModelPreviewMode_) {
		if (suppressPreviewExitUntilMouseRelease_) {
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				suppressPreviewExitUntilMouseRelease_ = false;
			}
		} else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			const Vector2 mouseScreen = Input::GetInstance()->GetMouseScreen();
			if (!ImGuiManager::GetInstance()->IsMouseOverGameView(mouseScreen.x, mouseScreen.y)) {
				ExitModelPreview();
			}
		}
	}
#endif
	// スケルトンをGame Viewの映像座標へ合わせ、モチE��の上へ重�Eて表示する、E
	if (ImGuiManager::GetInstance()->IsSkeletonDebugDrawEnabled() && !animationObjects.empty())
	{
		Object3d* animationObject = animationObjects[0].get();
		if (animationObject->IsSkeletal())
		{
			const Transform& animationTransform = animationObject->GetTransform();
			const Matrix4x4 animationWorldMatrix = MakeAffineMatrix(
				animationTransform.scale,
				animationTransform.rotate,
				animationTransform.translate);

			ImGuiManager::GetInstance()->SkeletonDebugDraw(
				animationObject->GetSkeleton(),
				animationWorldMatrix,
				cameraManager->GetActiveCamera()->GetViewProjectionMatrix());
		}
	}

	ImGuiManager::GetInstance()->End();

	UpdateUiSmoke();
	UpdateParticleEffectEmission();


	//Cキーが押されたらCylinderエフェクト�E表示を�Eり替える
	if (Input::GetInstance()->TriggerKey(DIK_0))
	{
		if (isCylinderEffectVisible_) {
			ParticleManager::GetInstance()->ClearParticles("Cylinder");
			isCylinderEffectVisible_ = false;
			cylinderEffect_.enabled = false;
			lastCylinderEffectEnabled_ = false;
		} else {
			OutputDebugStringA("Hit C\n");
			Vector3 hitPosition = GetParticleEffectPosition();
			ParticleManager::GetInstance()->EmitCylinderEffect("Cylinder", static_cast<uint32_t>(cylinderEffect_.emitCount), hitPosition, cylinderEffect_.scale);
			isCylinderEffectVisible_ = true;
			cylinderEffect_.enabled = true;
			lastCylinderEffectEnabled_ = true;
			lastCylinderEffectEmitCount_ = cylinderEffect_.emitCount;
			lastCylinderEffectScale_ = cylinderEffect_.scale;
			lastCylinderEffectBillboard_ = cylinderEffect_.billboard;
		}
	}

	//数字�EPキーが押されてぁE��めE
	if (Input::GetInstance()->TriggerKey(DIK_P))
	{
		Vector3 hitPosition = objectAxis->GetTransform().translate;
		hitPosition.x += 1.0f;
		hitPosition.y += 1.0f;
		OutputDebugStringA("Hit p\n");
		ParticleManager::GetInstance()->EmitHitEffect("Hit", static_cast<uint32_t>(hitEffect_.emitCount), hitPosition, hitEffect_.scale);
		ParticleManager::GetInstance()->EmitRingEffect("Ring", static_cast<uint32_t>(ringEffect_.emitCount), hitPosition, ringEffect_.scale);
	}
}

void GamePlayScene::Draw()
{

	//描画前�E琁E
	DirectXCommon::GetInstance()->PreDraw();
	SrvManager::GetInstance()->PreDraw();

	// 先に通常モチE��用を描画(処琁E��軽くするためE
	if (isModelPreviewMode_ && previewObject_) {
		object3dCommon->SetCommonDrawSetting();
		previewObject_->Draw();
	} else if (isModelPreviewMode_ && previewSprite_) {
		spriteCommon->SetCommonDrawSetting();
		previewSprite_->Draw();
	} else {
	object3dCommon->SetCommonDrawSetting();
	for (const auto& object3d : normalObjects) {
		if (!object3d->IsSkeletal()) {
			object3d->Draw();
		}
	}
	//skyBox描画
	if (skyBox_) {
		skyBox_->Draw();
	}

	// アニメーションモチE��用の描画
	object3dCommon->SetCommonDrawSetting();
	for (const auto& object3d : animationObjects) {
		if (object3d->IsSkeletal()) {
			object3d->Draw();
		}
	}

	//パ�EチE��クル描画
	ParticleManager::GetInstance()->Draw();

	// スプライト描画
	//Spriteの描画準備Spriteの描画に共通�EグラフィチE��スコマンドを積�E
	spriteCommon->SetCommonDrawSetting();
	for (const auto& sprite : sprites)
	{
		sprite->Draw();
	}

	// SceneはRenderTextureへ描画済みなので、ImGuiの直前にSwapChainへ刁E��替える
	}
	if (PostEffect::GetInstance()->IsEnabled()) {
		DirectXCommon::GetInstance()->PreDrawForPostEffectTexture();
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), true);
	}
	DirectXCommon::GetInstance()->PreDrawForSwapChain(PostEffect::GetInstance()->IsEnabled());
#ifndef USE_IMGUI
	// RenderTextureのSceneを�E画面三角形でSwapChainへコピ�Eする
	if (PostEffect::GetInstance()->IsEnabled()) {
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetPostEffectTextureSrvIndex(), false);
	} else {
		PostEffect::GetInstance()->Draw(DirectXCommon::GetInstance()->GetRenderTextureSrvIndex(), false);
	}
#endif
	ImGuiManager::GetInstance()->Draw(DirectXCommon::GetInstance());

	DirectXCommon::GetInstance()->PostDraw();
	UpdateRecordingCapture();
	UpdateUiSmokeAfterDraw();

}

void GamePlayScene::Finalize()
{
	//GPUの完亁E��E��
	DirectXCommon::GetInstance()->WaitForGPU();

	// 中途半端に生き残ってぁE��粒子が原因のアクセス違反を防ぁE
	ParticleManager::GetInstance()->SetCameraManager(nullptr);
	ParticleManager::GetInstance()->ClearAllGroups();

	activeEmitter = nullptr;
	objectPlane = nullptr;
	objectAxis = nullptr;
	EndRecordingAvi();
	isRecordingGameView_ = false;
	recordingTime_ = 0.0f;
	recordingFrameTimer_ = 0.0f;
	recordingFrameIndex_ = 0;
	recordingDirectory_.clear();
	recordingVideoPath_.clear();
	lastScreenshotPath_.clear();
	lastVideoPath_.clear();
	lastCaptureMessage_.clear();
	lastModelShelfMessage_.clear();
	baseNormalObjectCount_ = 0;
	baseAnimationObjectCount_ = 0;
	baseSpriteCount_ = 0;
	ClearSceneObjectSelection();
	ClearSceneSpriteSelection();
	previewObject_.reset();
	selectedLibraryModel_.clear();
	previewModelFile_.clear();
	modelLibrary_.clear();
	isModelPreviewMode_ = false;
	suppressPreviewExitUntilMouseRelease_ = false;
	emitterCircle.reset();
	emitterPlane.reset();
	skyBox_.reset();
	sprites.clear();
	normalObjects.clear();
	animationObjects.clear();
	upCamera.reset();
	mainCamera.reset();
	cameraManager.reset();
}

