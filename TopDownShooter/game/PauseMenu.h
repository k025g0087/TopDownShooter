#pragma once
#include "GameSettings.h"
#include "SettingsMenu.h"
#include "Input.h"
#include "UiKit.h"

// ポーズ画面（再開・設定・操作説明・キャラ選択へ戻る）
// 世界を止めるのは呼び出し側（StageScene）の仕事。ここは画面を出して、選ばれたものを返す
class PauseMenu
{
public:
	enum class Result {
		kNone,   // 何も選ばれていない（ポーズ中）
		kResume, // ゲームに戻る
		kQuit,   // キャラ選択へ戻る
	};

	void Open() { isOpen = true; showGuide = false; focus = 0; }
	bool IsOpen() const { return isOpen; }

	// 開いている間、毎フレーム呼ぶ（ui は BeginFrame 済みのものを渡す）
	Result Update(Input* input, UiKit* ui)
	{
		if (!isOpen) {
			return Result::kNone;
		}
		// 設定画面が開いている間は、そちらだけを動かす
		if (settingsMenu.IsOpen()) {
			settingsMenu.Update(input, ui);
			return Result::kNone;
		}
		// 操作説明を見ている間：スペース・Esc・クリックで戻る
		if (showGuide) {
			if (input->TriggerKey(DIK_ESCAPE) || MenuKeys::Decide(input) || input->TriggerMouse(Input::MouseButton::kLeft)) {
				showGuide = false;
			} else {
				DrawGuide(ui);
				return Result::kNone;
			}
		}
		// Esc：再開
		if (input->TriggerKey(DIK_ESCAPE)) {
			isOpen = false;
			return Result::kResume;
		}

		const int32_t count = 4;
		const char* labels[count] = { JP("再開"), JP("設定（キー配置・音量）"), JP("操作説明"), JP("キャラ選択へ戻る") };
		const float panelW = 420.0f, panelH = 380.0f;
		const float panelX = (1280.0f - panelW) / 2.0f, panelY = (720.0f - panelH) / 2.0f;
		const float itemX = panelX + 30.0f, itemW = panelW - 60.0f, itemH = 54.0f;
		auto itemY = [&](int32_t i) { return panelY + 90.0f + static_cast<float>(i) * (itemH + 14.0f); };

		if (MenuKeys::Up(input)) { focus = (focus + count - 1) % count; }
		if (MenuKeys::Down(input)) { focus = (focus + 1) % count; }
		bool decided = MenuKeys::Decide(input);
		const Vector2& mouse = input->GetMousePosition();
		for (int32_t i = 0; i < count; ++i) {
			if (UiKit::Inside(mouse, itemX, itemY(i), itemW, itemH)) {
				if (mouseItem != i) { focus = i; }
				mouseItem = i;
				if (input->TriggerMouse(Input::MouseButton::kLeft)) { focus = i; decided = true; }
			} else if (mouseItem == i) {
				mouseItem = -1;
			}
		}

		Result result = Result::kNone;
		if (decided) {
			switch (focus) {
			case 0: isOpen = false; result = Result::kResume; break;
			case 1: settingsMenu.Open(); break;
			case 2: showGuide = true; break;
			default: isOpen = false; result = Result::kQuit; break;
			}
		}

		// ---- 描く ----
		ui->Rect(0.0f, 0.0f, 1280.0f, 720.0f, UiColor::kDim);
		ui->Rect(panelX, panelY, panelW, panelH, UiColor::kPanel);
		ui->Frame(panelX, panelY, panelW, panelH, 3.0f, UiColor::kBorder);
		ui->Text(JP("ポーズ"), panelX + panelW / 2.0f, panelY + 20.0f, 38.0f, UiColor::kText, UiKit::Align::kCenter);
		for (int32_t i = 0; i < count; ++i) {
			bool focused = (i == focus);
			ui->Rect(itemX, itemY(i), itemW, itemH, focused ? UiColor::kFocus : UiColor::kButton);
			ui->Frame(itemX, itemY(i), itemW, itemH, 2.0f, focused ? UiColor::kAccent : UiColor::kBorder);
			ui->Text(labels[i], itemX + itemW / 2.0f, itemY(i) + 12.0f, 28.0f, UiColor::kText, UiKit::Align::kCenter);
		}
		ui->Text(JP("Esc：再開"), panelX + panelW / 2.0f, panelY + panelH - 32.0f, 16.0f, UiColor::kTextDim, UiKit::Align::kCenter);
		return result;
	}

private:
	// 操作説明（キーの名前は、今の設定から出す。キー配置を変えると説明も変わる）
	void DrawGuide(UiKit* ui)
	{
		GameSettings* s = GameSettings::GetInstance();
		auto key = [&](GameAction a) { return GameSettings::KeyName(s->GetKey(a)); };
		const float panelW = 760.0f, panelH = 460.0f;
		const float panelX = (1280.0f - panelW) / 2.0f, panelY = (720.0f - panelH) / 2.0f;
		ui->Rect(0.0f, 0.0f, 1280.0f, 720.0f, UiColor::kDim);
		ui->Rect(panelX, panelY, panelW, panelH, UiColor::kPanel);
		ui->Frame(panelX, panelY, panelW, panelH, 3.0f, UiColor::kBorder);
		ui->Text(JP("操作説明"), panelX + panelW / 2.0f, panelY + 18.0f, 36.0f, UiColor::kText, UiKit::Align::kCenter);

		struct Line { std::string name; std::string value; };
		const Line lines[] = {
			{ JP("移動"), key(GameAction::kMoveUp) + " / " + key(GameAction::kMoveLeft) + " / " + key(GameAction::kMoveDown) + " / " + key(GameAction::kMoveRight) },
			{ JP("狙う"), JP("マウス") },
			{ JP("撃つ"), JP("マウス左ボタン（押しつづける）") },
			{ JP("ロール（回避）"), key(GameAction::kRoll) + std::string(JP("　／　マウス右ボタン")) },
			{ JP("調べる・使う"), key(GameAction::kInteract) },
			{ JP("マップ・ワープ"), "Tab" },
			{ JP("ポーズ"), "Esc" },
			{ JP("フルスクリーン"), "Alt + Enter" },
		};
		float y = panelY + 84.0f;
		for (const Line& line : lines) {
			ui->Text(line.name, panelX + 50.0f, y, 26.0f, UiColor::kAccent);
			ui->Text(line.value, panelX + 300.0f, y, 26.0f, UiColor::kText);
			y += 40.0f;
		}
		ui->Text(JP("パッド：左スティック移動／右スティック狙う／RT撃つ／LT・LB・Aでロール／Xで調べる"), panelX + panelW / 2.0f, panelY + panelH - 74.0f, 16.0f, UiColor::kTextDim, UiKit::Align::kCenter);
		ui->Text(JP("スペース・Esc・クリックで戻る"), panelX + panelW / 2.0f, panelY + panelH - 40.0f, 18.0f, UiColor::kText, UiKit::Align::kCenter);
	}

	bool isOpen = false;
	bool showGuide = false;
	int32_t focus = 0;
	int32_t mouseItem = -1;
	SettingsMenu settingsMenu;
};
