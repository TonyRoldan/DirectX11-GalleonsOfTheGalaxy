// The level system is responsible for transitioning the various levels in the game
#ifndef LEVELLOGIC_H
#define LEVELLOGIC_H

// Contains our global game settings
#include "../GameConfig.h"

// Entities for players, enemies & bullets
#include "../Entities/PlayerData.h"
#include "../Entities/BulletData.h"

#include "../Components/Identification.h"
#include "../Components/Gameplay.h"
#include "../Components/Physics.h"

#include "../Utils/AudioData.h"

// example space game (avoid name collisions)
namespace GOG
{
	class LevelLogic
	{

	private:

		// shared connection to the main ECS engine
		std::shared_ptr<flecs::world> flecsWorld;
		// async version of above for threaded operations
		flecs::world flecsWorldAsync;
		// you must ensure the async_stage is thread safe as it has no built-in synchronization
		// be sure to unlock when done so the main thread can safely merge the changes
		GW::CORE::GThreadShared flecsWorldLock;
		flecs::query<const Player, const Transform> playerQuery;
		flecs::query<const Civilian, const Score> civilianQuery;
		flecs::query<const SmartBomb> smartBombQuery;
		flecs::query<const Projectile> projectileQuery;
		flecs::query<const Enemy> enemyQuery;
		flecs::query<const Camera> camQuery;
		// non-ownership handle to configuration settings
		std::weak_ptr<const GameConfig> gameConfig;
		// Level system will also load and switch music
		GW::AUDIO::GMusic currentTrack;

		GW::CORE::GEventResponder eventHandler;
		GW::CORE::GEventGenerator eventPusher;

#pragma region Spawning / Wave Management

		GW::SYSTEM::GDaemon playerSpawner{};
		GW::SYSTEM::GDaemon batchSpawner{};
		// How many unique waves there are
		unsigned int maxWaves = 0;
		// Which wave of enemies the game is on.
		unsigned int waveNum = 1;
		// Enemies spawned so far this wave.
		unsigned int curEnemiesPerWave = 0;
		// How many enemies are alive currently.
		unsigned int livingEnemies = 0;

#pragma endregion

#pragma region Game Settings

		float worldTop = 0;
		float worldWidth = 0;
		float worldBottom = 0;
		// Time before a wave starts.
		unsigned int spawnWaveDelay = 0;
		// Time between spawning a batch of one type of enemy within waves.
		unsigned int spawnBatchRate = 0;
		/* The max number of enemies that can be spawned per wave. This is a soft limit. A batch can surpass this
		limit, but no more batches will be spawned for the current wave once it is surpassed. */
		unsigned int minCivisPerWave = 0;
		unsigned int maxCivisPerWave = 0;
		std::vector<unsigned int> smartBombsPerWave;
		std::vector<unsigned int> minEnemyLevelPerWave;
		std::vector<unsigned int> maxEnemyLevelPerWave;
		std::vector<unsigned int> maxEnemyMultiplierPerBatch;
		std::vector<unsigned int> maxEnemiesPerWave;

#pragma endregion

		void SpawnPlayer();
		// Recursive and delayed. Allows indefinite and time intervaled enemy spawning.
		void SpawnWave();
		// Update wave stat trackers.
		void UpdateWaveCounts();
		// Generate a random size for thr group of enemies to spawn within their type's batch size range.
		unsigned int GenerateBatchSize(flecs::entity _enemyType);
		// Spawn an enemy into the game after its info and stats have been decided.
		void SpawnEnemy(flecs::entity _newEnemy);
		void PuaseWave();
		GW::MATH::GVECTORF GenerateBaiterPos(float _distFromPlayer);
		GW::MATH::GVECTORF GenerateBomberPos(float _distFromPlayer);
		// If we are about to spawn on the player, then move out of the way.
		GW::MATH::GVECTORF StopSpawningOnPlayer(float _spawnPos_x, float _spawnPos_y, flecs::entity _enemy);

	public:

		// attach the required logic to the ECS 
		bool Init(	std::shared_ptr<flecs::world> _game,
					std::weak_ptr<const GameConfig> _gameConfig,
					AudioData& _audioData,
					GW::CORE::GEventGenerator _eventPusher);
		void Reset();
		// control if the system is actively running
		bool Activate(bool runSystem);
		// release any resources allocated by the system
		bool Shutdown();
	};

};

#endif