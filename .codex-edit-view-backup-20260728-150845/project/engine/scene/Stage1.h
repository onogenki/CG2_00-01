#pragma once

#include "BaseScene.h"
#include "CameraController.h"
#include "FileHotReload.h"
#include "LevelLoader.h"
#include "Mirror.h"
#include "Player.h"
#include <memory>
#include <string>
#include <vector>

using namespace MyMath;

// 鏡の機能を段階ごとに作成・確認するための小さなテスト用シーンです。
class Stage1 : public BaseScene {
public:
	// シーンの開始時に、カメラ・照明・テスト用モデルを作成します。
	void Initialize() override;
	// シーンを終了するときに、Stage1 が所有するモデルを解放します。
	void Finalize() override;
	// 毎フレーム、デバッグ UI とモデルの座標を更新します。
	void Update() override;
	// 毎フレーム、部屋・球・鏡の板を画面に描画します。
	void Draw() override;

private:
	// JSON上の追加オブジェクトと、実行中のObject3d・コライダーを結び付けます。
	struct StageMapRuntimeObject
	{
		std::string sourceName;
		std::unique_ptr<Object3d> visual;
		Vector3 colliderLocalCenter{};
		Vector3 colliderLocalHalfSize{};
		OBB collider{};
		bool hasBoxCollider = false;
		Vector3 pathBasePosition{};
		std::vector<Vector3> controlPoints;
		float pathSpeed = 1.0f;
		float pathProgress = 0.0f;
		bool pathLoop = true;
	};

	// Playerが入った時にイベントカメラを起動する、描画されないBOXです。
	struct StageEventTrigger
	{
		std::string sourceName;
		std::string eventId;
		std::string eventCameraName;
		Transform transform{};
		Vector3 colliderLocalCenter{};
		Vector3 colliderLocalHalfSize{ 1.0f, 1.0f, 1.0f };
		OBB collider{};
		bool isPlayerInside = false;
	};

	// イベント中だけ使用するCameraと、カメラが見る注視点です。
	struct StageEventCamera
	{
		std::string sourceName;
		std::unique_ptr<Camera> camera;
		Vector3 focus{};
		bool hasFocus = false;
	};

	// 指定したモデルを使う 3D オブジェクトを作成し、照明を設定します。
	std::unique_ptr<Object3d> CreateObject(const std::string& modelName);
	// カメラと照明を渡してから、3D オブジェクトの行列を更新します。
	void UpdateObject(Object3d& object);
	// Mirror のデータを、画面に表示する鏡の板へ反映します。
	void SyncMirrorVisual();
	// 通常カメラを鏡面で反転し、反射カメラの位置と回転を更新します。
	void UpdateReflectionCamera();
	//プレイヤーを追従し、右マウス操作で周回できる通常カメラを更新します。
	void UpdateMainCamera();
	// カメラが現在見ている正面方向を取得します。
	Vector3 GetCameraForward(const Camera& camera) const;
	// ImGuiManager に鏡の設定用 UI の表示を依頼し、変更を板へ反映します。
	void DrawMirrorDebugUi();
	//床のOBBを、衝突状態に応じた色のワイヤーで表示します。
	void DrawCollisionDebugUi();
	// Stage1の外部マップファイルが保存されたかを確認します。
	void UpdateStageMapHotReload();
	// Stage1の外部マップファイルを読み、成功した場合だけ床と鏡へ反映します。
	bool ReloadStageMap();
	// ゲーム内で編集したLevelDataをJSONへ保存します。
	bool SaveStageMap();
	// 読み込んだLevelDataを床・鏡・追加オブジェクトへ反映します。
	bool ApplyStageMapData(bool rebuildRuntimeObjects);
	// ワンボタン配置用のsphere.objデータをLevelDataへ追加します。
	bool AddStageMapSphere();
	// 選択中の追加オブジェクトをLevelDataから削除します。
	bool RemoveSelectedStageMapObject();
	// イベントトリガーとイベントカメラを一組でLevelDataへ追加します。
	bool AddStageMapEventPair();
	// Playerとイベントトリガーを判定し、使用するCameraを切り替えます。
	void UpdateStageEvents();
	// EventCameraの位置と注視点からCameraの回転を更新します。
	void UpdateStageEventCamera(StageEventCamera& eventCamera, const LevelLoader::ObjectData& objectData);
	// 制御点を持つ追加オブジェクトを、曲線上で毎フレーム移動させます。
	void UpdateStageMapPaths(float deltaTime);
	// 制御点移動を設定したsphere.objをLevelDataへ追加します。
	bool AddStageMapPathSphere();

	// 床と球です。後で鏡へ映す対象にもなります。
	std::vector<std::unique_ptr<Object3d>> sceneObjects_;
	// 鏡の見た目を描画する、一枚の平面モデルです。
	std::unique_ptr<Object3d> mirrorVisual_;
	// 鏡の中心・正面・大きさを管理するデータです。
	Mirror mirror_;
	//鏡のローカル座標における半分の大きさ
	Vector3 mirrorLocalHalfSize_{ 1.0f,1.0f,0.05f };
	//鏡のローカル座標におけるコライダー中心です。
	Vector3 mirrorColliderLocalCenter_{};
	//鏡の見た目と一致する衝突判定用の薄いOBBです。
	OBB mirrorObb_{};
	// 鏡の向こう側から部屋を見るための、二台目のカメラです。
	std::unique_ptr<Camera> reflectionCamera_;
	//WASD移動とジャンプを行う球のプレイヤーです。
	std::unique_ptr<Player> player_;
	//見た目とOBBを共有する床モデルです。
	Object3d* floor_ = nullptr;
	//floor.objのローカル座標における半分の大きさです。
	Vector3 floorLocalHalfSize_{ 10.0f, 1.500001f, 10.0f };
	//floor.objのローカル座標におけるコライダー中心です。
	Vector3 floorColliderLocalCenter_{};
	//毎フレーム、床モデルのTransformから作る衝突判定用OBBです。
	OBB floorObb_{};
	// Player などの対象を追従する、三人称カメラ専用の操作役です。
	std::unique_ptr<CameraController> cameraController_;
	// Stage1の外部マップファイルが保存された瞬間を検出します。
	FileHotReload stageMapHotReload_;
	// Stage1のJSONをゲーム内で編集できる形で保持します。
	std::unique_ptr<LevelLoader::LevelData> stageMapData_;
	// JSONから生成した、床と鏡以外の追加オブジェクトです。
	std::vector<StageMapRuntimeObject> stageMapRuntimeObjects_;
	// JSONから生成したイベントトリガーです。
	std::vector<StageEventTrigger> stageEventTriggers_;
	// JSONから生成したイベント専用カメラです。
	std::vector<StageEventCamera> stageEventCameras_;
	// 現在起動中のイベントカメラ名です。空文字なら通常カメラです。
	std::string activeEventCameraName_;
	// Hot Reloadウィンドウで選択しているオブジェクト番号です。
	int selectedStageMapObjectIndex_ = 0;
	// ファイル保存時に自動で再読込するかを切り替えます。
	bool autoStageMapReload_ = true;
	// ImGuiへ表示する、直近の再読込結果です。
	std::string stageMapReloadStatus_ = "Not loaded yet.";
	// Stage1 では、壁の鏡をまず簡単に回せるよう、Y 軸回転だけを編集します。
	float mirrorYaw_ = 3.14159265f;
	// Stage1 内の全モデルで共有する平行光源です。
	Object3d::DirectionalLight directionalLight_{};
	// Stage1 内の全モデルで共有する点光源です。
	Object3d::PointLight pointLight_{};
};
