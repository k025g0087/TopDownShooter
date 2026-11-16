#pragma once
#include <string>
#include "GameSettings.h"
#include "Input.h"
#include "SoundManager.h"
#include "UiKit.h"

// メニューで使う色
namespace UiColor {
	inline const Vector4 kDim = { 0.0f, 0.0f, 0.0f, 0.6f };
	inline const Vector4 kPanel = { 0.07f, 0.06f, 0.12f, 0.94f };
	inline const Vector4 kBorder = { 0.75f, 0.45f, 1.0f, 1.0f };
	inline const Vector4 kFocus = { 0.38f, 0.25f, 0.62f, 0.95f };
	inline const Vector4 kButton = { 0.18f, 0.15f, 0.28f, 1.0f };
	inline const Vector4 kText = { 1.0f, 1.0f, 1.0f, 1.0f };
	inline const Vector4 kTextDim = { 0.7f, 0.7f, 0.78f, 1.0f };
	inline const Vector4 kAccent = { 1.0f, 0.9f, 0.3f, 1.0f };
}

// メニューのキー操作（決定はスペース・選ぶのは矢印／W,S,A,D。ここは変えられない）
namespace MenuKeys {
	inline bool Up(Input* i) { return i->TriggerKey(DIK_UP) || i->TriggerKey(DIK_W); }
	inline bool Down(Input* i) { return i->TriggerKey(DIK_DOWN) || i->TriggerKey(DIK_S); }
	inline bool Left(Input* i) { return i->TriggerKey(DIK_LEFT) || i->TriggerKey(DIK_A); }
	inline bool Right(Input* i) { return i->TriggerKey(DIK_RIGHT) || i->TriggerKey(DIK_D); }
	inline bool Decide(Input* i) { return i->TriggerKey(DIK_SPACE); }
}

// 設定画面（キー配置の変更と、音量）。タイトルとポーズ画面から開く
// 文字や四角は UiKit で描く（ImGuiを使わないので、リリースビルドでも出る）
class SettingsMenu
{
public:
	void Open() { isOpen = true; waitingAction = -1; focus = 0; escConsumed = false; }
	bool IsOpen() const { return isOpen; }

	// 開いている間、毎フレーム呼ぶ（ui は BeginFrame 済みのものを渡す）
	void Update(Input* input, UiKit* ui)
	{
		if (!isOpen) {
			return;
		}
		GameSettings* settings = GameSettings::GetInstance();
		const int32_t actionCount = static_cast<int32_t>(GameAction::kCount);
		const int32_t rowReset = actionCount, rowVolume = actionCount + 1, rowClose = actionCount + 2;
		const int32_t rowCount = actionCount + 3;

		// ---- 位置（画面は 1280x720）----
		const float panelX = 320.0f, panelY = 60.0f, panelW = 640.0f, panelH = 600.0f;
		const float rowY0 = 140.0f, rowH = 46.0f;
		const float valueX = 630.0f, valueW = 260.0f;
		const float barW = 190.0f; // 音量の棒の長さ（右に数字を出す余白を残す）
		auto rowTop = [&](int32_t row) {
			float y = rowY0 + static_cast<float>(row) * rowH;
			if (row >= rowReset) { y += 52.0f; }          // 説明の文のぶん、少し空ける
			if (row >= rowVolume) { y += 12.0f; }
			if (row >= rowClose) { y += 8.0f; }
			return y;
		};

		bool decided = false;

		// ---- キーの入力待ち：押されたキーを割り当てる（Esc で取りやめ）----
		if (waitingAction >= 0) {
			if (input->TriggerKey(DIK_ESCAPE)) {
				waitingAction = -1;
				escConsumed = true;
			} else {
				for (int32_t k = 1; k < 256; ++k) {
					if (k == DIK_ESCAPE || !input->TriggerKey(static_cast<BYTE>(k))) {
						continue;
					}
					settings->SetKey(static_cast<GameAction>(waitingAction), static_cast<BYTE>(k));
					settings->Save();
					waitingAction = -1;
					escConsumed = true; // 同じフレームで、ほかの入力に反応しないように
					break;
				}
			}
		} else {
			if (input->TriggerKey(DIK_ESCAPE) && !escConsumed) {
				isOpen = false; // 待っていないときの Esc：閉じる
				return;
			}
			escConsumed = false;
			// 選ぶ
			if (MenuKeys::Up(input)) { focus = (focus + rowCount - 1) % rowCount; }
			if (MenuKeys::Down(input)) { focus = (focus + 1) % rowCount; }
			// 音量はキーでも変えられる
			if (focus == rowVolume) {
				float step = 0.0f;
				if (MenuKeys::Left(input)) { step = -0.05f; }
				if (MenuKeys::Right(input)) { step = 0.05f; }
				if (step != 0.0f) {
					SetVolume(settings->GetMasterVolume() + step);
					settings->Save();
				}
			}
			decided = MenuKeys::Decide(input);
		}

		// ---- マウス ----
		const Vector2& mouse = input->GetMousePosition();
		bool clicked = input->TriggerMouse(Input::MouseButton::kLeft);
		if (waitingAction < 0) {
			for (int32_t row = 0; row < rowCount; ++row) {
				if (UiKit::Inside(mouse, panelX + 20.0f, rowTop(row), panelW - 40.0f, rowH - 6.0f)) {
					if (mouseRow != row) { focus = row; }
					mouseRow = row;
					if (clicked) { focus = row; decided = (row != rowVolume); }
				} else if (mouseRow == row) {
					mouseRow = -1;
				}
			}
			// 音量の棒：クリック・ドラッグで決める
			float barY = rowTop(rowVolume);
			if (UiKit::Inside(mouse, valueX, barY - 6.0f, barW, rowH) && (clicked || (input->PushMouse(Input::MouseButton::kLeft) && draggingVolume))) {
				draggingVolume = true;
				SetVolume((mouse.x - valueX) / barW);
			}
			if (!input->PushMouse(Input::MouseButton::kLeft) && draggingVolume) {
				draggingVolume = false;
				settings->Save();
			}
		}

		// ---- 決定されたとき ----
		if (decided && waitingAction < 0) {
			if (focus < actionCount) {
				waitingAction = focus;
			} else if (focus == rowReset) {
				settings->ResetKeys();
				settings->Save();
			} else if (focus == rowClose) {
				isOpen = false;
				return;
			}
		}

		// ---- 描く ----
		ui->Rect(0.0f, 0.0f, 1280.0f, 720.0f, UiColor::kDim);
		ui->Rect(panelX, panelY, panelW, panelH, UiColor::kPanel);
		ui->Frame(panelX, panelY, panelW, panelH, 3.0f, UiColor::kBorder);
		ui->Text(JP("設定"), panelX + panelW / 2.0f, panelY + 18.0f, 34.0f, UiColor::kText, UiKit::Align::kCenter);
		ui->Text(JP("キー配置"), panelX + 28.0f, rowY0 - 34.0f, 20.0f, UiColor::kAccent);
		for (int32_t row = 0; row < rowCount; ++row) {
			float y = rowTop(row);
			bool focused = (row == focus);
			if (focused) {
				ui->Rect(panelX + 16.0f, y - 2.0f, panelW - 32.0f, rowH - 4.0f, UiColor::kFocus);
			}
			if (row < actionCount) {
				GameAction action = static_cast<GameAction>(row);
				ui->Text(GameSettings::ActionLabel(action), panelX + 40.0f, y + 6.0f, 26.0f, UiColor::kText);
				ui->Rect(valueX, y + 2.0f, valueW, rowH - 10.0f, UiColor::kButton);
				ui->Frame(valueX, y + 2.0f, valueW, rowH - 10.0f, 2.0f, focused ? UiColor::kAccent : UiColor::kBorder);
				bool waiting = (waitingAction == row);
				ui->Text(waiting ? std::string(JP("キーを押してください")) : GameSettings::KeyName(settings->GetKey(action)),
					valueX + valueW / 2.0f, y + 8.0f, 24.0f, waiting ? UiColor::kAccent : UiColor::kText, UiKit::Align::kCenter);
			} else if (row == rowReset) {
				ui->Text(JP("ボタンを押してから、割り当てたいキーを押します（Esc で取りやめ）"), panelX + 28.0f, rowTop(rowReset) - 50.0f, 16.0f, UiColor::kTextDim);
				ui->Text(JP("攻撃：マウス左　狙い：マウス　決定：スペース（変えられません）"), panelX + 28.0f, rowTop(rowReset) - 28.0f, 16.0f, UiColor::kTextDim);
				ui->Text(JP("キー配置を初期設定に戻す"), panelX + panelW / 2.0f, y + 6.0f, 24.0f, UiColor::kText, UiKit::Align::kCenter);
			} else if (row == rowVolume) {
				ui->Text(JP("全体の音量"), panelX + 40.0f, y + 6.0f, 26.0f, UiColor::kText);
				float volume = settings->GetMasterVolume();
				ui->Rect(valueX, y + 12.0f, barW, 14.0f, UiColor::kButton);
				ui->Rect(valueX, y + 12.0f, barW * volume, 14.0f, UiColor::kBorder);
				ui->Rect(valueX + barW * volume - 5.0f, y + 6.0f, 10.0f, 26.0f, UiColor::kText);
				ui->Text(std::to_string(static_cast<int>(volume * 100.0f + 0.5f)) + "%", valueX + barW + 16.0f, y + 8.0f, 20.0f, UiColor::kText);
			} else {
				ui->Text(JP("閉じる"), panelX + panelW / 2.0f, y + 6.0f, 24.0f, UiColor::kText, UiKit::Align::kCenter);
			}
		}
		ui->Text(JP("フルスクリーン：Alt + Enter　／　Esc：閉じる"), panelX + panelW / 2.0f, panelY + panelH - 30.0f, 16.0f, UiColor::kTextDim, UiKit::Align::kCenter);
	}

private:
	void SetVolume(float volume)
	{
		GameSettings* settings = GameSettings::GetInstance();
		settings->SetMasterVolume(volume);
		SoundManager::GetInstance()->SetMasterVolume(settings->GetMasterVolume());
	}

	bool isOpen = false;
	int32_t waitingAction = -1; // キーの入力待ちの操作（-1 で待っていない）
	int32_t focus = 0;
	int32_t mouseRow = -1;
	bool draggingVolume = false;
	bool escConsumed = false;
};
