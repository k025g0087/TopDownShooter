#pragma once
#include <array>
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <random>
#include <vector>
#include "BaseScene.h"
#include "Mathutility.h"
#include "Object3d.h"
#include "PlayerInput.h"
#include "Player.h"
#include "BulletManager.h"
#include "Enemy.h"
#include "BuildSystem.h"
#include "BuildHud.h"
#include "GrowthChoice.h"
#include "ConfirmDialog.h"
#include "GachaEffect.h"
#include "LegendaryWeapon.h"
#include "ResultScreen.h"
#include "BossBase.h"
#include "DragonBoss.h"
#include "ItemData.h"
#include "CoinManager.h"
#include "InventoryHud.h"
#include "Barrier.h"
#include "CharacterData.h"
#include "DungeonGenerator.h"
#include "RoomTemplate.h"
#include "DungeonMap.h"
#include "Minimap.h"
#include "StageHud.h"

// 前方宣言（ヘッダではポインタしか使わないのでインクルードしない）
class DirectXCommon;
class SpriteCommon;
class Object3dCommon;
class Input;
class Camera;

// ステージシーン（神殿の1フロア。ランダムにつながった部屋を探索して、最奥のボス部屋を目指す）
class StageScene : public BaseScene
{
public: // メンバ関数
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private: // メンバ関数
	// フロアを作る（部屋の並び・マップ・ミニマップ）
	void BuildFloor();
	// 部屋の進行：入ったら扉を閉めて敵を出す、全部倒したら扉を開ける
	void UpdateRooms();
	// 敵を出す
	void SpawnEnemies(int32_t roomIndex);
	// カメラ：プレイヤーを斜め上から見下ろして、狙っている方へ少しずらす
	void UpdateCamera();
	// 弾と壁・敵の当たり判定
	void CheckBulletHits();
	// 爆発：中心から半径内の敵にダメージ（ignoreIndex：直接当たった敵の番号）
	void Explode(const Vector3& center, float radius, float damage, size_t ignoreIndex, bool ignoreBoss);
	// 貫通する弾の残りを減らす（消えたら true）
	bool ConsumePierce(BulletManager::Bullet& bullet);
	// ボス
	void SpawnBoss(int32_t roomIndex);
	void UpdateBoss();
	void DamageBoss(float damage);
	// ボスを倒したときの、大きめの力の選択
	void OpenBossGrowthChoice();
	// 次のフロアへ（フロアの決まりが足りなくなったら、最後のフロアの決まりをくり返す＝エンドレス）
	void GoToNextFloor();
	// 出口の小部屋：階段・転送陣に触れたら確認画面を出す
	void UpdateExit();
	// 確認画面（はい／いいえ）が出ている間の更新
	void UpdateConfirm();
	// この挑戦を終わらせて、リザルトを開く（returned：帰還したか）
	void FinishRun(bool returned);
	// プレイヤーのステータスを計算し直す（ビルド＋アイテム）
	void RefreshPlayerStats();
	// プレイヤーの攻撃を撃つ（アイテムの拡散で弾が増える）
	void FirePlayerShot(const Vector3& position, const Vector3& direction);
	// アイテム：追尾（弾を近くの敵へ曲げる）・跳ね返り・分裂
	void UpdateHoming();
	void BounceBullet(BulletManager::Bullet& bullet);
	void SplitBullet(BulletManager::Bullet& bullet);
	// ショップのガチャ：近づいたら案内を出して、E／パッドのXで回す
	void UpdateGacha();
	// 宝部屋の宝箱：近づいたら案内を出して、E／パッドのXで開ける（バリアが1枚もらえる）
	void UpdateChest();
	// 伝説の武器を手に入れる
	void GiveLegendary();
	// 伝説の武器の効果：聖剣（斬撃で敵の弾を消す）・天弓（消えた矢の場所に矢の雨）・見た目
	void UpdateLegendary();
	// 範囲の中の敵とボスにダメージ（炎・矢の雨で使う。演出は呼ぶ側で出す）
	void DamageArea(const Vector3& center, float radius, float damage);
	// アイテムを手に入れる（showPopup：「○○を手に入れた！」を出すか）
	void GiveItem(ItemType type, bool showPopup = true);
	// 敵にダメージを与える（倒したら数を数えて、画面を揺らす）
	void DamageEnemy(Enemy& enemy, float damage, const Vector3& direction);
	// 敵の更新（動き・弾・押し合い・壁）
	void UpdateEnemies();
	// ひな形で種類が決まっていない敵の種類を、部屋の種類に合わせてランダムに選ぶ
	EnemyType ChooseRandomEnemyType(RoomType roomType);
	// 敵の弾・体当たりとプレイヤーの当たり判定
	void CheckPlayerHits();
	// プレイヤーにダメージを与える（無敵中なら何もしない）
	void DamagePlayer(int32_t amount);
	// ゲームオーバー・帰還の後の更新（リザルトを出して、入力でやり直す）
	void UpdateGameOver();
	// 部屋をクリアしたら、取り込む力を選ぶ画面を開く
	void OpenGrowthChoice();
	// 力を選んでいる間の更新
	void UpdateGrowthChoice();
	// 力を取り込む（ステータスを計算し直して、表示を光らせる）
	void ApplyGrowth(GrowthStat stat, int32_t amount);
	// 世界を止めたまま、カメラ・画面表示・パーティクルだけ動かす（ゲームオーバー中・選んでいる間）
	void UpdateFrozenWorld();
	// 調整用の画面（ImGui）
	void DrawDebugWindow();

private: // メンバ変数
	// エンジン層の汎用システム（シングルトンから取得する。deleteしない）
	DirectXCommon* dxCommon = nullptr;
	SpriteCommon* spriteCommon = nullptr;
	Object3dCommon* object3dCommon = nullptr;
	Input* input = nullptr;
	// このシーンのカメラ（CameraManager が持っている。deleteしない）
	Camera* camera = nullptr;

	// 入力をまとめる係
	PlayerInputReader inputReader;
	PlayerInput playerInput;

	// プレイヤー・弾・敵
	std::unique_ptr<Player> player;
	BulletManager bullets;
	// 今のキャラの攻撃の性質（ImGui でその場で変えられるようにコピーを持つ）
	ShotStyle shotStyle;
	// 敵の弾の性質
	ShotStyle enemyShotStyle;
	ShotStyle fireShotStyle; // ドラゴンの炎の弾
	// 今の部屋の敵（部屋をクリアしたら空にする）
	std::vector<std::unique_ptr<Enemy>> enemies;
	// 弾の hitMask（64ビット）で覚えられる敵の数
	static inline const size_t kMaxEnemiesInRoom = 63;
	// ボスに当たったかを覚えるビット（敵は 0〜62 番を使う）
	static inline const uint64_t kBossHitBit = uint64_t{ 1 } << 63;

	// ボス（ボス部屋に入ったら出てくる）と、このフロアのボスの種類
	std::unique_ptr<BossBase> boss;
	BossType floorBoss = BossType::kDragon;
	// 出口の小部屋（ボス部屋の先）：下へ行く階段と帰還の転送陣。ボスを倒すまで扉が閉まっている
	std::unique_ptr<Object3d> stairs;
	std::unique_ptr<Object3d> portal;
	bool exitOpen = false;
	bool stairsArmed = true;  // 一度離れたか（確認で「いいえ」を選んだら、離れるまでは触れても出さない）
	bool portalArmed = true;
	static inline const float kStairsTouchRadius = 0.9f;  // これより近づくと階段に触れた
	static inline const float kPortalTouchRadius = 1.3f;  // これより近づくと転送陣に触れた
	static inline const float kRearmDistance = 2.2f;      // これより離れると、また触れられる

	// ---- アイテム・コイン・ガチャ ----
	ItemInventory items;
	CoinManager coins;
	InventoryHud inventoryHud;
	int32_t coinCount = 0;
	std::unique_ptr<Object3d> gachaMachine;
	Vector3 gachaPosition = { 0.0f, 0.0f, 0.0f };
	bool nearGacha = false;
	std::unique_ptr<Sprite> gachaPrompt;     // 「E・Ⓧ：ガチャを回す（40コイン）」
	std::unique_ptr<Sprite> gachaPromptPoor; // 「コインが足りない（40コイン）」
	static inline const int32_t kGachaPrice = 40;        // ガチャ1回のコイン（案内の画像にも 40 と書いてある）
	static inline const float kGachaRadius = 0.9f;       // ガチャの当たり判定の半径
	static inline const float kGachaUseDistance = 2.2f;  // これより近いと回せる
	static inline const float kHomingRange = 10.0f;      // 追尾する弾が敵を探す距離

	// ---- 宝箱・バリア ----
	Barrier barrier;
	std::unique_ptr<Object3d> chest;
	Vector3 chestPosition = { 0.0f, 0.0f, 0.0f };
	bool chestOpened = false;
	bool nearChest = false;
	std::unique_ptr<Sprite> chestPrompt; // 「E・Ⓧ：宝箱を開ける」
	static inline const float kChestRadius = 0.8f;       // 宝箱の当たり判定の半径
	static inline const float kChestUseDistance = 2.0f;  // これより近いと開けられる
	// 今のフロア（0から）と、遊んだ時間（秒）
	int32_t floorIndex = 0;
	float playTime = 0.0f;
	bool isGameClear = false; // 帰還した
	// この挑戦全体で倒した敵・集めたコイン（リザルトで出す。killCount はフロアごと）
	int32_t runKills = 0;
	int32_t runCoins = 0;
	// 確認画面（次の層へ進む？・帰還する？）と、リザルト
	ConfirmDialog confirmDialog;
	// ガチャを回したときの、画面全体の演出
	GachaEffect gachaEffect;
	// シークレット：伝説の武器（とても低い確率でガチャから出る。1回の挑戦で1つだけ）
	LegendaryWeapon legendary;
	static inline const int32_t kSecretOdds = 10000; // 1万回に1回（0.01%）
	bool forceSecretNext = false;                    // 調整用：次のガチャを必ずシークレットにする
	std::string legendaryWeaponModel;                // 伝説の武器を持ったとき、虹色に光らせる手のモデル
	// 矢の雨（天弓）：前のフレームで「消えたら雨を降らせる弾」だったか（弾の番号ごと）
	std::array<bool, BulletManager::kMaxBullets> rainBulletAlive = {};
	ResultScreen resultScreen;
	static inline const float kResultDelayAfterDeath = 1.6f; // 力尽きてからリザルトが出るまで（秒）

	// ---- ダンジョン ----
	RoomTemplateLibrary roomLibrary;
	DungeonLayout layout;
	DungeonMap map;
	Minimap minimap;
	StageHud hud;
	// ビルド（倒した敵でステータスが伸びる）と、その表示
	BuildSystem build;
	BuildHud buildHud;
	GrowthChoice growthChoice;
	// 今の部屋で倒した数（敵の種類ごと）
	std::array<int32_t, static_cast<size_t>(EnemyType::kCount)> roomKillCounts = {};
	// 画面（力の選択・確認など）を閉じたあと、攻撃ボタンを一度はなすまでは撃たない
	// （クリックで決定したとき、そのまま押しっぱなしで弾が出てしまわないように）
	bool holdFireUntilRelease = false;
	// ゲームオーバーになったか
	bool isGameOver = false;
	float gameOverTimer = 0.0f; // ゲームオーバーになってからの時間（すぐには入力を受けない）
	// 無敵モード（調整用。ImGui で切り替える）
	bool godMode = false;
	// 部屋ごとの状態
	enum class RoomState {
		kUnvisited, // まだ入っていない
		kFighting,  // 戦闘中（扉が閉まっている）
		kCleared,   // クリア（扉が開いている）
	};
	std::vector<RoomState> roomStates;
	std::vector<bool> roomVisited;
	int32_t currentRoom = -1;  // 今いる部屋（通路なら -1）
	int32_t fightingRoom = -1; // 戦闘中の部屋（なければ -1）
	float spawnTimer = 0.0f;   // 扉が閉まってから敵が出てくるまでの残り時間
	bool enemiesSpawned = false;
	bool bossCleared = false;

	// 平行光源（スプライトのルートシグネチャで使うので、使わなくてもセットする必要がある）
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	Object3d::DirectionalLight* directionalLightData = nullptr;

	// カメラの見ている点（なめらかに追いかける）
	Vector3 cameraFocus = { 0.0f, 0.0f, 0.0f };
	// 画面の揺れ（0〜1。だんだん減る）
	float shake = 0.0f;
	float shakeTime = 0.0f;

	// カメラの調整値（ImGui でその場で変えられる）
	struct CameraSettings {
		float pitch = 1.0f;         // 見下ろす角度（ラジアン）。大きいほど真上から
		float distance = 30.0f;     // 見ている点からの距離。大きいほど広く見える
		float lookAhead = 0.3f;     // 狙う点の方へずらす割合
		float lookAheadMax = 5.0f;  // ずらす距離の上限
		float follow = 0.15f;       // 追いかける速さ（1で即座）
	};
	CameraSettings cameraSettings;

	// 倒した数（あとでビルドの仕組みにつなげる）
	int32_t killCount = 0;

	std::mt19937 random{ std::random_device{}() };
	uint32_t seed = 0; // 今のフロアの種（同じ種なら同じ並びになる）

	// ---- 調整用の数値 ----
	static inline const float kDeltaTime = 1.0f / 60.0f; // 1フレームの秒数（エンジンが60fps固定）
	static inline const float kSpawnDelay = 0.6f;         // 扉が閉まってから敵が出てくるまで（秒）
	static inline const int32_t kEnemiesWithoutSpawnPoint = 5; // ひな形に e がないときの敵の数
	static inline const int32_t kSwarmGroupSize = 3;           // 群れの敵は何体まとめて出るか
};