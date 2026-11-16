#include "ConfirmDialog.h"
#include <algorithm>
#include <cmath>
#include "WinApp.h"
#include "SpriteCommon.h"
#include "Input.h"

using namespace MathUtility;

namespace {
	Vector2 ScreenCenter()
	{
		return { static_cast<float>(WinApp::kClientWidth) / 2.0f, static_cast<float>(WinApp::kClientHeight) / 2.0f };
	}
}

void ConfirmDialog::Initialize(SpriteCommon* spriteCommon)
{
	const Vector2 screenSize = { static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) };
	const Vector2 center = ScreenCenter();

	background = std::make_unique<Sprite>();
	background->Initialize(spriteCommon, "resources/white.png");
	background->SetSize(screenSize);

	panel = std::make_unique<Sprite>();
	panel->Initialize(spriteCommon, kTexturePanel);
	panel->SetAnchorPoint({ 0.5f, 0.5f });

	message = std::make_unique<Sprite>();
	message->Initialize(spriteCommon, kTextureMessageNext);
	message->SetAnchorPoint({ 0.5f, 0.0f });

	const char* buttonTextures[2] = { kTextureYes, kTextureNo };
	for (size_t i = 0; i < buttons.size(); ++i) {
		buttons[i] = std::make_unique<Sprite>();
		buttons[i]->Initialize(spriteCommon, buttonTextures[i]);
		buttons[i]->SetAnchorPoint({ 0.5f, 0.5f });
	}

	hint = std::make_unique<Sprite>();
	hint->Initialize(spriteCommon, kTextureHint);
	hint->SetAnchorPoint({ 0.5f, 0.0f });
	hint->SetPosition({ center.x, center.y + kPanelHeight / 2.0f + 20.0f });
	isOpen = false;
}

void ConfirmDialog::Open(Kind newKind, bool defaultYes)
{
	kind = newKind;
	isOpen = true;
	selected = defaultYes ? 0 : 1;
	openTime = 0.0f;
	wasStickTilted = true; // 開いた瞬間にスティックが倒れていても動かないように
	hasLastMouse = false;
	buttonLift = {};
	message->ChangeTexture(kind == Kind::kNextFloor ? kTextureMessageNext : kTextureMessageReturn);
	message->SetSize(message->GetTextureSize());
}

Vector2 ConfirmDialog::GetButtonCenter(size_t index) const
{
	Vector2 center = ScreenCenter();
	float offset = (index == 0) ? -kButtonSpacing / 2.0f : kButtonSpacing / 2.0f;
	return { center.x + offset, center.y + 60.0f };
}

ConfirmDialog::Answer ConfirmDialog::Update(Input* input, float deltaTime)
{
	if (!isOpen) {
		return Answer::kNone;
	}
	openTime += deltaTime;

	// 左右で選ぶ（はい・いいえの2つなので、どちらに押しても切りかわる）
	bool toggle = input->TriggerKey(DIK_A) || input->TriggerKey(DIK_LEFT) || input->TriggerPad(XINPUT_GAMEPAD_DPAD_LEFT) ||
		input->TriggerKey(DIK_D) || input->TriggerKey(DIK_RIGHT) || input->TriggerPad(XINPUT_GAMEPAD_DPAD_RIGHT);
	float stickX = input->GetLeftStick().x;
	bool tilted = std::fabs(stickX) >= 0.5f;
	if (tilted && !wasStickTilted) {
		selected = (stickX < 0.0f) ? 0 : 1;
	}
	wasStickTilted = tilted;
	if (toggle) {
		selected = 1 - selected;
	}

	// マウス：動いたときだけ、乗っているボタンを選ぶ
	const Vector2& mouse = input->GetMousePosition();
	int32_t hovered = -1;
	for (size_t i = 0; i < buttons.size(); ++i) {
		Vector2 c = GetButtonCenter(i);
		if (std::fabs(mouse.x - c.x) < kButtonWidth / 2.0f && std::fabs(mouse.y - c.y) < kButtonHeight / 2.0f) {
			hovered = static_cast<int32_t>(i);
		}
	}
	if (!hasLastMouse) {
		lastMouse = mouse;
		hasLastMouse = true;
	}
	bool mouseMoved = std::fabs(mouse.x - lastMouse.x) > 1.0f || std::fabs(mouse.y - lastMouse.y) > 1.0f;
	lastMouse = mouse;
	if (hovered >= 0 && mouseMoved) {
		selected = static_cast<size_t>(hovered);
	}

	// 見た目：ふわっと大きくなって出てくる。選ばれているボタンは大きく明るく、もう一方は暗く
	const Vector2 center = ScreenCenter();
	float appear = std::clamp(openTime / 0.18f, 0.0f, 1.0f);
	float pop = 0.9f + 0.1f * appear;
	background->SetColor({ 0.0f, 0.0f, 0.03f, 0.5f * appear });
	background->Update();
	panel->SetPosition(center);
	panel->SetSize({ kPanelWidth * pop, kPanelHeight * pop });
	panel->SetColor({ 1.0f, 1.0f, 1.0f, appear });
	panel->Update();
	message->SetPosition({ center.x, center.y - kPanelHeight / 2.0f * pop + 30.0f });
	message->SetColor({ 1.0f, 1.0f, 1.0f, appear });
	message->Update();
	for (size_t i = 0; i < buttons.size(); ++i) {
		buttonLift[i] = Lerp(buttonLift[i], (i == selected) ? 1.0f : 0.0f, 0.3f);
		float scale = (1.0f + 0.08f * buttonLift[i]) * pop;
		float brightness = 0.55f + 0.45f * buttonLift[i];
		buttons[i]->SetPosition(GetButtonCenter(i));
		buttons[i]->SetSize({ kButtonWidth * scale, kButtonHeight * scale });
		buttons[i]->SetColor({ brightness, brightness, brightness, appear });
		buttons[i]->Update();
	}
	hint->SetColor({ 1.0f, 1.0f, 1.0f, appear });
	hint->Update();

	// Esc・Ⓑ はいつでも「いいえ」
	if (input->TriggerKey(DIK_ESCAPE) || input->TriggerPad(XINPUT_GAMEPAD_B)) {
		isOpen = false;
		return Answer::kNo;
	}
	// 決定（開いてすぐは受けない。撃っていたボタンをそのまま押してしまわないように）
	if (openTime >= kInputDelay) {
		bool decide = input->TriggerKey(DIK_SPACE) || input->TriggerPad(XINPUT_GAMEPAD_A);
		if (input->TriggerMouse(Input::MouseButton::kLeft) && hovered >= 0) {
			selected = static_cast<size_t>(hovered);
			decide = true;
		}
		if (decide) {
			isOpen = false;
			return (selected == 0) ? Answer::kYes : Answer::kNo;
		}
	}
	return Answer::kNone;
}

void ConfirmDialog::Draw()
{
	if (!isOpen) {
		return;
	}
	background->Draw();
	panel->Draw();
	message->Draw();
	for (std::unique_ptr<Sprite>& button : buttons) {
		button->Draw();
	}
	hint->Draw();
}