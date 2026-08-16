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
	// Keyboard・GamePad・自動テストの入力を、同じ移動処理へ渡すための値です。
	struct ControlInput
	{
		float forward = 0.0f;
		float right = 0.0f;
		bool jumpPressed = false;
	};

	//モデルを読み込み、開始位置と当たり判定用の半径を設定する
	void Initialize(Object3dCommon* object3dCommon, const std::string& modelName, const Vector3& startPosition, float radius);
	// Cameraの正面方向を基準に入力を読み取り、移動・重力・OBBとの接地判定を更新する
	void Update(
		float deltaTime,
		const std::vector<OBB>& solidObbs,
		const Vector3& cameraForward);
	// 入力機器を直接読まず、渡された入力値で同じ移動・重力・衝突処理を行います。
	void UpdateWithControl(
		float deltaTime,
		const std::vector<OBB>& solidObbs,
		const Vector3& cameraForward,
		const ControlInput& controlInput);
	// 携帯Mirrorを構えている間は、移動中でもPlayerの向きを固定します。
	void SetMirrorGuardMode(bool isMirrorGuardMode) { isMirrorGuardMode_ = isMirrorGuardMode; }
	bool IsMirrorGuardMode() const { return isMirrorGuardMode_; }

	//Stage1の共通描画処理で使用する3Dオブジェクトを取得する
	Object3d& GetObject() { return object_; }
	const Vector3& GetPosition() const { return position_; }
	const Vector3& GetVelocity() const { return velocity_; }
	// このフレームにPlayerが移動した水平方向です。Cameraの先読みと自動整列に使います。
	const Vector3& GetMoveDirection() const { return moveDirection_; }
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
	// このフレームに入力された、Camera基準へ変換済みの移動方向です。
	Vector3 moveDirection_{};
	//球の半径。床へめり込ませないために使用する
	float radius_ = 1.0f;
	//WASDで移動する速さ
	float moveSpeed_ = 4.0f;
	// Mirrorを構えている間に掛ける移動速度の倍率です。
	float mirrorGuardMoveSpeedRate_ = 0.55f;
	//毎秒ごとにY方向へ加える重力
	float gravity_ = -18.0f;
	//ジャンプした瞬間に与える上向きの速さ
	float jumpSpeed_ = 7.0f;
	// Mirrorを構えている間に掛けるジャンプ速度の倍率です。
	float mirrorGuardJumpSpeedRate_ = 0.70f;
	// 左クリックでMirrorを構えている間だけtrueになり、移動してもPlayerの向きを変えません。
	bool isMirrorGuardMode_ = false;
	//床に立っているかどうか
	bool isGrounded_ = false;
	//床のOBBと重なっているかどうか
	bool isColliding_ = false;
};
