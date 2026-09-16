#pragma once
#include "Object3d.h"
#include "BaseScene.h"
#include "TitleEditor.h"
#include <memory>
#include <string>
#include <vector>

class SkyBox;
class Sprite;

class TitleScene : public BaseScene
{
public:
	// 前方宣言した所有型を安全に扱うため、実装はTitleScene.cppに置きます。
	TitleScene();
	// unique_ptrが前方宣言したTitle用型を安全に解放できるよう、実装はTitleScene.cppに置きます。
	~TitleScene() override;
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
	// ---------- 初期化の補助関数 ----------

	// DirectX・Camera・Object3dを、TitleSceneが使える初期状態へそろえます。
	void InitializeRenderSystems();
	// JSONを使わないTitle画面用の標準照明を設定します。
	void InitializeDefaultLighting();
	// 背景の平面モデルと初期Spriteを作成します。
	bool InitializeTitleObjects();
	// SkyBoxとTitle開始時の音声を準備します。
	void InitializeSkyBoxAndAudio();
	// Camera・Light・3Dモデル・Sprite・SkyBoxを、このフレームの状態へ更新します。
	void UpdateSceneContent();
	// Title専用の編集UIを更新します。
	void UpdateEditorUi(bool isGameViewActive);
	// TitleからDebugまたはStage1へ移る入力を確認します。
	void UpdateSceneTransition(bool isGameViewActive);

	// ---------- Title固有の生成ルール ----------

	// 指定した3DモデルをTitleの通常モデル一覧へ追加します。
	bool AddModelToTitle(const std::string& fileName);
	// 指定したTextureをTitleのSprite一覧へ追加します。
	bool AddTextureToTitle(const std::string& textureFilePath);
	// Edit Viewから追加したTitleモデル・Spriteだけを削除します。
	void ClearAddedTitleObjects();
	// TitleEditorへ渡す、TitleScene所有データと追加・削除操作の窓口を作ります。
	TitleEditor::Context MakeTitleEditorContext();

	// ---------- 空・3Dモデル・Sprite ----------

	// Titleの背景として描画するSkyBoxです。
	std::unique_ptr<SkyBox> skyBox_;

	// アニメーションを使わないTitle用の3Dモデルです。
	std::vector<std::unique_ptr<Object3d>> normalObjects_;
	// アニメーション再生を行うTitle用の3Dモデルです。
	std::vector<std::unique_ptr<Object3d>> animationObjects_;
	// Title上へ追加した2D画像です。
	std::vector<std::unique_ptr<Sprite>> addedSprites_;
	// TitleのModel Shelf・Inspector・Edit Viewを担当するUI部品です。
	TitleEditor titleEditor_{};

	// ---------- Title全体のライト ----------

	// 太陽光のように、Title全体へ同じ方向から当てる光です。
	Object3d::DirectionalLight directionalLight_{};
	// 指定した一点から周囲へ広がる光です。
	Object3d::PointLight pointLight_{};
	// 円すい状の範囲だけを照らす光です。
	Object3d::SpotLight spotLight_{};

	// ---------- Edit Viewの削除境界 ----------

	// Initialize時からあるモデル数です。追加分だけを消す境界に使います。
	size_t baseNormalObjectCount_ = 0;
	// Initialize時からあるAnimationモデル数です。追加分だけを消す境界に使います。
	size_t baseAnimationObjectCount_ = 0;
	//最初からタイトルに置くスプライトの数です。追加したスプライトだけを削除できるようにします。
	size_t baseSpriteCount_ = 0;
	// ---------- Titleの進行状態 ----------
	// trueになると、SceneManagerがTitleSceneを終了できます。
	bool isFinished_ = false;
};
