#include "PlayerData.h"
#include "Prefabs.h"
#include "../Components/Gameplay.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"
#include "../Components/Visuals.h"
#include "../Components/AudioSource.h"

using namespace GW;
using namespace MATH;
using namespace MATH2D;

bool GOG::PlayerData::Load(std::shared_ptr<flecs::world> _flecsWorld,
	std::weak_ptr<const GameConfig> _gameConfig,
	unsigned int _playerCount,
	unsigned int _modelIndex,
	ActorData* _actorData,
	ActorData::Model& _model,
	AudioData& _audioData)
{
	// Perform any necessary operations with the config info to build the player prefab.

	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();
	std::string prefabType = "PlayerPrefab_" + std::to_string(_playerCount);

	if (readCfg->find(prefabType) == readCfg->end())
		return false;

	// Transform

	GMATRIXF transform = GIdentityMatrixF;
	GVECTORF scale{ readCfg->at(prefabType.c_str()).at("xScale").as<float>(),
					readCfg->at(prefabType.c_str()).at("yScale").as<float>(),
					readCfg->at(prefabType.c_str()).at("zScale").as<float>(), };
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
	for (unsigned int i = _model.vertexStart; i <= _model.vertexStart + _model.vertexCount; i += 1)
	{
		GVECTORF vertex{ _actorData->vertices[i].pos.x,
							_actorData->vertices[i].pos.y,
							_actorData->vertices[i].pos.z };

		// Scale the player's vertices for bound box info.
		vertex.x *= scale.x;
		vertex.y *= scale.y;
		vertex.z *= scale.z;

		//minX = min(minX, vertex.x); minY = min(minY, vertex.y); minZ = min(minZ, vertex.z);
		maxX = max(maxX, vertex.x); maxY = max(maxY, vertex.y); maxZ = max(maxZ, vertex.z);
	}
	//float extentX = maxX - minX, extentY = maxY - minY, extentZ = maxZ - minZ;
	float extentX = maxX, extentY = maxY, extentZ = maxZ;
	GOBBF boundBox{ transform.row4, {extentX, extentY, extentZ}, GIdentityQuaternionF };

	// Audio

	std::string deathFXName = (*readCfg).at(prefabType).at("deathFX").as<std::string>();
	float deathFXVolume = (*readCfg).at(prefabType).at("deathVolume").as<float>();
	GW::AUDIO::GSound* deathFXClip = _audioData.CreateSound(deathFXName, deathFXVolume);

	std::string smartBombFX = (*readCfg).at("NukeDispenser").at("detonateFX").as<std::string>();
	float smartBombFXVolume = (*readCfg).at("NukeDispenser").at("detonateVolume").as<float>();
	GW::AUDIO::GSound* smartBombFXClip = _audioData.CreateSound(smartBombFX, smartBombFXVolume);

	std::string accelFXName = (*readCfg).at(prefabType).at("accelFX").as<std::string>();
	float accelFXVolume = (*readCfg).at(prefabType).at("accelVolume").as<float>();
	LoopingClip accelFXClip = { _audioData.CreateSoundLooping(accelFXName, accelFXVolume), accelFXVolume };

	// Make a player prefab for each different type of player model there is.
	auto newPrefab = _flecsWorld->prefab(prefabType.c_str())
		.override<Player>()
		.override<Alive>()
		.override<ControllerID>()
		.override<Collidable>()
		//.set<Health>({ readCfg->at(prefabType).at("health").as<float>() })
		.set_override<Transform>({ transform })
		.set_override<BoundBox>({ boundBox })
		.set_override<Velocity>({})
		.set_override<Acceleration>({})
		.set_override<PlayerMoveInfo>({ readCfg->at(prefabType).at("hAccelRate").as<float>(),
										readCfg->at(prefabType).at("hDeccelRate").as<float>(),
										readCfg->at(prefabType).at("hMaxSpeed").as<float>(),
										readCfg->at(prefabType).at("hFlipAccelRate").as<float>(),
										readCfg->at(prefabType).at("vAccelRate").as<float>(),
										readCfg->at(prefabType).at("vDeccelRate").as<float>(),
										readCfg->at(prefabType).at("vMaxSpeed").as<float>(),
										readCfg->at(prefabType).at("vFlipAccelRate").as<float>() })
		.set_override<FlipInfo>({ true, readCfg->at(prefabType).at("hFlipTime").as<int>(), 0 })
		.set<PlayerHaptics>({	readCfg->at("Haptics").at("lazerPan").as<float>(),
								readCfg->at("Haptics").at("lazerDuration").as<float>() / 1000,
								readCfg->at("Haptics").at("lazerStrength").as<float>(),
								readCfg->at("Haptics").at("smartBombPan").as<float>(),
								readCfg->at("Haptics").at("smartBombDuration").as<float>() / 1000,
								readCfg->at("Haptics").at("smartBombStrength").as<float>(),
								readCfg->at("Haptics").at("playerDeathPan").as<float>(),
								readCfg->at("Haptics").at("playerDeathDuration").as<float>() / 1000,
								readCfg->at("Haptics").at("playerDeathStength").as<float>() })
		.set<ModelIndex>({ _modelIndex })
		.set<SoundClips>({
			{{"Death", deathFXClip},
			{"SmartBomb", smartBombFXClip}}
			})
		.set<LoopingClips>({
			{{"Accel", accelFXClip}}
			});
	RegisterPrefab(prefabType.c_str(), newPrefab);

	_flecsWorld->entity("Persistent Player Stats")
		.add<Player>()
		.add<PersistentStats>()
		.set<Lives>({ readCfg->at(prefabType).at("lives").as<unsigned int>() })
		.set<Score>({ 0 })
		.set<NukeDispenser>(
		{	
			0, 
			readCfg->at("NukeDispenser").at("range").as<float>(),
			readCfg->at("NukeDispenser").at("maxCapacity").as<unsigned int>()
		});

	return true;
}

bool GOG::PlayerData::Unload(std::shared_ptr<flecs::world> _game)
{
	// remove all players
	_game->defer_begin(); // required when removing while iterating!
	_game->each([](flecs::entity _entity, Player&)
		{
			_entity.destruct(); // destroy this entitiy (happens at frame end)
		});
	_game->defer_end(); // required when removing while iterating!

	UnregisterPrefab("PlayerPrefab_1");

	return true;
}
