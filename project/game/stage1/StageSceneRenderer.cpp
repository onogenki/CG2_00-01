#include "StageSceneRenderer.h"

#include "Camera.h"
#include "CarryableMirror.h"
#include "FixedMirror.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Player.h"
#include "SceneRenderPipeline.h"
#include "StageHazardLights.h"
#include "StageLightPuzzle.h"

// 本編の通常描画対象を、Mirror Surfaceが最後に重なる順番で表示します。
void StageSceneRenderer::Draw(const Context& context)
{
	if (!context.object3dCommon || !context.activeCamera || !context.sceneObjects ||
		!context.stageMapRuntime || !context.fixedMirrors || !context.lightPuzzle ||
		!context.hazardLights) {
		return;
	}

	SceneRenderPipeline::DrawObjects(context.object3dCommon, *context.sceneObjects);
	context.stageMapRuntime->Draw();
	for (const std::unique_ptr<FixedMirror>& fixedMirror : *context.fixedMirrors) {
		if (!fixedMirror) {
			continue;
		}
		fixedMirror->DrawSurface(*context.activeCamera);
		// Mirror用Pipelineの後、通常Object用Pipelineを次の描画へ戻します。
		context.object3dCommon->SetCommonDrawSetting();
	}
	if (context.mirrorFloor) {
		context.mirrorFloor->DrawSurface(*context.activeCamera);
		context.object3dCommon->SetCommonDrawSetting();
	}
	if (context.carryableMirror) {
		context.carryableMirror->GetObject().Draw();
	}
	if (context.player) {
		context.player->GetObject().Draw();
		// モンスターボールの水色と混ざらない桃色で、壁の奥にいるPlayerだけを見せます。
		context.player->GetObject().DrawOccludedSilhouette({ 1.00f, 0.18f, 0.62f, 0.72f });
	}
	// Laserの線は壁の向こうへ描かず、発射位置だけを黄色で見えるようにします。
	// 将来発射装置Modelへ差し替えても、同じObject3dの呼び出しで利用できます。
	if (context.laserEmitter) {
		context.laserEmitter->DrawOccludedSilhouette({ 1.00f, 0.88f, 0.12f, 0.88f });
	}
	if (context.doorLaserEmitter) {
		context.doorLaserEmitter->DrawOccludedSilhouette({ 1.00f, 0.48f, 0.08f, 0.88f });
	}
	// 後続Objectが通常Pipelineを前提にできるよう、描画設定を戻します。
	context.object3dCommon->SetCommonDrawSetting();
	context.lightPuzzle->Draw(*context.activeCamera);
	context.hazardLights->Draw(*context.activeCamera);
}
