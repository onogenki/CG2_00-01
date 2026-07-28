#include "CameraManager.h"

void CameraManager::AddCamera(const std::string& name, Camera* camera)
{
	//カメラを書き込む
	cameras_[name] = camera;
	//現在使用中のカメラが決まってなければ
	if (activeCamera_ == nullptr)
	{//とりあえずメインカメラに設定
		activeCamera_ = camera;
	}

}

void CameraManager::SetActiveCamera(const std::string& name)
{
	//カメラの中から指定された名前を探す
	auto it = cameras_.find(name);
	
	//もし見つからなかったら
	if (it != cameras_.end())
	{//見つかったカメラを今のアクティブカメラに設定
		activeCamera_ = it->second;
	}
}

void CameraManager::Update()
{
	//全てのカメラを順番に取り出す
	for (auto& pair : cameras_)
	{
		pair.second->Update();
	}
}

CameraManager::~CameraManager()
{
	// コンテナの中身も空にしておく（安全のため）
	cameras_.clear();
}
