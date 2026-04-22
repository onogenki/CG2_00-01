#pragma once

#include"BaseScene.h"
#include<string>

//シーン工場
class AbstractSceneFactory
{
public:

	//仮想デストラクタ
	virtual ~AbstractSceneFactory() = default;
	//シーン生成
	virtual BaseScene* CreateScene(const std::string& sceneName) = 0;

};