#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Mathutility.h"
#include "Sprite.h"

class SpriteCommon;

// ステージの画面表示（HPのハート・ダメージを受けたときの赤い点滅・ボスのHPバー・ゲームオーバー／クリア画面）
class StageHud
{
public:
	// 使う画像（シーンで読み込む）
	static inline const char* kTextureHeartFull = "resources/heart_full.png";
	static inline const char* kTextureHeartHalf = "resources/heart_half.png";
	static inline const char* kTextureHeartEmpty = "resources/heart_empty.png";
	static inline const char* kTextureGameOver = "resources/gameover.png";
	static inline const char* kTextureWhite = "resources/white.png";
	static inline const char* kTextureGameClear = "resources/gameclear.png";
	static inline const char* kTextureDigits = "resources/hud_digits.png"; // 「0123456789+%:」

	void Initialize(SpriteCommon* spriteCommon);
	// hp：今のHP、maxHp：最大HP（どちらも1でハート半分）
	void Update(int32_t hp, int32_t maxHp, float deltaTime);
	void Draw();

	// ダメージを受けた（画面を赤く光らせて、ハートを揺らす）
	void OnDamaged();
	// ゲームオーバー画面を出す
	void ShowGameOver();
	// クリア画面を出す（seconds：かかった時間）
	void ShowClear(float seconds);
	// ボスのHPバー（nameTexture：名前の画像。nullptr で消す）
	void SetBoss(const char* nameTexture);
	void SetBossHpRate(float rate) { bossHpRate = rate; }
	void Reset();

private:
	SpriteCommon* spriteCommon = nullptr;
	std::vector<std::unique_ptr<Sprite>> hearts;
	std::unique_ptr<Sprite> damageFlash;   // 画面全体の赤い板
	std::unique_ptr<Sprite> gameOverBack;  // 画面全体の黒い板
	std::unique_ptr<Sprite> gameOverText;
	std::unique_ptr<Sprite> gameClearText;
	std::array<std::unique_ptr<Sprite>, 5> timeDigits; // 「mm:ss」
	// ボスのHPバー
	std::unique_ptr<Sprite> bossName;
	std::unique_ptr<Sprite> bossBarBack;
	std::unique_ptr<Sprite> bossBarDelay; // 減った分を少し遅れて縮める（どれだけ削ったか分かるように）
	std::unique_ptr<Sprite> bossBarFill;
	bool showBoss = false;
	float bossHpRate = 1.0f;
	float bossDelayRate = 1.0f;
	bool gameClear = false;
	std::string clearTimeText;

	float flashTimer = 0.0f;    // 赤い点滅の残り時間
	float shakeTimer = 0.0f;    // ハートが揺れる残り時間
	bool gameOver = false;
	float gameOverTime = 0.0f;  // ゲームオーバーになってからの時間（だんだん出す）
	float time = 0.0f;

	// ---- 見た目の数値（ピクセル）----
	static inline const float kHeartLeft = 20.0f;
	static inline const float kHeartTop = 18.0f;
	static inline const float kHeartSpacing = 46.0f;
	static inline const float kFlashTime = 0.35f;
	static inline const float kShakeTime = 0.3f;
	static inline const float kBossBarWidth = 640.0f;
	static inline const float kBossBarHeight = 16.0f;
	static inline const float kBossBarTop = 44.0f;
};