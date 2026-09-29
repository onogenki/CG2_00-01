#pragma once
#include "Object3d.h"
#include "BaseScene.h"
#include "TitleEditor.h"
#include "TitleObjectManager.h"
#include <memory>

class SkyBox;

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
	// SkyBox Textureが読めた時だけ、SkyBoxとEditor用Resource一覧を準備します。
	bool InitializeSkyBoxAndEditorResources();
	// Camera・Light・3Dモデル・Sprite・SkyBoxを、このフレームの状態へ更新します。
	void UpdateSceneContent();
	// Title専用の編集UIを更新します。
	void UpdateEditorUi(bool isGameViewActive);
	// TitleからDebugまたはStage1へ移る入力を確認します。
	void UpdateSceneTransition(bool isGameViewActive);

	// TitleEditorへManager所有の一覧を貸し、追加・削除操作を結び付けます。
	TitleEditor::Context MakeTitleEditorContext();

	// ---------- 空・3Dモデル・Sprite ----------

	// Titleの背景として描画するSkyBoxです。
	std::unique_ptr<SkyBox> skyBox_;
	// モデル・Spriteの生成と寿命はManagerが所有し、Sceneは更新・描画の順番を決めます。
	TitleObjectManager titleObjects_{};
	// TitleのModel Shelf・Inspector・Edit Viewを担当するUI部品です。
	TitleEditor titleEditor_{};

	// ---------- Title全体のライト ----------

	// 太陽光のように、Title全体へ同じ方向から当てる光です。
	Object3d::DirectionalLight directionalLight_{};
	// 指定した一点から周囲へ広がる光です。
	Object3d::PointLight pointLight_{};
	// 円すい状の範囲だけを照らす光です。
	Object3d::SpotLight spotLight_{};

	// ---------- Titleの進行状態 ----------
	// trueになると、SceneManagerがTitleSceneを終了できます。
	bool isFinished_ = false;
	// 必須モデル・画像・SkyBoxがそろわない時は、未生成の描画物を更新しません。
	bool isInitialized_ = false;
};
