#pragma once
#include <string>
#include <vector>
#include "DungeonGenerator.h"

// 部屋のひな形（マップチップ）
// resources/rooms/ のテキストファイルから読む。1文字が1マス
//   #：壁　o：柱（壁と同じく通れない）　.：床
//   敵が出てくる場所（床）：e：ランダム　c：突進　s：射撃　w：群れ（3体）　t：重装
// 大きさは DungeonConfig::kRoomWidth x kRoomHeight。外の壁の真ん中が扉になる（隣に部屋があるときだけ）
struct RoomTemplate {
	std::string name;
	std::vector<std::string> rows; // 上（奥）の行から順に

	char Get(int32_t x, int32_t y) const { return rows[static_cast<size_t>(y)][static_cast<size_t>(x)]; }
};

// 部屋の種類ごとのひな形のリスト
class RoomTemplateLibrary
{
public:
	// フォルダの中のひな形を全部読む（ファイル名の頭で種類を分ける：start_ / combat_ / treasure_ / boss_）
	void Load(const std::string& directory);

	const std::vector<RoomTemplate>& Get(RoomType type) const;

private:
	// 1つのファイルを読む（大きさが合わなければ false）
	static bool LoadFile(const std::string& path, RoomTemplate& roomTemplate);
	// 何も読めなかったときのための、ただの四角い部屋
	static RoomTemplate MakeEmptyRoom();

	std::vector<RoomTemplate> startRooms;
	std::vector<RoomTemplate> combatRooms;
	std::vector<RoomTemplate> treasureRooms;
	std::vector<RoomTemplate> bossRooms;
};