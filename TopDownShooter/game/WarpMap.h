#pragma once
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>
#include "Mathutility.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "Input.h"
#include "WinApp.h"
#include "DungeonGenerator.h"
#include "GameSettings.h"
#include "UiKit.h"

// 大きなマップ（Tabで開く）とワープ
// ワープの部屋（青い菱形のしるしがある部屋）に行ったことがあれば、そこへ一瞬で移動できる
// 世界を止めるのは呼び出し側（StageScene）の仕事。ここは画面を出して、選ばれたものを返す
class WarpMap
{
public:
	enum class ResultType {
		kNone,   // 開いたまま
		kClose,  // 閉じた
		kWarp,   // ワープする（room に行き先）
	};
	struct Result {
		ResultType type = ResultType::kNone;
		int32_t room = -1;
	};

	static inline const char* kTexture = "resources/white.png";
	static inline const char* kTextureIcon = "resources/warp_icon.png"; // 転送陣のアイコン

	void Initialize(SpriteCommon* spriteCommon, const DungeonLayout& sourceLayout, const std::vector<bool>& warpRoomFlags)
	{
		layout = sourceLayout;
		warpRoom = warpRoomFlags;
		isOpen = false;

		// 部屋がある範囲（マス目）を調べて、画面の真ん中に大きく描く
		int32_t minX = DungeonConfig::kGridSize, minY = DungeonConfig::kGridSize, maxX = 0, maxY = 0;
		for (const RoomNode& room : layout.rooms) {
			minX = (std::min)(minX, room.gridX);
			minY = (std::min)(minY, room.gridY);
			maxX = (std::max)(maxX, room.gridX);
			maxY = (std::max)(maxY, room.gridY);
		}
		float cellW = kCellWidth, cellH = kCellHeight;
		float width = static_cast<float>(maxX - minX + 1) * cellW;
		float height = static_cast<float>(maxY - minY + 1) * cellH;
		// 画面に収まらなければ縮める
		float maxWidth = static_cast<float>(WinApp::kClientWidth) - 160.0f;
		float maxHeight = static_cast<float>(WinApp::kClientHeight) - 220.0f;
		float scale = (std::min)(1.0f, (std::min)(maxWidth / width, maxHeight / height));
		cellW *= scale; cellH *= scale; width *= scale; height *= scale;
		roomW = kRoomWidth * scale;
		roomH = kRoomHeight * scale;
		float iconSize = roomH * 0.85f;
		float left = (static_cast<float>(WinApp::kClientWidth) - width) / 2.0f;
		float top = (static_cast<float>(WinApp::kClientHeight) - height) / 2.0f + 10.0f;
		auto cellCenter = [&](int32_t gridX, int32_t gridY) {
			return Vector2{ left + (static_cast<float>(gridX - minX) + 0.5f) * cellW, top + (static_cast<float>(gridY - minY) + 0.5f) * cellH };
		};

		auto makeSprite = [&](Vector2 position, Vector2 size, Vector4 color, bool center) {
			std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
			sprite->Initialize(spriteCommon, kTexture);
			if (center) {
				sprite->SetAnchorPoint({ 0.5f, 0.5f });
			}
			sprite->SetPosition(position);
			sprite->SetSize(size);
			sprite->SetColor(color);
			sprite->Update();
			return sprite;
		};

		background = makeSprite({ 0.0f, 0.0f }, { static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) }, { 0.0f, 0.0f, 0.0f, 0.78f }, false);

		connections.clear();
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
				Vector2 a = cellCenter(room.gridX, room.gridY);
				const RoomNode& other = layout.rooms[static_cast<size_t>(neighbor)];
				Vector2 b = cellCenter(other.gridX, other.gridY);
				Vector2 size = direction == kDirectionRight ? Vector2{ cellW - roomW + 2.0f, 8.0f } : Vector2{ 8.0f, cellH - roomH + 2.0f };
				connection.sprite = makeSprite({ (a.x + b.x) / 2.0f, (a.y + b.y) / 2.0f }, size, { 0.7f, 0.7f, 0.75f, 1.0f }, true);
				connections.push_back(std::move(connection));
			}
		}

		rooms.clear();
		for (const RoomNode& room : layout.rooms) {
			RoomSprites sprites;
			sprites.center = cellCenter(room.gridX, room.gridY);
			sprites.frame = makeSprite(sprites.center, { roomW + 10.0f, roomH + 10.0f }, { 0.8f, 0.4f, 1.0f, 1.0f }, true);
			sprites.body = makeSprite(sprites.center, { roomW, roomH }, { 1.0f, 1.0f, 1.0f, 1.0f }, true);
			// 転送陣のアイコン（ワープの部屋の真ん中に重ねる）
			sprites.mark = std::make_unique<Sprite>();
			sprites.mark->Initialize(spriteCommon, kTextureIcon);
			sprites.mark->SetAnchorPoint({ 0.5f, 0.5f });
			sprites.mark->SetPosition(sprites.center);
			sprites.mark->SetSize({ iconSize, iconSize });
			sprites.mark->Update();
			rooms.push_back(std::move(sprites));
		}
		selectedRoom = -1;
	}

	bool IsOpen() const { return isOpen; }
	void Open(int32_t current)
	{
		isOpen = true;
		time = 0.0f;
		selectedRoom = -1;
		currentRoom = current;
	}

	// 開いている間、毎フレーム呼ぶ
	// visited：行ったことのある部屋 / warpReady：今ワープで行ける部屋 / canWarp：今ワープしていい状態か（戦闘中は false）
	Result Update(Input* input, const std::vector<bool>& visited, const std::vector<bool>& warpReady, int32_t current, bool canWarp, UiKit* ui)
	{
		Result result;
		if (!isOpen) {
			return result;
		}
		time += 1.0f / 60.0f;
		currentRoom = current;
		warpAllowed = canWarp;

		// 閉じる：Tab か Esc
		if (input->TriggerKey(DIK_TAB) || input->TriggerKey(DIK_ESCAPE)) {
			isOpen = false;
			result.type = ResultType::kClose;
			return result;
		}

		// 行き先の選び方：行ける部屋の中から、押した向きに一番近い部屋へ（最初は今の部屋に近いもの）
		if (selectedRoom < 0 || !IsReady(warpReady, selectedRoom)) {
			selectedRoom = FindNearest(warpReady, current >= 0 ? rooms[static_cast<size_t>(current)].center : Vector2{ 640.0f, 360.0f }, current);
		}
		Vector2 direction = { 0.0f, 0.0f };
		if (input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_A)) { direction.x -= 1.0f; }
		if (input->TriggerKey(DIK_RIGHT) || input->TriggerKey(DIK_D)) { direction.x += 1.0f; }
		if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W)) { direction.y -= 1.0f; }
		if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S)) { direction.y += 1.0f; }
		if ((direction.x != 0.0f || direction.y != 0.0f) && selectedRoom >= 0) {
			int32_t best = -1;
			float bestScore = 1.0e9f;
			Vector2 from = rooms[static_cast<size_t>(selectedRoom)].center;
			for (size_t i = 0; i < rooms.size(); ++i) {
				if (static_cast<int32_t>(i) == selectedRoom || !IsReady(warpReady, static_cast<int32_t>(i))) {
					continue;
				}
				Vector2 delta = { rooms[i].center.x - from.x, rooms[i].center.y - from.y };
				float along = delta.x * direction.x + delta.y * direction.y;
				if (along <= 1.0f) {
					continue; // 押した向きと逆
				}
				float across = std::fabs(delta.x * direction.y - delta.y * direction.x);
				float score = along + across * 2.0f; // 向きがずれているほど不利
				if (score < bestScore) {
					bestScore = score;
					best = static_cast<int32_t>(i);
				}
			}
			if (best >= 0) {
				selectedRoom = best;
			}
		}

		// マウスで行き先を選ぶ（カーソルが乗った部屋が選ばれる。クリックで決定）
		const Vector2& mouse = input->GetMousePosition();
		bool clicked = input->TriggerMouse(Input::MouseButton::kLeft);
		for (size_t i = 0; i < rooms.size(); ++i) {
			if (!IsReady(warpReady, static_cast<int32_t>(i))) {
				continue;
			}
			if (std::fabs(mouse.x - rooms[i].center.x) < roomW * 0.5f + 5.0f && std::fabs(mouse.y - rooms[i].center.y) < roomH * 0.5f + 5.0f) {
				if (mouseOverRoom != static_cast<int32_t>(i)) {
					selectedRoom = static_cast<int32_t>(i);
				}
				mouseOverRoom = static_cast<int32_t>(i);
				if (clicked) {
					return TryWarp(static_cast<int32_t>(i));
				}
				break;
			}
			if (static_cast<int32_t>(i) == mouseOverRoom) {
				mouseOverRoom = -1;
			}
		}

		// スペースで決定
		if (input->TriggerKey(DIK_SPACE) && selectedRoom >= 0) {
			return TryWarp(selectedRoom);
		}

		// 見た目の更新
		for (size_t i = 0; i < rooms.size(); ++i) {
			bool shown = visited[i];
			rooms[i].shown = shown;
			if (!shown) {
				continue;
			}
			const RoomNode& room = layout.rooms[i];
			Vector4 color = { 0.85f, 0.85f, 0.9f, 1.0f };
			if (room.type == RoomType::kBoss) { color = { 0.95f, 0.3f, 0.3f, 1.0f }; }
			else if (room.type == RoomType::kTreasure) { color = { 0.95f, 0.75f, 0.2f, 1.0f }; }
			else if (room.type == RoomType::kShop) { color = { 0.35f, 0.85f, 0.95f, 1.0f }; }
			else if (room.type == RoomType::kExit) { color = { 0.55f, 0.5f, 1.0f, 1.0f }; }
			if (static_cast<int32_t>(i) == currentRoom) {
				color = { 1.0f, 0.85f, 0.25f, 1.0f };
			}
			rooms[i].body->SetColor(color);
			rooms[i].body->Update();
			bool ready = IsReady(warpReady, static_cast<int32_t>(i));
			rooms[i].isWarp = i < warpRoom.size() && warpRoom[i];
			rooms[i].isReady = ready;
			// 選んでいる部屋のわくは、黄色く点滅させる
			bool selected = static_cast<int32_t>(i) == selectedRoom;
			Vector4 frameColor = selected ? Vector4{ 1.0f, 0.9f, 0.2f, 0.6f + 0.4f * std::sin(time * 10.0f) } : Vector4{ 0.8f, 0.4f, 1.0f, 1.0f };
			if (!warpAllowed) {
				frameColor = { 0.5f, 0.5f, 0.5f, 1.0f };
			}
			rooms[i].frame->SetColor(frameColor);
			rooms[i].frame->Update();
			// 使える転送陣は紫に光るアイコン、まだ使えない（敵がいる・今いる部屋など）は灰色
			bool lit = warpAllowed && ready;
			rooms[i].mark->SetColor(lit ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : Vector4{ 0.4f, 0.4f, 0.42f, 0.9f });
			rooms[i].mark->Update();
		}
		for (Connection& connection : connections) {
			connection.shown = visited[static_cast<size_t>(connection.roomA)] || visited[static_cast<size_t>(connection.roomB)];
		}

		// 説明の文字
		ui->Text(JP("マップ"), 640.0f, 24.0f, 36.0f, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, UiKit::Align::kCenter);
		if (warpAllowed) {
			ui->Text(JP("紫の転送陣のある部屋へワープできます：クリック、または 矢印キー＋スペース"), 640.0f, 70.0f, 20.0f, Vector4{ 0.9f, 0.7f, 1.0f, 1.0f }, UiKit::Align::kCenter);
		} else {
			ui->Text(JP("戦闘中はワープできません"), 640.0f, 70.0f, 20.0f, Vector4{ 1.0f, 0.5f, 0.5f, 1.0f }, UiKit::Align::kCenter);
		}
		ui->Text(JP("Tab / Esc：閉じる"), 640.0f, 672.0f, 18.0f, Vector4{ 0.75f, 0.75f, 0.8f, 1.0f }, UiKit::Align::kCenter);
		return result;
	}

	void Draw()
	{
		if (!isOpen) {
			return;
		}
		background->Draw();
		for (Connection& connection : connections) {
			if (connection.shown) {
				connection.sprite->Draw();
			}
		}
		for (RoomSprites& room : rooms) {
			if (!room.shown) {
				continue;
			}
			if (room.isWarp) {
				room.frame->Draw();
			}
			room.body->Draw();
			if (room.isWarp) {
				room.mark->Draw();
			}
		}
	}

private:
	struct Connection {
		int32_t roomA = 0;
		int32_t roomB = 0;
		std::unique_ptr<Sprite> sprite;
		bool shown = false;
	};
	struct RoomSprites {
		Vector2 center = { 0.0f, 0.0f };
		std::unique_ptr<Sprite> frame; // ワープの部屋のわく
		std::unique_ptr<Sprite> body;
		std::unique_ptr<Sprite> mark;  // 菱形のしるし
		bool shown = false;
		bool isWarp = false;
		bool isReady = false;
	};

	// 今そこへワープできるか（今いる部屋は行き先にしない）
	bool IsReady(const std::vector<bool>& warpReady, int32_t room) const
	{
		return room >= 0 && room < static_cast<int32_t>(warpReady.size()) && warpReady[static_cast<size_t>(room)] && room != currentRoom;
	}
	int32_t FindNearest(const std::vector<bool>& warpReady, Vector2 from, int32_t) const
	{
		int32_t best = -1;
		float bestDistance = 1.0e9f;
		for (size_t i = 0; i < rooms.size(); ++i) {
			if (!IsReady(warpReady, static_cast<int32_t>(i))) {
				continue;
			}
			float dx = rooms[i].center.x - from.x, dy = rooms[i].center.y - from.y;
			float distance = dx * dx + dy * dy;
			if (distance < bestDistance) {
				bestDistance = distance;
				best = static_cast<int32_t>(i);
			}
		}
		return best;
	}
	Result TryWarp(int32_t room)
	{
		Result result;
		if (!warpAllowed) {
			return result;
		}
		isOpen = false;
		result.type = ResultType::kWarp;
		result.room = room;
		return result;
	}

	// ---- 見た目の数値（ピクセル。画面に収まらなければ縮める）----
	static inline const float kCellWidth = 110.0f;
	static inline const float kCellHeight = 80.0f;
	static inline const float kRoomWidth = 80.0f;
	static inline const float kRoomHeight = 52.0f;

	DungeonLayout layout;
	std::vector<bool> warpRoom; // ワープの部屋か
	std::unique_ptr<Sprite> background;
	std::vector<Connection> connections;
	std::vector<RoomSprites> rooms;
	float roomW = kRoomWidth, roomH = kRoomHeight;
	bool isOpen = false;
	bool warpAllowed = true;
	int32_t selectedRoom = -1;
	int32_t mouseOverRoom = -1;
	int32_t currentRoom = -1;
	float time = 0.0f;
};
