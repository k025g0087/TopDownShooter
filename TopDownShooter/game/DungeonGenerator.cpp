#include "DungeonGenerator.h"
#include <algorithm>

using namespace DungeonConfig;

int32_t DungeonLayout::GetRoomAtCell(int32_t gridX, int32_t gridY) const
{
	if (gridX < 0 || gridY < 0 || gridX >= kGridSize || gridY >= kGridSize) {
		return -1;
	}
	return cellToRoom[static_cast<size_t>(gridY * kGridSize + gridX)];
}

int32_t DungeonGenerator::OffsetX(int32_t direction)
{
	if (direction == kDirectionRight) { return 1; }
	if (direction == kDirectionLeft) { return -1; }
	return 0;
}

int32_t DungeonGenerator::OffsetY(int32_t direction)
{
	if (direction == kDirectionDown) { return 1; }
	if (direction == kDirectionUp) { return -1; }
	return 0;
}

DungeonLayout DungeonGenerator::Generate(std::mt19937& random, int32_t roomCount)
{
	DungeonLayout layout;
	// うまくいくまで作り直す（ほとんど1〜2回で成功する）
	for (int32_t attempt = 0; attempt < 100; ++attempt) {
		if (TryGenerate(random, roomCount, layout)) {
			return layout;
		}
	}
	// 万が一失敗し続けたら、最後に作ったものをそのまま使う
	return layout;
}

bool DungeonGenerator::TryGenerate(std::mt19937& random, int32_t roomCount, DungeonLayout& layout)
{
	layout = DungeonLayout{};
	layout.cellToRoom.fill(-1);

	// スタートの部屋：マス目の真ん中
	RoomNode start;
	start.gridX = kGridSize / 2;
	start.gridY = kGridSize / 2;
	start.type = RoomType::kStart;
	layout.rooms.push_back(start);
	layout.cellToRoom[static_cast<size_t>(start.gridY * kGridSize + start.gridX)] = 0;

	// 今ある部屋から1つ選んで、空いている隣に部屋を足していく
	// 足す先のマスの隣に部屋が1つだけ（＝つなぐ元だけ）の場所にしか足さないので、枝分かれした迷路のような形になる
	std::uniform_int_distribution<int32_t> directionDistribution(0, kDirectionCount - 1);
	int32_t tries = 0;
	while (static_cast<int32_t>(layout.rooms.size()) < roomCount && tries < 2000) {
		tries++;
		std::uniform_int_distribution<size_t> roomDistribution(0, layout.rooms.size() - 1);
		size_t fromIndex = roomDistribution(random);
		int32_t direction = directionDistribution(random);
		int32_t x = layout.rooms[fromIndex].gridX + OffsetX(direction);
		int32_t y = layout.rooms[fromIndex].gridY + OffsetY(direction);
		if (x < 0 || y < 0 || x >= kGridSize || y >= kGridSize || layout.GetRoomAtCell(x, y) >= 0) {
			continue;
		}
		int32_t occupiedNeighbors = 0;
		for (int32_t d = 0; d < kDirectionCount; ++d) {
			if (layout.GetRoomAtCell(x + OffsetX(d), y + OffsetY(d)) >= 0) {
				occupiedNeighbors++;
			}
		}
		if (occupiedNeighbors != 1) {
			continue;
		}

		RoomNode room;
		room.gridX = x;
		room.gridY = y;
		room.type = RoomType::kCombat;
		room.depth = layout.rooms[fromIndex].depth + 1;
		int32_t newIndex = static_cast<int32_t>(layout.rooms.size());
		room.neighbors[static_cast<size_t>(Opposite(direction))] = static_cast<int32_t>(fromIndex);
		layout.rooms[fromIndex].neighbors[static_cast<size_t>(direction)] = newIndex;
		layout.rooms.push_back(room);
		layout.cellToRoom[static_cast<size_t>(y * kGridSize + x)] = newIndex;
	}
	if (static_cast<int32_t>(layout.rooms.size()) < roomCount) {
		return false;
	}

	// 行き止まり（つながりが1つだけの部屋）を集める。スタートは除く
	std::vector<int32_t> deadEnds;
	for (size_t i = 1; i < layout.rooms.size(); ++i) {
		int32_t connections = 0;
		for (int32_t neighbor : layout.rooms[i].neighbors) {
			if (neighbor >= 0) {
				connections++;
			}
		}
		if (connections == 1) {
			deadEnds.push_back(static_cast<int32_t>(i));
		}
	}
	// ボス部屋と宝部屋で、行き止まりが2つ以上いる
	if (deadEnds.size() < 2) {
		return false;
	}

	// ボス部屋：スタートから一番遠い行き止まり
	std::sort(deadEnds.begin(), deadEnds.end(), [&layout](int32_t a, int32_t b) {
		return layout.rooms[static_cast<size_t>(a)].depth > layout.rooms[static_cast<size_t>(b)].depth;
	});
	layout.bossRoom = deadEnds[0];
	layout.rooms[static_cast<size_t>(layout.bossRoom)].type = RoomType::kBoss;
	// ボス部屋がスタートに近すぎると探索にならないので、作り直す
	if (layout.rooms[static_cast<size_t>(layout.bossRoom)].depth < 3) {
		return false;
	}

	// 宝部屋：残りの行き止まりからランダム
	std::uniform_int_distribution<size_t> deadEndDistribution(1, deadEnds.size() - 1);
	layout.treasureRoom = deadEnds[deadEndDistribution(random)];
	layout.rooms[static_cast<size_t>(layout.treasureRoom)].type = RoomType::kTreasure;
	layout.startRoom = 0;
	return true;
}
