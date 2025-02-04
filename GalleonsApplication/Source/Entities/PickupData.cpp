#include "PickupData.h"
#include "Prefabs.h"

#include "../Components/AudioSource.h"
#include "../Components/Identification.h"
#include "../Components/Gameplay.h"
#include "../Components/Physics.h"
#include "../Components/Visuals.h"

using namespace GOG;
using namespace flecs;
using namespace GW;
using namespace MATH;

bool PickupData::Load(	std::shared_ptr<world> _flecsWorld,
							std::weak_ptr<const GameConfig> _gameConfig,
							AudioData& _audioData,
							unsigned int _prefabCount,
							unsigned int _modelIndex,
							ActorData* _actorData,
							ActorData::Model& _model)
{
	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();

	std::string prefabType = "PickupPrefab_" + std::to_string(_prefabCount);
	if (readCfg->find(prefabType) == readCfg->end())
		return false;

	// Rotation

	GMATRIXF transform{ GIdentityMatrixF };
	float x_rot = readCfg->at(prefabType).at("xRot").as<float>();
	float y_rot = readCfg->at(prefabType).at("yRot").as<float>();
	float z_rot = readCfg->at(prefabType).at("zRot").as<float>();
	GMatrix::RotateXGlobalF(transform, x_rot, transform);
	GMatrix::RotateYGlobalF(transform, y_rot, transform);
	GMatrix::RotateZGlobalF(transform, z_rot, transform);

	// Scale

	GVECTORF prefabScale{	readCfg->at(prefabType).at("xScale").as<float>(),
							readCfg->at(prefabType).at("yScale").as<float>(),
							readCfg->at(prefabType).at("zScale").as<float>(),
							1 };
	GMatrix::ScaleLocalF(transform, prefabScale, transform);

	// Audio

	std::string pickupFXName = (*readCfg).at(prefabType).at("pickupFX").as<std::string>();
	float pickupFXVolume = (*readCfg).at(prefabType).at("pickupVolume").as<float>();
	GW::AUDIO::GSound* pickupFXClip = _audioData.CreateSound(pickupFXName, pickupFXVolume);

	// Find the boundaries of the box collider for the prefab's model.

	float maxX = 0, maxY = 0, maxZ = 0;
	for (unsigned int i = _model.vertexStart; i < _model.vertexStart + _model.vertexCount; i += 1)
	{
		GVECTORF vertex{	_actorData->vertices[i].pos.x,
							_actorData->vertices[i].pos.y,
							_actorData->vertices[i].pos.z };

		// Scale the projectile's vertices for bound box info.
		vertex.x *= prefabScale.x;
		vertex.y *= prefabScale.y;
		vertex.z *= prefabScale.z;

		maxX = max(maxX, vertex.x); maxY = max(maxY, vertex.y); maxZ = max(maxZ, vertex.z);
	}
	float extentX = maxX, extentY = maxY, extentZ = maxZ;
	GOBBF boxCollider{ transform.row4, {extentX, extentY, extentZ}, GIdentityQuaternionF };

	auto newPrefab = _flecsWorld->prefab(prefabType.c_str())
		.override<Pickup>()
		.override<Alive>()
		.override<Collidable>()
		.set_override<Transform>({ transform })
		.set_override<BoundBox>({ boxCollider })
		.set<PickupType>({ _prefabCount })
		.set<ModelIndex>({ _modelIndex })
		.set<SoundClips>({
			{{"Pickup", pickupFXClip}}
			});
	RegisterPrefab(prefabType.c_str(), newPrefab);

	switch (newPrefab.get<PickupType>()->type)
	{
		case PICKUP_TYPE::SMART_BOMB:
		{
			newPrefab.override<SmartBomb>();
			break;
		}
		case PICKUP_TYPE::CIVILIAN:
		{
			newPrefab.override<Civilian>()
				.set_override<CaptureInfo>({ false })
				.set_override<FlipInfo>({ true, readCfg->at(prefabType).at("flipTime").as<int>(), 0 })
				.set_override<CiviMovementStats>(
					{	true,
						readCfg->at(prefabType).at("speed").as<float>(),
						readCfg->at(prefabType).at("dirChangeIntervalMin").as<unsigned int>(),
						readCfg->at(prefabType).at("dirChangeIntervalMax").as<unsigned int>(),
						std::chrono::milliseconds(0),
						std::chrono::high_resolution_clock::now() 
					})
				.set<Offset>({ readCfg->at(prefabType).at("hangOffset").as<float>() })
				.set<Score>({ readCfg->at(prefabType).at("score").as<unsigned int>() });
			break;
		}
		default:
		{
			break;
		}
	}

	return true;
}

bool PickupData::Unload(std::shared_ptr<world> _flecsWorld)
{
	_flecsWorld->defer_begin(); // required when removing while iterating!
	_flecsWorld->each([](entity _entity, Pickup&)
	{
		_entity.destruct();
	});
	_flecsWorld->defer_end(); // required when removing while iterating!

	UnregisterPrefab("PickupPrefab_1");
	UnregisterPrefab("PickupPrefab_2");

	return true;
}
