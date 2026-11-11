#include "StageScene.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <string>
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "Model.h"
#include "Camera.h"
#include "CameraManager.h"
#include "ParticleManager.h"
#include "Input.h"
#include "SceneManager.h"
#include "BlendMode.h"
#include "ImGuiManager.h"

using namespace MathUtility;

namespace {
	// このシーンで使うカメラとパーティクルグループの名前
	const std::string kCameraName = "stage";
	const std::string kParticleSpark = "spark";
	// 仮のモデル
	const std::string kModelBullet = "test_bullet.obj";
	const std::string kModelEnemyBullet = "test_enemy_bullet.obj";
	const std::string kModelFireBullet = "test_fire.obj";
	const std::string kModelStairs = "test_stairs.obj";

	// フロアの決まり（フロアを増やすときはここに足す。ボスは候補からランダム）
	struct FloorConfig {
		int32_t roomCount;               // 部屋の数（スタート・宝・ボスを含む）
		std::vector<BossType> bossPool;  // ボスの候補
	};
	const std::vector<FloorConfig>& GetFloorConfigs()
	{
		static const std::vector<FloorConfig> floors = {
			{ 10, { BossType::kDragon } }, // 1F
		};
		return floors;
	}

	// ボスを作る（ボスを増やしたらここに足す）
	std::unique_ptr<BossBase> CreateBoss(BossType type)
	{
		switch (type) {
		case BossType::kDragon:
		default:
			return std::make_unique<DragonBoss>();
		}
	}
	// 部屋のひな形のフォルダ
	const std::string kRoomDirectory = "resources/rooms";

	const char* RoomTypeName(RoomType type)
	{
		switch (type) {
		case RoomType::kStart: return "Start";
		case RoomType::kTreasure: return "Treasure";
		case RoomType::kBoss: return "Boss";
		case RoomType::kCombat:
		default: return "Combat";
		}
	}
}

void StageScene::Initialize()
{
	// エンジン層の汎用システムは、シングルトンから取得する
	spriteCommon = SpriteCommon::GetInstance();
	dxCommon = spriteCommon->GetDxCommon();
	object3dCommon = Object3dCommon::GetInstance();
	input = Input::GetInstance();

	// カメラ（2回目に入ったときは、前に作ったものを使う）
	CameraManager* cameraManager = CameraManager::GetInstance();
	camera = cameraManager->FindCamera(kCameraName);
	if (camera == nullptr) {
		camera = cameraManager->CreateCamera(kCameraName);
	}
	camera->SetFarClip(200.0f);
	cameraManager->SetActiveCamera(kCameraName);

	// 選択画面で選んだキャラ
	const CharacterData& character = GetCharacterData(CharacterSelection::GetSelected());
	shotStyle = character.shot;

	// モデルの読み込み（2回目は自動で飛ばされる）。陰影がつくように Half Lambert にする
	ModelManager* modelManager = ModelManager::GetInstance();
	std::vector<std::string> litModels = { character.modelName, DungeonMap::kModelWall, DungeonMap::kModelDoor, DragonBoss::kModelBody };
	for (size_t i = 0; i < static_cast<size_t>(EnemyType::kCount); ++i) {
		litModels.push_back(Enemy::GetData(static_cast<EnemyType>(i)).modelName);
	}
	for (const std::string& name : litModels) {
		modelManager->LoadModel(name);
		modelManager->FindModel(name)->SetEnableLighting(2);
	}
	// 床と攻撃は陰影なし（床は暗くならないように、攻撃は光っているように見せる）
	const std::string unlitModels[] = {
		kModelBullet, kModelEnemyBullet, kModelFireBullet, kModelStairs, DragonBoss::kModelShadow, DragonBoss::kModelWarning, character.shot.modelName,
		DungeonMap::kModelFloorRoom, DungeonMap::kModelFloorBoss, DungeonMap::kModelFloorTreasure, DungeonMap::kModelFloorCorridor };
	for (const std::string& name : unlitModels) {
		modelManager->LoadModel(name);
		modelManager->FindModel(name)->SetEnableLighting(0);
	}

	// 画面表示の画像（ミニマップ・ハート・ゲームオーバー）
	TextureManager* textureManager = TextureManager::GetInstance();
	for (const char* path : { Minimap::kTexture, StageHud::kTextureHeartFull, StageHud::kTextureHeartHalf,
		StageHud::kTextureHeartEmpty, StageHud::kTextureGameOver, StageHud::kTextureWhite, BuildHud::kTextureDigits,
		GrowthChoice::kTextureTitle, GrowthChoice::kTextureHint, StageHud::kTextureGameClear, DragonBoss::kTextureName }) {
		textureManager->LoadTexture(path);
	}
	for (size_t i = 0; i < static_cast<size_t>(GrowthStat::kCount); ++i) {
		textureManager->LoadTexture(BuildHud::GetIconTexture(static_cast<GrowthStat>(i)));
		textureManager->LoadTexture(BuildHud::GetPopupTexture(static_cast<GrowthStat>(i)));
		textureManager->LoadTexture(GrowthChoice::GetCardTexture(static_cast<GrowthStat>(i)));
	}

	// 敵の弾：赤く光る玉。遅めで見やすく、くるくる回る
	enemyShotStyle = ShotStyle{};
	enemyShotStyle.modelName = kModelEnemyBullet;
	enemyShotStyle.radius = 0.22f;
	enemyShotStyle.spin = true;
	// 炎の弾（ドラゴン）：オレンジで少し大きい
	fireShotStyle = enemyShotStyle;
	fireShotStyle.modelName = kModelFireBullet;
	fireShotStyle.radius = 0.3f;

	// 階段（ボスを倒すと出る。次のフロアへ）
	stairs = std::make_unique<Object3d>();
	stairs->Initialize(object3dCommon);
	stairs->SetModel(kModelStairs);

	// パーティクル（当たったときの火花）
	ParticleManager* particleManager = ParticleManager::GetInstance();
	if (!particleManager->ContainsParticleGroup(kParticleSpark)) {
		particleManager->CreateParticleGroup(kParticleSpark, "resources/circle.png");
	}

	// テクスチャの転送を実行して、中間リソースを解放する（読み込みの後でまとめて1回）
	dxCommon->ExecuteCommandList();
	TextureManager::GetInstance()->ReleaseIntermediateResources();

	// 平行光源（スプライトはライティングしないが、ルートシグネチャに必要なので作っておく）
	directionalLightResource = dxCommon->CreateBufferResource(sizeof(Object3d::DirectionalLight));
	HRESULT hr = directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	assert(SUCCEEDED(hr));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	// プレイヤー
	player = std::make_unique<Player>();
	player->Initialize({ 0.0f, 0.0f, 0.0f }, character);

	// 弾
	bullets.Initialize(kModelBullet);

	// 画面表示
	hud.Initialize(spriteCommon);
	buildHud.Initialize(spriteCommon);
	growthChoice.Initialize(spriteCommon);
	// ビルド（キャラの初期ステータスから始める）
	build.Reset(character.stats);

	// 部屋のひな形を読んで、1フロア目を作る
	roomLibrary.Load(kRoomDirectory);
	floorIndex = 0;
	playTime = 0.0f;
	BuildFloor();
}

void StageScene::BuildFloor()
{
	// 種を決めて、部屋の並び → マップ → ミニマップの順に作る
	seed = std::random_device{}();
	random.seed(seed);
	const FloorConfig& floor = GetFloorConfigs()[static_cast<size_t>(floorIndex)];
	layout = DungeonGenerator::Generate(random, floor.roomCount);
	// このフロアのボスを候補から選ぶ
	std::uniform_int_distribution<size_t> bossDistribution(0, floor.bossPool.size() - 1);
	floorBoss = floor.bossPool[bossDistribution(random)];
	boss.reset();
	stairsActive = false;
	map.Build(layout, roomLibrary, random);
	minimap.Initialize(spriteCommon, layout);

	roomStates.assign(layout.rooms.size(), RoomState::kUnvisited);
	roomVisited.assign(layout.rooms.size(), false);
	// 敵のいない部屋は、最初からクリア扱い
	for (size_t i = 0; i < layout.rooms.size(); ++i) {
		RoomType type = layout.rooms[i].type;
		if (type == RoomType::kStart || type == RoomType::kTreasure) {
			roomStates[i] = RoomState::kCleared;
		}
	}
	enemies.clear();
	currentRoom = -1;
	fightingRoom = -1;
	enemiesSpawned = false;
	bossCleared = false;
	killCount = 0;
	isGameOver = false;
	isGameClear = false;
	gameOverTimer = 0.0f;
	hud.Reset();

	// プレイヤーをスタートの部屋の真ん中に置いて、カメラも合わせる
	player->SetPosition(map.GetRoomCenter(layout.startRoom));
	cameraFocus = player->GetPosition();
	UpdateCamera();
}

void StageScene::Finalize()
{
	enemies.clear();
	boss.reset();
	stairs.reset();
	player.reset();
	directionalLightResource.Reset();
	directionalLightData = nullptr;
	camera = nullptr;
}

void StageScene::Update()
{
	// Esc でキャラ選択へ戻る（仮。キャラを遊び比べやすいように）
	if (input->TriggerKey(DIK_ESCAPE)) {
		SceneManager::GetInstance()->ChangeScene("SELECT");
		return;
	}

	// ゲームオーバー中は、世界を止めてやり直しの入力だけ待つ
	if (isGameOver || isGameClear) {
		UpdateGameOver();
		return;
	}
	// 力を選んでいる間は、世界を止めて選ぶ画面だけ動かす
	if (growthChoice.IsOpen()) {
		UpdateGrowthChoice();
		return;
	}

	// 遊んだ時間（止まっている間は数えない）
	playTime += kDeltaTime;

	// 入力を読む（カメラは前のフレームのものを使う）
	playerInput = inputReader.Read(input, camera, player->GetPosition(), Player::kShotHeight);

	// プレイヤー（動いた後で壁から押し戻す）
	Player::ShotRequest shot = player->Update(playerInput, kDeltaTime);
	player->SetPosition(map.ResolveCircle(player->GetPosition(), player->GetRadius()));
	// 撃つ位置が壁の中なら撃たない（壁に張り付いて撃つと、壁の向こうに弾が出てしまうので）
	if (shot.shoot && !map.IsSolidAt(shot.position)) {
		bullets.Fire(shot.position, shot.direction, player->GetStats(), shotStyle, true);
	}

	// 部屋の進行（入ったら閉める・全部倒したら開ける）
	UpdateRooms();

	// 敵とボス
	UpdateEnemies();
	UpdateBoss();

	// 階段：乗ったら次のフロアへ（最後のフロアならクリア）
	if (stairsActive) {
		Vector3 diff = { player->GetPosition().x - stairsPosition.x, 0.0f, player->GetPosition().z - stairsPosition.z };
		// 出た瞬間に乗っていても進まないように、一度離れてから乗ったら進む
		if (Length(diff) > 2.5f) {
			stairsArmed = true;
		}
		if (stairsArmed && Length(diff) < 1.2f) {
			GoToNextFloor();
			return;
		}
	}

	// 弾と当たり判定
	bullets.Update(kDeltaTime, 100000.0f);
	CheckBulletHits();
	CheckPlayerHits();

	// カメラ（プレイヤーが動いた後で）
	UpdateCamera();

	// マップ（見えているあたりだけ）
	map.Update(camera, cameraFocus);

	// ミニマップと画面表示
	minimap.Update(roomVisited, currentRoom);
	hud.Update(player->GetHp(), player->GetMaxHp(), kDeltaTime);
	buildHud.Update(build, kDeltaTime);

	// パーティクル（カメラの更新の後で）
	ParticleManager::GetInstance()->Update();

	DrawDebugWindow();
}

void StageScene::UpdateRooms()
{
	// 今いる部屋（扉のマスは部屋に含めない）
	currentRoom = map.GetRoomIndexAt(player->GetPosition(), 1);
	if (currentRoom >= 0) {
		roomVisited[static_cast<size_t>(currentRoom)] = true;
	}

	// まだ入っていない戦闘部屋に、しっかり入ったら（壁から2マス内側）扉を閉める
	int32_t deepRoom = map.GetRoomIndexAt(player->GetPosition(), 2);
	if (fightingRoom < 0 && deepRoom >= 0 && roomStates[static_cast<size_t>(deepRoom)] == RoomState::kUnvisited) {
		fightingRoom = deepRoom;
		roomStates[static_cast<size_t>(deepRoom)] = RoomState::kFighting;
		map.SetRoomLocked(deepRoom, true);
		roomKillCounts.fill(0);
		spawnTimer = kSpawnDelay;
		enemiesSpawned = false;
		shake = (std::min)(1.0f, shake + 0.25f);
		// 出てくる場所に予告の火花
		for (const DungeonMap::RoomArea::SpawnPoint& point : map.GetRoomArea(deepRoom).spawnPoints) {
			ParticleManager::GetInstance()->Emit(kParticleSpark, point.position + Vector3{ 0.0f, 0.3f, 0.0f }, 4u);
		}
	}

	if (fightingRoom < 0) {
		return;
	}

	// 少し待ってから敵を出す
	if (!enemiesSpawned) {
		spawnTimer -= kDeltaTime;
		if (spawnTimer <= 0.0f) {
			if (layout.rooms[static_cast<size_t>(fightingRoom)].type == RoomType::kBoss) {
				SpawnBoss(fightingRoom);
			} else {
				SpawnEnemies(fightingRoom);
			}
			enemiesSpawned = true;
		}
		return;
	}

	// 全部倒したらクリア：扉を開ける（ボスは倒れる演出が終わってから）
	bool anyAlive = std::any_of(enemies.begin(), enemies.end(), [](const std::unique_ptr<Enemy>& enemy) { return enemy->IsAlive(); });
	if (boss && !boss->IsDeathFinished()) {
		anyAlive = true;
	}
	if (!anyAlive) {
		roomStates[static_cast<size_t>(fightingRoom)] = RoomState::kCleared;
		map.SetRoomLocked(fightingRoom, false);
		if (layout.rooms[static_cast<size_t>(fightingRoom)].type == RoomType::kBoss) {
			bossCleared = true;
		}
		enemies.clear();
		// 残っている敵の弾も消す（クリアした後に当たらないように）
		bullets.ClearEnemyBullets();
		fightingRoom = -1;
		if (boss) {
			// ボス：倒したら大きめの力を選べて、部屋の真ん中に階段が出る
			hud.SetBoss(nullptr);
			OpenBossGrowthChoice();
			stairsActive = true;
			stairsArmed = false;
			stairsPosition = map.GetRoomCenter(layout.bossRoom);
			stairs->SetTranslate(stairsPosition);
			boss.reset();
		} else {
			// この部屋で倒した敵の中から、取り込む力を選ぶ
			OpenGrowthChoice();
		}
	}
}

void StageScene::SpawnEnemies(int32_t roomIndex)
{
	enemies.clear();
	RoomType roomType = layout.rooms[static_cast<size_t>(roomIndex)].type;
	std::vector<DungeonMap::RoomArea::SpawnPoint> points = map.GetRoomArea(roomIndex).spawnPoints;
	// ひな形に出てくる場所がなければ、部屋の中のランダムな場所にする
	if (points.empty()) {
		Vector3 center = map.GetRoomCenter(roomIndex);
		float halfX = (static_cast<float>(DungeonConfig::kRoomWidth) / 2.0f - 3.0f) * DungeonConfig::kTileSize;
		float halfZ = (static_cast<float>(DungeonConfig::kRoomHeight) / 2.0f - 3.0f) * DungeonConfig::kTileSize;
		std::uniform_real_distribution<float> rangeX(-halfX, halfX);
		std::uniform_real_distribution<float> rangeZ(-halfZ, halfZ);
		for (int32_t i = 0; i < kEnemiesWithoutSpawnPoint; ++i) {
			Vector3 point = center + Vector3{ rangeX(random), 0.0f, rangeZ(random) };
			if (!map.IsSolidAt(point)) {
				points.push_back({ point, 'e' });
			}
		}
	}
	for (const DungeonMap::RoomArea::SpawnPoint& point : points) {
		// 文字で種類を決める（e はランダム）
		EnemyType type = EnemyType::kShooter;
		switch (point.kind) {
		case 'c': type = EnemyType::kCharger; break;
		case 's': type = EnemyType::kShooter; break;
		case 'w': type = EnemyType::kSwarm; break;
		case 't': type = EnemyType::kTank; break;
		default: type = ChooseRandomEnemyType(roomType); break;
		}
		// 群れの敵は3体まとめて出す
		int32_t count = (type == EnemyType::kSwarm) ? kSwarmGroupSize : 1;
		for (int32_t i = 0; i < count; ++i) {
			if (enemies.size() >= kMaxEnemiesInRoom) {
				break;
			}
			Vector3 position = point.position;
			if (count > 1) {
				float angle = std::numbers::pi_v<float> *2.0f * static_cast<float>(i) / static_cast<float>(count);
				position += Vector3{ std::sin(angle), 0.0f, std::cos(angle) } * 0.9f;
				position = map.ResolveCircle(position, Enemy::GetData(type).radius);
			}
			std::unique_ptr<Enemy> enemy = std::make_unique<Enemy>();
			enemy->Initialize(type, position, static_cast<uint32_t>(random()));
			enemy->SetCamera(camera);
			enemies.push_back(std::move(enemy));
		}
		ParticleManager::GetInstance()->Emit(kParticleSpark, point.position + Vector3{ 0.0f, 0.5f, 0.0f }, 6u);
	}
}

EnemyType StageScene::ChooseRandomEnemyType(RoomType roomType)
{
	// 出やすさ（突進・射撃・群れ・重装）。ボス部屋は重装が多め
	std::array<float, static_cast<size_t>(EnemyType::kCount)> weights = { 25.0f, 35.0f, 25.0f, 15.0f };
	if (roomType == RoomType::kBoss) {
		weights = { 20.0f, 25.0f, 20.0f, 35.0f };
	}
	std::discrete_distribution<int32_t> distribution(weights.begin(), weights.end());
	return static_cast<EnemyType>(distribution(random));
}

void StageScene::UpdateEnemies()
{
	for (std::unique_ptr<Enemy>& enemy : enemies) {
		Enemy::ShotRequest enemyShot = enemy->Update(player->GetPosition(), kDeltaTime);
		// 弾を撃つ（count 発を spread ずつずらして。真ん中がプレイヤーの方）
		if (enemyShot.shoot && player->IsAlive()) {
			float baseAngle = std::atan2(enemyShot.direction.x, enemyShot.direction.z);
			for (int32_t i = 0; i < enemyShot.count; ++i) {
				float angle = baseAngle + (static_cast<float>(i) - static_cast<float>(enemyShot.count - 1) / 2.0f) * enemyShot.spread;
				Vector3 direction = { std::sin(angle), 0.0f, std::cos(angle) };
				bullets.Fire(enemyShot.position, direction, enemyShot.speed, enemyShot.lifeTime, enemyShot.damage, enemyShotStyle, false);
			}
		}
	}

	// 敵どうしが重ならないように押し合う
	for (size_t i = 0; i < enemies.size(); ++i) {
		if (!enemies[i]->IsAlive()) {
			continue;
		}
		for (size_t j = i + 1; j < enemies.size(); ++j) {
			if (!enemies[j]->IsAlive()) {
				continue;
			}
			Vector3 a = enemies[i]->GetPosition();
			Vector3 b = enemies[j]->GetPosition();
			Vector3 diff = { b.x - a.x, 0.0f, b.z - a.z };
			float distance = Length(diff);
			float minDistance = enemies[i]->GetRadius() + enemies[j]->GetRadius();
			if (distance >= minDistance || distance < 0.0001f) {
				continue;
			}
			Vector3 push = diff / distance * ((minDistance - distance) * 0.5f);
			enemies[i]->SetPosition(a - push);
			enemies[j]->SetPosition(b + push);
		}
	}

	// 壁から押し戻す（突進中に壁にぶつかったら止まる）
	for (std::unique_ptr<Enemy>& enemy : enemies) {
		if (!enemy->IsAlive()) {
			continue;
		}
		Vector3 before = enemy->GetPosition();
		Vector3 after = map.ResolveCircle(before, enemy->GetRadius());
		Vector3 moved = { after.x - before.x, 0.0f, after.z - before.z };
		if (Length(moved) > 0.0001f) {
			enemy->SetPosition(after);
			if (Length(moved) > 0.05f) {
				enemy->OnHitWall();
			}
		}
	}
}

void StageScene::UpdateCamera()
{
	// 見る点：プレイヤーの位置から、狙う点の方へ少しずらす（ガンジョン風。狙っている先が広く見える）
	Vector3 target = player->GetPosition();
	if (playerInput.hasAimPoint) {
		Vector3 toAim = { playerInput.aimPoint.x - target.x, 0.0f, playerInput.aimPoint.z - target.z };
		toAim = toAim * cameraSettings.lookAhead;
		if (Length(toAim) > cameraSettings.lookAheadMax) {
			toAim = Normalize(toAim) * cameraSettings.lookAheadMax;
		}
		target += toAim;
	} else if (Length(playerInput.aim) > 0.0f) {
		target += Vector3{ playerInput.aim.x, 0.0f, playerInput.aim.y } * (cameraSettings.lookAheadMax * 0.5f);
	}
	cameraFocus = Lerp(cameraFocus, target, cameraSettings.follow);

	// 揺れ（だんだん減る）
	shake = (shake > 0.0f) ? shake - kDeltaTime * 2.5f : 0.0f;
	shakeTime += kDeltaTime;
	float power = shake * shake * 0.4f;
	Vector3 shakeOffset = { std::sin(shakeTime * 47.0f) * power, 0.0f, std::cos(shakeTime * 39.0f) * power };

	// カメラは見る点から、見下ろす角度の分だけ後ろ上に置く（カメラの前の向き＝(0, -sin, cos)）
	Vector3 forward = { 0.0f, -std::sin(cameraSettings.pitch), std::cos(cameraSettings.pitch) };
	camera->SetRotate({ cameraSettings.pitch, 0.0f, 0.0f });
	camera->SetTranslate(cameraFocus + shakeOffset - forward * cameraSettings.distance);
	camera->Update();

	// 見た目のカメラをセット
	player->SetCamera(camera);
	bullets.SetCamera(camera);
	for (std::unique_ptr<Enemy>& enemy : enemies) {
		enemy->SetCamera(camera);
	}
	if (boss) {
		boss->SetCamera(camera);
	}
	if (stairs) {
		stairs->SetCamera(camera);
		stairs->SetRotate({ 0.0f, shakeTime * 0.8f, 0.0f });
		stairs->Update();
	}
}

void StageScene::CheckBulletHits()
{
	ParticleManager* particleManager = ParticleManager::GetInstance();
	for (BulletManager::Bullet& bullet : bullets.GetBullets()) {
		if (!bullet.active) {
			continue;
		}
		// 壁に当たったら消える（爆発する弾はその場で爆発）
		if (map.IsSolidAt(bullet.position)) {
			if (bullet.fromPlayer && bullet.explosionRadius > 0.0f) {
				Explode(bullet.position, bullet.explosionRadius, bullet.damage * bullet.explosionDamageRate, enemies.size(), false);
			} else {
				particleManager->Emit(kParticleSpark, bullet.position, 2u);
			}
			bullet.active = false;
			continue;
		}
		// 敵の弾は CheckPlayerHits で見る
		if (!bullet.fromPlayer) {
			continue;
		}
		float bulletRadius = BulletManager::GetCurrentRadius(bullet);

		// ボス（hitMask の一番上のビットを使う）
		if (boss && boss->IsHittable() && (bullet.hitMask & kBossHitBit) == 0) {
			Vector3 diff = { bullet.position.x - boss->GetPosition().x, 0.0f, bullet.position.z - boss->GetPosition().z };
			if (Length(diff) <= bulletRadius + boss->GetRadius()) {
				bullet.hitMask |= kBossHitBit;
				DamageBoss(bullet.damage);
				particleManager->Emit(kParticleSpark, bullet.position, 3u);
				if (bullet.explosionRadius > 0.0f) {
					Explode(bullet.position, bullet.explosionRadius, bullet.damage * bullet.explosionDamageRate, enemies.size(), true);
					bullet.active = false;
					continue;
				}
				if (ConsumePierce(bullet)) {
					continue;
				}
			}
		}

		for (size_t i = 0; i < enemies.size(); ++i) {
			Enemy* enemy = enemies[i].get();
			uint64_t bit = uint64_t{ 1 } << i;
			// 倒れている敵と、この弾がもう当たった敵（貫通中）は飛ばす
			if (!enemy->IsAlive() || (bullet.hitMask & bit) != 0) {
				continue;
			}
			// 円どうしの当たり判定（高さは見ない）
			Vector3 diff = { bullet.position.x - enemy->GetPosition().x, 0.0f, bullet.position.z - enemy->GetPosition().z };
			if (Length(diff) > bulletRadius + enemy->GetRadius()) {
				continue;
			}
			bullet.hitMask |= bit;
			DamageEnemy(*enemy, bullet.damage, bullet.velocity);
			particleManager->Emit(kParticleSpark, bullet.position, 3u);

			// 爆発する弾：周りの敵にもダメージを与えて消える
			if (bullet.explosionRadius > 0.0f) {
				Explode(bullet.position, bullet.explosionRadius, bullet.damage * bullet.explosionDamageRate, i, false);
				bullet.active = false;
				break;
			}
			if (ConsumePierce(bullet)) {
				break;
			}
		}
	}
}

bool StageScene::ConsumePierce(BulletManager::Bullet& bullet)
{
	// 貫通：回数が残っていれば飛び続ける（-1 は何体でも）。消えたら true
	if (bullet.pierceLeft == 0) {
		bullet.active = false;
		return true;
	}
	if (bullet.pierceLeft > 0) {
		bullet.pierceLeft--;
	}
	return false;
}

void StageScene::Explode(const Vector3& center, float radius, float damage, size_t ignoreIndex, bool ignoreBoss)
{
	ParticleManager::GetInstance()->Emit(kParticleSpark, center, 20u);
	shake = (std::min)(1.0f, shake + 0.2f);
	for (size_t i = 0; i < enemies.size(); ++i) {
		Enemy* enemy = enemies[i].get();
		// 直接当たった敵には、もうダメージを与えているので飛ばす
		if (i == ignoreIndex || !enemy->IsAlive()) {
			continue;
		}
		Vector3 diff = { enemy->GetPosition().x - center.x, 0.0f, enemy->GetPosition().z - center.z };
		if (Length(diff) > radius + enemy->GetRadius()) {
			continue;
		}
		// 爆発の中心から外へ押す
		Vector3 push = (Length(diff) > 0.001f) ? diff : Vector3{ 0.0f, 0.0f, 1.0f };
		DamageEnemy(*enemy, damage, push);
	}
	// ボスにも当たる
	if (!ignoreBoss && boss && boss->IsHittable()) {
		Vector3 diff = { boss->GetPosition().x - center.x, 0.0f, boss->GetPosition().z - center.z };
		if (Length(diff) <= radius + boss->GetRadius()) {
			DamageBoss(damage);
		}
	}
}

void StageScene::DamageEnemy(Enemy& enemy, float damage, const Vector3& direction)
{
	bool killed = enemy.Hit(damage, direction);
	if (killed) {
		killCount++;
		// この部屋で倒した数を種類ごとに数える（クリアしたときの選択肢になる）
		roomKillCounts[static_cast<size_t>(enemy.GetType())]++;
		shake = (std::min)(1.0f, shake + 0.35f);
		ParticleManager::GetInstance()->Emit(kParticleSpark, enemy.GetPosition() + Vector3{ 0.0f, 0.5f, 0.0f }, 16u);
	}
}

void StageScene::CheckPlayerHits()
{
	if (!player->IsAlive()) {
		return;
	}
	const Vector3& playerPosition = player->GetPosition();

	// 敵の弾（ロールの無敵中はすり抜ける。弾も消えない）
	for (BulletManager::Bullet& bullet : bullets.GetBullets()) {
		if (!bullet.active || bullet.fromPlayer) {
			continue;
		}
		Vector3 diff = { bullet.position.x - playerPosition.x, 0.0f, bullet.position.z - playerPosition.z };
		if (Length(diff) > BulletManager::GetCurrentRadius(bullet) + player->GetRadius()) {
			continue;
		}
		if (player->CanTakeDamage()) {
			bullet.active = false;
			DamagePlayer(static_cast<int32_t>(bullet.damage));
		}
	}

	// ボスの体当たりと、まわりの攻撃（しっぽのなぎ払いなど）
	if (boss) {
		Vector3 diff = { boss->GetPosition().x - playerPosition.x, 0.0f, boss->GetPosition().z - playerPosition.z };
		float distance = Length(diff);
		if (boss->IsTouchDangerous() && distance < boss->GetRadius() + player->GetRadius()) {
			DamagePlayer(1);
		}
		float area = boss->GetAreaAttackRadius();
		if (area > 0.0f && distance < area + player->GetRadius()) {
			DamagePlayer(1);
		}
	}

	// 敵の体当たり
	for (std::unique_ptr<Enemy>& enemy : enemies) {
		if (!enemy->IsAlive()) {
			continue;
		}
		Vector3 diff = { enemy->GetPosition().x - playerPosition.x, 0.0f, enemy->GetPosition().z - playerPosition.z };
		if (Length(diff) < enemy->GetRadius() + player->GetRadius()) {
			DamagePlayer(1);
		}
	}
}

void StageScene::DamagePlayer(int32_t amount)
{
	if (godMode || !player->CanTakeDamage()) {
		return;
	}
	bool died = player->TakeDamage(amount);
	hud.OnDamaged();
	shake = (std::min)(1.0f, shake + 0.6f);
	ParticleManager::GetInstance()->Emit(kParticleSpark, player->GetPosition() + Vector3{ 0.0f, 0.5f, 0.0f }, 10u);
	if (died) {
		// ゲームオーバー：敵の弾を消して、画面を暗くする
		isGameOver = true;
		gameOverTimer = 0.0f;
		bullets.ClearEnemyBullets();
		hud.ShowGameOver();
		shake = 1.0f;
		ParticleManager::GetInstance()->Emit(kParticleSpark, player->GetPosition() + Vector3{ 0.0f, 0.5f, 0.0f }, 30u);
	}
}

void StageScene::OpenGrowthChoice()
{
	// 倒した種類ごとに選択肢を作る（多く倒した順）
	std::vector<GrowthChoice::Option> options;
	for (size_t i = 0; i < roomKillCounts.size(); ++i) {
		if (roomKillCounts[i] <= 0) {
			continue;
		}
		GrowthChoice::Option option;
		option.stat = BuildSystem::GetGrowthStat(static_cast<EnemyType>(i));
		option.amount = roomKillCounts[i];
		option.percent = BuildSystem::GetPercentFor(option.stat, option.amount);
		options.push_back(option);
	}
	std::stable_sort(options.begin(), options.end(), [](const GrowthChoice::Option& a, const GrowthChoice::Option& b) {
		return a.amount > b.amount;
		});
	growthChoice.Open(options);
}

void StageScene::UpdateGrowthChoice()
{
	int32_t chosen = growthChoice.Update(input, kDeltaTime);
	if (chosen >= 0) {
		const GrowthChoice::Option& option = growthChoice.GetOption(static_cast<size_t>(chosen));
		ApplyGrowth(option.stat, option.amount);
		ParticleManager::GetInstance()->Emit(kParticleSpark, player->GetPosition() + Vector3{ 0.0f, 0.8f, 0.0f }, 20u);
	}
	UpdateFrozenWorld();
}

void StageScene::ApplyGrowth(GrowthStat stat, int32_t amount)
{
	build.AddGrowth(stat, amount);
	player->GetStats() = build.ComputeStats();
	buildHud.OnGrowth(stat);
}

void StageScene::UpdateFrozenWorld()
{
	// 止まった世界をそのまま見せる（カメラの揺れ・画面表示・パーティクルだけ動かす）
	UpdateCamera();
	map.Update(camera, cameraFocus);
	minimap.Update(roomVisited, currentRoom);
	hud.Update(player->GetHp(), player->GetMaxHp(), kDeltaTime);
	buildHud.Update(build, kDeltaTime);
	ParticleManager::GetInstance()->Update();
	DrawDebugWindow();
}

void StageScene::SpawnBoss(int32_t roomIndex)
{
	Vector3 center = map.GetRoomCenter(roomIndex);
	// 部屋の奥のほうに出す（入ってきたプレイヤーと距離を取る）
	Vector3 spawn = center;
	Vector3 toPlayer = { player->GetPosition().x - center.x, 0.0f, player->GetPosition().z - center.z };
	if (Length(toPlayer) > 0.1f) {
		spawn = center - Normalize(toPlayer) * 6.0f;
	}
	Vector2 halfSize = {
		(static_cast<float>(DungeonConfig::kRoomWidth) / 2.0f - 1.0f) * DungeonConfig::kTileSize,
		(static_cast<float>(DungeonConfig::kRoomHeight) / 2.0f - 1.0f) * DungeonConfig::kTileSize };
	boss = CreateBoss(floorBoss);
	boss->Initialize(spawn, center, halfSize, static_cast<uint32_t>(random()));
	boss->SetCamera(camera);
	hud.SetBoss(boss->GetNameTexture());
	shake = 1.0f;
}

void StageScene::UpdateBoss()
{
	if (!boss) {
		return;
	}
	BossBase::Events events;
	boss->Update(player->GetPosition(), kDeltaTime, events);
	if (boss->UsesWallCollision()) {
		boss->SetPosition(map.ResolveCircle(boss->GetPosition(), boss->GetRadius()));
	}
	// 弾
	if (player->IsAlive()) {
		for (const BossBase::Shot& shot : events.shots) {
			const ShotStyle& style = (shot.style == 1) ? fireShotStyle : enemyShotStyle;
			bullets.Fire(shot.position, shot.direction, shot.speed, shot.lifeTime, shot.damage, style, false);
		}
	}
	// 爆発と吠え声（火花と揺れ）
	for (const Vector3& point : events.explosions) {
		ParticleManager::GetInstance()->Emit(kParticleSpark, point + Vector3{ 0.0f, 0.3f, 0.0f }, 12u);
		shake = (std::min)(1.0f, shake + 0.25f);
	}
	if (events.roar) {
		shake = 1.0f;
	}
	hud.SetBossHpRate(boss->GetHpRate());
}

void StageScene::DamageBoss(float damage)
{
	if (!boss) {
		return;
	}
	bool killed = boss->Hit(damage);
	if (killed) {
		shake = 1.0f;
		bullets.ClearEnemyBullets();
	}
}

void StageScene::OpenBossGrowthChoice()
{
	// ボスの力：4つ全部から選べて、1部屋より大きく伸びる
	const int32_t amounts[] = { 5, 4, 10, 2 }; // 移動速度・射程・連射・威力（それぞれ +20〜24%）
	std::vector<GrowthChoice::Option> options;
	for (size_t i = 0; i < static_cast<size_t>(GrowthStat::kCount); ++i) {
		GrowthChoice::Option option;
		option.stat = static_cast<GrowthStat>(i);
		option.amount = amounts[i];
		option.percent = BuildSystem::GetPercentFor(option.stat, option.amount);
		options.push_back(option);
	}
	growthChoice.Open(options);
}

void StageScene::GoToNextFloor()
{
	stairsActive = false;
	if (floorIndex + 1 < static_cast<int32_t>(GetFloorConfigs().size())) {
		// 次のフロアへ（HP とビルドはそのまま持っていく）
		floorIndex++;
		bullets.Initialize(kModelBullet);
		BuildFloor();
	} else {
		// 最後のフロア：クリア
		isGameClear = true;
		gameOverTimer = 0.0f;
		hud.ShowClear(playTime);
	}
}

void StageScene::UpdateGameOver()
{
	gameOverTimer += kDeltaTime;
	// 押しっぱなしで飛ばさないように、少し待ってから入力を受ける
	if (gameOverTimer > 1.2f) {
		if (input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) || input->TriggerPad(XINPUT_GAMEPAD_A)) {
			// ゲームオーバー：同じキャラでもう一度（新しいフロア）　クリア：キャラ選択へ
			SceneManager::GetInstance()->ChangeScene(isGameClear ? "SELECT" : "STAGE");
			return;
		}
		if (input->TriggerKey(DIK_ESCAPE) || input->TriggerPad(XINPUT_GAMEPAD_B)) {
			SceneManager::GetInstance()->ChangeScene("SELECT");
			return;
		}
	}

	UpdateFrozenWorld();
}

void StageScene::Draw()
{
	// 3Dの描画（不透明）
	object3dCommon->CommonDrawSetting();
	object3dCommon->SetPipelineState(kBlendModeNormal, false);
	map.Draw();
	for (std::unique_ptr<Enemy>& enemy : enemies) {
		enemy->Draw();
	}
	if (boss) {
		boss->Draw();
	}
	if (stairsActive) {
		stairs->Draw();
	}
	player->Draw();
	bullets.Draw();

	// パーティクル（3Dの後）
	ParticleManager::GetInstance()->Draw();

	// 2D（ミニマップ・ハート・ゲームオーバー）
	spriteCommon->CommonDrawSetting(kBlendModeNormal);
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
	minimap.Draw();
	buildHud.Draw();
	hud.Draw();
	growthChoice.Draw();
}

void StageScene::DrawDebugWindow()
{
#ifdef USE_IMGUI
	ImGui::Begin("Stage");
	ImGui::Text("WASD:move  Mouse:aim  LClick:shoot  RClick/Space:roll  Esc:select");
	if (isGameOver) {
		ImGui::TextColored({ 1.0f, 0.3f, 0.3f, 1.0f }, "GAME OVER  Enter/Pad A: retry  Esc/Pad B: select");
	}
	ImGui::Text("Pad: LStick move / RStick aim / RT shoot / LT,LB,A roll");
	ImGui::Text("Device: %s", playerInput.usingPad ? "Pad" : "Keyboard+Mouse");
	ImGui::Text("Character: %s", GetCharacterData(CharacterSelection::GetSelected()).name);
	ImGui::Text("Kills: %d", killCount);
	ImGui::Text("HP: %d / %d", player->GetHp(), player->GetMaxHp());
	ImGui::Text("Floor: %d / %d   Time: %02d:%02d", floorIndex + 1, static_cast<int32_t>(GetFloorConfigs().size()),
		static_cast<int32_t>(playTime) / 60, static_cast<int32_t>(playTime) % 60);
	if (boss) {
		ImGui::Text("Boss HP: %.0f%%", boss->GetHpRate() * 100.0f);
		if (ImGui::Button("Boss -25% HP")) {
			DamageBoss(450.0f * 0.25f);
		}
	}
	if (ImGui::Button("Warp to Boss Room")) {
		// ボス部屋の入口の少し内側へ（テスト用）
		player->SetPosition(map.GetRoomCenter(layout.bossRoom) + Vector3{ 0.0f, 0.0f, -6.0f });
	}
	ImGui::Checkbox("God Mode (no damage)", &godMode);
	if (ImGui::CollapsingHeader("Build", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* statNames[] = { "Move Speed (Charger)", "Range (Shooter)", "Fire Rate (Swarm)", "Damage (Tank)" };
		for (size_t i = 0; i < static_cast<size_t>(GrowthStat::kCount); ++i) {
			GrowthStat stat = static_cast<GrowthStat>(i);
			ImGui::Text("%s: %d  +%d%%", statNames[i], build.GetPoints(stat), build.GetBonusPercent(stat));
		}
		// テスト用：ボタンでその種類を倒したことにする
		ImGui::Text("Test: add growth (1)");
		for (size_t i = 0; i < static_cast<size_t>(EnemyType::kCount); ++i) {
			if (i > 0) {
				ImGui::SameLine();
			}
			EnemyType type = static_cast<EnemyType>(i);
			if (ImGui::Button(Enemy::GetData(type).name)) {
				ApplyGrowth(BuildSystem::GetGrowthStat(type), 1);
			}
		}
	}
	if (ImGui::CollapsingHeader("Dungeon", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Seed: %u", seed);
		int32_t visitedCount = static_cast<int32_t>(std::count(roomVisited.begin(), roomVisited.end(), true));
		ImGui::Text("Visited: %d / %d", visitedCount, static_cast<int32_t>(layout.rooms.size()));
		if (currentRoom >= 0) {
			const RoomNode& room = layout.rooms[static_cast<size_t>(currentRoom)];
			ImGui::Text("Room: %d (%s) template %d", currentRoom, RoomTypeName(room.type), room.templateIndex);
		} else {
			ImGui::Text("Room: corridor");
		}
		ImGui::Text("Enemies: %d", static_cast<int32_t>(enemies.size()));
		if (bossCleared) {
			ImGui::TextColored({ 1.0f, 0.85f, 0.3f, 1.0f }, "BOSS ROOM CLEARED!");
		}
		if (ImGui::Button("New Floor")) {
			bullets.Initialize(kModelBullet);
			BuildFloor();
		}
	}
	if (ImGui::CollapsingHeader("Player Stats")) {
		PlayerStats& stats = player->GetStats();
		ImGui::SliderFloat("Move Speed", &stats.moveSpeed, 1.0f, 15.0f);
		ImGui::SliderFloat("Fire Interval", &stats.fireInterval, 0.03f, 0.6f);
		ImGui::SliderFloat("Bullet Speed", &stats.bulletSpeed, 4.0f, 40.0f);
		ImGui::SliderFloat("Bullet Damage", &stats.bulletDamage, 0.5f, 10.0f);
		ImGui::SliderFloat("Bullet Life", &stats.bulletLifeTime, 0.2f, 3.0f);
	}
	if (ImGui::CollapsingHeader("Shot Style")) {
		ImGui::SliderFloat("Radius", &shotStyle.radius, 0.05f, 2.0f);
		ImGui::SliderInt("Pierce (-1:inf)", &shotStyle.pierce, -1, 10);
		ImGui::SliderFloat("Explosion Radius", &shotStyle.explosionRadius, 0.0f, 5.0f);
		ImGui::SliderFloat("Explosion Damage", &shotStyle.explosionDamageRate, 0.0f, 2.0f);
		ImGui::SliderFloat("Growth", &shotStyle.growth, 0.0f, 4.0f);
		ImGui::Checkbox("Spin", &shotStyle.spin);
	}
	if (ImGui::CollapsingHeader("Camera")) {
		ImGui::SliderFloat("Distance", &cameraSettings.distance, 10.0f, 40.0f);
		ImGui::SliderFloat("Pitch", &cameraSettings.pitch, 0.6f, 1.5f);
		ImGui::SliderFloat("Look Ahead", &cameraSettings.lookAhead, 0.0f, 0.6f);
		ImGui::SliderFloat("Look Ahead Max", &cameraSettings.lookAheadMax, 0.0f, 10.0f);
		ImGui::SliderFloat("Follow", &cameraSettings.follow, 0.02f, 1.0f);
	}
	ImGui::End();
#endif
}