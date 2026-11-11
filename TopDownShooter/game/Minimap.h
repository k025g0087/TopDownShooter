#pragma once
#include <memory>
#include <vector>
#include "Mathutility.h"
#include "Sprite.h"
#include "DungeonGenerator.h"

class SpriteCommon;

// ミニマップ（画面の右上。行ったことのある部屋だけ見える）
class Minimap
{
public:
	// 使う画像（真っ白な小さい四角。色はスプライトごとに付ける）
	static inline const char* kTexture = "resources/white.png";

	void Initialize(SpriteCommon* spriteCommon, const DungeonLayout& layout);
	// visited：部屋ごとの「行ったことがあるか」、currentRoom：今いる部屋（通路なら -1）
	void Update(const std::vector<bool>& visited, int32_t currentRoom);
	void Draw();

private:
	DungeonLayout layout;
	std::unique_ptr<Sprite> background;
	std::vector<std::unique_ptr<Sprite>> roomSprites;     // 部屋と同じ番号
	struct Connection {
		int32_t roomA = 0;
		int32_t roomB = 0;
		std::unique_ptr<Sprite> sprite;
	};
	std::vector<Connection> connections;
	// 部屋ごとに、今見えているか（Draw で使う）
	std::vector<bool> roomShown;
	std::vector<bool> connectionShown;
	float time = 0.0f;

	// ---- 見た目の数値（ピクセル）----
	static inline const float kCellWidth = 30.0f;   // マス目1つ分の横幅
	static inline const float kCellHeight = 22.0f;  // マス目1つ分の縦幅
	static inline const float kRoomWidth = 24.0f;   // 部屋の四角の大きさ
	static inline const float kRoomHeight = 16.0f;
	static inline const float kMargin = 16.0f;      // 画面の端からの距離
	static inline const float kPadding = 10.0f;     // 背景の余白
};