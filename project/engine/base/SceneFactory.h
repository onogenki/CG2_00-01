#pragma once

#include"AbstractSceneFactory.h"
#include <functional>
#include <unordered_map>
#include <vector>

//このゲーム用のシーン工場
class SceneFactory : public AbstractSceneFactory
{
public:
	SceneFactory();

	BaseScene* CreateScene(const std::string& sceneName)override;
	const std::vector<std::string>& GetSceneNames() const override { return sceneNames_; }

private:
	using SceneCreator = std::function<BaseScene*()>;
	void RegisterScene(const std::string& sceneName, SceneCreator creator);
	std::unordered_map<std::string, SceneCreator> creators_;
	std::vector<std::string> sceneNames_;
};

