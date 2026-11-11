#pragma once
#include <array>
#include <cstdint>
#include <random>
#include <vector>

// ダンジョン（神殿の1フロア）の大きさの決まり
namespace DungeonConfig {
	inline constexpr float kTileSize = 2.0f;        // マップチップ1マスの大きさ
	inline constexpr int32_t kRoomWidth = 17;       // 部屋の横のマス数（外の壁を含む）
	inline constexpr int32_t kRoomHeight = 13;      // 部屋の縦のマス数（外の壁を含む）
	inline constexpr int32_t kCorridorLength = 3;   // 部屋と部屋の間の通路の長さ（マス）
	inline constexpr int32_t kSlotWidth = kRoomWidth + kCorridorLength;   // 部屋1つ分の区画の横幅
	inline constexpr int32_t kSlotHeight = kRoomHeight + kCorridorLength; // 部屋1つ分の区画の縦幅
	inline constexpr int32_t kGridSize = 7;         // 部屋を並べるマス目の大きさ（7x7）
	inline constexpr int32_t kDoorCenterX = kRoomWidth / 2;  // 上下の扉の真ん中の列
	inline constexpr int32_t kDoorCenterY = kRoomHeight / 2; // 左右の扉の真ん中の行
	inline constexpr int32_t kDoorHalfWidth = 0;    // 扉と通路の幅の半分（0：1マス／1：3マス）
}

// 部屋の種類
enum class RoomType {
	kStart,    // 最初の部屋（敵なし）
	kCombat,   // 戦闘部屋（入ると扉が閉まって、全部倒すと開く）
	kTreasure, // 宝部屋（中身はあとで決める）
	kBoss,     // ボス部屋（一番遠い行き止まり）
};

// 向き（部屋のつながりに使う。上＝画面の上＝奥）
enum Direction : int32_t {
	kDirectionUp = 0,
	kDirectionRight = 1,
	kDirectionDown = 2,
	kDirectionLeft = 3,
	kDirectionCount = 4,
};

// 部屋1つ
struct RoomNode {
	int32_t gridX = 0;              // マス目の上の位置
	int32_t gridY = 0;
	RoomType type = RoomType::kCombat;
	int32_t templateIndex = 0;      // 使う部屋のひな形（種類ごとのリストの番号）
	std::array<int32_t, kDirectionCount> neighbors = { -1, -1, -1, -1 }; // つながっている部屋（なければ -1）
	int32_t depth = 0;              // スタートから何部屋目か
};

// 1フロアの部屋の並び
struct DungeonLayout {
	std::vector<RoomNode> rooms;
	// マス目 → 部屋の番号（部屋がなければ -1）
	std::array<int32_t, DungeonConfig::kGridSize* DungeonConfig::kGridSize> cellToRoom = {};
	int32_t startRoom = 0;
	int32_t bossRoom = -1;
	int32_t treasureRoom = -1;

	int32_t GetRoomAtCell(int32_t gridX, int32_t gridY) const;
};

// 部屋の並びをランダムに作る係
class DungeonGenerator
{
public:
	// roomCount：部屋の数（スタート・ボス・宝部屋を含む）
	static DungeonLayout Generate(std::mt19937& random, int32_t roomCount);

	// 向きの反対
	static Direction Opposite(int32_t direction) { return static_cast<Direction>((direction + 2) % kDirectionCount); }
	// 向きのマス目の移動量
	static int32_t OffsetX(int32_t direction);
	static int32_t OffsetY(int32_t direction);

private:
	// 1回分の生成（行き止まりが足りないなど、うまくいかなければ false）
	static bool TryGenerate(std::mt19937& random, int32_t roomCount, DungeonLayout& layout);
};