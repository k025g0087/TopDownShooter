#include "Minimap.h"
#include <algorithm>
#include <cmath>
#include "WinApp.h"
#include "SpriteCommon.h"

namespace {
	const Vector4 kColorBackground = { 0.0f, 0.0f, 0.0f, 0.45f };
	const Vector4 kColorUnknown = { 0.45f, 0.45f, 0.5f, 0.9f };   // 見えているけど行っていない部屋
	const Vector4 kColorVisited = { 0.85f, 0.85f, 0.9f, 1.0f };   // 行ったことのある部屋
	const Vector4 kColorCurrent = { 1.0f, 0.85f, 0.25f, 1.0f };   // 今いる部屋
	const Vector4 kColorBoss = { 0.95f, 0.3f, 0.3f, 1.0f };       // ボス部屋
	const Vector4 kColorTreasure = { 0.95f, 0.75f, 0.2f, 1.0f };  // 宝部屋
	const Vector4 kColorConnection = { 0.7f, 0.7f, 0.75f, 1.0f };
}

void Minimap::Initialize(SpriteCommon* spriteCommon, const DungeonLayout& sourceLayout)
{
	layout = sourceLayout;
	roomSprites.clear();
	connections.clear();

	// 部屋がある範囲（マス目）を調べて、その範囲だけを画面の右上に描く
	int32_t minX = DungeonConfig::kGridSize, minY = DungeonConfig::kGridSize, maxX = 0, maxY = 0;
	for (const RoomNode& room : layout.rooms) {
		minX = (std::min)(minX, room.gridX);
		minY = (std::min)(minY, room.gridY);
		maxX = (std::max)(maxX, room.gridX);
		maxY = (std::max)(maxY, room.gridY);
	}
	float width = static_cast<float>(maxX - minX + 1) * kCellWidth;
	float height = static_cast<float>(maxY - minY + 1) * kCellHeight;
	float left = static_cast<float>(WinApp::kClientWidth) - kMargin - kPadding - width;
	float top = kMargin + kPadding;

	background = std::make_unique<Sprite>();
	background->Initialize(spriteCommon, kTexture);
	background->SetPosition({ left - kPadding, top - kPadding });
	background->SetSize({ width + kPadding * 2.0f, height + kPadding * 2.0f });
	background->SetColor(kColorBackground);
	background->Update();

	// マス目の位置 → 画面の位置（部屋の四角の真ん中）
	auto cellCenter = [&](int32_t gridX, int32_t gridY) {
		return Vector2{
			left + (static_cast<float>(gridX - minX) + 0.5f) * kCellWidth,
			top + (static_cast<float>(gridY - minY) + 0.5f) * kCellHeight };
		};

	// つながり（右と下だけ見れば、全部のつながりを1回ずつ数えられる）
	for (size_t i = 0; i < layout.rooms.size(); ++i) {
		const RoomNode& room = layout.rooms[i];
		for (int32_t direction : { static_cast<int32_t>(kDirectionRight), static_cast<int32_t>(kDirectionDown) }) {
			int32_t neighbor = room.neighbors[static_cast<size_t>(direction)];
			if (neighbor < 0) {
				continue;
			}
			Connection connection;
			connection.roomA = static_cast<int32_t>(i);
			connection.roomB = neighbor;
			connection.sprite = std::make_unique<Sprite>();
			connection.sprite->Initialize(spriteCommon, kTexture);
			connection.sprite->SetAnchorPoint({ 0.5f, 0.5f });
			Vector2 a = cellCenter(room.gridX, room.gridY);
			Vector2 b = cellCenter(layout.rooms[static_cast<size_t>(neighbor)].gridX, layout.rooms[static_cast<size_t>(neighbor)].gridY);
			connection.sprite->SetPosition({ (a.x + b.x) / 2.0f, (a.y + b.y) / 2.0f });
			connection.sprite->SetSize(direction == kDirectionRight ? Vector2{ kCellWidth - kRoomWidth + 2.0f, 5.0f } : Vector2{ 5.0f, kCellHeight - kRoomHeight + 2.0f });
			connection.sprite->SetColor(kColorConnection);
			connection.sprite->Update();
			connections.push_back(std::move(connection));
		}
	}

	// 部屋
	for (const RoomNode& room : layout.rooms) {
		std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon, kTexture);
		sprite->SetAnchorPoint({ 0.5f, 0.5f });
		sprite->SetPosition(cellCenter(room.gridX, room.gridY));
		sprite->SetSize({ kRoomWidth, kRoomHeight });
		roomSprites.push_back(std::move(sprite));
	}

	roomShown.assign(layout.rooms.size(), false);
	connectionShown.assign(connections.size(), false);
	time = 0.0f;
}

void Minimap::Update(const std::vector<bool>& visited, int32_t currentRoom)
{
	time += 1.0f / 60.0f;

	// 見える部屋：行ったことのある部屋だけ（入るまで先は分からない）
	for (size_t i = 0; i < layout.rooms.size(); ++i) {
		bool shown = visited[i];
		roomShown[i] = shown;
		if (!shown) {
			continue;
		}

		// 色：今いる部屋 ＞ ボス・宝 ＞ 行ったことがある ＞ まだ
		Vector4 color = visited[i] ? kColorVisited : kColorUnknown;
		const RoomNode& room = layout.rooms[i];
		if (room.type == RoomType::kBoss) {
			color = kColorBoss;
		} else if (room.type == RoomType::kTreasure) {
			color = kColorTreasure;
		}
		if (!visited[i]) {
			// 行っていない特別な部屋は、少し暗くする
			color = { color.x * 0.6f, color.y * 0.6f, color.z * 0.6f, color.w };
		}
		float scale = 1.0f;
		if (static_cast<int32_t>(i) == currentRoom) {
			color = kColorCurrent;
			// 今いる部屋は少し脈打たせる
			scale = 1.0f + 0.08f * std::sin(time * 6.0f);
		}
		roomSprites[i]->SetColor(color);
		roomSprites[i]->SetSize({ kRoomWidth * scale, kRoomHeight * scale });
		roomSprites[i]->Update();
	}

	// つながり（通路）：どちらかの部屋に行ったことがあれば見える（部屋のどの向きに次の部屋があるかが分かる）
	for (size_t i = 0; i < connections.size(); ++i) {
		connectionShown[i] = visited[static_cast<size_t>(connections[i].roomA)] || visited[static_cast<size_t>(connections[i].roomB)];
	}
}

void Minimap::Draw()
{
	background->Draw();
	for (size_t i = 0; i < connections.size(); ++i) {
		if (connectionShown[i]) {
			connections[i].sprite->Draw();
		}
	}
	for (size_t i = 0; i < roomSprites.size(); ++i) {
		if (roomShown[i]) {
			roomSprites[i]->Draw();
		}
	}
}