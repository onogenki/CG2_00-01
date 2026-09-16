#include "DebugUiSmoke.h"

#include "GameViewCapture.h"
#include "Object3d.h"
#include <Windows.h>
#include <algorithm>
#include <fstream>

// 環境変数を確認済みのDebugSceneから呼ばれ、UI自動確認の初期状態を作ります。
void DebugUiSmoke::Start(State& state, const std::string& timestamp)
{
	state.isEnabled = true;
	state.isFinished = false;
	state.isPendingCapture = false;
	state.frame = 0;
	state.stage = 0;
	state.modelFile.clear();

	std::error_code errorCode;
	std::filesystem::create_directories("logs", errorCode);
	state.logPath = std::filesystem::path("logs") / ("gameplay_ui_smoke_" + timestamp + ".log");
}

// モデル棚・プレビュー・Inspectorの自動確認を、画面初期化を待ちながら一段階ずつ進めます。
void DebugUiSmoke::Update(State& state, const Context& context)
{
	if (!state.isEnabled || state.isFinished) {
		return;
	}
	if (!context.modelLibrary || !context.normalObjects || !context.animationObjects || !context.sprites ||
		!context.getPreviewState || !context.getSelectionState ||
		!context.enterModelPreview || !context.enterTexturePreview || !context.resetModelPreviewCamera ||
		!context.exitModelPreview || !context.addModel || !context.addTexture ||
		!context.clearAddedSceneModels || !context.buildWorldAabb || !context.gameViewCapture) {
		Finish(state, false, "Required Debug UI test context was not initialized.");
		return;
	}

	++state.frame;
	if (state.frame < 6) {
		return;
	}

	if (state.stage == 0) {
		const auto modelIt = std::find_if(
			context.modelLibrary->begin(),
			context.modelLibrary->end(),
			[](const SceneEditor::ShelfEntry& entry)
			{
				return entry.canLoad && !entry.hasAnimation && !entry.isTexture;
			});
		if (modelIt == context.modelLibrary->end()) {
			Finish(state, false, "No loadable non-animation model was found in resources.");
			return;
		}

		state.modelFile = modelIt->fileName;
		if (!context.enterModelPreview(state.modelFile)) {
			Finish(state, false, "EnterModelPreview failed for " + state.modelFile);
			return;
		}
		const PreviewState previewState = context.getPreviewState();
		if (!previewState.isModelPreviewMode || !previewState.previewObject) {
			Finish(state, false, "Preview mode did not become active.");
			return;
		}

		MyMath::AABB previewAabb{};
		if (!context.buildWorldAabb(*previewState.previewObject, previewAabb)) {
			Finish(state, false, "Preview AABB could not be built.");
			return;
		}

		state.stage = 1;
		state.frame = 0;
		return;
	}

	if (state.stage != 1) {
		return;
	}

	context.resetModelPreviewCamera();
	context.exitModelPreview();
	const PreviewState modelPreviewState = context.getPreviewState();
	if (modelPreviewState.isModelPreviewMode || modelPreviewState.previewObject) {
		Finish(state, false, "ExitModelPreview did not leave preview mode.");
		return;
	}

	const size_t normalBefore = context.normalObjects->size();
	const size_t animationBefore = context.animationObjects->size();
	if (!context.addModel(state.modelFile)) {
		Finish(state, false, "AddModelToScene failed for " + state.modelFile);
		return;
	}
	if (context.normalObjects->size() != normalBefore + 1 ||
		context.animationObjects->size() != animationBefore) {
		Finish(state, false, "Non-animation model did not add to the normal model list.");
		return;
	}

	Object3d* addedObject = context.normalObjects->back().get();
	const size_t addedObjectIndex = context.normalObjects->size() - 1;
	MyMath::AABB addedAabb{};
	if (!addedObject || !context.buildWorldAabb(*addedObject, addedAabb)) {
		Finish(state, false, "Added model AABB could not be built.");
		return;
	}
	if (addedObject->GetModelName() != state.modelFile) {
		Finish(state, false, "Added model name was not reflected on the inspector target.");
		return;
	}
	if (addedObjectIndex < context.baseNormalObjectCount) {
		Finish(state, false, "Added model was not placed in the editable inspector range.");
		return;
	}
	const SelectionState modelSelectionState = context.getSelectionState();
	if (!modelSelectionState.hasSelectedObject || modelSelectionState.selectedObjectIsAnimation ||
		modelSelectionState.selectedObjectIndex != addedObjectIndex) {
		Finish(state, false, "Added normal model was not selected for inspector/gizmo editing.");
		return;
	}

	const auto textureIt = std::find_if(
		context.modelLibrary->begin(),
		context.modelLibrary->end(),
		[](const SceneEditor::ShelfEntry& entry)
		{
			return entry.isTexture;
		});
	if (textureIt == context.modelLibrary->end()) {
		Finish(state, false, "No 2D Texture was found in resources.");
		return;
	}
	if (!context.enterTexturePreview(textureIt->textureFilePath)) {
		Finish(state, false, "EnterTexturePreview failed for " + textureIt->textureFilePath);
		return;
	}
	const PreviewState texturePreviewState = context.getPreviewState();
	if (!texturePreviewState.isModelPreviewMode || !texturePreviewState.isTexturePreviewMode ||
		!texturePreviewState.previewSprite) {
		Finish(state, false, "2D Texture preview mode did not become active.");
		return;
	}
	context.resetModelPreviewCamera();
	context.exitModelPreview();
	const PreviewState exitedTexturePreviewState = context.getPreviewState();
	if (exitedTexturePreviewState.isModelPreviewMode || exitedTexturePreviewState.isTexturePreviewMode ||
		exitedTexturePreviewState.previewSprite) {
		Finish(state, false, "ExitModelPreview did not leave 2D Texture preview mode.");
		return;
	}

	const size_t spriteBefore = context.sprites->size();
	if (!context.addTexture(textureIt->textureFilePath)) {
		Finish(state, false, "AddTextureToScene failed for " + textureIt->textureFilePath);
		return;
	}
	const SelectionState spriteSelectionState = context.getSelectionState();
	if (context.sprites->size() != spriteBefore + 1 || !spriteSelectionState.hasSelectedSprite ||
		spriteSelectionState.selectedSpriteIndex != context.sprites->size() - 1 ||
		spriteSelectionState.hasSelectedObject) {
		Finish(state, false, "Added 2D Texture was not selected for Sprite inspector/gizmo editing.");
		return;
	}

	context.clearAddedSceneModels();
	if (context.normalObjects->size() != context.baseNormalObjectCount ||
		context.animationObjects->size() != context.baseAnimationObjectCount ||
		context.sprites->size() != context.baseSpriteCount) {
		Finish(state, false, "ClearAddedSceneModels did not restore base counts.");
		return;
	}

	state.isPendingCapture = true;
	state.stage = 2;
	state.frame = 0;
}

// Draw済みのGame Viewを保存し、作成結果まで確認してUI自動確認を終了します。
void DebugUiSmoke::UpdateAfterDraw(State& state, const Context& context)
{
	if (!state.isEnabled || state.isFinished || !state.isPendingCapture) {
		return;
	}
	if (!context.gameViewCapture) {
		Finish(state, false, "Game View capture was not initialized.");
		return;
	}

	std::vector<unsigned char> pixels;
	int width = 0;
	int height = 0;
	if (!context.gameViewCapture->CapturePixels(pixels, width, height)) {
		Finish(state, false, "CapturePixels failed.");
		return;
	}

	const std::filesystem::path screenshotPath =
		context.gameViewCapture->GetCaptureDirectory("Screenshots") /
		("CG2_ui_smoke_" + context.gameViewCapture->MakeTimestampString() + ".bmp");
	if (!context.gameViewCapture->SavePixelsAsBmp(screenshotPath, pixels, width, height)) {
		Finish(state, false, "SavePixelsAsBmp failed: " + screenshotPath.string());
		return;
	}

	const std::filesystem::path videoPath =
		context.gameViewCapture->GetCaptureDirectory("Videos") /
		("CG2_ui_smoke_" + context.gameViewCapture->MakeTimestampString() + ".avi");
	if (!context.gameViewCapture->SaveSingleFrameAvi(videoPath, pixels, width, height)) {
		Finish(state, false, "SaveSingleFrameAvi failed: " + videoPath.string());
		return;
	}

	std::error_code errorCode;
	const bool screenshotExists = std::filesystem::exists(screenshotPath, errorCode) && !errorCode;
	errorCode.clear();
	const bool videoExists = std::filesystem::exists(videoPath, errorCode) && !errorCode;
	if (!screenshotExists || !videoExists) {
		Finish(state, false, "Capture files were not created.");
		return;
	}

	state.isPendingCapture = false;
	Finish(state, true, "OK model=" + state.modelFile +
		" inspector=editable" +
		" screenshot=" + screenshotPath.string() +
		" video=" + videoPath.string());
}

// UI自動確認が有効な間だけtrueを返し、通常のキャプチャ更新との競合を防ぎます。
bool DebugUiSmoke::IsEnabled(const State& state)
{
	return state.isEnabled;
}

// 成功・失敗をログへ記録し、Smoke Test専用の実行を終了します。
void DebugUiSmoke::Finish(State& state, bool success, const std::string& message)
{
	if (state.isFinished) {
		return;
	}

	state.isFinished = true;
	std::ofstream log(state.logPath, std::ios::app);
	if (log) {
		log << (success ? "SUCCESS: " : "FAILURE: ") << message << '\n';
	}
	PostQuitMessage(success ? 0 : 1);
}
