#include "CharacterData.h"
#include <array>
#include <cassert>

namespace {

	// キャラのデータを作る（ここの数値をいじると、キャラの個性が変わる）
	std::array<CharacterData, static_cast<size_t>(CharacterType::kCount)> MakeCharacters()
	{
		std::array<CharacterData, static_cast<size_t>(CharacterType::kCount)> characters;

		// ナイトくん：太くて短い斬撃。敵を何体でも貫通する。HPが高いが、射程が短く連射が遅い
		{
			CharacterData& knight = characters[static_cast<size_t>(CharacterType::kKnight)];
			knight.type = CharacterType::kKnight;
			knight.name = "Knight";
			knight.modelName = "knight.obj"; // 前作「けちらせナイトくん」のモデル
			knight.cardTexture = "resources/card_knight.png";
			knight.parts.body = "knight_body.obj";
			knight.parts.foot = "knight_foot.obj";
			knight.parts.rightHand = "knight_hand_weapon.obj"; // 剣
			knight.parts.leftHand = "knight_hand_shield.obj";  // 盾
			knight.parts.bodyOffset = { 0.0f, 0.41f, 0.0f };
			knight.parts.footOffset = { 0.12f, 0.08f, 0.02f };
			knight.parts.rightHandOffset = { 0.34f, 0.39f, 0.1f };
			knight.parts.leftHandOffset = { -0.36f, 0.43f, 0.0f };
			knight.parts.attackMotion = AttackMotion::kSwing;
			knight.stats.moveSpeed = 6.0f;
			knight.stats.fireInterval = 0.34f;
			knight.stats.bulletSpeed = 14.0f;
			knight.stats.bulletDamage = 2.5f;
			knight.stats.bulletLifeTime = 0.36f;
			knight.stats.maxHp = 8;
			knight.shot.modelName = "test_slash.obj";
			knight.shot.radius = 0.75f;
			knight.shot.scale = { 1.0f, 1.0f, 1.0f };
			knight.shot.pierce = -1;
			knight.shot.spin = false;
			knight.shot.growth = 1.2f;
		}

		// ウィッチちゃん：遅めで大きい魔法弾。当たると爆発して周りにもダメージ。HPが低い
		{
			CharacterData& witch = characters[static_cast<size_t>(CharacterType::kWitch)];
			witch.type = CharacterType::kWitch;
			witch.name = "Witch";
			witch.modelName = "test_witch.obj";
			witch.cardTexture = "resources/card_witch.png";
			witch.parts.body = "witch_body.obj";
			witch.parts.foot = "witch_foot.obj";
			witch.parts.rightHand = "witch_hand_weapon.obj"; // 杖
			witch.parts.leftHand = "witch_hand.obj";
			witch.parts.bodyOffset = { 0.0f, 0.41f, 0.0f };
			witch.parts.footOffset = { 0.12f, 0.08f, 0.02f };
			witch.parts.rightHandOffset = { 0.34f, 0.39f, 0.1f };
			witch.parts.leftHandOffset = { -0.36f, 0.43f, 0.0f };
			witch.parts.attackMotion = AttackMotion::kThrust;
			// ウィッチとアーチャーのモデルは、ユーザーが Blender で作ったものを部品に分けて使っている
			witch.stats.moveSpeed = 5.6f;
			witch.stats.fireInterval = 0.42f;
			witch.stats.bulletSpeed = 9.0f;
			witch.stats.bulletDamage = 2.0f;
			witch.stats.bulletLifeTime = 1.2f;
			witch.stats.maxHp = 5;
			witch.shot.modelName = "test_magic.obj";
			witch.shot.radius = 0.35f;
			witch.shot.scale = { 1.0f, 1.0f, 1.0f };
			witch.shot.pierce = 0;
			witch.shot.explosionRadius = 2.0f;
			witch.shot.explosionDamageRate = 0.75f;
			witch.shot.spin = true;
		}

		// アーチャーさん：速くて細い矢。遠くまで届いて連射も速い。当てるのが難しい
		{
			CharacterData& archer = characters[static_cast<size_t>(CharacterType::kArcher)];
			archer.type = CharacterType::kArcher;
			archer.name = "Archer";
			archer.modelName = "test_archer.obj";
			archer.cardTexture = "resources/card_archer.png";
			archer.parts.body = "archer_body.obj";
			archer.parts.foot = "archer_foot.obj";
			archer.parts.rightHand = "archer_hand_weapon.obj"; // 弓（右手に持つ）
			archer.parts.leftHand = "archer_hand.obj";
			archer.parts.bodyOffset = { 0.0f, 0.41f, 0.0f };
			archer.parts.footOffset = { 0.12f, 0.08f, 0.02f };
			archer.parts.rightHandOffset = { 0.34f, 0.39f, 0.1f };
			archer.parts.leftHandOffset = { -0.36f, 0.43f, 0.0f };
			archer.parts.attackMotion = AttackMotion::kDraw;
			archer.stats.moveSpeed = 6.4f;
			archer.stats.fireInterval = 0.22f;
			archer.stats.bulletSpeed = 26.0f;
			archer.stats.bulletDamage = 1.8f;
			archer.stats.bulletLifeTime = 0.9f;
			archer.stats.maxHp = 5;
			archer.shot.modelName = "test_arrow.obj";
			archer.shot.radius = 0.14f;
			archer.shot.scale = { 1.0f, 1.0f, 1.0f };
			archer.shot.pierce = 0;
			archer.shot.spin = false;
		}

		return characters;
	}

}

const CharacterData& GetCharacterData(CharacterType type)
{
	static const std::array<CharacterData, static_cast<size_t>(CharacterType::kCount)> characters = MakeCharacters();
	size_t index = static_cast<size_t>(type);
	assert(index < characters.size());
	return characters[index];
}