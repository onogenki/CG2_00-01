#pragma once

#include "LevelLoader.h"
#include "StageEditViewport.h"
#include "StageLevelEditor.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Camera;
class CarryableMirror;
class FixedMirror;
class Object3d;
class Player;
class StageCameraEvents;
class StageMapRuntime;

// Stage1のEdit Viewに必要なUI部品をまとめる、編集専用の窓口です。
// Stage1はゲームデータを所有し、このクラスは編集UIを表示して変更操作を依頼します。
class StageEditor
{
public:
	struct Context
	{
		// Edit View以外では何も表示しないための状態です。
		bool isEditViewActive = false;
		// Hot Reloadウィンドウが編集するStage1所有の設定です。
		bool* autoMapReload = nullptr;
		std::string mapFilePath;
		std::string* mapReloadStatus = nullptr;
		// JSONと実行中のStageデータはStage1が所有します。
		LevelLoader::LevelData* levelData = nullptr;
		Vector3 playerPosition{};
		int* selectedObjectIndex = nullptr;
		Player* player = nullptr;
		const MyMath::OBB* floorObb = nullptr;
		Object3d* floor = nullptr;
		Camera* activeCamera = nullptr;
		const std::vector<std::unique_ptr<FixedMirror>>* fixedMirrors = nullptr;
		FixedMirror* mirrorFloor = nullptr;
		CarryableMirror* carryableMirror = nullptr;
		StageMapRuntime* mapRuntime = nullptr;
		const StageCameraEvents* stageCameraEvents = nullptr;
		// 再構築・保存・再読込はStage1の責任なので、必要な操作だけを受け取ります。
		std::function<bool(bool rebuildRuntimeObjects)> applyLevelData;
		std::function<bool()> saveLevelData;
		std::function<void()> reloadMap;
	};

	// Model Shelfを読み、Stage1のEdit Viewで使う準備をします。
	void Initialize();
	// Collider確認、Level編集、Lighting編集、Viewport操作を順番に表示します。
	void Draw(Context& context);
	// Scene終了時に、Model ShelfとViewportの選択状態を破棄します。
	void Finalize();

private:
	// Stage1が所有するColliderを、Edit View上へ確認用ワイヤーとして描画します。
	void DrawCollision(const Context& context) const;
	// StageLevelEditorへ渡す、Stage1所有データと再構築・保存の窓口を作ります。
	StageLevelEditor::Context MakeLevelEditorContext(Context& context) const;
	// JSONの追加・削除・保存・Lighting編集をまとめて処理します。
	void DrawLevelControls(
		Context& context,
		const StageLevelEditor::Context& levelEditorContext);
	// JSON由来のObjectをViewportで選択・移動・回転・拡縮します。
	void DrawViewport(Context& context);
	// Model追加・削除とViewport操作を担当する既存の小さなUI部品です。
	StageLevelEditor levelEditor_{};
	StageEditViewport editViewport_{};
};
