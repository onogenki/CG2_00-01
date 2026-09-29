#include "TitleObjectManager.h"

#include "Object3d.h"
#include "Object3dFactory.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <algorithm>
#include <utility>

// 前方宣言した所有型を、完全な型が分かる場所で生成します。
TitleObjectManager::TitleObjectManager() = default;

// 所有するモデル・Spriteを、完全な型が分かる場所で破棄します。
TitleObjectManager::~TitleObjectManager() = default;

// 初期背景と画像を作り、Editorの一括削除から保護する件数を記録します。
bool TitleObjectManager::Initialize(
	Object3dCommon* object3dCommon,
	SpriteCommon* spriteCommon,
	DirectXCommon* directXCommon)
{
	auto terrain = Object3dFactory::Create(object3dCommon, "plane.obj");
	if (!terrain) {
		return false;
	}
	// Camera・LightはSceneのRenderContextへ任せ、ここでは従来の配置だけを設定します。
	terrain->GetTransform().translate = { 1.0f, -2.0f, 10.0f };
	normalObjects_.push_back(std::move(terrain));
	baseNormalObjectCount_ = normalObjects_.size();
	baseAnimationObjectCount_ = animationObjects_.size();

	spriteCommon->Initialize(directXCommon);
	// 初期画像が読めなければ、未登録Textureを参照するSpriteを作らず失敗を伝えます。
	if (!TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png")) {
		return false;
	}
	auto titleSprite = std::make_unique<Sprite>();
	titleSprite->Initialize(spriteCommon, "Resources/uvChecker.png");
	titleSprite->SetPosition({ 0.0f, 0.0f });
	// InspectorのPos/Sizeと同じピクセル値です。画面幅からの計算ではなく、ここへ直接記入します。
	titleSprite->SetSize({ 512.0f, 512.0f });
	sprites_.push_back(std::move(titleSprite));
	baseSpriteCount_ = sprites_.size();
	return true;
}

// Shelfから選ばれたモデルを作り、通常モデルまたはAnimationモデルの一覧へ追加します。
bool TitleObjectManager::AddModel(Object3dCommon* object3dCommon, const std::string& fileName)
{
	auto object = Object3dFactory::Create(object3dCommon, fileName, true);
	if (!object) {
		return false;
	}
	// 追加順にX方向へずらし、同じ場所にモデルが重ならない従来の配置を保ちます。
	const float offset = static_cast<float>(normalObjects_.size() + animationObjects_.size()) * 1.4f;
	object->SetTranslate({ -2.0f + offset, 0.0f, 6.0f });
	object->SetScale({ 1.0f, 1.0f, 1.0f });
	if (object->IsSkeletal()) {
		Object3dFactory::LoadAndPlayAnimation(*object, fileName);
		animationObjects_.push_back(std::move(object));
	} else {
		normalObjects_.push_back(std::move(object));
	}
	return true;
}

// Shelfで追加した画像だけは縦横比を保った仮サイズにし、画像を引き伸ばさないようにします。
bool TitleObjectManager::AddTexture(SpriteCommon* spriteCommon, const std::string& textureFilePath)
{
	// 寸法取得前に読込結果を確認し、欠損画像でGetMetaDataを呼ばないようにします。
	if (!spriteCommon || !TextureManager::GetInstance()->LoadTexture(textureFilePath)) {
		return false;
	}
	const auto& metadata = TextureManager::GetInstance()->GetMetaData(textureFilePath);
	Vector2 size{ static_cast<float>(metadata.width), static_cast<float>(metadata.height) };
	const float largestSide = (std::max)(size.x, size.y);
	if (largestSide > 180.0f) {
		const float scale = 180.0f / largestSide;
		size = { size.x * scale, size.y * scale };
	}
	return AddTexture(spriteCommon, textureFilePath, { 180.0f, 160.0f }, size);
}

// 指定したピクセル位置・サイズで一度だけ配置し、以後のInspector編集を上書きしません。
bool TitleObjectManager::AddTexture(
	SpriteCommon* spriteCommon, const std::string& textureFilePath,
	const Vector2& position, const Vector2& size)
{
	// 直接配置の経路でも同じ読込確認を行い、失敗した画像を一覧に残しません。
	if (!spriteCommon || !TextureManager::GetInstance()->LoadTexture(textureFilePath)) {
		return false;
	}
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon, textureFilePath);
	sprite->SetAnchorPoint({ 0.5f, 0.5f });
	sprite->SetSize(size);
	sprite->SetPosition(position);
	sprites_.push_back(std::move(sprite));
	return true;
}

// 初期配置の件数より後ろだけを解放し、背景と初期画像を残します。
void TitleObjectManager::ClearAdded()
{
	if (normalObjects_.size() > baseNormalObjectCount_) {
		normalObjects_.resize(baseNormalObjectCount_);
	}
	if (animationObjects_.size() > baseAnimationObjectCount_) {
		animationObjects_.resize(baseAnimationObjectCount_);
	}
	if (sprites_.size() > baseSpriteCount_) {
		sprites_.resize(baseSpriteCount_);
	}
}

// GPU完了後にSceneから呼び、所有物と初期配置の境界をリセットします。
void TitleObjectManager::Finalize()
{
	sprites_.clear();
	normalObjects_.clear();
	animationObjects_.clear();
	baseNormalObjectCount_ = 0;
	baseAnimationObjectCount_ = 0;
	baseSpriteCount_ = 0;
}
