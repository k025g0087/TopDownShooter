#include "PlayerInput.h"
#include <cmath>
#include "Input.h"
#include "Camera.h"
#include "WinApp.h"

using namespace MathUtility;

namespace {
	// スティックを「狙っている」とみなす傾き
	const float kPadAimThreshold = 0.25f;
	// トリガーを「押している」とみなす深さ
	const float kTriggerThreshold = 0.3f;
}

PlayerInput PlayerInputReader::Read(Input* input, const Camera* camera, const Vector3& playerPosition, float aimHeight)
{
	PlayerInput result;

	// ---- 最後に触った機器を判定する ----
	const Vector2& mousePosition = input->GetMousePosition();
	bool keyboardMouseUsed =
		input->PushKey(DIK_W) || input->PushKey(DIK_A) || input->PushKey(DIK_S) || input->PushKey(DIK_D) ||
		input->PushKey(DIK_SPACE) ||
		input->PushMouse(Input::MouseButton::kLeft) || input->PushMouse(Input::MouseButton::kRight) ||
		Length(mousePosition - lastMousePosition) > 2.0f;
	lastMousePosition = mousePosition;
	bool padUsed = input->IsPadConnected() &&
		(Length(input->GetLeftStick()) > 0.0f || Length(input->GetRightStick()) > 0.0f ||
		 input->GetLeftTrigger() > kTriggerThreshold || input->GetRightTrigger() > kTriggerThreshold ||
		 input->PushPad(XINPUT_GAMEPAD_A) || input->PushPad(XINPUT_GAMEPAD_LEFT_SHOULDER) || input->PushPad(XINPUT_GAMEPAD_RIGHT_SHOULDER));
	if (padUsed) {
		lastUsedPad = true;
	} else if (keyboardMouseUsed) {
		lastUsedPad = false;
	}
	result.usingPad = lastUsedPad;

	// ---- 移動：キーボード（WASD）とスティックの、大きい方 ----
	Vector2 keyMove = { 0.0f, 0.0f };
	if (input->PushKey(DIK_D)) { keyMove.x += 1.0f; }
	if (input->PushKey(DIK_A)) { keyMove.x -= 1.0f; }
	if (input->PushKey(DIK_W)) { keyMove.y += 1.0f; }
	if (input->PushKey(DIK_S)) { keyMove.y -= 1.0f; }
	// 斜めでも速さが同じになるように長さを1までにする
	if (Length(keyMove) > 1.0f) {
		keyMove = Normalize(keyMove);
	}
	Vector2 padMove = input->GetLeftStick();
	result.move = (Length(padMove) > Length(keyMove)) ? padMove : keyMove;

	// ---- 狙う ----
	if (lastUsedPad) {
		// 右スティックの向き。離しているときは最後の向き（それもなければ移動の向き）
		Vector2 stick = input->GetRightStick();
		if (Length(stick) > kPadAimThreshold) {
			lastPadAim = Normalize(stick);
		} else if (Length(result.move) > 0.2f) {
			lastPadAim = Normalize(result.move);
		}
		result.aim = lastPadAim;
	} else {
		// マウスの位置の地面の点へ向ける
		Vector3 point;
		if (camera && ScreenToGround(camera, mousePosition, aimHeight, point)) {
			result.aimPoint = point;
			result.hasAimPoint = true;
			result.aim = Normalize(Vector2{ point.x - playerPosition.x, point.z - playerPosition.z });
		}
	}

	// ---- 撃つ・ロール ----
	result.fire = input->PushMouse(Input::MouseButton::kLeft) || input->GetRightTrigger() > kTriggerThreshold;
	result.roll = input->TriggerMouse(Input::MouseButton::kRight) || input->TriggerKey(DIK_SPACE) ||
		input->TriggerPad(XINPUT_GAMEPAD_LEFT_SHOULDER) || input->TriggerPad(XINPUT_GAMEPAD_A);
	// 左トリガーは「深く押した瞬間」をロールにする
	bool isLeftTrigger = input->GetLeftTrigger() > kTriggerThreshold;
	if (isLeftTrigger && !wasLeftTrigger) {
		result.roll = true;
	}
	wasLeftTrigger = isLeftTrigger;

	return result;
}

bool PlayerInputReader::ScreenToGround(const Camera* camera, const Vector2& screenPosition, float planeY, Vector3& outPoint)
{
	// ピクセル → -1.0〜1.0（右と上が+）
	float ndcX = screenPosition.x / static_cast<float>(WinApp::kClientWidth) * 2.0f - 1.0f;
	float ndcY = 1.0f - screenPosition.y / static_cast<float>(WinApp::kClientHeight) * 2.0f;
	// カメラから見た向き（カメラの前が +Z）
	float tanHalf = std::tan(camera->GetFovY() * 0.5f);
	Vector3 viewDirection = { ndcX * tanHalf * camera->GetAspectRatio(), ndcY * tanHalf, 1.0f };
	// ワールドの向きに直す（カメラのワールド行列の回転だけを使う）
	Vector3 direction = TransformDirection(viewDirection, camera->GetWorldMatrix());
	const Vector3& origin = camera->GetTranslate();
	// 水平な面と平行（または面から離れていく向き）なら交わらない
	if (std::fabs(direction.y) < 0.0001f) {
		return false;
	}
	float t = (planeY - origin.y) / direction.y;
	if (t <= 0.0f) {
		return false;
	}
	outPoint = origin + direction * t;
	return true;
}
