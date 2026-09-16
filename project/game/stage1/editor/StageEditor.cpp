#include "StageEditor.h"

#include "Camera.h"
#include "CarryableMirror.h"
#include "FixedMirror.h"
#include "ImGuiManager.h"
#include "Player.h"
#include "StageCameraEvents.h"
#include "StageMapRuntime.h"
#include "../debug/StagePuzzleDebugUi.h"

// Model Shelfの内容を初期化し、Stage1のEdit Viewで使えるようにします。
void StageEditor::Initialize()
{
	levelEditor_.Initialize();
}

// Edit Viewだけで、Collider、Level操作、Viewportの順に編集UIを表示します。
void StageEditor::Draw(Context& context)
{
	if (!context.isEditViewActive) {
		return;
	}

	DrawCollision(context);
	if (!context.autoMapReload || !context.mapReloadStatus || !context.levelData ||
		!context.selectedObjectIndex) {
		return;
	}

	const StageLevelEditor::Context levelEditorContext =
		MakeLevelEditorContext(context);
	DrawLevelControls(context, levelEditorContext);
	DrawViewport(context);
	// Viewportを先に描画してからModel ShelfとDropを処理し、従来のUI順序を維持します。
	levelEditor_.DrawModelShelf(levelEditorContext);
	levelEditor_.HandleShelfDropOnEditView(levelEditorContext);
}

// Collider確認用の表示に必要なStage1所有データを、Debug UI部品へ渡します。
void StageEditor::DrawCollision(const Context& context) const
{
	if (!context.player || !context.floorObb || !context.activeCamera ||
		!context.fixedMirrors || !context.mapRuntime || !context.stageCameraEvents) {
		return;
	}

	StagePuzzleDebugUi::CollisionContext collisionContext{};
	collisionContext.player = context.player;
	collisionContext.floorObb = context.floorObb;
	collisionContext.activeCamera = context.activeCamera;
	collisionContext.fixedMirrors = context.fixedMirrors;
	collisionContext.mirrorFloor = context.mirrorFloor;
	collisionContext.carryableMirror = context.carryableMirror;
	collisionContext.stageMapRuntime = context.mapRuntime;
	collisionContext.stageCameraEvents = context.stageCameraEvents;
	StagePuzzleDebugUi::DrawCollision(collisionContext);
}

// StageLevelEditorへ、Stage1が所有するJSONと保存・再構築操作だけを渡します。
StageLevelEditor::Context StageEditor::MakeLevelEditorContext(Context& context) const
{
	StageLevelEditor::Context levelEditorContext{};
	levelEditorContext.levelData = context.levelData;
	levelEditorContext.playerPosition = context.playerPosition;
	levelEditorContext.selectedObjectIndex = context.selectedObjectIndex;
	levelEditorContext.applyLevelData = context.applyLevelData;
	levelEditorContext.saveLevelData = context.saveLevelData;
	levelEditorContext.setStatus = [&context](const std::string& message)
	{
		if (context.mapReloadStatus) {
			*context.mapReloadStatus = message;
		}
	};
	return levelEditorContext;
}

// JSONの追加・削除・保存と、Lighting編集後の実行中Stageへの反映を行います。
void StageEditor::DrawLevelControls(
	Context& context,
	const StageLevelEditor::Context& levelEditorContext)
{
	auto setStatus = [&context](const std::string& message)
	{
		*context.mapReloadStatus = message;
	};

	const LevelEditorResult levelEditorResult =
		ImGuiManager::GetInstance()->LevelHotReloadWindow(
			*context.autoMapReload,
			context.mapFilePath,
			*context.mapReloadStatus,
			context.levelData,
			*context.selectedObjectIndex);
	if (levelEditorResult.reloadRequested) {
		if (context.reloadMap) {
			context.reloadMap();
		}
	} else {
		if (levelEditorResult.dataChanged && context.applyLevelData &&
			context.applyLevelData(false)) {
			setStatus("Edited in memory. Press Save Map to keep it.");
		}
		if (levelEditorResult.addSphereRequested && levelEditor_.AddSphere(levelEditorContext)) {
			context.saveLevelData();
		}
		if (levelEditorResult.addEventPairRequested && levelEditor_.AddEventPair(levelEditorContext)) {
			context.saveLevelData();
		}
		if (levelEditorResult.addCameraAreaRequested && levelEditor_.AddCameraArea(levelEditorContext)) {
			context.saveLevelData();
		}
		if (levelEditorResult.addPathSphereRequested && levelEditor_.AddPathSphere(levelEditorContext)) {
			context.saveLevelData();
		}
		if (levelEditorResult.removeSelectedRequested && levelEditor_.RemoveSelectedObject(levelEditorContext)) {
			context.saveLevelData();
		}
		if (levelEditorResult.saveRequested && context.saveLevelData) {
			context.saveLevelData();
		}
	}

	if (ImGuiManager::GetInstance()->StageLightingWindow(context.levelData->lighting)) {
		// UIが変えたLightを現在のStageへ反映し、Save MapでJSONへ保存できる状態にします。
		context.levelData->hasLighting = true;
		if (context.applyLevelData && context.applyLevelData(false)) {
			setStatus("Lighting edited in memory. Press Save Map to keep it.");
		}
	}

}

// Viewport専用の選択状態を使い、JSON由来モデルのTransformを編集します。
void StageEditor::DrawViewport(Context& context)
{
	if (!context.levelData || !context.mapRuntime || !context.selectedObjectIndex) {
		return;
	}

	StageEditViewport::Context viewportContext{};
	viewportContext.levelData = context.levelData;
	viewportContext.camera = context.activeCamera;
	viewportContext.floor = context.floor;
	viewportContext.fixedMirrors = context.fixedMirrors;
	viewportContext.mapRuntime = context.mapRuntime;
	viewportContext.selectedObjectIndex = context.selectedObjectIndex;
	viewportContext.applyEdits = [applyLevelData = context.applyLevelData]()
	{
		return applyLevelData && applyLevelData(false);
	};
	viewportContext.setStatus = [&context](const std::string& message)
	{
		if (context.mapReloadStatus) {
			*context.mapReloadStatus = message;
		}
	};
	editViewport_.Draw(viewportContext);
}

// Stage1終了時に、編集UIが保持するModel ShelfとViewportの選択状態を破棄します。
void StageEditor::Finalize()
{
	editViewport_.Finalize();
	levelEditor_.Finalize();
}
