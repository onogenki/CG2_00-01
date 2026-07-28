#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Vector3.h"
#include "../../externals/nlohmann/json.hpp"

// Blenderのレベルエディタが出力したJSONを、ゲームで扱うレベルデータへ変換する。
class LevelLoader
{

public:
	struct ColliderData
	{
		// コライダー種別とローカル座標系での形状情報。
		std::string type;
		Vector3 center;
		Vector3 size;
	};

	struct ObjectData
	{
		// オブジェクト本体、Transform、子オブジェクトをまとめた再帰データ。
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
		// シーン直下に置かれたオブジェクト一覧。
		std::vector<ObjectData> objects;
	};

	// resources/levels配下のJSONを読み込み、失敗時はnullptrを返す。
	static std::unique_ptr<LevelData> Load(const std::string& fileName);

	// JSONオブジェクト1件を再帰的にObjectDataへ変換する。
	static ObjectData LoadObject(const nlohmann::json& object);
	// JSON配列内のオブジェクトをObjectDataの配列へ追加する。
	static void LoadObjects(const nlohmann::json& objects, std::vector<ObjectData>& objectList);

};
