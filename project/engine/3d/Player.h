#pragma once

#include "Object3d.h"
#include "MyMath.h"
#include "Vector3.h"
#include <string>
#include <vector>

class Object3dCommon;

using namespace MyMath;

//プレイヤーの移動・重力・ジャンプを管理するクラス
class Player
{
public:
	//モデルを読み込み、開始位置と当たり判定用の半径を設定する
	void Initialize(Object3dCommon* object3dCommon, const std::string& modelName, const Vector3& startPosition, float radius);
	//入力を読み取り、移動・重力・OBBとの接地判定を更新する
	void Update(float deltaTime, const std::vector<OBB>& solidObbs);

	//Stage1の共通描画処理で使用する3Dオブジェクトを取得する
	Object3d& GetObject() { return object_; }
	const Vector3& GetPosition() const { return position_; }
	const Vector3& GetVelocity() const { return velocity_; }
	//最後に入力した移動方向へ向く、プレイヤーのY軸回転角です。
	float GetFacingYaw() const { return facingYaw_; }
	bool IsGrounded() const { return isGrounded_; }
	bool IsColliding() const { return isColliding_; }
	MyMath::Sphere GetCollider() const { return { position_, radius_ }; }

private:
	//画面に描画する球モデル
	Object3d object_;
	//プレイヤーの中心位置
	Vector3 position_{};
	//プレイヤーが最後に移動した方向を表す、Y軸回転角です。
	float facingYaw_ = 0.0f;
	//現在の移動速度。今回はY方向の落下・ジャンプに使用する
	Vector3 velocity_{};
	//球の半径。床へめり込ませないために使用する
	float radius_ = 1.0f;
	//WASDで移動する速さ
	float moveSpeed_ = 4.0f;
	//毎秒ごとにY方向へ加える重力
	float gravity_ = -18.0f;
	//ジャンプした瞬間に与える上向きの速さ
	float jumpSpeed_ = 7.0f;
	//床に立っているかどうか
	bool isGrounded_ = false;
	//床のOBBと重なっているかどうか
	bool isColliding_ = false;
};
