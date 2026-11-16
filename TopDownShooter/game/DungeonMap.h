#pragma once
#include <cstdint>
#include <memory>
#include <random>
#include <vector>
#include "Mathutility.h"
#include "Object3d.h"
#include "DungeonGenerator.h"
#include "RoomTemplate.h"

class Camera;

// 神殿の1フロアのマップ（マップチップの壁・床・扉の見た目と、壁の当たり判定）
// 座標：マス (x, y) は、ワールドの X が x*kTileSize 〜 (x+1)*kTileSize、Z が -(y+1)*kTileSize 〜 -y*kTileSize
//       （y が増えるほど画面の下＝手前。画面の上が奥＝+Z になるように）
class DungeonMap
{
public:
	// マスの種類
	enum class Tile : uint8_t {
		kVoid,  // 何もない（通れない）
		kFloor, // 床
		kWall,  // 壁・柱
		kDoor,  // 扉（部屋が閉まっているときだけ通れない）
	};

	// 部屋のマップ上の情報
	struct RoomArea {
		int32_t originX = 0;  // 部屋の左上のマス
		int32_t originY = 0;
		// 敵が出てくる場所（kind：ひな形の文字。e はランダム、c/s/w/t は種類を決める）
		struct SpawnPoint {
			Vector3 position = { 0.0f, 0.0f, 0.0f };
			char kind = 'e';
		};
		std::vector<SpawnPoint> spawnPoints;
		bool locked = false;  // 扉が閉まっているか
	};

	// layout の並びで、ひな形を使ってマップを組み立てる
	void Build(const DungeonLayout& layout, const RoomTemplateLibrary& library, std::mt19937& random);

	// 見えているあたりだけ更新する（focus：カメラの見ている点）
	void Update(Camera* camera, const Vector3& focus);
	void Draw();

	// 円（半径 radius）が壁にめり込んでいたら押し戻す
	Vector3 ResolveCircle(const Vector3& position, float radius) const;
	// その場所が通れないか（弾が壁に当たったかを見るのに使う）
	bool IsSolidAt(const Vector3& position) const;

	// 2点の間に壁がないか（radius：通り抜ける円の半径。壁越しに撃たないか・見えているかの判定に使う）
	bool HasLineOfSight(const Vector3& from, const Vector3& to, float radius) const;
	// 道順の地図を作り直す：target のマスからの歩数を、部屋（と、扉の外の1マス）の中のマスに書く
	// 敵が壁や柱をよけてプレイヤーのところへ行くための地図。毎フレームでなく、少しおきに呼べばよい
	void UpdateFlowField(const Vector3& target, int32_t roomIndex);
	// position から target へ近づくために、次に向かう向き（道順の地図がなければ長さ0）
	Vector3 GetFlowDirection(const Vector3& position) const;

	// その場所がどの部屋の中か（margin：壁から何マス内側までを部屋の中とするか）。部屋の外（通路）なら -1
	int32_t GetRoomIndexAt(const Vector3& position, int32_t margin) const;
	// 部屋の扉を閉める・開ける
	void SetRoomLocked(int32_t roomIndex, bool locked);

	const RoomArea& GetRoomArea(int32_t roomIndex) const { return roomAreas[static_cast<size_t>(roomIndex)]; }
	// 部屋の真ん中（ワールド座標）
	Vector3 GetRoomCenter(int32_t roomIndex) const;
	// 出口の小部屋の、下へ行く階段と帰還の転送陣の位置（ワールド座標）
	const Vector3& GetExitStairsPosition() const { return exitStairsPosition; }
	const Vector3& GetExitPortalPosition() const { return exitPortalPosition; }

	// マス ⇔ ワールド座標
	static Vector3 TileCenter(int32_t x, int32_t y);
	static int32_t ToTileX(float worldX);
	static int32_t ToTileY(float worldZ);

	// 使うモデル（シーンで読み込む）
	static inline const char* kModelWall = "test_wall.obj";
	static inline const char* kModelDoor = "test_door.obj";
	static inline const char* kModelFloorRoom = "test_floor_room.obj";
	static inline const char* kModelFloorBoss = "test_floor_boss.obj";
	static inline const char* kModelFloorTreasure = "test_floor_treasure.obj";
	static inline const char* kModelFloorShop = "test_floor_shop.obj";
	static inline const char* kModelFloorCorridor = "test_floor_corridor.obj";
	static inline const char* kModelFloorExit = "test_floor_exit.obj"; // 出口の小部屋の床（1マス）

private:
	Tile GetTile(int32_t x, int32_t y) const;
	void SetTile(int32_t x, int32_t y, Tile tile);
	bool IsSolidTile(int32_t x, int32_t y) const;
	// ひな形の部屋を書き込む（敵が出てくる場所も集める）
	void BuildTemplateRoom(RoomNode& room, RoomArea& area, const RoomTemplateLibrary& library, std::mt19937& random);
	// 出口の小部屋を書き込む（ひな形は使わず、まわりを壁で埋めた小さな部屋にする）
	void BuildExitRoom(const RoomArea& area);
	// 宝箱の小部屋を書き込む（ひな形は使わず、まわりを壁で埋めた小さな部屋にする。扉のある向きには道を通す）
	void BuildTreasureRoom(const RoomNode& room, const RoomArea& area);
	// 見た目のオブジェクトを1つ作る
	void AddObject(const char* modelName, const Vector3& position, const Vector2& halfSize, int32_t doorRoom);

	// マップ全体の大きさ（マス）
	static inline const int32_t kMapWidth = DungeonConfig::kGridSize * DungeonConfig::kSlotWidth;
	static inline const int32_t kMapHeight = DungeonConfig::kGridSize * DungeonConfig::kSlotHeight;

	std::vector<Tile> tiles;
	// 扉のマスが、どの部屋の扉か（扉でなければ -1）
	std::vector<int32_t> doorOwners;
	std::vector<RoomArea> roomAreas;
	// 道順の地図（マスごとの、目的地までの歩数。-1 は行けない・未計算）
	std::vector<int16_t> flowDistance;
	int32_t flowMinX = 0, flowMinY = 0, flowMaxX = -1, flowMaxY = -1; // 地図を作った範囲
	bool IsFlowWalkable(int32_t x, int32_t y) const;
	DungeonLayout layout;
	// 出口の小部屋
	Vector3 exitStairsPosition = { 0.0f, 0.0f, 0.0f };
	Vector3 exitPortalPosition = { 0.0f, 0.0f, 0.0f };
	int32_t exitStairsTileX = -1; // 階段の穴のマス。ここには床を置かない
	int32_t exitStairsTileY = -1;
	// 出口の小部屋の、床のある範囲（部屋の中のマス。両端を含む）
	static inline const int32_t kExitLeft = 4;
	static inline const int32_t kExitRight = 12;
	static inline const int32_t kExitTop = 3;
	static inline const int32_t kExitBottom = 9;
	// 宝箱の小部屋の、床のある範囲（部屋の中のマス。両端を含む）。7x5マスで、真ん中に宝箱を置く
	static inline const int32_t kTreasureLeft = 5;
	static inline const int32_t kTreasureRight = 11;
	static inline const int32_t kTreasureTop = 4;
	static inline const int32_t kTreasureBottom = 8;

	// 見た目（壁・扉・床）。doorRoom：扉なら持ち主の部屋（閉まっているときだけ描く）、扉でなければ -1
	struct MapObject {
		std::unique_ptr<Object3d> object;
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector2 halfSize = { 1.0f, 1.0f }; // 上から見た大きさの半分（x：横、y：奥行き）。見える範囲の判定に使う
		int32_t doorRoom = -1;
		bool visible = false; // 今のフレームで描くか
	};
	std::vector<MapObject> objects;

	// 画面に入るくらいの範囲（カメラの見ている点からの距離）。これより遠いものは更新も描画もしない
	static inline const float kViewRangeX = 30.0f;
	static inline const float kViewRangeZ = 24.0f;
};
