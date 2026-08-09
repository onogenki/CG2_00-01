#pragma once

#include "BaseScene.h"
#include "CameraController.h"
#include "CarryableMirror.h"
#include "FileHotReload.h"
#include "FixedMirror.h"
#include "LevelLoader.h"
#include "Laser.h"
#include "LaserRenderer.h"
#include "Player.h"
#include "SceneEditor.h"
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
		std::unique_ptr<CameraController> manualController;
		Vector3 focus{};
		bool hasFocus = false;
	};

	// 狭い通路やボス部屋などで、通常Cameraの設定だけを切り替える描画されないBOXです。
	struct StageCameraArea
	{
		std::string sourceName;
		Transform transform{};
		Vector3 colliderLocalCenter{};
		Vector3 colliderLocalHalfSize{ 1.0f, 1.0f, 1.0f };
		OBB collider{};
		CameraAreaSettings settings{};
		bool isPlayerInside = false;
	};

	// 指定したモデルを使う 3D オブジェクトを作成し、照明を設定します。
	std::unique_ptr<Object3d> CreateObject(const std::string& modelName);
	// カメラと照明を渡してから、3D オブジェクトの行列を更新します。
	void UpdateObject(Object3d& object);
	// Mirror のデータを、画面に表示する鏡の板へ反映します。
	void UpdateReflectionCameras();
	// 通常カメラを鏡面で反転し、反射カメラの位置と回転を更新します。
	// 反射CameraでSceneを鏡専用Textureへ一度だけ描画します。
	void DrawFixedMirrorReflections();
	// 反射描画後、Object3dの行列を現在のGame Camera用へ戻します。
	void RestoreSceneCameraMatrices();
	// 持てる鏡の拾う・置く処理と、全鏡を使うレーザー経路を更新します。
	void UpdateMirrorGameplay();
	// Playerを自動追尾する通常Cameraを更新します。
	void UpdateMainCamera();
	// Event Cameraゾーン内だけ、右マウスで操作できる手動Cameraを更新します。
	void UpdateEventManualCamera();
	// カメラが現在見ている正面方向を取得します。
	Vector3 GetCameraForward(const Camera& camera) const;
	// ImGuiManager に鏡の設定用 UI の表示を依頼し、変更を板へ反映します。
	void DrawMirrorDebugUi();
	//床のOBBを、衝突状態に応じた色のワイヤーで表示します。
	void DrawCollisionDebugUi();
	// Edit Viewで床・鏡・追加モデルをクリック編集し、LevelDataへ同期します。
	void DrawStageEditViewport();
	// Edit View下部へ共通モデル棚を表示し、Stage1へモデルを追加します。
	void DrawStageModelShelf();
	void HandleStageShelfDropOnEditView();
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
	bool AddStageMapModel(const std::string& fileName);
	void ClearStageMapEditorAddedObjects();
	// 選択中の追加オブジェクトをLevelDataから削除します。
	bool RemoveSelectedStageMapObject();
	// イベントトリガーとイベントカメラを一組でLevelDataへ追加します。
	bool AddStageMapEventPair();
	// 通常Cameraの距離・角度・視野角を切り替えるAreaをLevelDataへ追加します。
	bool AddStageMapCameraArea();
	// Playerとイベントトリガーを判定し、使用するCameraを切り替えます。
	void UpdateStageEvents();
	// EventCameraの位置と注視点からCameraの回転を更新します。
	void UpdateStageEventCamera(StageEventCamera& eventCamera, const LevelLoader::ObjectData& objectData);
	// Playerが入っているCamera Areaを調べ、通常Cameraの設定を切り替えます。
	void UpdateCameraAreas();
	// 制御点を持つ追加オブジェクトを、曲線上で毎フレーム移動させます。
	void UpdateStageMapPaths(float deltaTime);
	// 制御点移動を設定したsphere.objをLevelDataへ追加します。
	bool AddStageMapPathSphere();
	// 環境変数で起動した時だけ、携帯鏡・レーザー・床落下を自動検証します。
	void InitializeGameplaySmoke();
	void UpdateGameplaySmoke(float deltaTime);

	// 床と球です。後で鏡へ映す対象にもなります。
	std::vector<std::unique_ptr<Object3d>> sceneObjects_;
	// 大型の固定鏡です。複数枚それぞれが反射Cameraと専用Textureを持ちます。
	std::vector<std::unique_ptr<FixedMirror>> fixedMirrors_;
	// 複数の固定鏡を一枚ずつ順番に更新するための番号です。
	size_t reflectionUpdateCursor_ = 0;
	// 景色は映さず、PlayerがEキーで持ち運べるレーザー反射用の小型鏡です。
	std::unique_ptr<CarryableMirror> carryableMirror_;
	// 固定鏡と小型鏡の両方で反射するテスト用レーザーです。
	Laser laser_;
	std::unique_ptr<LaserRenderer> laserRenderer_;
	// 光の発射位置を見分けるために置く、小さな白い球です。
	Object3d* laserEmitter_ = nullptr;
	// 初期状態のPlayer正面から光を当て、正面に構えた携帯鏡で受けられる位置にする。
	Vector3 laserOrigin_{ 0.0f, 1.0f, 7.5f };
	Vector3 laserDirection_{ 0.0f, -1.8f, -2.5f };
	// 描画幅0.12の半分を、Playerとの線分判定にも使用します。
	float laserCollisionRadius_ = 0.06f;
	bool isPlayerHitByLaser_ = false;
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
	// PlayerとCameraが共通で使う、床・壁・鏡などの衝突判定用OBBです。
	std::vector<OBB> stageSolidObbs_;
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
	// JSONから生成した通常Camera設定の切替領域です。
	std::vector<StageCameraArea> stageCameraAreas_;
	// 現在起動中のイベントカメラ名です。空文字なら通常カメラです。
	std::string activeEventCameraName_;
	// 現在適用中のCamera Area名です。空文字なら通常設定です。
	std::string activeCameraAreaName_;
	// Hot Reloadウィンドウで選択しているオブジェクト番号です。
	int selectedStageMapObjectIndex_ = 0;
	SceneEditor::ViewportState viewportEditorState_{};
	SceneEditor::ShelfState stageShelfState_{};
	// ファイル保存時に自動で再読込するかを切り替えます。
	bool autoStageMapReload_ = true;
	// ImGuiへ表示する、直近の再読込結果です。
	std::string stageMapReloadStatus_ = "Not loaded yet.";
	// Stage1 内の全モデルで共有する平行光源です。
	Object3d::DirectionalLight directionalLight_{};
	// Stage1 内の全モデルで共有する点光源です。
	Object3d::PointLight pointLight_{};
	bool gameplaySmokeEnabled_ = false;
	bool gameplaySmokeSawGrounded_ = false;
	bool gameplaySmokeLeftFloor_ = false;
	bool gameplaySmokeFell_ = false;
	bool gameplaySmokePickedUpMirror_ = false;
	bool gameplaySmokeDroppedMirror_ = false;
	bool gameplaySmokeCarryMirrorReflectedLaser_ = false;
	bool gameplaySmokeCarriedMirrorBlocksPlayer_ = false;
	bool gameplaySmokeLaserHitsPlayer_ = false;
	bool gameplaySmokeMirrorBlocksPlayer_ = false;
	bool gameplaySmokeFixedMirrorReflection_ = false;
	bool gameplaySmokeCameraSteps_ = false;
	bool gameplaySmokeCameraWallBlock_ = false;
	bool gameplaySmokeCameraSmooth_ = false;
	float gameplaySmokeElapsedTime_ = 0.0f;
	float gameplaySmokeStartY_ = 0.0f;
	int gameplaySmokeFrame_ = 0;
};
