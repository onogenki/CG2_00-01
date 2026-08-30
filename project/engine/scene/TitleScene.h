#pragma once
#include "DirectXCommon.h"
#include "Object3dCommon.h"
#include "SpriteCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Object3d.h"
#include "Sprite.h"
#include "ParticleEmitter.h"
#include "Audio.h"
#include "BaseScene.h"
#include "SceneEditor.h"
#include "SkyBox.h"
#include <memory>
#include <string>
#include <vector>

class TitleScene : public BaseScene
{
public:
	// Title画面へ入った一度だけ、Camera・モデル・UI用データを作成します。
	void Initialize() override;
	// Title画面を抜ける時に、TitleSceneが所有するデータを解放します。
	void Finalize() override;
	// 毎フレーム、Titleの入力・Camera・編集UIを更新します。
	void Update() override;
	// 毎フレーム、Titleの3Dモデル・Sprite・ImGuiを描画します。
	void Draw() override;

	// trueなら、このSceneを終了して次のSceneへ切り替えられます。
	bool IsFinished() const { return isFinished_; }

private:
	// ---------- Edit View・モデル棚 ----------

	// resources内のモデルとTextureを調べ、Title用のモデル棚へ登録します。
	void ScanResourceShelf();
	// Titleのモデル一覧をImGuiに表示します。
	void DrawTitleModelShelfImGui();
	// 選択中モデル・SpriteのTransformを編集するImGuiを表示します。
	void DrawTitleInspectorImGui();
	// モデル棚からEdit Viewへ落としたモデルをTitleへ追加します。
	void HandleTitleShelfDropOnEditView();
	// 3Dモデルを置くTitle用のEdit Viewを描画します。
	void DrawTitleEditViewport();
	// 2D Spriteを置くTitle用のEdit Viewを描画します。
	void DrawTitleSpriteEditViewport();
	// 指定した3DモデルをTitleの通常モデル一覧へ追加します。
	bool AddModelToTitle(const std::string& fileName);
	// 指定したTextureをTitleのSprite一覧へ追加します。
	bool AddTextureToTitle(const std::string& textureFilePath);

	// ---------- 空・3Dモデル・Sprite ----------

	// Titleの背景として描画するSkyBoxです。
	std::unique_ptr<SkyBox> skyBox_;

	// アニメーションを使わないTitle用の3Dモデルです。
	std::vector<std::unique_ptr<Object3d>> normalObjects;
	// アニメーション再生を行うTitle用の3Dモデルです。
	std::vector<std::unique_ptr<Object3d>> animationObjects_;
	// Title上へ追加した2D画像です。
	std::vector<std::unique_ptr<Sprite>> addedSprites_;
	// モデル棚に表示するresources内のファイル一覧です。
	SceneEditor::ShelfState shelfState_;

	// ---------- Title全体のライト ----------

	// 太陽光のように、Title全体へ同じ方向から当てる光です。
	Object3d::DirectionalLight directionalLight_{};
	// 指定した一点から周囲へ広がる光です。
	Object3d::PointLight pointLight_{};
	// 円すい状の範囲だけを照らす光です。
	Object3d::SpotLight spotLight_{};

	// ---------- Edit Viewの選択状態 ----------

	// Initialize時からあるモデル数です。追加分だけを消す境界に使います。
	size_t baseNormalObjectCount_ = 0;
	//最初からタイトルに置くスプライトの数です。追加したスプライトだけを削除できるようにします。
	size_t baseSpriteCount_ = 0;
	// Inspectorで選択しているSpriteの番号です。
	size_t selectedTitleSpriteIndex_ = 0;
	// trueならselectedTitleSpriteIndex_のSpriteを編集します。
	bool hasSelectedTitleSprite_ = false;
	// モデル追加直後にInspectorへ選択状態を渡すための残りフレーム数です。
	int inspectorAutoSelectSpriteFrames_ = 0;
	// 3Dモデルを編集するViewの、Cameraや選択状態です。
	SceneEditor::ViewportState viewportEditorState_{};
	// Spriteを編集するViewの、選択状態です。
	SceneEditor::SpriteViewportState spriteViewportEditorState_{};
	// trueならselectedTitleObjectIndex_の3Dモデルを編集します。
	bool hasSelectedTitleObject_ = false;
	// 選択中3Dモデルがアニメーション用一覧にあるかを表します。
	bool selectedTitleObjectIsAnimation_ = false;
	// Inspectorで選択している3Dモデルの番号です。
	size_t selectedTitleObjectIndex_ = 0;
	// モデル追加直後にInspectorへ選択状態を渡すための残りフレーム数です。
	int inspectorAutoSelectModelFrames_ = 0;

	// ---------- Titleの進行状態 ----------

	// タイトル上で動かす代表モデルへの非所有ポインタです。
	Object3d* obj = nullptr;
	// trueになると、SceneManagerがTitleSceneを終了できます。
	bool isFinished_ = false;
};
