#include "LevelLoader.h"
#include <fstream>
#include <cassert>
#include <memory>
#include "../../externals/nlohmann/json.hpp"

std::unique_ptr<LevelLoader::LevelData> LevelLoader::Load(const std::string& fileName)
{
	const std::string fullpath = "resources/levels/" + fileName + ".json";

	//ファイルを開く
	std::ifstream file(fullpath);

	//ファイルオープン失敗をチェック
	if (file.fail()) {
		return nullptr;
	}

	//JSON文字列から解凍したデータ
	nlohmann::json deserialized;

	//解凍
	try {
		file >> deserialized;
	} catch (const nlohmann::json::exception&) {
		return nullptr;
	}

	if (!deserialized.is_object() || !deserialized.contains("name") ||
		deserialized.value("name", "") != "scene" ||
		!deserialized.contains("objects") || !deserialized["objects"].is_array()) {
		return nullptr;
	}

	//正しいレベルエディタファイルかチェック
	assert(deserialized.is_object());
	assert(deserialized.contains("name"));
	assert(deserialized["name"].get<std::string>() == "scene");
	assert(deserialized.contains("objects"));

	//レベルエディタ格納インスタンスを生成
	auto levelData = std::make_unique<LevelData>();

	try {
		LoadObjects(deserialized["objects"], levelData->objects);
	} catch (const nlohmann::json::exception&) {
		return nullptr;
	}

	return levelData;
}

LevelLoader::ObjectData LevelLoader::LoadObject(const nlohmann::json& object)
{
	assert(object.contains("type"));

	ObjectData objectData{};
	objectData.type = object["type"].get<std::string>();

	if (object.contains("name"))
	{
		objectData.name = object["name"].get<std::string>();
	}

	if (object.contains("tag"))
	{
		objectData.tag = object["tag"].get<std::string>();
	}

	if (object.contains("object_type"))
	{
		objectData.objectType = object["object_type"].get<std::string>();
	}

	if (object.contains("file_name"))
	{
		objectData.fileName = object["file_name"].get<std::string>();
	}

	if (object.contains("transform"))
	{
		const nlohmann::json& transform = object["transform"];

		objectData.translation.x = transform["translation"][0].get<float>();
		objectData.translation.y = transform["translation"][2].get<float>();
		objectData.translation.z = transform["translation"][1].get<float>();

		objectData.rotation.x = -transform["rotation"][0].get<float>();
		objectData.rotation.y = -transform["rotation"][2].get<float>();
		objectData.rotation.z = -transform["rotation"][1].get<float>();

		objectData.scaling.x = transform["scaling"][0].get<float>();
		objectData.scaling.y = transform["scaling"][2].get<float>();
		objectData.scaling.z = transform["scaling"][1].get<float>();
	}

	//コライダーのパラメータ読み込み
	if (object.contains("collider"))
	{
		const nlohmann::json& collider = object["collider"];

		objectData.hasCollider = true;
		objectData.collider.type = collider["type"].get<std::string>();

		objectData.collider.center.x = collider["center"][0].get<float>();
		objectData.collider.center.y = collider["center"][2].get<float>();
		objectData.collider.center.z = collider["center"][1].get<float>();

		objectData.collider.size.x = collider["size"][0].get<float>();
		objectData.collider.size.y = collider["size"][2].get<float>();
		objectData.collider.size.z = collider["size"][1].get<float>();
	}

	if (object.contains("children"))
	{
		LoadObjects(object["children"], objectData.children);
	}

	return objectData;
}

//再帰処理
void LevelLoader::LoadObjects(const nlohmann::json& objects, std::vector<ObjectData>& objectList)
{
	for (const nlohmann::json& object : objects)
	{
		objectList.push_back(LoadObject(object));
	}
}
