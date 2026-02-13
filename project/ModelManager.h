#pragma once
#include "Model.h"
#include "ModelCommon.h"
#include "DirectXCommon.h"
#include <map>
#include <string>
#include <memory>
//テクスチャマネージャー
class ModelManager
{
public:
	//シングルインスタンスの取得
	static ModelManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon);

	//終了
	void Finalize();

	//モデルファイルの読み込み
	void LoadModel(const std::string& filePath);

	//モデルファイルパス
	Model* FindModel(const std::string& filePath);

private:
	static ModelManager* instance;

	ModelManager() = default;
	~ModelManager();
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator =(const ModelManager&) = delete;

	ModelCommon* modelCommon = nullptr;

	//モデルデータ
	std::map<std::string, std::unique_ptr<Model>>models;
};

