#include <random>
#include "LevelLogic.h"

#include "../Components/Gameplay.h"
#include "../Components/AudioSource.h"

#include "../Entities/Prefabs.h"

#include "../Utils/Macros.h"

#include "../Events/Playevents.h"

using namespace GOG;
using namespace flecs;
using namespace GW;
using namespace CORE;
using namespace MATH;

// Connects logic to traverse any players and allow a controller to manipulate them
bool LevelLogic::Init(std::shared_ptr<world> _flecsWorld,
	std::weak_ptr<const GameConfig> _gameConfig,
	AudioData& _audioData,
	GEventGenerator _eventPusher)
{
	flecsWorld = _flecsWorld;
	// create an asynchronus version of the world
	flecsWorldAsync = flecsWorld->async_stage(); // just used for adding stuff, don't try to read data
	flecsWorldLock.Create();
	gameConfig = _gameConfig;
	eventPusher = _eventPusher;
	playerQuery = flecsWorld->query<const Player, const Transform>();
	civilianQuery = flecsWorld->query<const Civilian, const Score>();
	smartBombQuery = flecsWorld->query<const SmartBomb>();
	projectileQuery = flecsWorld->query<const Projectile>();
	enemyQuery = flecsWorld->query<const Enemy>();
	camQuery = flecsWorld->query<const Camera>();

	std::shared_ptr<const GameConfig> readCfg = gameConfig.lock();
	worldTop = readCfg->at("Game").at("worldTopBoundry").as<float>();
	worldBottom = readCfg->at("Game").at("worldBottomBoundry").as<float>();
	worldWidth = readCfg->at("Game").at("worldWidth").as<float>();

	spawnWaveDelay = readCfg->at("Waves").at("spawnWaveDelay").as<unsigned int>();
	spawnBatchRate = readCfg->at("Waves").at("spawnBatchRate").as<unsigned int>();
	minCivisPerWave = readCfg->at("Waves").at("minCivisPerWave").as<unsigned int>();
	maxCivisPerWave = readCfg->at("Waves").at("maxCivisPerWave").as<unsigned int>();
	maxWaves = readCfg->at("Waves").at("maxWaves").as<unsigned int>();
	for (int i = 1; i <= maxWaves; i++)
	{
		smartBombsPerWave.push_back(readCfg->at("Waves")
			.at("smartBombsPerWave_" + std::to_string(i)).as<unsigned int>());
		minEnemyLevelPerWave.push_back(readCfg->at("Waves")
			.at("minEnemyLevelPerWave_" + std::to_string(i)).as<unsigned int>());
		maxEnemyLevelPerWave.push_back(readCfg->at("Waves")
			.at("maxEnemyLevelPerWave_" + std::to_string(i)).as<unsigned int>());
		maxEnemyMultiplierPerBatch.push_back(readCfg->at("Waves")
			.at("maxEnemyMultiplierPerBatch_" + std::to_string(i)).as<float>());
		maxEnemiesPerWave.push_back(readCfg->at("Waves")
			.at("maxEnemiesPerWave_" + std::to_string(i)).as<float>());
	}

	SpawnPlayer();
	SpawnWave();

#pragma region Event Handler

	eventHandler.Create([this](const GW::GEvent& _event)
		{
			PLAY_EVENT event;
			PLAY_EVENT_DATA data;

			if (-_event.Read(event, data))
				return;

			switch (event)
			{
			case PLAY_EVENT::PLAYER_DESTROYED:
			{
				flecsWorld->defer_begin();
				// Destruct all the active projectiles.
				projectileQuery.each([](entity _projectile, const Projectile&)
					{
						_projectile.destruct();
					});
				/* Destruct all the active enemies and decrease the current wave count so the wave does not
				get advanced and the player does not get points from these enemies' destruction. */
				enemyQuery.each([this](entity _enemy, const Enemy&)
				{
					curEnemiesPerWave -= 1;
					livingEnemies -= 1;
					_enemy.destruct();
				});
				//  Destruct any remaining smart bombs from this wave.
				smartBombQuery.each([](entity _entity, const SmartBomb&)
				{
					_entity.destruct();
				});
				// Destruct all active civilians. Do not reward points.
				civilianQuery.each([this](entity _civi, const Civilian&, const Score&)
				{
					_civi.destruct();
				});
				flecsWorld->defer_end();

				// If the player has lives left, then respawn.
				if (data.value > 0)
				{
					playerSpawner.Create(1, [&]()
						{
							SpawnPlayer();
							playerSpawner.Pause(0, false);
						}, 1 * 1000);

					SpawnWave();
				}
				else
				{
					GW::GEvent gameOver;
					PLAY_EVENT_DATA data;
					gameOver.Write(GOG::PLAY_EVENT::GAME_OVER, data);
					eventPusher.Push(gameOver);
				}

				break;
			}
			case PLAY_EVENT::ENEMY_DESTROYED:
			{
				unsigned int waveSettingsIdx = min(waveNum, maxWaves - 1);

				/* Edge Case: When the player dies by colliding with an enemy, this will cause the livingEnemies
				count to reduce to 0, so by the time we get to this event handler, the usual subtraction
				for destroying the enemy on collision will cause our count to become garbage. We are protecting
				against this edge case with this if check. */
				if (livingEnemies > 0)
					livingEnemies -= 1;

				if (livingEnemies <= 0 && curEnemiesPerWave >= maxEnemiesPerWave[waveSettingsIdx -1])
				{
					GW::GEvent waveCleared;
					PLAY_EVENT_DATA waveData;
					waveNum += 1;
					waveData.value = waveNum;
					waveCleared.Write(GOG::PLAY_EVENT::WAVE_CLEARED, waveData);
					eventPusher.Push(waveCleared);
				}
				break;

			}
			case PLAY_EVENT::WAVE_CLEARED:
			{
				curEnemiesPerWave = 0;

				flecsWorld->defer_begin();
				// Destruct any remaining civis and give the player the points for them.
				civilianQuery.each([this](entity _entity, const Civilian&, const Score& _score)
					{
						GW::GEvent onCiviDestruct;
						PLAY_EVENT_DATA civiDestructData;
						civiDestructData.value = _score.value;
						onCiviDestruct.Write(GOG::PLAY_EVENT::CIVILIAN_DESTROYED, civiDestructData);
						eventPusher.Push(onCiviDestruct);
						_entity.destruct();
					});
				//  Destruct any remaining smart bombs from this wave.
				smartBombQuery.each([](entity _entity, const SmartBomb&)
					{
						_entity.destruct();
					});
				projectileQuery.each([](entity _projectile, const Projectile&)
					{
						_projectile.destruct();
					});
				flecsWorld->defer_end();

				waveNum = data.value;
				SpawnWave();

				break;
			}
			default:
			{
				break;
			}
			}
		});
	eventPusher.Register(eventHandler);

#pragma endregion

#pragma region Merge Async Stages

	// create a system the runs at the end of the frame only once to merge async changes
	struct MergeAsyncStages {}; // local definition so we control iteration counts
	_flecsWorld->entity("MergeAsyncStages").add<MergeAsyncStages>();
	// only happens once per frame at the very start of the frame
	flecsWorld->system<MergeAsyncStages>()
		.kind(OnLoad) // first defined phase
		.each([this](entity _entity, MergeAsyncStages& _mergeAsyncChanges)
			{
				// merge any waiting changes from the last frame that happened on other threads
				flecsWorldLock.LockSyncWrite();
				flecsWorldAsync.merge();
				flecsWorldLock.UnlockSyncWrite();
			});

#pragma endregion

	return true;
}

void LevelLogic::SpawnPlayer()
{
	entity newPlayer{};
	if (RetreivePrefab("PlayerPrefab_1", newPlayer))
	{
		GMATRIXF transform = newPlayer.get<Transform>()->value;
		if (camQuery.first().is_alive())
			transform.row4 = camQuery.first().get<Transform>()->value.row4;
		transform.row4.z = 0;
		flecsWorldLock.LockSyncWrite();
		flecs::entity spawnedPlayer = flecsWorldAsync.entity().is_a(newPlayer)
			.add<Player>()
			.add<Alive>()
			.add<Collidable>()
			.set<Transform>({ transform })
			.set<ControllerID>({ 0 });
		flecsWorldLock.UnlockSyncWrite();

		LoopingClips clips = *newPlayer.get<LoopingClips>();
		clips.sounds["Accel"].clip->Play();
	}

}

void LevelLogic::SpawnWave()
{
	unsigned int waveSettingsIdx = min(waveNum, maxWaves - 1);
	// Generate random stats for enemies.
	// Will be used to obtain a seed for the random number engine.
	std::random_device waveSeed;
	/* Standard mersenne_twister_engine seeded with rd() normal rand() doesn't work great
	multi-threaded. */
	std::mt19937 waveGen(waveSeed());

#pragma region Spawn Smart Bomb

	std::string smartBombPrefab = "PickupPrefab_" + std::to_string(PICKUP_TYPE::SMART_BOMB);
	std::uniform_real_distribution<float> smartBombSpawn_x(-worldWidth, worldWidth);
	std::uniform_real_distribution<float> smartBombSpawn_y(worldBottom, worldTop);

	for (int i = 0; i < smartBombsPerWave[waveSettingsIdx -1]; i += 1)
	{
		entity smartBomb{};
		if (RetreivePrefab(smartBombPrefab.c_str(), smartBomb))
		{
			GVECTORF spawnPos{ smartBombSpawn_x(waveGen), smartBombSpawn_y(waveGen), 0, 1 };
			GMATRIXF transform = smartBomb.get<Transform>()->value;
			transform.row4 = spawnPos;

			flecsWorldLock.LockSyncWrite();
			flecsWorldAsync.entity().is_a(smartBomb)
				.add<Pickup>()
				.add<Alive>()
				.add<Collidable>()
				.set<Transform>({ transform });
			flecsWorldLock.UnlockSyncWrite();
		}
	}

#pragma endregion

#pragma region Spawn Civilians

	std::uniform_int_distribution<unsigned int> civisPerWaveDist(minCivisPerWave, maxCivisPerWave);
	unsigned int civisPerWave = civisPerWaveDist(waveGen);
	std::uniform_real_distribution<float> civiSpawn_x(-worldWidth, worldWidth);

	for (int i = 0; i < civisPerWave; i += 1)
	{
		entity civi{};
		std::string civiPrefab = "PickupPrefab_" + std::to_string(PICKUP_TYPE::CIVILIAN);
		if (RetreivePrefab(civiPrefab.c_str(), civi))
		{
			GVECTORF spawnPos{ civiSpawn_x(waveGen), worldBottom, 0, 1 };
			GMATRIXF transform = civi.get<Transform>()->value;
			transform.row4 = spawnPos;

			flecsWorldLock.LockSyncWrite();
			flecsWorldAsync.entity().is_a(civi)
				.add<Pickup>()
				.add<Civilian>()
				.add<Alive>()
				.add<Collidable>()
				.set<Transform>({ transform });
			flecsWorldLock.UnlockSyncWrite();
		}
	}

#pragma endregion

	std::shared_ptr<const GameConfig> readCfg = gameConfig.lock();

	batchSpawner.Create(spawnBatchRate,
		[this, readCfg, waveSettingsIdx]()
		{
			// Generate random stats for enemies.
			// Will be used to obtain a seed for the random number engine.
			std::random_device batchSeed;
			/* Standard mersenne_twister_engine seeded with rd() normal rand() doesn't work great
			multi-threaded. */
			std::mt19937 batchGen(batchSeed());
			/* Here we decide which enemy type we will spawn for this batch, how many, and their individual stats. */
			unsigned int enemyMinLevel = minEnemyLevelPerWave[waveSettingsIdx -1];
			unsigned int enemyMaxLevel = maxEnemyLevelPerWave[waveSettingsIdx -1];
			std::uniform_int_distribution<unsigned int> levelRangeDist(enemyMinLevel, enemyMaxLevel);
			unsigned int enemyLevel = levelRangeDist(batchGen);
			std::string prefabName = "EnemyPrefab_" + std::to_string(enemyLevel);
			unsigned int enemyBatchSizeMin = readCfg->at(prefabName).at("batchSizeMin").as<unsigned int>();
			unsigned int enemyBatchSizeMax = readCfg->at(prefabName).at("batchSizeMax").as<unsigned int>();
			float enemyMaxMultiplier = maxEnemyMultiplierPerBatch[waveSettingsIdx -1];
			std::uniform_int_distribution<unsigned int> enemyBatchSizeDist(enemyBatchSizeMin, 
				(int)(enemyBatchSizeMax * enemyMaxMultiplier));
			unsigned int enemyBatchSize = enemyBatchSizeDist(batchGen);

			for (int i = 0; i < enemyBatchSize; i += 1)
			{
				entity newEnemy{};
				if (RetreivePrefab(prefabName.c_str(), newEnemy) == false)
					return;

				switch (newEnemy.get<EnemyType>()->type)
				{
				case ENEMY_TYPE::BOMBER:
				{
					std::uniform_real_distribution<float> xDirDist(-15, 15);
					std::uniform_real_distribution<float> yDirDist(-10, 10);

					float xDir = xDirDist(batchGen);
					float yDir = yDirDist(batchGen);

					GVECTORF velocity = { xDir,yDir, 0 };
					GVector::NormalizeF(velocity, velocity);
					GVector::ScaleF(velocity, newEnemy.get<Speed>()->value, velocity);

					std::uniform_real_distribution<float> spawn_x_dist(-worldWidth, worldWidth);
					std::uniform_real_distribution<float> spawn_y_dist(worldBottom, worldTop);
					GVECTORF spawnPos{ spawn_x_dist(batchGen), spawn_y_dist(batchGen), 0, 1 };
					GMATRIXF transform = newEnemy.get<Transform>()->value;
					transform.row4 = StopSpawningOnPlayer(spawnPos.x, spawnPos.y, newEnemy);

					flecsWorldLock.LockSyncWrite();
					flecsWorldAsync.entity().is_a(newEnemy)
						.add<Enemy>()
						.add<Bomber>()
						.add<Alive>()
						.add<Collidable>()
						.set<Transform>({ transform })
						.set<Velocity>({ velocity });
					UpdateWaveCounts();
					flecsWorldLock.UnlockSyncWrite();

					break;
				}
				case ENEMY_TYPE::BAITER:
				{
					float distFromPlayerMin = newEnemy.get<BaiterMovementStats>()->spawnDistFromPlayerMin;
					float distFromPlayerMax = newEnemy.get<BaiterMovementStats>()->spawnDistFromPlayerMax;
					Cannon missileLauncher{ newEnemy.get<Cannon>()->offset,
														newEnemy.get<Cannon>()->aimLeadScaler,
														newEnemy.get<Cannon>()->fireRate,
														std::chrono::high_resolution_clock::now() };
					std::uniform_real_distribution<float> distFromPlayer(distFromPlayerMin, distFromPlayerMax);
					float spawnDistFromPlayer = distFromPlayer(batchGen);
					GMATRIXF transform = newEnemy.get<Transform>()->value;
					transform.row4 = GenerateBaiterPos(spawnDistFromPlayer);
					// Check for function failure.
					if (transform.row4.x == 0 &&
						transform.row4.y == 0 &&
						transform.row4.z == 0 &&
						transform.row4.w == 0)
						continue;

					flecsWorldLock.LockSyncWrite();
					flecsWorldAsync.entity().is_a(newEnemy)
						.add<Enemy>()
						.add<Baiter>()
						.add<Alive>()
						.add<Collidable>()
						.set<Transform>({ transform })
						.set<Cannon>({ missileLauncher });
					UpdateWaveCounts();
					flecsWorldLock.UnlockSyncWrite();

					break;
				}
				case ENEMY_TYPE::LANDER:
				{
					std::uniform_real_distribution<float> spawn_x_dist(-worldWidth, worldWidth);
					std::uniform_real_distribution<float> spawn_y_dist(worldBottom, worldTop);
					GVECTORF spawnPos{ spawn_x_dist(batchGen), spawn_y_dist(batchGen), 0, 1 };
					GMATRIXF transform = newEnemy.get<Transform>()->value;
					transform.row4 = StopSpawningOnPlayer(spawnPos.x, spawnPos.y, newEnemy);
					PeaShooter peaShooter{ newEnemy.get<PeaShooter>()->offset,
											newEnemy.get<PeaShooter>()->range,
											newEnemy.get<PeaShooter>()->fireRate,
											std::chrono::high_resolution_clock::now() };

					flecsWorldLock.LockSyncWrite();
					flecsWorldAsync.entity().is_a(newEnemy)
						.add<Enemy>()
						.add<Lander>()
						.add<Alive>()
						.add<Collidable>()
						.set<Transform>({ transform })
						.set<PeaShooter>({ peaShooter });
					UpdateWaveCounts();
					flecsWorldLock.UnlockSyncWrite();

					break;
				}
				default:
				{
					break;
				}
				}

			}

			// Check, if we need to end this wave and start the next one.
			if (curEnemiesPerWave >= maxEnemiesPerWave[waveSettingsIdx -1])
				batchSpawner = nullptr;

		}, spawnWaveDelay);
}

GVECTORF LevelLogic::StopSpawningOnPlayer(float _spawnPos_x, float _spawnPos_y, entity _enemy)
{
	if (playerQuery.count() <= 0)
		return GIdentityVectorF;

	GVECTORF playerPos = playerQuery.first().get<Transform>()->value.row4;
	float distFromPlayer = DISTANCE_2D(_spawnPos_x, _spawnPos_y, playerPos.x, playerPos.y);
	if (distFromPlayer < _enemy.get<PlayerSpace>()->offset)
	{
		bool playerToRight = playerPos.x > _spawnPos_x;
		switch (playerToRight)
		{
		case false:
		{
			_spawnPos_x += _enemy.get<PlayerSpace>()->offset;
			break;
		}
		case true:
		{
			_spawnPos_x -= _enemy.get<PlayerSpace>()->offset;
			break;
		}
		}
	}

	return { _spawnPos_x, _spawnPos_y, 0, 1 };
}

void LevelLogic::UpdateWaveCounts()
{
	curEnemiesPerWave += 1;
	livingEnemies += 1;
}

unsigned int LevelLogic::GenerateBatchSize(entity _enemyType)
{
	unsigned int batchSizeMin = _enemyType.get<BatchSize>()->min;
	unsigned int batchSizeMax = _enemyType.get<BatchSize>()->max;
	std::uniform_int_distribution<unsigned int> batchSizeDist(batchSizeMin, batchSizeMax);
	std::random_device rd;
	std::mt19937 gen(rd());
	unsigned int batchSize = batchSizeDist(gen);
	return batchSize;
}

void LevelLogic::PuaseWave()
{
	GReturn returnCode{};
	returnCode = batchSpawner.Pause(false, 0);
	if (returnCode == GReturn::SUCCESS)
		batchSpawner.Resume();
	else
		std::cout << "levelLogic::PuaseWave FAILED" << std::endl;
}

GVECTORF LevelLogic::GenerateBaiterPos(float _distFromPlayer)
{
	GVECTORF baiterPos{};
	if (playerQuery.first().is_alive())
		baiterPos = playerQuery.first().get<Transform>()->value.row4;
	else
	{
		std::cout << "LevelLogic::GenerateBaiterPos FAILED" << std::endl;
		return { 0, 0, 0, 0 };
	}

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> direction(0, 1);
	int leftRight = direction(gen);
	int bottomTop = direction(gen);

	switch (leftRight)
	{
	case 0:
		baiterPos.x -= _distFromPlayer * 2;
		break;
	case 1:
		baiterPos.x += _distFromPlayer * 2;
		break;
	default:
		break;
	}

	switch (bottomTop)
	{
	case 0:
		baiterPos.y -= _distFromPlayer;
		break;
	case 1:
		baiterPos.y += _distFromPlayer;
		break;
	default:
		break;
	}

	return baiterPos;
}

void LevelLogic::Reset()
{
	waveNum = 1;
	curEnemiesPerWave = 0;
	livingEnemies = 0;

	SpawnPlayer();
	SpawnWave();
}

// Free any resources used to run this system
bool LevelLogic::Shutdown()
{
	batchSpawner.Pause(0, false);
	batchSpawner = nullptr; // stop adding enemies
	flecsWorldAsync.merge(); // get rid of any remaining commands
	flecsWorld->entity("Level System").destruct();
	// invalidate the shared pointers
	flecsWorld.reset();
	gameConfig.reset();
	return true;
}

// Toggle if a system's Logic is actively running
bool LevelLogic::Activate(bool runSystem)
{
	if (runSystem)
	{
		flecsWorld->entity("MergeAsyncStages").enable();
	}
	else
	{
		flecsWorld->entity("MergeAsyncStages").disable();
	}

	return false;
}

// **** SAMPLE OF MULTI_THREADED USE ****
//world world; // main world
//world async_stage = world.async_stage();
//
//// From thread
//lock(async_stage_lock);
//entity e = async_stage.entity().child_of(parent)...
//unlock(async_stage_lock);
//
//// From main thread, periodic
//lock(async_stage_lock);
//async_stage.merge(); // merge all commands to main world
//unlock(async_stage_lock);