#include "RunRecord.h"
#include <array>
#include <fstream>

namespace {
	using Table = std::array<RunRecord::Entry, static_cast<size_t>(CharacterType::kCount)>;

	// ファイルの形：1行に1キャラ（キャラの番号順）。「階層 秒」、記録がなければ「0 0」
	Table LoadAll(const char* path)
	{
		Table table = {};
		std::ifstream file(path);
		for (RunRecord::Entry& entry : table) {
			int32_t floor = 0;
			float seconds = 0.0f;
			if (!(file >> floor >> seconds)) {
				break;
			}
			entry.valid = (floor > 0);
			entry.floor = floor;
			entry.seconds = seconds;
		}
		return table;
	}

	void SaveAll(const char* path, const Table& table)
	{
		std::ofstream file(path);
		for (const RunRecord::Entry& entry : table) {
			file << (entry.valid ? entry.floor : 0) << ' ' << (entry.valid ? entry.seconds : 0.0f) << '\n';
		}
	}
}

RunRecord::Entry RunRecord::Load(CharacterType character)
{
	return LoadAll(kFilePath)[static_cast<size_t>(character)];
}

bool RunRecord::IsBetter(int32_t floorA, float secondsA, const Entry& b)
{
	if (!b.valid) {
		return true;
	}
	if (floorA != b.floor) {
		return floorA > b.floor;
	}
	return secondsA < b.seconds;
}

bool RunRecord::Submit(CharacterType character, int32_t floor, float seconds)
{
	Table table = LoadAll(kFilePath);
	Entry& best = table[static_cast<size_t>(character)];
	if (!IsBetter(floor, seconds, best)) {
		return false;
	}
	best.valid = true;
	best.floor = floor;
	best.seconds = seconds;
	SaveAll(kFilePath, table);
	return true;
}
