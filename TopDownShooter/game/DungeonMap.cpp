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

	// ---- 部屋：ひな形をマップに書き込む ----
	for (size_t i = 0; i < layout.rooms.size(); ++i) {
		RoomNode& room = layout.rooms[i];
		const std::vector<RoomTemplate>& templates = library.Get(room.type);
		std::uniform_int_distribution<size_t> templateDistribution(0, templates.size() - 1);
		room.templateIndex = static_cast<int32_t>(templateDistribution(random));
		const RoomTemplate& roomTemplate = templates[static_cast<size_t>(room.templateIndex)];

		RoomArea& area = roomAreas[i];
		area.originX = room.gridX * kSlotWidth;
		area.originY = room.gridY * kSlotHeight;
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
		const char* floorModel = kModelFloorRoom;
		if (room.type == RoomType::kBoss) {
			floorModel = kModelFloorBoss;
		} else if (room.type == RoomType::kTreasure) {
			floorModel = kModelFloorTreasure;
		}
		const Vector2 roomHalf = { static_cast<float>(kRoomWidth) * kTileSize * 0.5f, static_cast<float>(kRoomHeight) * kTileSize * 0.5f };
		AddObject(floorModel, GetRoomCenter(static_cast<int32_t>(i)), roomHalf, -1);

		const Vector2 tileHalfSize = { kTileSize * 0.5f, kTileSize * 0.5f };
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