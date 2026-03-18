#include "ModelManager.h"

ModelManager* ModelManager::instance = nullptr;

ModelManager* ModelManager::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new ModelManager();
	}
	return instance;
}

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
	modelCommon = new ModelCommon;
	modelCommon->Initialize(dxCommon);
}

void ModelManager::Finalize()
{
	delete instance;
	instance = nullptr;
}

void ModelManager::LoadModel(const std::string& filePath)
{
	if (models.contains(filePath))
	{//読み込み済みなら早期return
		return;
	}

	//モデルと生成とファイル読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->Initialize(modelCommon, "resources", filePath);

	//モデルをmapコンテナに格納する
	models.insert(std::make_pair(filePath, std::move(model)));

}

Model* ModelManager::FindModel(const std::string& filePath)
{//読み込み済みならモデルを検索
	if (models.contains(filePath))
	{//読み込みモデルを戻り値としてreturn
		return models.at(filePath).get();
	}
	//ファイル名一致なし
	return nullptr;
}

ModelManager::~ModelManager()
{
	// モデル共通部の解放
	if (modelCommon) {
		delete modelCommon;
		modelCommon = nullptr;
	}
	// map内のunique_ptrは自動的に解放されるため、明示的なdeleteは不要
}