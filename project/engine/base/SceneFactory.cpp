#include "SceneFactory.h"
#include "TitleScene.h"    
#include "GamePlayScene.h"
#include <utility>

SceneFactory::SceneFactory()
{
	// 新しいシーンは生成関数と名前をここへ登録する。
	RegisterScene("TITLE", []() { return new TitleScene(); });
	RegisterScene("GAMEPLAY", []() { return new GamePlayScene(); });
}

void SceneFactory::RegisterScene(const std::string& sceneName, SceneCreator creator)
{
	if (creators_.emplace(sceneName, std::move(creator)).second) {
		sceneNames_.push_back(sceneName);
	}
}

BaseScene* SceneFactory::CreateScene(const std::string& sceneName)
{
	const auto creator = creators_.find(sceneName);
	return creator != creators_.end() ? creator->second() : nullptr;
}
