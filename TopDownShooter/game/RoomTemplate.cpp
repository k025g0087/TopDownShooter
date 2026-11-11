#include "RoomTemplate.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "Logger.h"

using namespace DungeonConfig;

void RoomTemplateLibrary::Load(const std::string& directory)
{
	startRooms.clear();
	combatRooms.clear();
	treasureRooms.clear();
	bossRooms.clear();

	// フォルダの中の .txt を、名前の順に読む（毎回同じ順番になるように並べる）
	std::vector<std::filesystem::path> paths;
	if (std::filesystem::exists(directory)) {
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
			if (entry.is_regular_file() && entry.path().extension() == ".txt") {
				paths.push_back(entry.path());
			}
		}
	}
	std::sort(paths.begin(), paths.end());

	for (const std::filesystem::path& path : paths) {
		RoomTemplate roomTemplate;
		if (!LoadFile(path.string(), roomTemplate)) {
			Logger::Log("RoomTemplate: skipped (size mismatch): " + path.string() + "\n");
			continue;
		}
		const std::string& name = roomTemplate.name;
		if (name.rfind("start_", 0) == 0) {
			startRooms.push_back(roomTemplate);
		} else if (name.rfind("combat_", 0) == 0) {
			combatRooms.push_back(roomTemplate);
		} else if (name.rfind("treasure_", 0) == 0) {
			treasureRooms.push_back(roomTemplate);
		} else if (name.rfind("boss_", 0) == 0) {
			bossRooms.push_back(roomTemplate);
		}
	}

	// 足りない種類は、ただの四角い部屋で埋める（ファイルを置き忘れても止まらないように）
	for (std::vector<RoomTemplate>* list : { &startRooms, &combatRooms, &treasureRooms, &bossRooms }) {
		if (list->empty()) {
			list->push_back(MakeEmptyRoom());
		}
	}
}

const std::vector<RoomTemplate>& RoomTemplateLibrary::Get(RoomType type) const
{
	switch (type) {
	case RoomType::kStart:
		return startRooms;
	case RoomType::kTreasure:
		return treasureRooms;
	case RoomType::kBoss:
		return bossRooms;
	case RoomType::kCombat:
	default:
		return combatRooms;
	}
}

bool RoomTemplateLibrary::LoadFile(const std::string& path, RoomTemplate& roomTemplate)
{
	std::ifstream file(path);
	if (!file) {
		return false;
	}
	roomTemplate.name = std::filesystem::path(path).stem().string();
	roomTemplate.rows.clear();
	std::string line;
	while (std::getline(file, line)) {
		// Windows の改行（\r）と、行の終わりの空白を取る
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
			line.pop_back();
		}
		if (line.empty()) {
			continue;
		}
		roomTemplate.rows.push_back(line);
	}
	if (static_cast<int32_t>(roomTemplate.rows.size()) != kRoomHeight) {
		return false;
	}
	for (const std::string& row : roomTemplate.rows) {
		if (static_cast<int32_t>(row.size()) != kRoomWidth) {
			return false;
		}
	}
	return true;
}

RoomTemplate RoomTemplateLibrary::MakeEmptyRoom()
{
	RoomTemplate roomTemplate;
	roomTemplate.name = "empty";
	for (int32_t y = 0; y < kRoomHeight; ++y) {
		std::string row(static_cast<size_t>(kRoomWidth), '.');
		if (y == 0 || y == kRoomHeight - 1) {
			row.assign(static_cast<size_t>(kRoomWidth), '#');
		}
		row.front() = '#';
		row.back() = '#';
		roomTemplate.rows.push_back(row);
	}
	return roomTemplate;
}
