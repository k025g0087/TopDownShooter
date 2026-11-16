#include "DungeonMap.h"
#include <algorithm>
#include <cmath>
#include "Object3dCommon.h"

using namespace MathUtility;
using namespace DungeonConfig;

void DungeonMap::Build(const DungeonLayout& sourceLayout, const RoomTemplateLibrary& library, std::mt19937& random)
{
	layout = sourceLayout;
	tiles.assign(static_cast<size_t>(kMapWidth * kMapHeight), Tile::kVoid);
	doorOwners.assign(static_cast<size_t>(kMapWidth * kMapHeight), -1);
	roomAreas.assign(layout.rooms.size(), RoomArea{});
	objects.clear();
	flowDistance.clear(); // 道順の地図は作り直す
	flowMinX = 0; flowMinY = 0; flowMaxX = -1; flowMaxY = -1;
	exitStairsTileX = -1;
	exitStairsTileY = -1;

	// ---- 部屋：ひな形をマップに書き込む ----
	for (size_t i = 0; i < layout.rooms.size(); ++i) {
		RoomNode& room = layout.rooms[i];
		RoomArea& area = roomAreas[i];
		area.originX = room.gridX * kSlotWidth;
		area.originY = room.gridY * kSlotHeight;
		if (room.type == RoomType::kExit) {
			BuildExitRoom(area);
		} else if (room.type == RoomType::kTreasure || room.type == RoomType::kShop) {
			// 宝部屋とショップは、同じ小さな部屋（真ん中に宝箱・ガチャを置く）
			BuildTreasureRoom(room, area);
		} else {
			BuildTemplateRoom(room, area, library, random);
		}

		// ---- 扉：隣に部屋がある向きだけ、外の壁の真ん中3マスを扉にする ----
		for (int32_t direction = 0; direction < kDirectionCount; ++direction) {
			if (room.neighbors[static_cast<size_t>(direction)] < 0) {
				continue;
			}
			for (int32_t k = -kDoorHalfWidth; k <= kDoorHalfWidth; ++k) {
				int32_t doorX = 0, doorY = 0, innerX = 0, innerY = 0;
				switch (direction) {
				case kDirectionUp:
					doorX = kDoorCenterX + k; doorY = 0; innerX = doorX; innerY = 1;
					break;
				case kDirectionDown:
					doorX = kDoorCenterX + k; doorY = kRoomHeight - 1; innerX = doorX; innerY = kRoomHeight - 2;
					break;
				case kDirectionLeft:
					doorX = 0; doorY = kDoorCenterY + k; innerX = 1; innerY = doorY;
					break;
				case kDirectionRight:
				default:
					doorX = kRoomWidth - 1; doorY = kDoorCenterY + k; innerX = kRoomWidth - 2; innerY = doorY;
					break;
				}
				SetTile(area.originX + doorX, area.originY + doorY, Tile::kDoor);
				doorOwners[static_cast<size_t>((area.originY + doorY) * kMapWidth + area.originX + doorX)] = static_cast<int32_t>(i);
				// 扉のすぐ内側は、ひな形で柱を置いていても床にする（ふさがらないように）
				SetTile(area.originX + innerX, area.originY + innerY, Tile::kFloor);
			}
		}

		// ---- 通路：右と下の部屋へだけ作る（左と上は、相手の部屋が作るので重ならない）----
		if (room.neighbors[kDirectionRight] >= 0) {
			for (int32_t x = kRoomWidth; x < kSlotWidth; ++x) {
				for (int32_t k = -kDoorHalfWidth - 1; k <= kDoorHalfWidth + 1; ++k) {
					// 両端は壁、間は床
					Tile tile = (k == -kDoorHalfWidth - 1 || k == kDoorHalfWidth + 1) ? Tile::kWall : Tile::kFloor;
					SetTile(area.originX + x, area.originY + kDoorCenterY + k, tile);
				}
			}
		}
		if (room.neighbors[kDirectionDown] >= 0) {
			for (int32_t y = kRoomHeight; y < kSlotHeight; ++y) {
				for (int32_t k = -kDoorHalfWidth - 1; k <= kDoorHalfWidth + 1; ++k) {
					// 両端は壁、間は床
					Tile tile = (k == -kDoorHalfWidth - 1 || k == kDoorHalfWidth + 1) ? Tile::kWall : Tile::kFloor;
					SetTile(area.originX + kDoorCenterX + k, area.originY + y, tile);
				}
			}
		}
	}

	// ---- 見た目：床（部屋ごとに1枚。通路は幅を変えられるように1マスずつ）----
	for (size_t i = 0; i < layout.rooms.size(); ++i) {
		const RoomNode& room = layout.rooms[i];
		const RoomArea& area = roomAreas[i];
		const Vector2 tileHalfSize = { kTileSize * 0.5f, kTileSize * 0.5f };
		if (room.type == RoomType::kExit) {
			// 出口の小部屋：床のあるマスに1枚ずつ置く（階段の穴のところは置かない）
			for (int32_t y = 0; y < kRoomHeight; ++y) {
				for (int32_t x = 0; x < kRoomWidth; ++x) {
					int32_t mapX = area.originX + x;
					int32_t mapY = area.originY + y;
					bool inStairs = (mapX == exitStairsTileX && mapY == exitStairsTileY);
					if (GetTile(mapX, mapY) != Tile::kWall && !inStairs) {
						AddObject(kModelFloorExit, TileCenter(mapX, mapY), tileHalfSize, -1);
					}
				}
			}
		}
		if (room.type == RoomType::kTreasure || room.type == RoomType::kShop) {
			// 宝箱・ガチャの小部屋：床のあるマスに1枚ずつ置く（壁で埋めた部分に、大きな床の板が見えないように）
			const char* smallFloor = room.type == RoomType::kShop ? kModelFloorShop : kModelFloorTreasure;
			for (int32_t y = 0; y < kRoomHeight; ++y) {
				for (int32_t x = 0; x < kRoomWidth; ++x) {
					int32_t mapX = area.originX + x;
					int32_t mapY = area.originY + y;
					if (GetTile(mapX, mapY) != Tile::kWall) {
						AddObject(smallFloor, TileCenter(mapX, mapY), tileHalfSize, -1);
					}
				}
			}
		}
		const char* floorModel = kModelFloorRoom;
		if (room.type == RoomType::kBoss) {
			floorModel = kModelFloorBoss;
		} else if (room.type == RoomType::kTreasure) {
			floorModel = kModelFloorTreasure;
		} else if (room.type == RoomType::kShop) {
			floorModel = kModelFloorShop;
		}
		const Vector2 roomHalf = { static_cast<float>(kRoomWidth) * kTileSize * 0.5f, static_cast<float>(kRoomHeight) * kTileSize * 0.5f };
		if (room.type != RoomType::kExit && room.type != RoomType::kTreasure && room.type != RoomType::kShop) {
			AddObject(floorModel, GetRoomCenter(static_cast<int32_t>(i)), roomHalf, -1);
		}

		if (room.neighbors[kDirectionRight] >= 0) {
			for (int32_t x = kRoomWidth; x < kSlotWidth; ++x) {
				for (int32_t k = -kDoorHalfWidth; k <= kDoorHalfWidth; ++k) {
					AddObject(kModelFloorCorridor, TileCenter(area.originX + x, area.originY + kDoorCenterY + k), tileHalfSize, -1);
				}
			}
		}
		if (room.neighbors[kDirectionDown] >= 0) {
			for (int32_t y = kRoomHeight; y < kSlotHeight; ++y) {
				for (int32_t k = -kDoorHalfWidth; k <= kDoorHalfWidth; ++k) {
					AddObject(kModelFloorCorridor, TileCenter(area.originX + kDoorCenterX + k, area.originY + y), tileHalfSize, -1);
				}
			}
		}
	}

	// ---- 見た目：壁（床の隣にある壁だけ。埋まって見えない壁は作らない）と扉 ----
	const Vector2 tileHalf = { kTileSize * 0.5f, kTileSize * 0.5f };
	for (int32_t y = 0; y < kMapHeight; ++y) {
		for (int32_t x = 0; x < kMapWidth; ++x) {
			Tile tile = GetTile(x, y);
			if (tile == Tile::kDoor) {
				AddObject(kModelDoor, TileCenter(x, y), tileHalf, doorOwners[static_cast<size_t>(y * kMapWidth + x)]);
				continue;
			}
			if (tile != Tile::kWall) {
				continue;
			}
			bool visible = false;
			for (int32_t dy = -1; dy <= 1 && !visible; ++dy) {
				for (int32_t dx = -1; dx <= 1; ++dx) {
					Tile neighbor = GetTile(x + dx, y + dy);
					if (neighbor == Tile::kFloor || neighbor == Tile::kDoor) {
						visible = true;
						break;
					}
				}
			}
			if (visible) {
				AddObject(kModelWall, TileCenter(x, y), tileHalf, -1);
			}
		}
	}
}

void DungeonMap::BuildTemplateRoom(RoomNode& room, RoomArea& area, const RoomTemplateLibrary& library, std::mt19937& random)
{
	// 種類に合うひな形をランダムに1つ選んで、1文字ずつマップに書き込む
	const std::vector<RoomTemplate>& templates = library.Get(room.type);
	std::uniform_int_distribution<size_t> templateDistribution(0, templates.size() - 1);
	room.templateIndex = static_cast<int32_t>(templateDistribution(random));
	const RoomTemplate& roomTemplate = templates[static_cast<size_t>(room.templateIndex)];
	for (int32_t y = 0; y < kRoomHeight; ++y) {
		for (int32_t x = 0; x < kRoomWidth; ++x) {
			char c = roomTemplate.Get(x, y);
			int32_t mapX = area.originX + x;
			int32_t mapY = area.originY + y;
			if (c == '#' || c == 'o') {
				SetTile(mapX, mapY, Tile::kWall);
			} else {
				SetTile(mapX, mapY, Tile::kFloor);
				// 敵が出てくる場所（e：ランダム／c：突進／s：射撃／w：群れ／t：重装）
				if (c == 'e' || c == 'c' || c == 's' || c == 'w' || c == 't') {
					area.spawnPoints.push_back({ TileCenter(mapX, mapY), c });
				}
			}
		}
	}
}

void DungeonMap::BuildTreasureRoom(const RoomNode& room, const RoomArea& area)
{
	// まわりは全部壁。真ん中の小さな四角だけ床にする
	for (int32_t y = 0; y < kRoomHeight; ++y) {
		for (int32_t x = 0; x < kRoomWidth; ++x) {
			bool inside = x >= kTreasureLeft && x <= kTreasureRight && y >= kTreasureTop && y <= kTreasureBottom;
			SetTile(area.originX + x, area.originY + y, inside ? Tile::kFloor : Tile::kWall);
		}
	}
	// 扉のある向きごとに、扉から小さな四角まで1マス幅の道を通す
	for (int32_t direction = 0; direction < kDirectionCount; ++direction) {
		if (room.neighbors[static_cast<size_t>(direction)] < 0) {
			continue;
		}
		for (int32_t step = 1; step < kRoomWidth; ++step) {
			int32_t x = kDoorCenterX, y = kDoorCenterY;
			switch (direction) {
			case kDirectionUp: y = step; break;
			case kDirectionDown: y = kRoomHeight - 1 - step; break;
			case kDirectionLeft: x = step; break;
			case kDirectionRight:
			default: x = kRoomWidth - 1 - step; break;
			}
			if (x >= kTreasureLeft && x <= kTreasureRight && y >= kTreasureTop && y <= kTreasureBottom) {
				break;
			}
			SetTile(area.originX + x, area.originY + y, Tile::kFloor);
		}
	}
}

void DungeonMap::BuildExitRoom(const RoomArea& area)
{
	// まわりは全部壁。真ん中の小さな四角だけ床にする
	for (int32_t y = 0; y < kRoomHeight; ++y) {
		for (int32_t x = 0; x < kRoomWidth; ++x) {
			bool inside = x >= kExitLeft && x <= kExitRight && y >= kExitTop && y <= kExitBottom;
			SetTile(area.originX + x, area.originY + y, inside ? Tile::kFloor : Tile::kWall);
		}
	}
	// 入口の扉から小さな四角まで、1マス幅の道を通す
	int32_t direction = layout.exitDirection;
	for (int32_t step = 1; step < kRoomWidth; ++step) {
		int32_t x = kDoorCenterX, y = kDoorCenterY;
		switch (direction) {
		case kDirectionUp: y = step; break;
		case kDirectionDown: y = kRoomHeight - 1 - step; break;
		case kDirectionLeft: x = step; break;
		case kDirectionRight:
		default: x = kRoomWidth - 1 - step; break;
		}
		if (x >= kExitLeft && x <= kExitRight && y >= kExitTop && y <= kExitBottom) {
			break;
		}
		SetTile(area.originX + x, area.originY + y, Tile::kFloor);
	}
	// 階段（1マスの穴）と転送陣：入口の道をふさがないように、道をはさんで両側に並べる
	int32_t stairsX = 0, stairsY = 0;     // 階段の穴のマス（部屋の中）
	int32_t portalX = 0, portalY = 0;     // 転送陣のマス（部屋の中）
	if (direction == kDirectionUp || direction == kDirectionDown) {
		// 道は縦（真ん中の列）。階段は左、転送陣は右
		stairsX = 6; stairsY = 6;
		portalX = 10; portalY = 6;
	} else {
		// 道は横（真ん中の行）。階段は奥、転送陣は手前
		stairsX = 8; stairsY = 4;
		portalX = 8; portalY = 8;
	}
	exitStairsTileX = area.originX + stairsX;
	exitStairsTileY = area.originY + stairsY;
	exitStairsPosition = TileCenter(exitStairsTileX, exitStairsTileY);
	exitPortalPosition = TileCenter(area.originX + portalX, area.originY + portalY);
}

void DungeonMap::AddObject(const char* modelName, const Vector3& position, const Vector2& halfSize, int32_t doorRoom)
{
	MapObject mapObject;
	mapObject.object = std::make_unique<Object3d>();
	mapObject.object->Initialize(Object3dCommon::GetInstance());
	mapObject.object->SetModel(modelName);
	mapObject.object->SetTranslate(position);
	mapObject.position = position;
	mapObject.halfSize = halfSize;
	mapObject.doorRoom = doorRoom;
	objects.push_back(std::move(mapObject));
}

void DungeonMap::Update(Camera* camera, const Vector3& focus)
{
	for (MapObject& mapObject : objects) {
		// オブジェクトの大きさの分だけ範囲を広げて見る（床は大きいので、端が見えていれば描く）
		bool inRange = std::fabs(mapObject.position.x - focus.x) < kViewRangeX + mapObject.halfSize.x &&
			std::fabs(mapObject.position.z - focus.z) < kViewRangeZ + mapObject.halfSize.y;
		// 扉は、閉まっているときだけ見える
		bool doorOpen = mapObject.doorRoom >= 0 && !roomAreas[static_cast<size_t>(mapObject.doorRoom)].locked;
		mapObject.visible = inRange && !doorOpen;
		if (mapObject.visible) {
			mapObject.object->SetCamera(camera);
			mapObject.object->Update();
		}
	}
}

void DungeonMap::Draw()
{
	for (MapObject& mapObject : objects) {
		if (mapObject.visible) {
			mapObject.object->Draw();
		}
	}
}

Vector3 DungeonMap::ResolveCircle(const Vector3& position, float radius) const
{
	Vector3 result = position;
	// 角に入ったときもきれいに押し出せるように、2回くり返す
	for (int32_t iteration = 0; iteration < 2; ++iteration) {
		int32_t minX = ToTileX(result.x - radius);
		int32_t maxX = ToTileX(result.x + radius);
		int32_t minY = ToTileY(result.z + radius);
		int32_t maxY = ToTileY(result.z - radius);
		for (int32_t y = minY; y <= maxY; ++y) {
			for (int32_t x = minX; x <= maxX; ++x) {
				if (!IsSolidTile(x, y)) {
					continue;
				}
				// マスの四角の中で、円の中心に一番近い点
				float left = static_cast<float>(x) * kTileSize;
				float right = left + kTileSize;
				float top = -static_cast<float>(y) * kTileSize;
				float bottom = top - kTileSize;
				float closestX = Clamp(result.x, left, right);
				float closestZ = Clamp(result.z, bottom, top);
				float dx = result.x - closestX;
				float dz = result.z - closestZ;
				float distance = std::sqrt(dx * dx + dz * dz);
				if (distance >= radius) {
					continue;
				}
				if (distance > 0.0001f) {
					// 近い点から離れる向きに、めり込んだ分だけ押す
					float push = radius - distance;
					result.x += dx / distance * push;
					result.z += dz / distance * push;
				} else {
					// 中心がマスの中に入ってしまった：一番近い辺の外へ出す
					float toLeft = result.x - left;
					float toRight = right - result.x;
					float toTop = top - result.z;
					float toBottom = result.z - bottom;
					float smallest = (std::min)((std::min)(toLeft, toRight), (std::min)(toTop, toBottom));
					if (smallest == toLeft) {
						result.x = left - radius;
					} else if (smallest == toRight) {
						result.x = right + radius;
					} else if (smallest == toTop) {
						result.z = top + radius;
					} else {
						result.z = bottom - radius;
					}
				}
			}
		}
	}
	return result;
}

bool DungeonMap::HasLineOfSight(const Vector3& from, const Vector3& to, float radius) const
{
	Vector3 diff = { to.x - from.x, 0.0f, to.z - from.z };
	float length = std::sqrt(diff.x * diff.x + diff.z * diff.z);
	if (length < 0.01f) {
		return true;
	}
	Vector3 direction = diff / length;
	Vector3 side = { -direction.z, 0.0f, direction.x };
	// 線の上を少しずつ見ていって、壁のマスに当たったら見えない。体の幅（radius）の分、左右も見る
	const float kStep = 0.4f;
	for (float distance = 0.0f; distance <= length; distance += kStep) {
		Vector3 point = { from.x + direction.x * distance, 0.0f, from.z + direction.z * distance };
		if (IsSolidAt(point)) {
			return false;
		}
		if (radius > 0.0f) {
			if (IsSolidAt({ point.x + side.x * radius, 0.0f, point.z + side.z * radius }) ||
				IsSolidAt({ point.x - side.x * radius, 0.0f, point.z - side.z * radius })) {
				return false;
			}
		}
	}
	return true;
}

bool DungeonMap::IsFlowWalkable(int32_t x, int32_t y) const
{
	return x >= flowMinX && x <= flowMaxX && y >= flowMinY && y <= flowMaxY && !IsSolidTile(x, y);
}

void DungeonMap::UpdateFlowField(const Vector3& target, int32_t roomIndex)
{
	if (roomIndex < 0 || roomIndex >= static_cast<int32_t>(roomAreas.size())) {
		return;
	}
	size_t total = static_cast<size_t>(kMapWidth) * static_cast<size_t>(kMapHeight);
	if (flowDistance.size() != total) {
		flowDistance.assign(total, -1);
	}
	// 前に作った範囲を消す
	for (int32_t y = flowMinY; y <= flowMaxY; ++y) {
		for (int32_t x = flowMinX; x <= flowMaxX; ++x) {
			flowDistance[static_cast<size_t>(y * kMapWidth + x)] = -1;
		}
	}
	// 部屋と、そのまわり1マス（扉の外）を範囲にする
	const RoomArea& area = roomAreas[static_cast<size_t>(roomIndex)];
	flowMinX = (std::max)(0, area.originX - 1);
	flowMaxX = (std::min)(kMapWidth - 1, area.originX + kRoomWidth);
	flowMinY = (std::max)(0, area.originY - 1);
	flowMaxY = (std::min)(kMapHeight - 1, area.originY + kRoomHeight);

	int32_t startX = ToTileX(target.x);
	int32_t startY = ToTileY(target.z);
	if (!IsFlowWalkable(startX, startY)) {
		return;
	}
	// 幅優先探索：目的地のマスから、8方向に歩数を広げていく
	std::vector<int32_t> queue;
	queue.reserve(static_cast<size_t>((flowMaxX - flowMinX + 1) * (flowMaxY - flowMinY + 1)));
	flowDistance[static_cast<size_t>(startY * kMapWidth + startX)] = 0;
	queue.push_back(startY * kMapWidth + startX);
	for (size_t head = 0; head < queue.size(); ++head) {
		int32_t currentX = queue[head] % kMapWidth;
		int32_t currentY = queue[head] / kMapWidth;
		int16_t currentDistance = flowDistance[static_cast<size_t>(queue[head])];
		for (int32_t dy = -1; dy <= 1; ++dy) {
			for (int32_t dx = -1; dx <= 1; ++dx) {
				if (dx == 0 && dy == 0) {
					continue;
				}
				int32_t nextX = currentX + dx;
				int32_t nextY = currentY + dy;
				if (!IsFlowWalkable(nextX, nextY) || flowDistance[static_cast<size_t>(nextY * kMapWidth + nextX)] >= 0) {
					continue;
				}
				// 斜めは、角をかすめないように、両隣も通れるときだけ
				if (dx != 0 && dy != 0 && (!IsFlowWalkable(currentX + dx, currentY) || !IsFlowWalkable(currentX, currentY + dy))) {
					continue;
				}
				flowDistance[static_cast<size_t>(nextY * kMapWidth + nextX)] = static_cast<int16_t>(currentDistance + 1);
				queue.push_back(nextY * kMapWidth + nextX);
			}
		}
	}
}

Vector3 DungeonMap::GetFlowDirection(const Vector3& position) const
{
	if (flowDistance.empty()) {
		return { 0.0f, 0.0f, 0.0f };
	}
	int32_t tileX = ToTileX(position.x);
	int32_t tileY = ToTileY(position.z);
	if (!IsFlowWalkable(tileX, tileY)) {
		return { 0.0f, 0.0f, 0.0f };
	}
	int16_t currentDistance = flowDistance[static_cast<size_t>(tileY * kMapWidth + tileX)];
	if (currentDistance < 0) {
		return { 0.0f, 0.0f, 0.0f };
	}
	// まわり8マスの中で、一番歩数が少ないマスへ向かう
	int16_t bestDistance = currentDistance;
	int32_t bestX = tileX, bestY = tileY;
	for (int32_t dy = -1; dy <= 1; ++dy) {
		for (int32_t dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0) {
				continue;
			}
			int32_t nextX = tileX + dx;
			int32_t nextY = tileY + dy;
			if (!IsFlowWalkable(nextX, nextY)) {
				continue;
			}
			if (dx != 0 && dy != 0 && (!IsFlowWalkable(tileX + dx, tileY) || !IsFlowWalkable(tileX, tileY + dy))) {
				continue;
			}
			int16_t nextDistance = flowDistance[static_cast<size_t>(nextY * kMapWidth + nextX)];
			if (nextDistance >= 0 && nextDistance < bestDistance) {
				bestDistance = nextDistance;
				bestX = nextX;
				bestY = nextY;
			}
		}
	}
	if (bestDistance == currentDistance) {
		return { 0.0f, 0.0f, 0.0f };
	}
	Vector3 goal = TileCenter(bestX, bestY);
	Vector3 direction = { goal.x - position.x, 0.0f, goal.z - position.z };
	float length = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	return (length > 0.001f) ? direction / length : Vector3{ 0.0f, 0.0f, 0.0f };
}

bool DungeonMap::IsSolidAt(const Vector3& position) const
{
	return IsSolidTile(ToTileX(position.x), ToTileY(position.z));
}

int32_t DungeonMap::GetRoomIndexAt(const Vector3& position, int32_t margin) const
{
	int32_t x = ToTileX(position.x);
	int32_t y = ToTileY(position.z);
	if (x < 0 || y < 0 || x >= kMapWidth || y >= kMapHeight) {
		return -1;
	}
	int32_t gridX = x / kSlotWidth;
	int32_t gridY = y / kSlotHeight;
	int32_t roomIndex = layout.GetRoomAtCell(gridX, gridY);
	if (roomIndex < 0) {
		return -1;
	}
	int32_t localX = x - gridX * kSlotWidth;
	int32_t localY = y - gridY * kSlotHeight;
	if (localX < margin || localX > kRoomWidth - 1 - margin || localY < margin || localY > kRoomHeight - 1 - margin) {
		return -1;
	}
	return roomIndex;
}

void DungeonMap::SetRoomLocked(int32_t roomIndex, bool locked)
{
	roomAreas[static_cast<size_t>(roomIndex)].locked = locked;
}

Vector3 DungeonMap::GetRoomCenter(int32_t roomIndex) const
{
	const RoomArea& area = roomAreas[static_cast<size_t>(roomIndex)];
	return {
		(static_cast<float>(area.originX) + static_cast<float>(kRoomWidth) / 2.0f) * kTileSize, 0.0f,
		-(static_cast<float>(area.originY) + static_cast<float>(kRoomHeight) / 2.0f) * kTileSize };
}

Vector3 DungeonMap::TileCenter(int32_t x, int32_t y)
{
	return { (static_cast<float>(x) + 0.5f) * kTileSize, 0.0f, -(static_cast<float>(y) + 0.5f) * kTileSize };
}

int32_t DungeonMap::ToTileX(float worldX)
{
	return static_cast<int32_t>(std::floor(worldX / kTileSize));
}

int32_t DungeonMap::ToTileY(float worldZ)
{
	return static_cast<int32_t>(std::floor(-worldZ / kTileSize));
}

DungeonMap::Tile DungeonMap::GetTile(int32_t x, int32_t y) const
{
	if (x < 0 || y < 0 || x >= kMapWidth || y >= kMapHeight) {
		return Tile::kVoid;
	}
	return tiles[static_cast<size_t>(y * kMapWidth + x)];
}

void DungeonMap::SetTile(int32_t x, int32_t y, Tile tile)
{
	if (x < 0 || y < 0 || x >= kMapWidth || y >= kMapHeight) {
		return;
	}
	tiles[static_cast<size_t>(y * kMapWidth + x)] = tile;
}

bool DungeonMap::IsSolidTile(int32_t x, int32_t y) const
{
	Tile tile = GetTile(x, y);
	if (tile == Tile::kFloor) {
		return false;
	}
	if (tile == Tile::kDoor) {
		int32_t owner = doorOwners[static_cast<size_t>(y * kMapWidth + x)];
		return owner >= 0 && roomAreas[static_cast<size_t>(owner)].locked;
	}
	return true;
}
