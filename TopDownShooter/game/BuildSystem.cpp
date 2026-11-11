#include "BuildSystem.h"
#include <cmath>

namespace {
	// 1体取り込むごとの伸び（初期ステータスに対する割合）
	const float kGrowthRates[] = {
		0.04f, // 移動速度 +4%（突進。1部屋に2〜3体）
		0.05f, // 射程 +5%（射撃。1部屋に2〜4体）
		0.02f, // 連射 +2%（群れ。3体ずつ出てきて数が多いので、1体あたりは小さめ）
		0.12f, // 威力 +12%（重装。1部屋に1体くらいで硬いので、1体あたりは大きめ）
	};
	static_assert(sizeof(kGrowthRates) / sizeof(kGrowthRates[0]) == static_cast<size_t>(GrowthStat::kCount), "伸びの数とステータスの数が合っていない");
}

void BuildSystem::Reset(const PlayerStats& base)
{
	baseStats = base;
	points.fill(0);
}

void BuildSystem::AddGrowth(GrowthStat stat, int32_t amount)
{
	points[static_cast<size_t>(stat)] += amount;
}

GrowthStat BuildSystem::GetGrowthStat(EnemyType type)
{
	switch (type) {
	case EnemyType::kCharger: return GrowthStat::kMoveSpeed;
	case EnemyType::kShooter: return GrowthStat::kRange;
	case EnemyType::kSwarm: return GrowthStat::kFireRate;
	case EnemyType::kTank:
	default: return GrowthStat::kDamage;
	}
}

float BuildSystem::GetGrowthRate(GrowthStat stat)
{
	return kGrowthRates[static_cast<size_t>(stat)];
}

PlayerStats BuildSystem::ComputeStats() const
{
	auto bonus = [this](GrowthStat stat) {
		return 1.0f + GetGrowthRate(stat) * static_cast<float>(points[static_cast<size_t>(stat)]);
		};
	PlayerStats stats = baseStats;
	stats.moveSpeed = baseStats.moveSpeed * bonus(GrowthStat::kMoveSpeed);
	// 射程：飛ぶ時間を伸ばして、弾の速さも少しだけ上げる
	stats.bulletLifeTime = baseStats.bulletLifeTime * bonus(GrowthStat::kRange);
	stats.bulletSpeed = baseStats.bulletSpeed * (1.0f + (bonus(GrowthStat::kRange) - 1.0f) * 0.4f);
	// 連射：間隔を短くする（割り算なので、伸びるほど効きがゆるやかになる）
	stats.fireInterval = baseStats.fireInterval / bonus(GrowthStat::kFireRate);
	stats.bulletDamage = baseStats.bulletDamage * bonus(GrowthStat::kDamage);
	return stats;
}

int32_t BuildSystem::GetBonusPercent(GrowthStat stat) const
{
	return GetPercentFor(stat, points[static_cast<size_t>(stat)]);
}

int32_t BuildSystem::GetPercentFor(GrowthStat stat, int32_t amount)
{
	float rate = GetGrowthRate(stat) * static_cast<float>(amount);
	return static_cast<int32_t>(std::round(rate * 100.0f));
}