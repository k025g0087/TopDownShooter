#pragma once
#include "Mathutility.h"

class Input;
class Camera;

// プレイヤーの操作（キーボード＋マウス／ゲームパッドを、ゲームで使う形にまとめたもの）
// プレイヤーはキーやボタンを直接見ずに、これだけを見る（割り当てを変えるときは PlayerInput.cpp だけ直せばよい）
struct PlayerInput {
	// 移動の向き（-1.0〜1.0。x が右、y が画面の上＝ワールドの +Z）
	Vector2 move = { 0.0f, 0.0f };
	// 狙う向き（地面の上の向き。x が右、y がワールドの +Z。長さ1。狙っていないときは0）
	Vector2 aim = { 0.0f, 0.0f };
	// マウスで狙っているときの、地面の上の狙う点
	Vector3 aimPoint = { 0.0f, 0.0f, 0.0f };
	bool hasAimPoint = false;
	// 撃つ（押している間ずっと）
	bool fire = false;
	// ロール回避（押した瞬間）
	bool roll = false;
	// 最後に触ったのがゲームパッドか（狙い方を切り替えるのに使う）
	bool usingPad = false;
};

// 入力を読んで PlayerInput を作る
// playerPosition：狙う向きを出すためのプレイヤーの位置 / camera：マウスの位置から地面の点を出すためのカメラ
// aimHeight：狙う点を取る高さ（弾が飛ぶ高さ）
class PlayerInputReader
{
public:
	PlayerInput Read(Input* input, const Camera* camera, const Vector3& playerPosition, float aimHeight);

	// 画面のピクセル位置から、高さ planeY の水平な面との交点を求める（見つかれば true）
	static bool ScreenToGround(const Camera* camera, const Vector2& screenPosition, float planeY, Vector3& outPoint);

private:
	// 最後に触った機器（ゲームパッドなら true）
	bool lastUsedPad = false;
	// ゲームパッドで最後に狙っていた向き（スティックを離しても向きを保つ）
	Vector2 lastPadAim = { 0.0f, 1.0f };
	// 前のフレームのマウスの位置（マウスを動かしたら「キーボード＋マウスで操作中」に切り替える）
	Vector2 lastMousePosition = { 0.0f, 0.0f };
	// 前のフレームに左トリガーを押していたか（押した瞬間をロールにするため）
	bool wasLeftTrigger = false;
};
