#pragma once
#include <array>
#include <cstdint>
#include "CharacterData.h"
#include "Enemy.h"

// 伸びるステータスの種類（倒した敵の種類と1対1）
enum class GrowthStat {
	kMoveSpeed, // 移動速度（突進する敵）
	kRange,     // 射程（射撃する敵）
	kFireRate,  // 連射（群れの敵）
	kDamage,    // 威力（重装の敵）
	kCount,
};

// ビルドの仕組み：部屋をクリアしたら、その部屋で倒した敵の中から1種類を選んで力を取り込む
// 取り込んだ分（倒した数）だけ、その種類のステータスが伸びる（＝自分流のビルド）
class BuildSystem
{
public:
	// base：キャラの初期ステータス
	void Reset(const PlayerStats& base);
	// 力を取り込む（amount：取り込む数＝その部屋で倒した数）
	void AddGrowth(GrowthStat stat, int32_t amount);
	// 今のステータス（初期ステータス＋伸びた分）
	PlayerStats ComputeStats() const;

	// 敵の種類 → 伸びるステータス
	static GrowthStat GetGrowthStat(EnemyType type);
	// そのステータスが何％伸びているか（表示用。連射は「速くなった割合」）
	int32_t GetBonusPercent(GrowthStat stat) const;
	// 取り込んだ数（1体ぶん＝1）
	int32_t GetPoints(GrowthStat stat) const { return points[static_cast<size_t>(stat)]; }
	// amount 体ぶん取り込んだら何％伸びるか（選ぶ画面の表示用）
	static int32_t GetPercentFor(GrowthStat stat, int32_t amount);
	// 1体あたりの伸び（割合）
	static float GetGrowthRate(GrowthStat stat);

private:
	PlayerStats baseStats;
	std::array<int32_t, static_cast<size_t>(GrowthStat::kCount)> points = {};
};