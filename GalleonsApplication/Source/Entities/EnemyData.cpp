#include "EnemyData.h"
#include "Prefabs.h"
#include "../Components/AudioSource.h"
#include "../Components/Gameplay.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"
#include "../Components/visuals.h"

using namespace GOG;
using namespace flecs;
using namespace GW;
using namespace MATH;

bool GOG::EnemyData::Load(	std::shared_ptr<world> _flecsWorld,
							std::weak_ptr<const GameConfig> _gameConfig,
							AudioData& _audioData,
							unsigned int _prefabCount,
							unsigned int _modelIndex,
							ActorData* _actorData,
							ActorData::Model &_model)
{
	// Grab init settings for players
	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();

	/*AUDIO::GSound* damageFXClip = &_audioData.sounds[damageFX];
	damageFXClip->SetVolume(0.025f);
	AUDIO::GSound* deathFXClip = &_audioData.sounds[deathFX];
	deathFXClip->SetVolume(0.05f);*/

	// default projectile orientation & scale

	// Add prefabs to ECS
	std::string prefabType = "EnemyPrefab_" + std::to_string(_prefabCount);

	if (readCfg->find(prefabType) == readCfg->end())
		return false;

		// Scale the enemy's transform

		GMATRIXF transform = GIdentityMatrixF;
		GVECTORF scale{	readCfg->at(prefabType).at("xScale").as<float>(),
						readCfg->at(prefabType).at("yScale").as<float>(),
						readCfg->at(prefabType).at("zScale").as<float>(),
						1};
		GMatrix::ScaleLocalF(transform, scale, transform);
		GVECTORF dir{	G_DEGREE_TO_RADIAN_F(readCfg->at(prefabType.c_str()).at("xRot").as<float>()),
						G_DEGREE_TO_RADIAN_F(readCfg->at(prefabType.c_str()).at("yRot").as<float>()),
						G_DEGREE_TO_RADIAN_F(readCfg->at(prefabType.c_str()).at("zRot").as<float>()), };
		GMatrix::RotateXLocalF(transform, dir.x, transform);
		GMatrix::RotateYLocalF(transform, dir.y, transform);
		GMatrix::RotateZLocalF(transform, dir.z, transform);

		// Find the boundaries of the box collider for the prefab's model.

		float minX = 0, minY = 0, minZ = 0;
		float maxX = 0, maxY = 0, maxZ = 0;
		for (unsigned int i = _model.vertexStart; i < _model.vertexStart + _model.vertexCount; i += 1)
		{
			GVECTORF vertex{ _actorData->vertices[i].pos.x,
								_actorData->vertices[i].pos.y,
								_actorData->vertices[i].pos.z };

			// Scale the enemy's vertices for bound box info.
			vertex.x *= scale.x;
			vertex.y *= scale.y;
			vertex.z *= scale.z;

			maxX = max(maxX, vertex.x); maxY = max(maxY, vertex.y); maxZ = max(maxZ, vertex.z);
		}
		float extentX = maxX, extentY = maxY, extentZ = maxZ;
		GOBBF boundBox{ transform.row4, {extentX, extentY, extentZ}, GIdentityQuaternionF };

		// Audio

		std::string deathFXName = (*readCfg).at(prefabType).at("deathFX").as<std::string>();
		float deathFXVolume = (*readCfg).at(prefabType).at("deathVolume").as<float>();
		GW::AUDIO::GSound* deathFXClip = _audioData.CreateSound(deathFXName, deathFXVolume);

		auto newPrefab = _flecsWorld->prefab(prefabType.c_str())
			.override<Enemy>()
			.override<Alive>()
			.override<Collidable>()
			.set_override<Transform>({ transform })
			.set_override<BoundBox>({ boundBox })
			.set<EnemyType>({ _prefabCount})
			.set<Score>({ readCfg->at(prefabType).at("score").as<unsigned int>() })
			.set<BatchSize>({	readCfg->at(prefabType).at("batchSizeMin").as<unsigned int>(),
								readCfg->at(prefabType).at("batchSizeMax").as<unsigned int>() })
			.set<Speed>({ readCfg->at(prefabType).at("speed").as<float>() })
			// Velocity and acceleration are not used by all enemies, so should be moved to specific case.
			.set_override<Velocity>({})
			.set_override<Acceleration>({})
			// ------------------------------------------------------------------------------------------
			.set_override<FlipInfo>({ true, readCfg->at(prefabType).at("flipTime").as<int>(), 0 })
			.set<ModelIndex>({ _modelIndex })
			.set<SoundClips>({ {{"Death", deathFXClip}} });

		switch (newPrefab.get<EnemyType>()->type)
		{
			case ENEMY_TYPE::BOMBER :
			{
			newPrefab.override<Bomber>()
				.set_override<BomberTrap>({ readCfg->at("TrapEjector").at("launchOffset").as<float>(),
											std::chrono::milliseconds(readCfg->at("TrapEjector").at("fireRate").as<int>()),
											std::chrono::high_resolution_clock::now() })
				.set<PlayerSpace>({ readCfg->at(prefabType).at("playerSpace").as<float>() });
				break;
			}
			case ENEMY_TYPE::BAITER :
			{
				float launchOffset = readCfg->at("Cannon").at("launchOffset").as<float>();
				float aimLeadScaler = readCfg->at("Cannon").at("aimLeadScaler").as<float>();
				int fireRate = readCfg->at("Cannon").at("fireRate").as<int>();
				float spawnDistFromPlayerMin = readCfg->at(prefabType).at("spawnDistFromPlayerMin").as<float>();
				float spawnDistFromPlayerMax = readCfg->at(prefabType).at("spawnDistFromPlayerMax").as<float>();
				float speed = readCfg->at(prefabType).at("speed").as<float>();
				float followDistance = readCfg->at(prefabType).at("followDistance").as<float>();
				float boostSpeed = readCfg->at(prefabType).at("boostSpeed").as<float>();
				int boostIntervalMin = readCfg->at(prefabType).at("boostIntervalMin").as<int>();
				int boostIntervaleMax = readCfg->at(prefabType).at("boostIntervalMax").as<int>();
				int boostDurationMin = readCfg->at(prefabType).at("boostDurationMin").as<int>();
				int boostDurationMax = readCfg->at(prefabType).at("boostDurationMax").as<int>();

				newPrefab.override<Baiter>()
				.set_override<BaiterMovementStats>({	spawnDistFromPlayerMin, 
														spawnDistFromPlayerMax, 
														speed, 
														followDistance })
				.set_override<Cannon>({	launchOffset,
										aimLeadScaler,
										std::chrono::milliseconds(fireRate),
										std::chrono::high_resolution_clock::now() })
				.set_override<SpeedBoost>({ false,
											false,
											boostSpeed,
											std::chrono::milliseconds(boostIntervalMin),
											std::chrono::milliseconds(boostIntervaleMax),
											std::chrono::milliseconds(0),
											std::chrono::milliseconds(boostDurationMin),
											std::chrono::milliseconds(boostDurationMax),
											std::chrono::milliseconds(0),
											std::chrono::high_resolution_clock::now() });
				break;
			}
			case ENEMY_TYPE::LANDER:
			{
				float offset = readCfg->at("PeaShooter").at("offset").as<float>();
				float range = readCfg->at("PeaShooter").at("range").as<float>();
				int fireRate = readCfg->at("PeaShooter").at("fireRate").as<int>();
				float playerSpace = readCfg->at(prefabType).at("playerSpace").as<float>();

				newPrefab.override<Lander>()
					.set_override<PeaShooter>({ offset,
												range,
												std::chrono::milliseconds(fireRate),
												std::chrono::high_resolution_clock::now() })
					.set<PlayerSpace>({ playerSpace });
				break;
			}
			default:
			{
				break;
			}
		}

		RegisterPrefab(prefabType.c_str(), newPrefab);

	return true;
}

bool GOG::EnemyData::Unload(std::shared_ptr<world> _flecsWorld)
{
	// remove all bullets and their prefabs
	_flecsWorld->defer_begin(); // required when removing while iterating!
	_flecsWorld->each([](entity _entity, Enemy&) 
	{
		_entity.destruct(); // destroy this entitiy (happens at frame end)
	});
	_flecsWorld->defer_end(); // required when removing while iterating!

	UnregisterPrefab("EnemyPrefab_1");
	UnregisterPrefab("EnemyPrefab_2");
	UnregisterPrefab("EnemyPrefab_3");

	return true;
}
