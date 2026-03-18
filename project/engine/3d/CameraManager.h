#pragma once
#include <string>
#include "Camera.h"
#include <unordered_map>
class CameraManager
{

public:
	//カメラ登録
	void AddCamera(const std::string& name, Camera* camera);
	//使うカメラを切り替える
	void SetActiveCamera(const std::string& name);
	Camera* GetActiveCamera() { return activeCamera_; }
	void Update();

private:
	//文字列とカメラをセットで保存
	std::unordered_map<std::string, Camera*> cameras_;
	Camera* activeCamera_ = nullptr;

};

