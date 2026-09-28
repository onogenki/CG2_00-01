#pragma once

#include "LevelLoader.h"
#include <memory>
#include <vector>

class FixedMirror;
class Object3dCommon;

// Levelの配置データを、固定Mirrorの生成・編集へ変換するゲーム側のFactoryです。
// JSONの読込はLevelLoader、鏡の描画とColliderはFixedMirror、一覧の所有はSceneが担当します。
class StageMirrorFactory
{
public:
	// データ順に鏡を生成します。出力はSceneが用意する仮一覧で、成功後に本番一覧へ移します。
	// 失敗時はfalseを返します。出力に生成済みの鏡が残るため、本番一覧には渡さないでください。
	static bool CreateFixedMirrors(
		Object3dCommon* object3dCommon,
		const std::vector<const LevelLoader::ObjectData*>& mirrorDataList,
		std::vector<std::unique_ptr<FixedMirror>>& outFixedMirrors);
	// 同じ枚数の既存Mirrorへ位置・Y回転・サイズ・Colliderを反映します。GPU資源は作り直しません。
	// Sceneは枚数の変更やモデル再読込が必要な場合、こちらではなくCreateFixedMirrorsを使います。
	static bool ApplyFixedMirrorEdits(
		const std::vector<const LevelLoader::ObjectData*>& mirrorDataList,
		std::vector<std::unique_ptr<FixedMirror>>& fixedMirrors);
};
