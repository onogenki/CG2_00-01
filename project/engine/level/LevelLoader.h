#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Vector3.h"
#include "../../externals/nlohmann/json.hpp"

class LevelLoader
{

public:
	struct ColliderData
	{
		std::string type;
		Vector3 center;
		Vector3 size;
	};

	struct ObjectData
	{
		std::string type;
		std::string name;
		std::string tag;
		std::string objectType;
		std::string fileName;
		Vector3 translation{};
		Vector3 rotation{};
		Vector3 scaling{ 1.0f, 1.0f, 1.0f };
		bool hasCollider = false;
		ColliderData collider;
		std::vector<ObjectData> children;
	};

	struct LevelData
	{
		std::vector<ObjectData> objects;
	};

	static std::unique_ptr<LevelData> Load(const std::string& fileName);

	static ObjectData LoadObject(const nlohmann::json& object);
	static void LoadObjects(const nlohmann::json& objects, std::vector<ObjectData>& objectList);

};
