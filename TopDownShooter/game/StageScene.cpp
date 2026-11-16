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
#include "RunRecord.h"
#include "GameSettings.h"
#include "PauseMenu.h"
#include "WarpMap.h"

using namespace MathUtility;

namespace {
	// このシーンで使うカメラとパーティクルグループの名前
	const std::string kCameraName = "stage";
	const std::string kParticleSpark = "spark";
	// 仮のモデル
	const std::string kModelBullet = "test_bullet.obj";
	const std::string kModelEnemyBullet = "test_enemy_bullet.obj";
	const std::string kModelFireBullet = "test_fire.obj";
	const std::string kModelStairs = "test_exit_stairs.obj"; // 下へ行く階段（出口の小部屋）
	const std::string kModelWarpOff = "test_warp_off.obj";  // ワープの転送陣（まだ使えない：灰色）
	const std::string kModelWarpOn = "test_warp_on.obj";    // ワープの転送陣（使える：光る）
	const std::string kModelPortal = "test_portal.obj";      // 帰還の転送陣（出口の小部屋）
	const std::string kModelGacha = "test_gacha.obj";
	const char* kTextureGachaPrompt = "resources/prompt_gacha.png";
	const char* kTextureGachaPromptPoor = "resources/prompt_gacha_poor.png";
	// 宝箱（宝部屋の真ん中。開けるとバリアが1枚もらえる）
	const std::string kModelChest = "test_chest.obj";
	const std::string kModelChestOpen = "test_chest_open.obj";
	const char* kTextureChestPrompt = "resources/prompt_chest.png";
	const char* kTextureBarrierGet = "resources/barrier_get.png";

	// 敵を倒したときに落ちるコインの枚数（突進・射撃・群れ・重装）
	const int32_t kCoinsByEnemy[] = { 2, 2, 1, 5 };
	const int32_t kCoinsByBoss = 15;

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
	// index 番目のフロアの決まり（足りなければ最後のものをくり返す）
	const FloorConfig& GetFloorConfig(int32_t index)
	{
		const std::vector<FloorConfig>& floors = GetFloorConfigs();
		size_t clamped = (std::min)(static_cast<size_t>((std::max)(index, 0)), floors.size() - 1);
		return floors[clamped];
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
		case RoomType::kShop: return "Shop";
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
	std::vector<std::string> litModels = { character.parts.body, character.parts.foot, character.parts.rightHand, character.parts.leftHand, DungeonMap::kModelWall, DungeonMap::kModelDoor, DragonBoss::kModelBody, kModelGacha, kModelChest, kModelChestOpen, kModelStairs };
	for (size_t i = 0; i < static_cast<size_t>(EnemyType::kCount); ++i) {
		litModels.push_back(Enemy::GetData(static_cast<EnemyType>(i)).modelName);
	}
	for (const std::string& name : litModels) {
		modelManager->LoadModel(name);
		modelManager->FindModel(name)->SetEnableLighting(2);
	}
	// 床と攻撃は陰影なし（床は暗くならないように、攻撃は光っているように見せる）
	const std::string unlitModels[] = {
		kModelBullet, kModelEnemyBullet, kModelFireBullet, kModelPortal, kModelWarpOff, kModelWarpOn, LegendaryWeapon::kModelFireZone, DungeonMap::kModelFloorExit, DragonBoss::kModelShadow, DragonBoss::kModelWarning, Enemy::kModelAimLine, CoinManager::kModelCoin, DungeonMap::kModelFloorShop, Barrier::kModel, Barrier::kModelBreak, character.shot.modelName,
		DungeonMap::kModelFloorRoom, DungeonMap::kModelFloorBoss, DungeonMap::kModelFloorTreasure, DungeonMap::kModelFloorCorridor };
	for (const std::string& name : unlitModels) {
		modelManager->LoadModel(name);
		modelManager->FindModel(name)->SetEnableLighting(0);
	}

	// 画面表示の画像（ミニマップ・ハート・ゲームオーバー）
	TextureManager* textureManager = TextureManager::GetInstance();
	for (const char* path : { Minimap::kTexture, WarpMap::kTextureIcon, UiKit::kTextureFont, StageHud::kTextureHeartFull, StageHud::kTextureHeartHalf,
		StageHud::kTextureHeartEmpty, StageHud::kTextureGameOver, StageHud::kTextureWhite, BuildHud::kTextureDigits,
		GrowthChoice::kTextureTitle, GrowthChoice::kTextureHint,
		InventoryHud::kTextureCoin, kTextureGachaPrompt, kTextureGachaPromptPoor, kTextureChestPrompt, kTextureBarrierGet, StageHud::kTextureGameClear, DragonBoss::kTextureName,
		ConfirmDialog::kTexturePanel, ConfirmDialog::kTextureMessageNext, ConfirmDialog::kTextureMessageReturn,
		ConfirmDialog::kTextureYes, ConfirmDialog::kTextureNo, ConfirmDialog::kTextureHint,
		ResultScreen::kTextureTitleReturn, ResultScreen::kTextureTitleDead, ResultScreen::kTextureLabelFloor, ResultScreen::kTextureLabelTime,
		ResultScreen::kTextureLabelKills, ResultScreen::kTextureLabelCoins, ResultScreen::kTextureLabelBest, ResultScreen::kTextureNewRecord,
		ResultScreen::kTextureHint, ResultScreen::kTextureDigits,
		GachaEffect::kTextureRays, GachaEffect::kTextureGlow, GachaEffect::kTextureStar, GachaEffect::kTextureCapsuleTop,
		GachaEffect::kTextureCapsuleBottom, GachaEffect::kTextureHint, LegendaryWeapon::kTextureSecret }) {
		textureManager->LoadTexture(path);
	}
	for (size_t i = 0; i < static_cast<size_t>(GrowthStat::kCount); ++i) {
		textureManager->LoadTexture(BuildHud::GetIconTexture(static_cast<GrowthStat>(i)));
		textureManager->LoadTexture(BuildHud::GetPopupTexture(static_cast<GrowthStat>(i)));
		textureManager->LoadTexture(GrowthChoice::GetCardTexture(static_cast<GrowthStat>(i)));
	}
	for (size_t i = 0; i < static_cast<size_t>(CharacterType::kCount); ++i) {
		CharacterType type = static_cast<CharacterType>(i);
		for (const char* path : { LegendaryWeapon::GetBigIconTexture(type), LegendaryWeapon::GetIconTexture(type),
			LegendaryWeapon::GetGetTexture(type), LegendaryWeapon::GetDescriptionTexture(type) }) {
			textureManager->LoadTexture(path);
		}
	}
	for (size_t i = 0; i < static_cast<size_t>(ItemType::kCount); ++i) {
		textureManager->LoadTexture(ItemInventory::GetData(static_cast<ItemType>(i)).iconTexture);
		textureManager->LoadTexture(ItemInventory::GetData(static_cast<ItemType>(i)).getTexture);
		textureManager->LoadTexture(GachaEffect::GetBigIconTexture(static_cast<ItemType>(i)));
		textureManager->LoadTexture(GachaEffect::GetDescriptionTexture(static_cast<ItemType>(i)));
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

	// 出口の小部屋の、下へ行く階段と帰還の転送陣
	stairs = std::make_unique<Object3d>();
	stairs->Initialize(object3dCommon);
	stairs->SetModel(kModelStairs);
	portal = std::make_unique<Object3d>();
	portal->Initialize(object3dCommon);
	portal->SetModel(kModelPortal);

	// ガチャ（ショップの真ん中に置く）とコイン
	gachaMachine = std::make_unique<Object3d>();
	gachaMachine->Initialize(object3dCommon);
	gachaMachine->SetModel(kModelGacha);
	coins.Initialize();
	chest = std::make_unique<Object3d>();
	chest->Initialize(object3dCommon);
	chest->SetModel(kModelChest);
	barrier.Initialize();

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
	inventoryHud.Initialize(spriteCommon);
	// 「ガチャを回す」の案内（ガチャに近づいたときだけ出す。コインが足りないときは別の画像）
	for (std::unique_ptr<Sprite>* prompt : { &gachaPrompt, &gachaPromptPoor }) {
		*prompt = std::make_unique<Sprite>();
		(*prompt)->Initialize(spriteCommon, (prompt == &gachaPrompt) ? kTextureGachaPrompt : kTextureGachaPromptPoor);
		(*prompt)->SetAnchorPoint({ 0.5f, 0.5f });
		(*prompt)->SetPosition({ 640.0f, 600.0f });
		(*prompt)->Update();
	}
	// 持ち物（アイテムとコイン）はシーンの最初に空にする（フロアをまたいでも持ち越す）
	items.Reset();
	coinCount = 0;
	barrier.Reset();
	chestPrompt = std::make_unique<Sprite>();
	chestPrompt->Initialize(spriteCommon, kTextureChestPrompt);
	chestPrompt->SetAnchorPoint({ 0.5f, 0.5f });
	chestPrompt->SetPosition({ 640.0f, 600.0f });
	chestPrompt->Update();
	growthChoice.Initialize(spriteCommon);
	confirmDialog.Initialize(spriteCommon);
	gachaEffect.Initialize(spriteCommon);
	// 伝説の武器（矢の雨にはキャラの攻撃のモデルを使う）
	legendary.Initialize(CharacterSelection::GetSelected(), character.shot.modelName);
	forceSecretNext = false;
	rainBulletAlive.fill(false);
	// 武器を持っている手のモデル（名前に _weapon が入っている方）。前の挑戦で虹色にしていたら白に戻す
	legendaryWeaponModel = (character.parts.leftHand.find("_weapon") != std::string::npos) ? character.parts.leftHand : character.parts.rightHand;
	ModelManager::GetInstance()->FindModel(legendaryWeaponModel)->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	resultScreen.Initialize(spriteCommon);
	// ビルド（キャラの初期ステータスから始める）
	build.Reset(character.stats);

	// 部屋のひな形を読んで、1フロア目を作る
	roomLibrary.Load(kRoomDirectory);
	floorIndex = 0;
	playTime = 0.0f;
	runKills = 0;
	runCoins = 0;
	BuildFloor();
}

void StageScene::BuildFloor()
{
	// 種を決めて、部屋の並び → マップ → ミニマップの順に作る
	seed = std::random_device{}();
	random.seed(seed);
	const FloorConfig& floor = GetFloorConfig(floorIndex);
	layout = DungeonGenerator::Generate(random, floor.roomCount);
	// このフロアのボスを候補から選ぶ
	std::uniform_int_distribution<size_t> bossDistribution(0, floor.bossPool.size() - 1);
	floorBoss = floor.bossPool[bossDistribution(random)];
	boss.reset();
	map.Build(layout, roomLibrary, random);
	// 出口の小部屋：階段と転送陣を置いて、ボスを倒すまで扉を閉めておく
	exitOpen = false;
	stairsArmed = true;
	portalArmed = true;
	if (layout.exitRoom >= 0) {
		stairs->SetTranslate(map.GetExitStairsPosition());
		portal->SetTranslate(map.GetExitPortalPosition());
		map.SetRoomLocked(layout.exitRoom, true);
	}
	minimap.Initialize(spriteCommon, layout);
	ui.Initialize(spriteCommon);
	// ワープの部屋：最初の部屋・ショップ・宝部屋と、スタートから3の倍数の深さにある戦闘部屋
	warpRooms.assign(layout.rooms.size(), false);
	for (size_t i = 0; i < layout.rooms.size(); ++i) {
		const RoomNode& room = layout.rooms[i];
		if (room.type == RoomType::kStart || room.type == RoomType::kShop || room.type == RoomType::kTreasure ||
			(room.type == RoomType::kCombat && room.depth > 0 && room.depth % kWarpDepthInterval == 0)) {
			warpRooms[i] = true;
		}
	}
	warpMap.Initialize(spriteCommon, layout, warpRooms);
	// ワープの転送陣を、ワープの部屋に置く
	warpPads.clear();
	for (size_t i = 0; i < warpRooms.size(); ++i) {
		if (!warpRooms[i]) {
			continue;
		}
		WarpPad pad;
		pad.room = static_cast<int32_t>(i);
		pad.position = FindWarpPadPosition(static_cast<int32_t>(i));
		pad.off = std::make_unique<Object3d>();
		pad.off->Initialize(object3dCommon);
		pad.off->SetModel(kModelWarpOff);
		pad.on = std::make_unique<Object3d>();
		pad.on->Initialize(object3dCommon);
		pad.on->SetModel(kModelWarpOn);
		warpPads.push_back(std::move(pad));
	}
	warpPhase = WarpPhase::kNone;
	bossIntroTimer = 0.0f;
	bossIntroName = std::make_unique<Sprite>();
	bossIntroName->Initialize(spriteCommon, StageHud::kTextureWhite);
	bossIntroName->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
	warpFlash = std::make_unique<Sprite>();
	warpFlash->Initialize(spriteCommon, StageHud::kTextureWhite);
	warpFlash->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
	warpFlash->SetColor({ 1.0f, 0.9f, 1.0f, 0.0f });
	warpFlash->Update();
	// 伝説の武器の炎と矢の雨は、フロアが変わったら消す（武器は持ったまま）
	legendary.ClearEffects();
	// コインは床に残っていても消す。ガチャはショップの真ん中に置く（マップを作った後でないと部屋の位置が分からない）
	coins.Clear();
	nearGacha = false;
	if (layout.shopRoom >= 0) {
		gachaPosition = map.GetRoomCenter(layout.shopRoom);
		gachaMachine->SetTranslate(gachaPosition);
	}
	// 宝箱は宝部屋の真ん中に、閉じた状態で置く
	nearChest = false;
	chestOpened = false;
	chest->SetModel(kModelChest);
	if (layout.treasureRoom >= 0) {
		chestPosition = map.GetRoomCenter(layout.treasureRoom);
		chest->SetTranslate(chestPosition);
	}

	roomStates.assign(layout.rooms.size(), RoomState::kUnvisited);
	roomVisited.assign(layout.rooms.size(), false);
	// 敵のいない部屋は、最初からクリア扱い
	for (size_t i = 0; i < layout.rooms.size(); ++i) {
		RoomType type = layout.rooms[i].type;
		if (type == RoomType::kStart || type == RoomType::kTreasure || type == RoomType::kShop || type == RoomType::kExit) {
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
	// 文字や四角の道具：今フレームの描く内容を空にする（ポーズや設定のメニューが積む）
	ui.BeginFrame();
	// ゲームオーバー・帰還の後は、世界を止めてリザルトとやり直しの入力だけ
	if (isGameOver || isGameClear) {
		UpdateGameOver();
		return;
	}
	// ガチャの演出中は世界を止める（終わったらアイテムを渡す）
	if (gachaEffect.IsPlaying()) {
		if (gachaEffect.Update(input, kDeltaTime)) {
			// 演出で大きく見せたので、「手に入れた！」の小さな表示は出さない
			if (gachaEffect.IsLegendary()) {
				GiveLegendary();
			} else {
				GiveItem(gachaEffect.GetItem(), false);
			}
		}
		UpdateFrozenWorld();
		return;
	}
	// 確認画面（次の層へ進む？・帰還する？）が出ている間も世界を止める（Esc はここで「いいえ」になる）
	if (confirmDialog.IsOpen()) {
		UpdateConfirm();
		return;
	}

	// ワープの演出中：世界を止めて、演出だけ進める
	if (warpPhase != WarpPhase::kNone) {
		UpdateWarpEffect();
		return;
	}
	// ボスの登場演出中：世界を止めて、演出だけ進める
	if (bossIntroTimer > 0.0f && boss) {
		UpdateBossIntro();
		return;
	}
	// ポーズ中：世界を止めて、ポーズ画面だけ動かす
	if (pauseMenu.IsOpen()) {
		PauseMenu::Result pauseResult = pauseMenu.Update(input, &ui);
		if (pauseResult == PauseMenu::Result::kQuit) {
			SceneManager::GetInstance()->ChangeScene("SELECT");
		} else if (pauseResult == PauseMenu::Result::kResume) {
			holdFireUntilRelease = true; // メニューを押したクリックで撃たないように
		}
		UpdateFrozenWorld();
		return;
	}
	// 大きなマップを開いている間：世界を止めて、ワープの行き先を選ぶ
	if (warpMap.IsOpen()) {
		// 今ワープで行ける部屋（行ったことがあって、敵のいない部屋）
		std::vector<bool> warpReady(layout.rooms.size(), false);
		for (size_t i = 0; i < warpReady.size(); ++i) {
			warpReady[i] = warpRooms[i] && roomVisited[i] && roomStates[i] == RoomState::kCleared;
		}
		WarpMap::Result warpResult = warpMap.Update(input, roomVisited, warpReady, currentRoom, fightingRoom < 0, &ui);
		if (warpResult.type != WarpMap::ResultType::kNone) {
			holdFireUntilRelease = true; // クリックで撃たないように
		}
		if (warpResult.type == WarpMap::ResultType::kWarp) {
			WarpToRoom(warpResult.room);
		}
		UpdateFrozenWorld();
		return;
	}
	// Tab で大きなマップを開く（ワープもここから）
	if (input->TriggerKey(DIK_TAB)) {
		warpMap.Open(currentRoom);
		UpdateFrozenWorld();
		return;
	}
	// Esc でポーズ画面を開く
	if (input->TriggerKey(DIK_ESCAPE)) {
		pauseMenu.Open();
		UpdateFrozenWorld();
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
	// 画面を閉じた直後：攻撃ボタンをはなすまでは撃たない
	if (holdFireUntilRelease) {
		if (playerInput.fire) {
			playerInput.fire = false;
		} else {
			holdFireUntilRelease = false;
		}
	}

	// プレイヤー（動いた後で壁から押し戻す）
	Player::ShotRequest shot = player->Update(playerInput, kDeltaTime);
	player->SetPosition(map.ResolveCircle(player->GetPosition(), player->GetRadius()));
	// 撃つ位置が壁の中なら撃たない（壁に張り付いて撃つと、壁の向こうに弾が出てしまうので）
	if (shot.shoot && !map.IsSolidAt(shot.position)) {
		FirePlayerShot(shot.position, shot.direction);
	}

	// 部屋の進行（入ったら閉める・全部倒したら開ける）
	UpdateRooms();

	// 敵とボス
	UpdateEnemies();
	UpdateBoss();

	// コイン（拾った分だけ増える）とガチャ
	int32_t collected = coins.Update(player->GetPosition(), map, kDeltaTime);
	if (collected > 0) {
		coinCount += collected;
		runCoins += collected;
		inventoryHud.OnCoinCollected();
	}
	UpdateGacha();
	UpdateChest();
	barrier.Update(player->GetPosition(), kDeltaTime);

	// 出口の小部屋：階段・転送陣に触れたら確認画面を出す
	UpdateExit();
	if (confirmDialog.IsOpen()) {
		return;
	}

	// 弾と当たり判定
	UpdateHoming();
	bullets.Update(kDeltaTime, 100000.0f);
	CheckBulletHits();
	UpdateLegendary();
	CheckPlayerHits();

	// カメラ（プレイヤーが動いた後で）
	UpdateCamera();

	// マップ（見えているあたりだけ）
	map.Update(camera, cameraFocus);

	// ミニマップと画面表示
	minimap.Update(roomVisited, currentRoom);
	hud.Update(player->GetHp(), player->GetMaxHp(), kDeltaTime);
	buildHud.Update(build, kDeltaTime);
	inventoryHud.Update(coinCount, items, kDeltaTime);

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
			// ボス：倒したら大きめの力を選べて、出口の小部屋の扉が開く
			hud.SetBoss(nullptr);
			OpenBossGrowthChoice();
			if (layout.exitRoom >= 0) {
				exitOpen = true;
				map.SetRoomLocked(layout.exitRoom, false);
			}
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
	flowTimer = 0.0f; // 出てすぐ、道順の地図を作る
	comboTimer = 0.0f;
	previousPlayerPosition = player->GetPosition();
	playerVelocity = { 0.0f, 0.0f, 0.0f };
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
				float angle = std::numbers::pi_v<float> * 2.0f * static_cast<float>(i) / static_cast<float>(count);
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
	Vector3 playerPosition = player->GetPosition();

	// プレイヤーの動く速さ（先読みして撃つのに使う）。急に変わるので、少しなめらかにする
	Vector3 instantVelocity = (playerPosition - previousPlayerPosition) / kDeltaTime;
	instantVelocity.y = 0.0f;
	if (Length(instantVelocity) > 14.0f) {
		instantVelocity = { 0.0f, 0.0f, 0.0f }; // 瞬間移動（部屋の切りかわりなど）は無視する
	}
	// ロール中は先読みしない（ロールは急に速くなるので、先読みすると「ロールの行き先」を撃たれてしまう）
	// 歩いている向きだけを先読みするので、ロールしている間とその直後は、先読みの速さを0にする
	if (player->IsRolling()) {
		rollCooldown = kLeadRollCooldown;
	}
	rollCooldown -= kDeltaTime;
	if (rollCooldown > 0.0f) {
		instantVelocity = { 0.0f, 0.0f, 0.0f };
		playerVelocity = Lerp(playerVelocity, instantVelocity, 0.5f);
	} else {
		playerVelocity = Lerp(playerVelocity, instantVelocity, 0.25f);
	}
	previousPlayerPosition = playerPosition;

	enemyClock += kDeltaTime;
	comboTimer -= kDeltaTime;

	// 道順の地図（壁をよけて、プレイヤーのところへ行くため）は、少しおきに作り直す
	flowTimer -= kDeltaTime;
	if (flowTimer <= 0.0f && fightingRoom >= 0) {
		map.UpdateFlowField(playerPosition, fightingRoom);
		flowTimer = kFlowInterval;
	}

	// ---- 攻撃の許可と、陣取りの場所を配る ----
	// 同時に攻撃できる敵の数を制限する（全員が一斉に撃つと避けようがないので、順番に攻撃させる）
	// 群れ（飛びかかり）とそれ以外（射撃・突進・重装）で、別々に数える
	int32_t attackers[2] = { 0, 0 };
	int32_t rangedCount = 0, swarmCount = 0;
	for (const std::unique_ptr<Enemy>& enemy : enemies) {
		if (!enemy->IsAlive()) {
			continue;
		}
		bool isSwarm = enemy->GetType() == EnemyType::kSwarm;
		if (enemy->IsAttacking()) {
			++attackers[isSwarm ? 1 : 0];
		}
		// 陣取りの場所：射撃と重装はプレイヤーを挟むように、群れは囲むように、みんなで角度を分ける
		if (isSwarm) {
			++swarmCount;
		} else if (enemy->GetType() == EnemyType::kShooter || enemy->GetType() == EnemyType::kTank) {
			++rangedCount;
		}
	}
	int32_t rangedIndex = 0, swarmIndex = 0;
	const float kTwoPi = std::numbers::pi_v<float> * 2.0f;

	// 許可を配る順番を毎フレームずらす
	size_t enemyTotal = enemies.size();
	if (enemyTotal > 0) {
		enemyTurn = (enemyTurn + 1) % enemyTotal;
	}
	// 先に、全員の陣取りの角度を決める（順番に関係なく、いつも同じ割り当てになるように）
	std::vector<float> ringAngles(enemyTotal, 0.0f);
	for (size_t i = 0; i < enemyTotal; ++i) {
		const Enemy& enemy = *enemies[i];
		if (!enemy.IsAlive()) {
			continue;
		}
		if (enemy.GetType() == EnemyType::kSwarm) {
			ringAngles[i] = enemyClock * 0.4f + kTwoPi * static_cast<float>(swarmIndex++) / static_cast<float>((std::max)(swarmCount, 1));
		} else if (enemy.GetType() == EnemyType::kShooter || enemy.GetType() == EnemyType::kTank) {
			ringAngles[i] = enemyClock * 0.12f + kTwoPi * static_cast<float>(rangedIndex++) / static_cast<float>((std::max)(rangedCount, 1));
		}
	}

	for (size_t k = 0; k < enemyTotal; ++k) {
		size_t i = (enemyTurn + k) % enemyTotal;
		std::unique_ptr<Enemy>& enemy = enemies[i];
		if (!enemy->IsAlive()) {
			continue;
		}
		bool isSwarm = enemy->GetType() == EnemyType::kSwarm;
		int32_t group = isSwarm ? 1 : 0;

		EnemyPerception perception;
		perception.targetPosition = playerPosition;
		perception.targetVelocity = playerVelocity;
		// 壁にさえぎられていないか（体の幅の分も見る）
		perception.hasLineOfSight = map.HasLineOfSight(enemy->GetPosition(), playerPosition, enemy->GetRadius() * 0.8f);
		perception.flowDirection = map.GetFlowDirection(enemy->GetPosition());
		perception.canAttack = enemy->IsAttacking() || attackers[group] < kMaxAttackers;
		perception.comboReady = comboTimer > 0.0f && enemy->GetType() == EnemyType::kCharger;
		perception.ringAngle = ringAngles[i];

		bool wasAttacking = enemy->IsAttacking();
		Enemy::ShotRequest enemyShot = enemy->Update(perception, kDeltaTime);
		if (!wasAttacking && enemy->IsAttacking()) {
			++attackers[group];
		}

		// 弾を撃つ（count 発を spread ずつずらして。真ん中がプレイヤーの方）
		if (enemyShot.shoot && player->IsAlive()) {
			float baseAngle = std::atan2(enemyShot.direction.x, enemyShot.direction.z);
			for (int32_t n = 0; n < enemyShot.count; ++n) {
				float angle = baseAngle + (static_cast<float>(n) - static_cast<float>(enemyShot.count - 1) / 2.0f) * enemyShot.spread;
				Vector3 direction = { std::sin(angle), 0.0f, std::cos(angle) };
				bullets.Fire(enemyShot.position, direction, enemyShot.speed, enemyShot.lifeTime, enemyShot.damage, enemyShotStyle, false);
			}
			// 射撃が撃った直後は、突進が飛び込みやすい（連携）
			if (enemy->GetType() == EnemyType::kShooter) {
				comboTimer = kComboTime;
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
	// ボスの登場演出中は、ボスを見る
	if (bossIntroTimer > 0.0f && boss) {
		target = boss->GetPosition();
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
	coins.SetCamera(camera);
	barrier.SetCamera(camera);
	if (chest) {
		chest->SetCamera(camera);
		chest->Update();
	}
	if (gachaMachine) {
		gachaMachine->SetCamera(camera);
		gachaMachine->Update();
	}
	if (stairs) {
		stairs->SetCamera(camera);
		stairs->Update();
	}
	legendary.SetCamera(camera);
	if (portal) {
		// 転送陣はゆっくり回る（扉が開いたら速く回って、使えるのが分かるように）
		portal->SetCamera(camera);
		portal->SetRotate({ 0.0f, shakeTime * (exitOpen ? 0.9f : 0.15f), 0.0f });
		portal->Update();
	}
	UpdateWarpPads();
}

void StageScene::CheckBulletHits()
{
	ParticleManager* particleManager = ParticleManager::GetInstance();
	for (BulletManager::Bullet& bullet : bullets.GetBullets()) {
		if (!bullet.active) {
			continue;
		}
		// 壁に当たったら消える（爆発する弾はその場で爆発）。跳ね返りのアイテムがあれば跳ね返る
		if (map.IsSolidAt(bullet.position)) {
			if (bullet.fromPlayer && bullet.bounceLeft > 0) {
				BounceBullet(bullet);
				continue;
			}
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
				SplitBullet(bullet);
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
			SplitBullet(bullet);

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
	// 星の杖：爆発した場所に炎を残す（自分には当たらない）
	if (legendary.IsOwned() && legendary.GetCharacter() == CharacterType::kWitch) {
		legendary.AddFireZone(center, radius * 0.8f, player->GetStats().bulletDamage * 0.25f);
	}
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
		runKills++;
		// この部屋で倒した数を種類ごとに数える（クリアしたときの選択肢になる）
		roomKillCounts[static_cast<size_t>(enemy.GetType())]++;
		// コインを落とす
		coins.Spawn(enemy.GetPosition(), kCoinsByEnemy[static_cast<size_t>(enemy.GetType())]);
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
	// バリアがあれば、ダメージの代わりにバリアが1枚割れる。場にある敵の弾も全部消える
	if (barrier.Consume()) {
		bullets.ClearEnemyBullets();
		player->StartInvincible();
		shake = (std::min)(1.0f, shake + 0.5f);
		ParticleManager::GetInstance()->Emit(kParticleSpark, player->GetPosition() + Vector3{ 0.0f, 0.6f, 0.0f }, 25u);
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
	RefreshPlayerStats();
	buildHud.OnGrowth(stat);
}

void StageScene::UpdateFrozenWorld()
{
	// 止まった世界をそのまま見せる（カメラの揺れ・画面表示・パーティクルだけ動かす）
	// 画面が出ている間に押したボタンで、閉じた後に弾が出ないようにする
	holdFireUntilRelease = true;
	UpdateCamera();
	map.Update(camera, cameraFocus);
	minimap.Update(roomVisited, currentRoom);
	hud.Update(player->GetHp(), player->GetMaxHp(), kDeltaTime);
	buildHud.Update(build, kDeltaTime);
	inventoryHud.Update(coinCount, items, kDeltaTime);
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
	// 登場演出を始める（体力バーは、演出が終わってから出す）
	bossIntroTimer = 0.0001f;
	bossIntroTime = 0.0f;
	bossIntroFlash = 0.0f;
	bossIntroName->ChangeTexture(boss->GetNameTexture());
	bossIntroName->SetSize(bossIntroName->GetTextureSize());
}

void StageScene::UpdateBossIntro()
{
	bossIntroTime += kDeltaTime;
	float t = bossIntroTime;

	// ボスは、自分の登場（地面から出てきて吠える）の間だけ動かす。吠えたあとは時間を止めて、名前の表示を見せる
	BossBase::Events events;
	boss->Update(player->GetPosition(), t < kBossIntroRiseTime ? kDeltaTime : 0.0f, events);
	if (events.roar) {
		shake = 1.0f;
		bossIntroFlash = 1.0f;
		ParticleManager::GetInstance()->Emit(kParticleSpark, boss->GetPosition() + Vector3{ 0.0f, 1.5f, 0.0f }, 40u);
	}
	// 出てくる間は、じわじわ揺れが強くなる
	if (t > 0.3f && t < kBossIntroRiseTime) {
		shake = (std::max)(shake, (t - 0.3f) / (kBossIntroRiseTime - 0.3f) * 0.45f);
	}

	// 上下の黒い帯（映画のように）。最初にせり出して、最後に引っこむ
	auto easeOut = [](float x) { x = std::clamp(x, 0.0f, 1.0f); return 1.0f - (1.0f - x) * (1.0f - x); };
	float bar = kBossIntroBarHeight * easeOut(t / 0.5f) * (1.0f - easeOut((t - (kBossIntroTotalTime - 0.5f)) / 0.5f));
	ui.Rect(0.0f, 0.0f, 1280.0f, bar, { 0.0f, 0.0f, 0.0f, 1.0f });
	ui.Rect(0.0f, 720.0f - bar, 1280.0f, bar, { 0.0f, 0.0f, 0.0f, 1.0f });

	// 吠えた瞬間の白い光
	if (bossIntroFlash > 0.0f) {
		ui.Rect(0.0f, 0.0f, 1280.0f, 720.0f, { 1.0f, 1.0f, 1.0f, bossIntroFlash * 0.8f });
		bossIntroFlash = (std::max)(0.0f, bossIntroFlash - kDeltaTime * 3.0f);
	}

	// 名前：吠えたあとに、大きくから元の大きさへ。二つ名はその下に
	float nameStart = kBossIntroRiseTime + 0.1f;
	float nameEnd = kBossIntroTotalTime - 0.4f;
	float appear = std::clamp((t - nameStart) / 0.3f, 0.0f, 1.0f);
	float vanish = std::clamp((nameEnd - t) / 0.3f, 0.0f, 1.0f);
	bossIntroNameAlpha = appear * vanish;
	float scale = 2.0f + 0.8f * (1.0f - easeOut(appear));
	bossIntroName->SetAnchorPoint({ 0.5f, 0.5f });
	bossIntroName->SetPosition({ 640.0f, 560.0f });
	bossIntroName->SetSize({ bossIntroName->GetTextureSize().x * scale, bossIntroName->GetTextureSize().y * scale });
	bossIntroName->SetColor({ 1.0f, 1.0f, 1.0f, bossIntroNameAlpha });
	bossIntroName->Update();
	if (bossIntroNameAlpha > 0.0f) {
		ui.Text(boss->GetEpithet(), 640.0f, 612.0f, 28.0f, { 1.0f, 0.85f, 0.4f, bossIntroNameAlpha }, UiKit::Align::kCenter);
	}

	// スペースで飛ばせる（吠えたあと）
	if (t > kBossIntroRiseTime && input->TriggerKey(DIK_SPACE)) {
		bossIntroTime = kBossIntroTotalTime;
	}
	if (bossIntroTime >= kBossIntroTotalTime) {
		bossIntroTimer = 0.0f;
		bossIntroNameAlpha = 0.0f;
		hud.SetBoss(boss->GetNameTexture());
		holdFireUntilRelease = true;
	}
	UpdateFrozenWorld();
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
		coins.Spawn(boss->GetPosition(), kCoinsByBoss);
		bullets.ClearEnemyBullets();
	}
}

void StageScene::OpenBossGrowthChoice()
{
	// ボスの力：4つ全部から選べて、1部屋より大きく伸びる
	const int32_t amounts[] = { 5, 4, 10, 2 }; // 移動速度・射程・連射・威力（どれも +20%）
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
	// 次のフロアへ（HP・ビルド・アイテム・コイン・バリアはそのまま持っていく）
	floorIndex++;
	bullets.Initialize(kModelBullet);
	BuildFloor();
}

void StageScene::UpdateExit()
{
	if (!exitOpen || layout.exitRoom < 0) {
		return;
	}
	// 触れたか見る。一度離れるまでは、触れても開かない（「いいえ」の後にすぐまた開かないように）
	auto checkTouch = [this](const Vector3& position, float radius, bool& armed) {
		Vector3 diff = { player->GetPosition().x - position.x, 0.0f, player->GetPosition().z - position.z };
		float distance = Length(diff);
		if (distance > kRearmDistance) {
			armed = true;
		}
		if (armed && distance < radius) {
			armed = false;
			return true;
		}
		return false;
	};
	if (checkTouch(map.GetExitStairsPosition(), kStairsTouchRadius, stairsArmed)) {
		// 次の層へ：最初は「はい」を選んでおく
		confirmDialog.Open(ConfirmDialog::Kind::kNextFloor, true);
	} else if (checkTouch(map.GetExitPortalPosition(), kPortalTouchRadius, portalArmed)) {
		// 帰還：冒険が終わってしまうので、最初は「いいえ」を選んでおく
		confirmDialog.Open(ConfirmDialog::Kind::kReturn, false);
	}
}

void StageScene::UpdateConfirm()
{
	ConfirmDialog::Answer answer = confirmDialog.Update(input, kDeltaTime);
	if (answer == ConfirmDialog::Answer::kYes) {
		if (confirmDialog.GetKind() == ConfirmDialog::Kind::kNextFloor) {
			GoToNextFloor();
		} else {
			FinishRun(true);
		}
		return;
	}
	UpdateFrozenWorld();
}

void StageScene::FinishRun(bool returned)
{
	// 帰還：ここまでのフロアを踏破した。力尽きた：今いるフロアまで来た。どちらも「到達階層」は今のフロア
	isGameClear = returned;
	gameOverTimer = 0.0f;
	ResultScreen::Result result;
	result.returned = returned;
	result.floor = floorIndex + 1;
	result.seconds = playTime;
	result.kills = runKills;
	result.coins = runCoins;
	// ベスト記録と比べて、更新したら保存する（比べる前の記録を出すと「前のベスト」になってしまうので、出すのは更新後）
	CharacterType character = CharacterSelection::GetSelected();
	bool isNewRecord = RunRecord::Submit(character, result.floor, result.seconds);
	resultScreen.Open(result, RunRecord::Load(character), isNewRecord);
}

void StageScene::RefreshPlayerStats()
{
	// ビルドのステータスに、アイテムの影響（拡散で連射が少し遅くなる）を足す
	PlayerStats stats = build.ComputeStats();
	stats.fireInterval *= items.ComputeModifiers().fireIntervalRate;
	player->GetStats() = stats;
}

void StageScene::FirePlayerShot(const Vector3& position, const Vector3& direction)
{
	// 拡散：真ん中を狙う向きにして、左右に spreadAngle ずつずらして撃つ
	ShotModifiers modifiers = items.ComputeModifiers();
	float baseAngle = std::atan2(direction.x, direction.z);
	for (int32_t i = 0; i < modifiers.shotCount; ++i) {
		float offset = (static_cast<float>(i) - static_cast<float>(modifiers.shotCount - 1) / 2.0f) * modifiers.spreadAngle;
		Vector3 shotDirection = { std::sin(baseAngle + offset), 0.0f, std::cos(baseAngle + offset) };
		BulletManager::Bullet* bullet = bullets.Fire(position, shotDirection, player->GetStats(), shotStyle, modifiers);
		// 天弓：真ん中の矢だけ、消えた場所に矢の雨を降らせる（拡散で全部に付くと強すぎるので）
		bool isCenter = (i == modifiers.shotCount / 2);
		if (bullet && isCenter && legendary.IsOwned() && legendary.GetCharacter() == CharacterType::kArcher) {
			bullet->rainOnEnd = true;
		}
	}
}

void StageScene::UpdateHoming()
{
	for (BulletManager::Bullet& bullet : bullets.GetBullets()) {
		if (!bullet.active || !bullet.fromPlayer || bullet.homingTurnSpeed <= 0.0f) {
			continue;
		}
		// 一番近い敵（とボス）を探す
		Vector3 target = { 0.0f, 0.0f, 0.0f };
		float best = kHomingRange;
		bool found = false;
		for (std::unique_ptr<Enemy>& enemy : enemies) {
			if (!enemy->IsAlive()) {
				continue;
			}
			Vector3 diff = { enemy->GetPosition().x - bullet.position.x, 0.0f, enemy->GetPosition().z - bullet.position.z };
			float distance = Length(diff);
			if (distance < best) {
				best = distance;
				target = enemy->GetPosition();
				found = true;
			}
		}
		if (boss && boss->IsHittable()) {
			Vector3 diff = { boss->GetPosition().x - bullet.position.x, 0.0f, boss->GetPosition().z - bullet.position.z };
			if (Length(diff) < best) {
				target = boss->GetPosition();
				found = true;
			}
		}
		if (!found) {
			continue;
		}
		// 今の向きから、敵の方へ少しだけ曲げる（1フレームに曲がれる角度に上限がある）
		float currentAngle = std::atan2(bullet.velocity.x, bullet.velocity.z);
		float targetAngle = std::atan2(target.x - bullet.position.x, target.z - bullet.position.z);
		float diffAngle = targetAngle - currentAngle;
		while (diffAngle > std::numbers::pi_v<float>) { diffAngle -= std::numbers::pi_v<float> * 2.0f; }
		while (diffAngle < -std::numbers::pi_v<float>) { diffAngle += std::numbers::pi_v<float> * 2.0f; }
		float maxTurn = bullet.homingTurnSpeed * kDeltaTime;
		float newAngle = currentAngle + std::clamp(diffAngle, -maxTurn, maxTurn);
		float speed = Length(bullet.velocity);
		bullet.velocity = { std::sin(newAngle) * speed, 0.0f, std::cos(newAngle) * speed };
	}
}

void StageScene::BounceBullet(BulletManager::Bullet& bullet)
{
	// 1フレーム前の位置に戻して、どちらの向きで壁にぶつかったかを調べて、その向きの速さを反対にする
	Vector3 previous = bullet.position - bullet.velocity * kDeltaTime;
	bool hitX = map.IsSolidAt({ bullet.position.x, 0.0f, previous.z });
	bool hitZ = map.IsSolidAt({ previous.x, 0.0f, bullet.position.z });
	if (hitX) {
		bullet.velocity.x = -bullet.velocity.x;
	}
	if (hitZ) {
		bullet.velocity.z = -bullet.velocity.z;
	}
	if (!hitX && !hitZ) {
		// 角にぴったり当たった：来た方向へ戻す
		bullet.velocity = -bullet.velocity;
	}
	bullet.position = previous;
	bullet.bounceLeft--;
	ParticleManager::GetInstance()->Emit(kParticleSpark, bullet.position, 1u);
}

void StageScene::SplitBullet(BulletManager::Bullet& bullet)
{
	// 分裂：当たったところから、まわりに均等に小さな弾を出す（1発につき1回だけ）
	if (bullet.splitCount <= 0) {
		return;
	}
	float baseAngle = std::atan2(bullet.velocity.x, bullet.velocity.z);
	for (int32_t i = 0; i < bullet.splitCount; ++i) {
		float angle = baseAngle + std::numbers::pi_v<float> * 2.0f * static_cast<float>(i) / static_cast<float>(bullet.splitCount);
		bullets.FireSplit(bullet, { std::sin(angle), 0.0f, std::cos(angle) }, shotStyle.modelName);
	}
	bullet.splitCount = 0;
}

void StageScene::UpdateGacha()
{
	nearGacha = false;
	if (layout.shopRoom < 0) {
		return;
	}
	Vector3 diff = { player->GetPosition().x - gachaPosition.x, 0.0f, player->GetPosition().z - gachaPosition.z };
	float distance = Length(diff);

	// ガチャは通り抜けられない（円で押し戻す）
	float minDistance = kGachaRadius + player->GetRadius();
	if (distance < minDistance && distance > 0.0001f) {
		Vector3 pushed = gachaPosition + diff / distance * minDistance;
		pushed.y = player->GetPosition().y;
		player->SetPosition(pushed);
		distance = minDistance;
	}

	// 近くで E／パッドの X：コインが足りれば回す
	nearGacha = distance < kGachaUseDistance;
	if (!nearGacha || !(GameSettings::GetInstance()->Trigger(input, GameAction::kInteract) || input->TriggerPad(XINPUT_GAMEPAD_X))) {
		return;
	}
	if (coinCount < kGachaPrice) {
		shake = (std::min)(1.0f, shake + 0.1f);
		return;
	}
	coinCount -= kGachaPrice;
	// 出るアイテムはここで決めて、画面全体の演出を始める（アイテムは演出が終わってから渡す）
	// シークレット：1万回に1回、伝説の武器（まだ持っていないときだけ）
	std::uniform_int_distribution<int32_t> secretDistribution(0, kSecretOdds - 1);
	bool secret = !legendary.IsOwned() && (forceSecretNext || secretDistribution(random) == 0);
	forceSecretNext = false;
	if (secret) {
		gachaEffect.StartLegendary(CharacterSelection::GetSelected());
	} else {
		std::uniform_int_distribution<int32_t> itemDistribution(0, static_cast<int32_t>(ItemType::kCount) - 1);
		gachaEffect.Start(static_cast<ItemType>(itemDistribution(random)));
	}
	nearGacha = false;
	ParticleManager::GetInstance()->Emit(kParticleSpark, gachaPosition + Vector3{ 0.0f, 1.5f, 0.0f }, 30u);
	shake = (std::min)(1.0f, shake + 0.3f);
}

void StageScene::UpdateChest()
{
	nearChest = false;
	if (layout.treasureRoom < 0) {
		return;
	}
	Vector3 diff = { player->GetPosition().x - chestPosition.x, 0.0f, player->GetPosition().z - chestPosition.z };
	float distance = Length(diff);

	// 宝箱も通り抜けられない（円で押し戻す。ガチャと同じ）
	float minDistance = kChestRadius + player->GetRadius();
	if (distance < minDistance && distance > 0.0001f) {
		Vector3 pushed = chestPosition + diff / distance * minDistance;
		pushed.y = player->GetPosition().y;
		player->SetPosition(pushed);
		distance = minDistance;
	}
	if (chestOpened) {
		return;
	}

	// 近くで E／パッドの X：開けてバリアを1枚もらう（1フロアに1回）
	nearChest = distance < kChestUseDistance;
	if (!nearChest || !(GameSettings::GetInstance()->Trigger(input, GameAction::kInteract) || input->TriggerPad(XINPUT_GAMEPAD_X))) {
		return;
	}
	chestOpened = true;
	nearChest = false;
	chest->SetModel(kModelChestOpen);
	barrier.Add();
	inventoryHud.ShowGetPopup(kTextureBarrierGet);
	ParticleManager::GetInstance()->Emit(kParticleSpark, chestPosition + Vector3{ 0.0f, 1.0f, 0.0f }, 30u);
	shake = (std::min)(1.0f, shake + 0.3f);
}

void StageScene::GiveLegendary()
{
	legendary.Obtain();
	inventoryHud.SetLegendaryIcon(LegendaryWeapon::GetIconTexture(legendary.GetCharacter()));
	shake = (std::min)(1.0f, shake + 0.5f);
}

void StageScene::UpdateLegendary()
{
	ParticleManager* particleManager = ParticleManager::GetInstance();
	std::array<BulletManager::Bullet, BulletManager::kMaxBullets>& list = bullets.GetBullets();

	// 聖剣：プレイヤーの斬撃に触れた敵の弾を消す（斬撃はそのまま飛び続ける）
	if (legendary.IsOwned() && legendary.GetCharacter() == CharacterType::kKnight) {
		for (BulletManager::Bullet& slash : list) {
			if (!slash.active || !slash.fromPlayer) {
				continue;
			}
			float slashRadius = BulletManager::GetCurrentRadius(slash);
			for (BulletManager::Bullet& enemyBullet : list) {
				if (!enemyBullet.active || enemyBullet.fromPlayer) {
					continue;
				}
				Vector3 diff = { enemyBullet.position.x - slash.position.x, 0.0f, enemyBullet.position.z - slash.position.z };
				if (Length(diff) <= slashRadius + enemyBullet.radius) {
					enemyBullet.active = false;
					particleManager->Emit(kParticleSpark, enemyBullet.position, 2u);
				}
			}
		}
	}

	// 天弓：「消えたら雨を降らせる矢」が、このフレームで消えていたら、その場所に矢の雨
	for (size_t i = 0; i < list.size(); ++i) {
		BulletManager::Bullet& bullet = list[i];
		if (rainBulletAlive[i] && !bullet.active) {
			legendary.AddArrowRain(bullet.position, player->GetStats().bulletDamage * 0.4f);
		}
		rainBulletAlive[i] = bullet.active && bullet.rainOnEnd;
	}

	// 炎・矢の雨・キラキラ
	legendary.Update(kDeltaTime, player->GetPosition(),
		[this](const Vector3& center, float radius, float damage) { DamageArea(center, radius, damage); },
		[this](const Vector3& position) { return map.IsSolidAt(position); });

	// 武器を持っている手を、虹色に少しだけ光らせる
	if (legendary.IsOwned()) {
		Vector4 hue = LegendaryWeapon::HueToColor(shakeTime * 0.5f);
		const float mix = 0.4f;
		ModelManager::GetInstance()->FindModel(legendaryWeaponModel)->SetColor({
			1.0f + (hue.x - 1.0f) * mix, 1.0f + (hue.y - 1.0f) * mix, 1.0f + (hue.z - 1.0f) * mix, 1.0f });
	}
}

void StageScene::DamageArea(const Vector3& center, float radius, float damage)
{
	for (std::unique_ptr<Enemy>& enemy : enemies) {
		if (!enemy->IsAlive()) {
			continue;
		}
		Vector3 diff = { enemy->GetPosition().x - center.x, 0.0f, enemy->GetPosition().z - center.z };
		if (Length(diff) > radius + enemy->GetRadius()) {
			continue;
		}
		Vector3 push = (Length(diff) > 0.001f) ? diff : Vector3{ 0.0f, 0.0f, 1.0f };
		DamageEnemy(*enemy, damage, push);
	}
	if (boss && boss->IsHittable()) {
		Vector3 diff = { boss->GetPosition().x - center.x, 0.0f, boss->GetPosition().z - center.z };
		if (Length(diff) <= radius + boss->GetRadius()) {
			DamageBoss(damage);
		}
	}
}

void StageScene::GiveItem(ItemType type, bool showPopup)
{
	items.Add(type);
	RefreshPlayerStats();
	inventoryHud.ShowItemGet(type, showPopup);
}

void StageScene::UpdateGameOver()
{
	gameOverTimer += kDeltaTime;
	// 力尽きたときは、「GAME OVER」を少し見せてからリザルトを出す
	if (isGameOver && !resultScreen.IsOpen() && gameOverTimer > kResultDelayAfterDeath) {
		FinishRun(false);
	}
	resultScreen.Update(kDeltaTime);

	// 押しっぱなしで飛ばさないように、リザルトが出て少し待ってから入力を受ける
	if (resultScreen.CanAcceptInput()) {
		if (input->TriggerKey(DIK_SPACE) || input->TriggerPad(XINPUT_GAMEPAD_A)) {
			// 同じキャラでもう一度（1階から）
			SceneManager::GetInstance()->ChangeScene("STAGE");
			return;
		}
		if (input->TriggerKey(DIK_ESCAPE) || input->TriggerPad(XINPUT_GAMEPAD_B)) {
			SceneManager::GetInstance()->ChangeScene("SELECT");
			return;
		}
	}

	UpdateFrozenWorld();
}

void StageScene::WarpToRoom(int32_t roomIndex)
{
	if (roomIndex < 0 || roomIndex >= static_cast<int32_t>(layout.rooms.size())) {
		return;
	}
	// 演出を始める（行き先に着くのは、縮む演出が終わってから）
	warpTarget = roomIndex;
	warpPhase = WarpPhase::kOut;
	warpTimer = 0.0f;
	bullets.ClearEnemyBullets();
}

void StageScene::UpdateWarpEffect()
{
	ParticleManager* particles = ParticleManager::GetInstance();
	warpTimer += kDeltaTime;
	float t = (std::min)(1.0f, warpTimer / (warpPhase == WarpPhase::kOut ? kWarpOutTime : kWarpInTime));

	if (warpPhase == WarpPhase::kOut) {
		// 光の柱になって、空へ吸い込まれるように：細く、高く伸びる
		float k = t * t;
		player->SetBodyScale({ 1.0f - k, 1.0f + 3.0f * k, 1.0f - k });
		Vector3 position = player->GetPosition();
		particles->Emit(kParticleSpark, position + Vector3{ 0.0f, 0.3f + 2.5f * t, 0.0f }, 3u);
		warpFlash->SetColor({ 1.0f, 0.9f, 1.0f, k });
		shake = (std::min)(1.0f, shake + 0.03f);
		if (warpTimer >= kWarpOutTime) {
			// 行き先へ（部屋の転送陣のそば）
			Vector3 destination = FindWarpPadPosition(warpTarget);
			for (const WarpPad& pad : warpPads) {
				if (pad.room == warpTarget) {
					destination = pad.position;
				}
			}
			// 転送陣の手前に降りる（陣の上に乗り続けないように、少しだけ下へずらす）
			destination += Vector3{ 0.0f, 0.0f, -1.8f };
			player->SetPosition(destination);
			cameraFocus = destination;
			warpArrivalRoom = warpTarget;
			warpPhase = WarpPhase::kIn;
			warpTimer = 0.0f;
			particles->Emit(kParticleSpark, destination + Vector3{ 0.0f, 0.5f, 0.0f }, 40u);
			shake = (std::min)(1.0f, shake + 0.35f);
		}
	} else {
		// 到着：光の柱から元の姿に戻り、光が引いていく
		float k = (1.0f - t) * (1.0f - t);
		player->SetBodyScale({ 1.0f - k, 1.0f + 3.0f * k, 1.0f - k });
		particles->Emit(kParticleSpark, player->GetPosition() + Vector3{ 0.0f, 0.3f + 2.0f * (1.0f - t), 0.0f }, 2u);
		warpFlash->SetColor({ 1.0f, 0.9f, 1.0f, k });
		if (warpTimer >= kWarpInTime) {
			player->SetBodyScale({ 1.0f, 1.0f, 1.0f });
			warpFlash->SetColor({ 1.0f, 0.9f, 1.0f, 0.0f });
			warpPhase = WarpPhase::kNone;
			warpArrivalRoom = -1;
		}
	}
	warpFlash->Update();
	UpdateFrozenWorld();
}

Vector3 StageScene::FindWarpPadPosition(int32_t roomIndex) const
{
	// 部屋の真ん中は、宝箱・ガチャ・出入口の通り道なので、少しずらした場所の中から、壁や柱のない場所を探す
	Vector3 center = map.GetRoomCenter(roomIndex);
	const Vector2 offsets[] = {
		{ 4.0f, 4.0f }, { -4.0f, 4.0f }, { 4.0f, -4.0f }, { -4.0f, -4.0f },
		{ 5.0f, 3.0f }, { -5.0f, 3.0f }, { 5.0f, -3.0f }, { -5.0f, -3.0f },
		{ 0.0f, 4.0f }, { 0.0f, -4.0f }, { 4.0f, 0.0f }, { -4.0f, 0.0f },
		{ 3.0f, 3.0f }, { -3.0f, 3.0f }, { 3.0f, -3.0f }, { -3.0f, -3.0f },
	};
	for (const Vector2& offset : offsets) {
		Vector3 position = center + Vector3{ offset.x, 0.0f, offset.y };
		// 陣の大きさ（半径1.2）の範囲に、壁や柱がないか（真ん中と、まわり8方向を見る）
		bool free = !map.IsSolidAt(position);
		for (int32_t n = 0; n < 8 && free; ++n) {
			float angle = std::numbers::pi_v<float> * 2.0f * static_cast<float>(n) / 8.0f;
			free = !map.IsSolidAt(position + Vector3{ std::sin(angle) * 1.3f, 0.0f, std::cos(angle) * 1.3f });
		}
		if (free) {
			return position;
		}
	}
	return center + Vector3{ 4.0f, 0.0f, 4.0f };
}

void StageScene::UpdateWarpPads()
{
	for (WarpPad& pad : warpPads) {
		// 使えるか：行ったことがあって、敵を倒しきった部屋
		size_t room = static_cast<size_t>(pad.room);
		bool active = roomVisited[room] && roomStates[room] == RoomState::kCleared;
		if (active && !pad.wasActive) {
			// 使えるようになった瞬間：光が弾ける
			ParticleManager::GetInstance()->Emit(kParticleSpark, pad.position + Vector3{ 0.0f, 0.5f, 0.0f }, 30u);
			shake = (std::min)(1.0f, shake + 0.15f);
		}
		pad.wasActive = active;

		Object3d* object = active ? pad.on.get() : pad.off.get();
		float pulse = 1.0f;
		float spin = 0.0f;
		float y = 0.0f;
		if (active) {
			// 光っている陣は、ゆっくり回って、ふわっと明滅する。着いた直後の陣は、強く光る
			spin = shakeTime * 0.8f;
			pulse = 1.0f + 0.05f * std::sin(shakeTime * 3.0f);
			if (pad.room == warpArrivalRoom) {
				pulse += 0.25f * std::sin(shakeTime * 18.0f);
				spin = shakeTime * 4.0f;
			}
			// 近くにいるとき、時々光の粒を出す
			if (!isGameOver && Length(player->GetPosition() - pad.position) < 12.0f && static_cast<int32_t>(shakeTime * 60.0f) % 30 == 0) {
				ParticleManager::GetInstance()->Emit(kParticleSpark, pad.position + Vector3{ 0.0f, 0.4f, 0.0f }, 1u);
			}
		}
		object->SetCamera(camera);
		object->SetScale({ kWarpPadScale * pulse, kWarpPadScale, kWarpPadScale * pulse });
		object->SetRotate({ 0.0f, spin, 0.0f });
		object->SetTranslate(pad.position + Vector3{ 0.0f, y, 0.0f });
		object->Update();
		pad.shown = true; // 行ったことのない部屋の陣も、灰色で見える
		pad.isActive = active;
	}
}

void StageScene::Draw()
{
	// 3Dの描画（不透明）
	object3dCommon->CommonDrawSetting();
	object3dCommon->SetPipelineState(kBlendModeNormal, false);
	map.Draw();
	// 星の杖の炎（床の上に置くので、床の次に描く）
	legendary.Draw();
	coins.Draw();
	if (layout.shopRoom >= 0) {
		gachaMachine->Draw();
	}
	if (layout.treasureRoom >= 0) {
		chest->Draw();
	}
	for (std::unique_ptr<Enemy>& enemy : enemies) {
		enemy->Draw();
	}
	if (boss) {
		boss->Draw();
	}
	// ワープの転送陣（行ったことのある部屋のものだけ。使えるものは光る色、使えないものは灰色）
	for (const WarpPad& pad : warpPads) {
		if (pad.shown) {
			(pad.isActive ? pad.on : pad.off)->Draw();
		}
	}
	if (layout.exitRoom >= 0) {
		stairs->Draw();
		portal->Draw();
	}
	player->Draw();
	bullets.Draw();
	// バリア（半透明なので、不透明なものの後）
	barrier.Draw();

	// パーティクル（3Dの後）
	ParticleManager::GetInstance()->Draw();

	// 2D（ミニマップ・ハート・ゲームオーバー）
	spriteCommon->CommonDrawSetting(kBlendModeNormal);
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
	minimap.Draw();
	buildHud.Draw();
	inventoryHud.Draw();
	hud.Draw();
	warpMap.Draw(); // 大きなマップ（開いているときだけ）
	if (warpPhase != WarpPhase::kNone) {
		warpFlash->Draw(); // ワープの光
	}
	if (bossIntroTimer > 0.0f && bossIntroNameAlpha > 0.0f) {
		bossIntroName->Draw(); // ボスの名前（登場演出）
	}
	ui.Draw(); // 文字・メニュー（一番手前）
	growthChoice.Draw();
	confirmDialog.Draw();
	if (nearGacha && !growthChoice.IsOpen() && !confirmDialog.IsOpen() && !gachaEffect.IsPlaying() && !isGameOver && !isGameClear) {
		(coinCount >= kGachaPrice ? gachaPrompt : gachaPromptPoor)->Draw();
	}
	if (nearChest && !growthChoice.IsOpen() && !confirmDialog.IsOpen() && !gachaEffect.IsPlaying() && !isGameOver && !isGameClear) {
		chestPrompt->Draw();
	}
	// ガチャの演出（光るところは加算で描くので、ブレンドモードを切りかえる関数を渡す）
	gachaEffect.Draw([this](BlendMode blendMode) {
		spriteCommon->CommonDrawSetting(blendMode);
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
	});
	// リザルトは一番上に
	resultScreen.Draw();
}

void StageScene::DrawDebugWindow()
{
#ifdef USE_IMGUI
	ImGui::Begin("Stage");
	ImGui::Text("Mouse:aim  LClick:shoot  RClick:roll  Esc:pause (keys: see Pause > Settings)");
	if (isGameOver) {
		ImGui::TextColored({ 1.0f, 0.3f, 0.3f, 1.0f }, "GAME OVER  Space/Pad A: retry  Esc/Pad B: select");
	}
	ImGui::Text("Pad: LStick move / RStick aim / RT shoot / LT,LB,A roll");
	ImGui::Text("Device: %s", playerInput.usingPad ? "Pad" : "Keyboard+Mouse");
	ImGui::Text("Character: %s", GetCharacterData(CharacterSelection::GetSelected()).name);
	ImGui::Text("Kills: %d   Coins: %d", killCount, coinCount);
	if (ImGui::CollapsingHeader("Items", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Button("+100 Coins")) {
			coinCount += 100;
		}
		ImGui::SameLine();
		if (ImGui::Button("Warp to Shop")) {
			player->SetPosition(map.GetRoomCenter(layout.shopRoom) + Vector3{ 0.0f, 0.0f, -3.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("Warp to Treasure")) {
			player->SetPosition(map.GetRoomCenter(layout.treasureRoom) + Vector3{ 0.0f, 0.0f, -3.0f });
		}
		// シークレット（伝説の武器）のテスト用
		ImGui::Checkbox("Next Gacha: SECRET", &forceSecretNext);
		ImGui::SameLine();
		if (ImGui::Button("Get Legendary") && !legendary.IsOwned()) {
			GiveLegendary();
		}
		ImGui::Text("Barrier: %d", barrier.GetCount());
		ImGui::SameLine();
		if (ImGui::Button("+1 Barrier")) {
			barrier.Add();
		}
		// テスト用：ボタンでアイテムを足す
		for (size_t i = 0; i < static_cast<size_t>(ItemType::kCount); ++i) {
			ItemType type = static_cast<ItemType>(i);
			ImGui::Text("%s x%d", ItemInventory::GetData(type).name, items.GetCount(type));
			ImGui::SameLine();
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::Button("+1")) {
				GiveItem(type);
			}
			ImGui::PopID();
		}
	}
	ImGui::Text("HP: %d / %d", player->GetHp(), player->GetMaxHp());
	ImGui::Text("Floor: %dF   Time: %s   Run kills: %d  coins: %d", floorIndex + 1, ResultScreen::FormatTime(playTime).c_str(), runKills, runCoins);
	{
		RunRecord::Entry best = RunRecord::Load(CharacterSelection::GetSelected());
		if (best.valid) {
			ImGui::Text("Best: %dF %s", best.floor, ResultScreen::FormatTime(best.seconds).c_str());
		} else {
			ImGui::Text("Best: (none)");
		}
	}
	// テスト用：出口の小部屋を開けてそこへ行く・すぐ帰還する
	if (ImGui::Button("Open Exit + Warp")) {
		// ボスを倒したことにして、出口の小部屋へ（テスト用）
		if (layout.exitRoom >= 0) {
			exitOpen = true;
			map.SetRoomLocked(layout.exitRoom, false);
			player->SetPosition(map.GetRoomCenter(layout.exitRoom));
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Return Now")) {
		FinishRun(true);
	}
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
	// テスト用：ミニマップ（と大きなマップ）を全部見えるようにする
	if (ImGui::Button("Reveal Full Map")) {
		std::fill(roomVisited.begin(), roomVisited.end(), true);
	}
	ImGui::SameLine();
	// テスト用：マップの全部の部屋で、敵を倒したことにする（扉も全部開く）
	if (ImGui::Button("Clear All Rooms")) {
		bullets.ClearEnemyBullets();
		if (fightingRoom >= 0 && !boss) {
			enemies.clear(); // 戦闘中の部屋の敵を消す（ボスは下で、倒したことにする）
		}
		if (boss && fightingRoom >= 0) {
			DamageBoss(1.0e9f); // ボス戦中なら、いつもの流れで倒れる演出と報酬へ進める
		}
		for (size_t i = 0; i < layout.rooms.size(); ++i) {
			if (roomStates[i] == RoomState::kCleared || (boss && static_cast<int32_t>(i) == fightingRoom)) {
				continue;
			}
			roomStates[i] = RoomState::kCleared;
			map.SetRoomLocked(static_cast<int32_t>(i), false);
			if (layout.rooms[i].type == RoomType::kBoss) {
				// ボス部屋もクリア：出口の小部屋を開ける
				bossCleared = true;
				if (layout.exitRoom >= 0) {
					exitOpen = true;
					map.SetRoomLocked(layout.exitRoom, false);
				}
			}
			if (static_cast<int32_t>(i) == fightingRoom) {
				fightingRoom = -1;
			}
		}
	}
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
