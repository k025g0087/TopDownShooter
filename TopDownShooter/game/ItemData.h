#pragma once
#include <array>
#include <cstdint>

// アイテムの種類（弾の性能が変わる。ショップのガチャで手に入る。同じものを重ねるほど強くなる）
enum class ItemType {
	kBounce, // 跳ね返り：壁に当たると跳ね返る（1つにつき1回）
	kSplit,  // 分裂：敵に当たると小さな弾に分かれる（3つ → 5つ → 7つ）
	kHoming, // 追尾：近くの敵へ曲がる（重ねるほどよく曲がる）
	kSpread, // 拡散：一度に何方向にも撃つ（3方向 → 5方向 → 7方向。連射は少し遅くなる）
	kGiant,  // 巨大化：弾が大きくなる（1.4倍 → 1.8倍 → 2.2倍）
	kCount,
};

// アイテム1種類の情報
struct ItemData {
	const char* name = "";        // ImGui 用の名前（英字）
	const char* iconTexture = ""; // 左下に並べるアイコン
	const char* getTexture = "";  // ガチャで出たときに出す「○○を手に入れた！」の画像
};

// 弾がアイテムでどう変わるか（持っている数から計算する）
struct ShotModifiers {
	int32_t shotCount = 1;           // 一度に撃つ弾の数（拡散）
	float spreadAngle = 0.0f;        // 弾と弾の間の角度（ラジアン）
	float fireIntervalRate = 1.0f;   // 攻撃の間隔の倍率（拡散で少し遅くなる）
	float sizeRate = 1.0f;           // 弾の大きさの倍率（巨大化）
	int32_t bounceCount = 0;         // 壁で跳ね返る回数（跳ね返り）
	float homingTurnSpeed = 0.0f;    // 敵へ曲がる速さ（ラジアン／秒。追尾）
	int32_t splitCount = 0;          // 敵に当たったときに分かれる数（分裂。0なら分かれない）
};

// 持っているアイテム
class ItemInventory
{
public:
	static const ItemData& GetData(ItemType type);

	void Reset() { counts.fill(0); }
	void Add(ItemType type) { counts[static_cast<size_t>(type)]++; }
	int32_t GetCount(ItemType type) const { return counts[static_cast<size_t>(type)]; }
	// 持っているアイテムから、弾の変わり方を計算する
	ShotModifiers ComputeModifiers() const;

private:
	std::array<int32_t, static_cast<size_t>(ItemType::kCount)> counts = {};
};
