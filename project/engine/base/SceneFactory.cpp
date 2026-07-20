#include "SceneFactory.h"
#include "TitleScene.h"    
#include "GamePlayScene.h"
#include <utility>

SceneFactory::SceneFactory()
{
	// 新しいシーンは生成関数と名前をここへ登録する。
	RegisterScene("TITLE", []() { return std::make_unique<TitleScene>(); });
	RegisterScene("GAMEPLAY", []() { return std::make_unique<GamePlayScene>(); });
}

void SceneFactory::RegisterScene(const std::string& sceneName, SceneCreator creator)
{
	if (creators_.emplace(sceneName, std::move(creator)).second) {
		sceneNames_.push_back(sceneName);
	}
}

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	const auto creator = creators_.find(sceneName);
	return creator != creators_.end() ? creator->second() : nullptr;
}
