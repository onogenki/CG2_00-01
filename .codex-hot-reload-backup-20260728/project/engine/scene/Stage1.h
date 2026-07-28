#pragma once

#include "BaseScene.h"
#include "CameraController.h"
#include "Mirror.h"
#include "Player.h"
#include <memory>
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

	// 床と球です。後で鏡へ映す対象にもなります。
	std::vector<std::unique_ptr<Object3d>> sceneObjects_;
	// 鏡の見た目を描画する、一枚の平面モデルです。
	std::unique_ptr<Object3d> mirrorVisual_;
	// 鏡の中心・正面・大きさを管理するデータです。
	Mirror mirror_;
	//鏡のローカル座標における半分の大きさ
	Vector3 mirrorLocalHalfSize_{ 1.0f,1.0f,0.05f };
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
	//毎フレーム、床モデルのTransformから作る衝突判定用OBBです。
	OBB floorObb_{};
	// Player などの対象を追従する、三人称カメラ専用の操作役です。
	std::unique_ptr<CameraController> cameraController_;
	// Stage1 では、壁の鏡をまず簡単に回せるよう、Y 軸回転だけを編集します。
	float mirrorYaw_ = 3.14159265f;
	// Stage1 内の全モデルで共有する平行光源です。
	Object3d::DirectionalLight directionalLight_{};
	// Stage1 内の全モデルで共有する点光源です。
	Object3d::PointLight pointLight_{};
};
