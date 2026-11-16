#pragma once
#include <cstdint>
#include "CharacterData.h"

// キャラごとのベスト記録（到達階層とタイム）をファイルに残す
// 階層が深いほど良い。同じ階層なら、タイムが短いほど良い
class RunRecord
{
public:
	struct Entry {
		bool valid = false;  // 記録があるか
		int32_t floor = 0;   // 到達階層（1から）
		float seconds = 0.0f;
	};

	// 記録を読む（ファイルがなければ「記録なし」）
	static Entry Load(CharacterType character);
	// 今回の結果を出す。ベストを更新したら保存して true を返す
	static bool Submit(CharacterType character, int32_t floor, float seconds);

	// どちらが良い記録か（a の方が良ければ true）
	static bool IsBetter(int32_t floorA, float secondsA, const Entry& b);

private:
	static inline const char* kFilePath = "records.txt";
};
