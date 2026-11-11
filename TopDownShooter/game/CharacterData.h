#pragma once
#include <cstdint>
#include <string>
#include "Mathutility.h"

// 選べるキャラクター
enum class CharacterType {
	kKnight, // ナイトくん：斬撃を飛ばす
	kWitch,  // ウィッチちゃん：魔法弾（当たると爆発）
	kArcher, // アーチャーさん：弓矢
	kCount,  // キャラの数（選択画面で使う）
};

// プレイヤーの能力値（倒した敵によってここが変わって、自分流のビルドになる予定）
struct PlayerStats {
	float moveSpeed = 6.0f;        // 歩く速さ（1秒あたり）
	float fireInterval = 0.14f;    // 攻撃の間隔（秒）
	float bulletSpeed = 18.0f;     // 攻撃が飛ぶ速さ（1秒あたり）
	float bulletDamage = 1.0f;     // 攻撃の威力
	float bulletLifeTime = 1.0f;   // 攻撃が消えるまでの時間（秒）＝射程
	int32_t maxHp = 6;             // 最大HP
};

// 攻撃の種類ごとの性質（ステータスとは別。キャラの個性になる部分）
struct ShotStyle {
	std::string modelName;             // 攻撃の見た目のモデル
	float radius = 0.16f;              // 当たり判定の半径
	Vector3 scale = { 1.0f, 1.0f, 1.0f }; // 見た目の大きさ
	int32_t pierce = 0;                // 貫通できる敵の数（0：最初の1体で消える／-1：何体でも貫通）
	float explosionRadius = 0.0f;      // 当たったときの爆発の半径（0なら爆発しない）
	float explosionDamageRate = 0.0f;  // 爆発のダメージ（威力に対する割合）
	bool spin = false;                 // くるくる回すか（false なら進む向きを向く）
	float growth = 0.0f;               // 飛びながら大きくなる割合（1秒あたり）
};

// キャラ1人分のデータ
struct CharacterData {
	CharacterType type = CharacterType::kKnight;
	const char* name = "";            // 表示名（ImGui 用。英字）
	std::string modelName;            // プレイヤーの見た目のモデル
	std::string cardTexture;          // 選択画面の説明カードの画像
	PlayerStats stats;                // 初期ステータス
	ShotStyle shot;                   // 攻撃の性質
};

// キャラのデータを取得する
const CharacterData& GetCharacterData(CharacterType type);

// 選択画面で選んだキャラ（シーンをまたいで覚えておく）
class CharacterSelection
{
public:
	static CharacterType GetSelected() { return selected; }
	static void SetSelected(CharacterType type) { selected = type; }

private:
	static inline CharacterType selected = CharacterType::kKnight;
};
