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
#include "StageStart.h"
#include <array>
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
	// ---------- JSONを実行中のオブジェクトへ変換する補助構造体 ----------

	// 三灯照明は最大3本に固定し、残りのSpotLight枠をLaser演出へ残します。
	static constexpr size_t kStageLightingSpotLightCount = 3;

	// JSON上の追加オブジェクトと、実行中のObject3d・コライダーを結び付けます。
	struct StageMapRuntimeObject
	{
		// JSON上のnameです。編集UIでどのObjectかを見分けます。
		std::string sourceName;
		// JSONから生成した、画面に描く3Dモデルです。
		std::unique_ptr<Object3d> visual;
		// モデル原点から見たCollider中心です。
		Vector3 colliderLocalCenter{};
		// モデル原点から見たColliderの半分の大きさです。
		Vector3 colliderLocalHalfSize{};
		// 実際のTransformを反映した衝突判定です。
		OBB collider{};
		// trueならPlayer・Camera・Lightに使うBOX Colliderがあります。
		bool hasBoxCollider = false;
		// 制御点移動を始める前のモデル位置です。
		Vector3 pathBasePosition{};
		// JSONに書いた移動経路の点です。
		std::vector<Vector3> controlPoints;
		// 一秒あたりに経路を進む速さです。
		float pathSpeed = 1.0f;
		// 現在、経路のどこまで進んだかを0から1で表します。
		float pathProgress = 0.0f;
		// trueなら終点に着いた後、始点へ戻って繰り返します。
		bool pathLoop = true;
	};

	// Playerが入った時にイベントカメラを起動する、描画されないBOXです。
	struct StageEventTrigger
	{
		// JSON上のnameです。編集UIでどのTriggerかを見分けます。
		std::string sourceName;
		// TriggerとEvent Cameraを結び付ける共通IDです。
		std::string eventId;
		// このTriggerで有効にするEvent Cameraのnameです。
		std::string eventCameraName;
		// Trigger BOXの位置・回転・大きさです。
		Transform transform{};
		// Trigger BOXのローカル中心です。
		Vector3 colliderLocalCenter{};
		// Trigger BOXのローカル半分サイズです。
		Vector3 colliderLocalHalfSize{ 1.0f, 1.0f, 1.0f };
		// 実際のTransformを反映したTrigger用OBBです。
		OBB collider{};
		// trueなら前フレームでPlayerがこのBOX内にいました。
		bool isPlayerInside = false;
	};

	// イベント中だけ使用するCameraと、カメラが見る注視点です。
	struct StageEventCamera
	{
		// JSON上のnameです。Triggerから選ぶ時に使います。
		std::string sourceName;
		// Event中だけCameraManagerへ登録するCameraです。
		std::unique_ptr<Camera> camera;
		// Event中に右MouseでCameraを回すための操作役です。
		std::unique_ptr<CameraController> manualController;
		// Cameraが見る注視点です。
		Vector3 focus{};
		// trueならfocusを使い、falseならCameraの回転値を使います。
		bool hasFocus = false;
	};

	// 狭い通路やボス部屋などで、通常Cameraの設定だけを切り替える描画されないBOXです。
	struct StageCameraArea
	{
		// JSON上のnameです。現在有効なArea表示に使います。
		std::string sourceName;
		// Area BOXの位置・回転・大きさです。
		Transform transform{};
		// Area BOXのローカル中心です。
		Vector3 colliderLocalCenter{};
		// Area BOXのローカル半分サイズです。
		Vector3 colliderLocalHalfSize{ 1.0f, 1.0f, 1.0f };
		// 実際のTransformを反映したArea用OBBです。
		OBB collider{};
		// Area内だけ通常Cameraへ反映する距離・角度・画角です。
		CameraAreaSettings settings{};
		// trueなら前フレームでPlayerがこのArea内にいました。
		bool isPlayerInside = false;
	};

	// ---------- 更新・描画の補助関数 ----------

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
	// ---------- 鏡・Laserギミック ----------

	// 持てる鏡の拾う・置く処理と、全鏡を使うレーザー経路を更新します。
	void UpdateMirrorGameplay();
	// 固定鏡・持てる鏡・鏡床を、Laserが反射できるMirror一覧として返します。
	std::vector<const Mirror*> GetLaserReflectors() const;
	// 一本の危険LightをLaserとして追跡し、反射後の全線分を返します。
	std::vector<LaserSegment> ReflectHazardLightSegments(const std::vector<LaserSegment>& sourceSegments) const;
	// 反射したレーザーがSwitchへ届く時間を計測し、Doorを滑らかに開きます。
	void UpdateLightPuzzle(float deltaTime);
	// 時間に合わせて動く4種類の危険Lightと、床へ出す予告表示を更新します。
	void UpdateHazardLights(float deltaTime);
	// 反射後を含むLaser線分から、床や壁を実際に照らすSpotLight配列を作ります。
	void UpdateLaserSpotLights();
	// ---------- Camera制御 ----------

	// Playerを自動追尾する通常Cameraを更新します。
	void UpdateMainCamera();
	// Event Cameraゾーン内だけ、右マウスで操作できる手動Cameraを更新します。
	void UpdateEventManualCamera();
	// カメラが現在見ている正面方向を取得します。
	Vector3 GetCameraForward(const Camera& camera) const;
	// ---------- ImGuiの確認・編集UI ----------

	// ImGuiManager に鏡の設定用 UI の表示を依頼し、変更を板へ反映します。
	void DrawMirrorDebugUi();
	// Lightの位置・方向と、反射Puzzleの状態をImGuiManagerへ表示します。
	void DrawLightPuzzleDebugUi();
	//床のOBBを、衝突状態に応じた色のワイヤーで表示します。
	void DrawCollisionDebugUi();
	// Edit Viewで床・鏡・追加モデルをクリック編集し、LevelDataへ同期します。
	void DrawStageEditViewport();
	// Edit View下部へ共通モデル棚を表示し、Stage1へモデルを追加します。
	void DrawStageModelShelf();
	void HandleStageShelfDropOnEditView();
	// ---------- stage1.jsonの読込・保存・編集 ----------

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
	// ---------- 自動確認用テスト ----------

	// 環境変数で起動した時だけ、携帯鏡・レーザー・床落下を自動検証します。
	void InitializeGameplaySmoke();
	void UpdateGameplaySmoke(float deltaTime);

	// ---------- Stage1が所有する3Dオブジェクト ----------

	// 床と球です。後で鏡へ映す対象にもなります。
	std::vector<std::unique_ptr<Object3d>> sceneObjects_;
	// ---------- 鏡の状態 ----------

	// 大型の固定鏡です。複数枚それぞれが反射Cameraと専用Textureを持ちます。
	std::vector<std::unique_ptr<FixedMirror>> fixedMirrors_;
	// 持ち運べず、両面から来たLightを反射する鏡床ギミックです。
	std::unique_ptr<FixedMirror> mirrorFloor_;
	// 通常床とは別に置く、持ち運べない鏡床ギミックの配置です。
	Vector3 mirrorFloorPosition_{ 5.0f, -1.985f, 11.0f };
	float mirrorFloorWidth_ = 4.5f;
	float mirrorFloorHeight_ = 4.5f;
	// 複数の固定鏡を一枚ずつ順番に更新するための番号です。
	size_t reflectionUpdateCursor_ = 0;
	// 景色は映さず、PlayerがEキーで持ち運べるレーザー反射用の小型鏡です。
	std::unique_ptr<CarryableMirror> carryableMirror_;
	// ---------- Laserの状態 ----------

	// 携帯MirrorでCharge Switchへ送る、シアン色の充電用Laserです。
	Laser laser_;
	std::unique_ptr<LaserRenderer> laserRenderer_;
	// Charge Laserの発射位置を見分けるために置く、小さな白い球です。
	Object3d* laserEmitter_ = nullptr;
	// Player正面の携帯鏡へ入射し、大型Mirrorの表側へ届く初期Laserです。
	Vector3 laserOrigin_{ 0.0f, 2.0f, 7.5f };
	Vector3 laserDirection_{ 0.0f, -0.85f, -1.2f };
	// 描画幅0.12の半分を、Playerとの線分判定にも使用します。
	float laserCollisionRadius_ = 0.06f;
	// 当たり判定とは別に、画面で見やすくするためのLaser描画幅です。
	float laserVisualWidth_ = 0.32f;
	bool isPlayerHitByLaser_ = false;
	// 大型Mirrorの回転後にDoor Switchへ送る、オレンジ色の専用Laserです。
	Laser doorLaser_;
	std::unique_ptr<LaserRenderer> doorLaserRenderer_;
	Object3d* doorLaserEmitter_ = nullptr;
	Vector3 doorLaserOrigin_{ -6.0f, 1.0f, 8.0f };
	Vector3 doorLaserDirection_{ 1.0f, 0.0f, 0.0f };
	// ---------- 時間制御で動く危険Light ----------
	// 上の発射点を固定し、床へ当たる先端だけを左右へ振るLightです。
	std::vector<LaserSegment> ceilingSweepLightSegments_;
	std::unique_ptr<LaserRenderer> ceilingSweepLightRenderer_;
	Vector3 ceilingSweepStart_{ 18.0f, 4.25f, 26.0f };
	float ceilingSweepDistance_ = 5.0f;
	// 横一直線のLightを、停止とイージング移動を繰り返しながら奥へ動かします。
	std::vector<LaserSegment> horizontalMoveLightSegments_;
	std::unique_ptr<LaserRenderer> horizontalMoveLightRenderer_;
	Vector3 horizontalMoveNearStart_{ 32.0f, 1.00f, 30.0f };
	Vector3 horizontalMoveFarStart_{ 32.0f, 1.00f, 55.0f };
	// 下から出るLightと、出現三秒前に床へ出す赤い予告範囲です。
	std::vector<LaserSegment> bottomPulseLightSegments_;
	std::vector<LaserSegment> bottomPulseWarningSegments_;
	std::unique_ptr<LaserRenderer> bottomPulseLightRenderer_;
	std::unique_ptr<LaserRenderer> bottomPulseWarningRenderer_;
	Vector3 bottomPulsePosition_{ 28.0f, -1.98f, 45.0f };
	// 三本が円を描きながら、半径を広げたり閉じたりする上からのLightです。
	std::vector<LaserSegment> orbitLightSegments_;
	std::unique_ptr<LaserRenderer> orbitLightRenderer_;
	Vector3 orbitLightCenter_{ 24.0f, 0.0f, 62.0f };
	float hazardLightTime_ = 0.0f;
	bool isPlayerHitByHazardLight_ = false;
	// ---------- Light PuzzleのSwitchとDoor ----------

	// 携帯鏡で反射したLaserを受け、一定時間で大型Mirrorを起動する充電Switchです。
	Object3d* chargeSwitch_ = nullptr;
	// 初期Laserの経路上に置き、Playerが少し動いても充電しやすい位置です。
	Vector3 chargeSwitchPosition_{ 0.0f, 0.65f, 7.00f };
	float chargeSwitchRadius_ = 0.45f;
	bool isChargeSwitchReceivingLight_ = false;
	float mirrorCharge_ = 0.0f;
	bool isLargeMirrorCharged_ = false;
	// 充電完了後に大型Mirrorを床と平行に横へ振る、0～1の回転進行度です。
	float largeMirrorRotationAmount_ = 0.0f;
	// 初期状態は正面を向き、充電後は斜め方向へ反射する角度まで回します。
	float largeMirrorBaseYaw_ = 3.14159265f;
	float largeMirrorTargetYawOffset_ = 1.04719755f;
	// 斜めを向いた大型Mirrorの反射Laserを受け、Doorを開けるSwitchです。
	Object3d* doorSwitch_ = nullptr;
	// Door LaserはX=-6から大型Mirrorへ進み、回転後は左手前へ斜め反射する。
	Vector3 doorSwitchPosition_{ -3.0f, 1.0f, 2.80f };
	float doorSwitchRadius_ = 0.80f;
	bool isDoorSwitchReceivingLight_ = false;
	// Door Switchへ光が当たっている間だけ上へ移動するDoorです。開いたDoorはPlayerの衝突一覧から外します。
	Object3d* lightDoor_ = nullptr;
	Vector3 doorClosedPosition_{ -4.5f, -0.5f, 2.80f };
	Vector3 doorColliderLocalHalfSize_{ 10.0f, 1.5f, 10.0f };
	OBB doorCollider_{};
	float doorOpenHeight_ = 4.5f;
	float doorOpenAmount_ = 0.0f;
	// ---------- Player・床・当たり判定 ----------

	//WASD移動とジャンプを行う球のプレイヤーです。
	std::unique_ptr<Player> player_;
	// stage1.jsonのPlayerStartから読み込んだ、本編開始時のPlayer座標です。
	Vector3 stagePlayerStartPosition_{ 0.0f, -0.8f, 5.0f };
	// PlayerStartがstage1.jsonに書かれていた時だけtrueになります。
	bool hasStagePlayerStart_ = false;
	//見た目とOBBを共有する床モデルです。
	Object3d* floor_ = nullptr;
	//floor.objのローカル座標における半分の大きさです。
	Vector3 floorLocalHalfSize_{ 10.0f, 1.500001f, 10.0f };
	//floor.objのローカル座標におけるコライダー中心です。
	Vector3 floorColliderLocalCenter_{};
	//毎フレーム、床モデルのTransformから作る衝突判定用OBBです。
	OBB floorObb_{};
	// ---------- 通常Cameraと開始演出 ----------

	// Player などの対象を追従する、三人称カメラ専用の操作役です。
	std::unique_ptr<CameraController> cameraController_;
	// 本編開始時だけPlayerとCameraを動かす、開始演出の操作役です。
	std::unique_ptr<StageStart> stageStart_;
	// stage1.jsonのstage_startから読み込む、開始演出の調整値です。
	StageStart::Settings stageStartSettings_{};
	// PlayerとCameraが共通で使う、床・壁・鏡などの衝突判定用OBBです。
	std::vector<OBB> stageSolidObbs_;
	// Lightが通り抜けてはいけない床・Door・壁用のOBBです。Mirrorは反射計算を優先するため含めません。
	std::vector<OBB> stageLightBlockingObbs_;
	// ---------- stage1.jsonの実行中データ ----------

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
	// ---------- Stage1全体のライト ----------

	// Stage1 内の全モデルで共有する平行光源です。
	Object3d::DirectionalLight directionalLight_{};
	// Stage1 内の全モデルで共有する点光源です。
	Object3d::PointLight pointLight_{};
	// JSONで配置するキー・フィル・バックライトです。先頭から優先して描画へ渡します。
	std::vector<Object3d::SpotLight> stageSpotLights_;
	// Laserの各線分へ対応する、暗い場所を照らす実際のSpotLightです。
	std::array<Object3d::SpotLight, Object3d::kMaximumSpotLightCount> spotLights_{};
	// ---------- 自動確認用テストの途中結果 ----------

	// trueの時だけ、環境変数で指定した自動確認を実行します。
	bool gameplaySmokeEnabled_ = false;
	// Playerが一度でも床へ着地したかを記録します。
	bool gameplaySmokeSawGrounded_ = false;
	// Playerが床の外へ出たかを記録します。
	bool gameplaySmokeLeftFloor_ = false;
	// Playerが床から落下できたかを記録します。
	bool gameplaySmokeFell_ = false;
	// 携帯MirrorをEキー操作で拾えたかを記録します。
	bool gameplaySmokePickedUpMirror_ = false;
	// 携帯MirrorをEキー操作で置けたかを記録します。
	bool gameplaySmokeDroppedMirror_ = false;
	// 携帯MirrorがLaserを反射したかを記録します。
	bool gameplaySmokeCarryMirrorReflectedLaser_ = false;
	// 携帯Mirrorの裏面がLaserを反射しないかを記録します。
	bool gameplaySmokeCarryMirrorBackfaceIgnored_ = false;
	// 水平に持つMirrorの操作が正しく切り替わるかを記録します。
	bool gameplaySmokeCarryMirrorHorizontalControl_ = false;
	// 傾けた水平MirrorがLaserの進行方向を変えるかを記録します。
	bool gameplaySmokeCarryMirrorTiltRedirectsLaser_ = false;
	// 持っているMirrorがPlayerへ届くLaserを遮るかを記録します。
	bool gameplaySmokeCarriedMirrorBlocksPlayer_ = false;
	// LaserがPlayerに当たる判定を記録します。
	bool gameplaySmokeLaserHitsPlayer_ = false;
	// 固定MirrorがPlayerの移動を止めるかを記録します。
	bool gameplaySmokeMirrorBlocksPlayer_ = false;
	// 固定Mirrorが反射Textureを更新できたかを記録します。
	bool gameplaySmokeFixedMirrorReflection_ = false;
	// 鏡床がLaserを反射できたかを記録します。
	bool gameplaySmokeMirrorFloorReflectsLaser_ = false;
	// 鏡床が反射Textureを更新できたかを記録します。
	bool gameplaySmokeMirrorFloorReflection_ = false;
	// 危険LightがMirrorで反射したかを記録します。
	bool gameplaySmokeHazardLightReflection_ = false;
	// Doorが初期状態で閉じているかを記録します。
	bool gameplaySmokeDoorStartsClosed_ = false;
	// Door用Laserが斜めに反射できたかを記録します。
	bool gameplaySmokeDoorDiagonalReflection_ = false;
	// 矢印キーによるCamera段階操作を記録します。
	bool gameplaySmokeCameraSteps_ = false;
	// 壁側へCameraを回せないことを記録します。
	bool gameplaySmokeCameraWallBlock_ = false;
	// Cameraの回転が一瞬で移動せず補間されるかを記録します。
	bool gameplaySmokeCameraSmooth_ = false;
	// PlayerがCamera側へ動いた時に追従できるかを記録します。
	bool gameplaySmokeCameraBacktracks_ = false;
	// 自動確認を開始してからの経過時間です。
	float gameplaySmokeElapsedTime_ = 0.0f;
	// 自動確認開始時のPlayerの高さです。
	float gameplaySmokeStartY_ = 0.0f;
	// 自動確認でUpdateしたフレーム数です。
	int gameplaySmokeFrame_ = 0;
};
