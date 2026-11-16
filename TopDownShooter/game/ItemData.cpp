#include "ItemData.h"
#include <cassert>

namespace {
	const std::array<ItemData, static_cast<size_t>(ItemType::kCount)> kItemData = { {
		{ "Bounce", "resources/item_icon_bounce.png", "resources/item_get_bounce.png" },
		{ "Split",  "resources/item_icon_split.png",  "resources/item_get_split.png" },
		{ "Homing", "resources/item_icon_homing.png", "resources/item_get_homing.png" },
		{ "Spread", "resources/item_icon_spread.png", "resources/item_get_spread.png" },
		{ "Giant",  "resources/item_icon_giant.png",  "resources/item_get_giant.png" },
	} };
}

const ItemData& ItemInventory::GetData(ItemType type)
{
	size_t index = static_cast<size_t>(type);
	assert(index < kItemData.size());
	return kItemData[index];
}

ShotModifiers ItemInventory::ComputeModifiers() const
{
	ShotModifiers modifiers;

	// 拡散：1つで3方向、重ねるごとに2方向ずつ増える。そのぶん連射を少し遅くする
	int32_t spread = GetCount(ItemType::kSpread);
	if (spread > 0) {
		modifiers.shotCount = 1 + 2 * spread;
		modifiers.spreadAngle = 0.2f;
		modifiers.fireIntervalRate = 1.0f + 0.15f * static_cast<float>(spread);
	}

	// 巨大化：1つにつき 0.4 倍ずつ大きくなる
	modifiers.sizeRate = 1.0f + 0.4f * static_cast<float>(GetCount(ItemType::kGiant));

	// 跳ね返り：1つにつき1回
	modifiers.bounceCount = GetCount(ItemType::kBounce);

	// 追尾：1つにつき、1秒に 3.5 ラジアン（約200度）まで曲がれる
	modifiers.homingTurnSpeed = 3.5f * static_cast<float>(GetCount(ItemType::kHoming));

	// 分裂：1つで3つ、重ねるごとに2つずつ増える
	int32_t split = GetCount(ItemType::kSplit);
	modifiers.splitCount = (split > 0) ? 1 + 2 * split : 0;

	return modifiers;
}
