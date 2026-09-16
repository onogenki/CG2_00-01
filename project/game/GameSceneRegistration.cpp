#include "GameSceneRegistration.h"

#include "GameSceneRegistry.h"
#include "SceneFactory.h"
#include "Stage1.h"
#include <memory>

namespace {
// Stage1の生成規則をゲーム側へ置き、engine本体が鏡パズルへ依存しないようにします。
void RegisterStage1Scene(SceneFactory& sceneFactory)
{
	sceneFactory.RegisterScene("STAGE1", []() { return std::make_unique<Stage1>(); });
}

// gameがリンクされた時だけ、起動前にStage1の登録処理をengineへ渡します。
const bool isStage1SceneRegistered = []()
{
	GameSceneRegistry::Register(RegisterStage1Scene);
	return true;
}();
}
