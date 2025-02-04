#include "ProjectileData.h"
#include "Prefabs.h"

#include "../Components/AudioSource.h"
#include "../Components/Identification.h"
#include "../Components/Gameplay.h"
#include "../Components/Physics.h"
#include "../Components/Visuals.h"
#include "../Components/Lights.h"

using namespace GW;
using namespace MATH;

bool GOG::ProjectileData::Load(	std::shared_ptr<flecs::world> _flecsWorld,
								std::weak_ptr<const GameConfig> _gameConfig,
								AudioData& _audioData,
								unsigned int _prefabCount,
								unsigned int _modelIndex,
								ActorData* _actorData,
								ActorData::Model &_model)
{
	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();

	

	std::string prefabType = "ProjectilePrefab_" + std::to_string(_prefabCount);

	if (readCfg->find(prefabType) == readCfg->end())
		return false;

	GMATRIXF transform{ GIdentityMatrixF };

	// Audio

	std::string shootFXName = (*readCfg).at(prefabType).at("shootFX").as<std::string>();
	float shootFXVolume = (*readCfg).at(prefabType).at("shootVolume").as<float>();
	GW::AUDIO::GSound* shootFXClip = _audioData.CreateSound(shootFXName, shootFXVolume);

	// Lighting
	float lightRadius = { (*readCfg).at(prefabType).at("lightRadius").as<float>() };
	LightOffset lightOffset = { 0, 0, (*readCfg).at(prefabType).at("lightZOffset").as<float>() };
	LightColor lightColor = { 
		(*readCfg).at(prefabType).at("lightColorR").as<float>(),
		(*readCfg).at(prefabType).at("lightColorG").as<float>(),
		(*readCfg).at(prefabType).at("lightColorB").as<float>() 
	};

	// Rotation

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
		.override<Projectile>()
		.override<Alive>()
		.override<Collidable>()
		.override<Sender>()
		.set_override<Transform>({ transform })
		.set_override<BoundBox>({ boxCollider })
		.set<Speed>({ readCfg->at(prefabType).at("speed").as<float>() })
		.set<ProjectileType>({ _prefabCount })
		.set<ModelIndex>({ _modelIndex })
		.set<SoundClips>({ {{"Shoot", shootFXClip}} })
		.add<Light>()
		.set<LightType>({ LIGHT_TYPE::point })
		.set<LightColor>(lightColor)
		.set<LightOffset>({ lightOffset })
		.set<LightRadius>({ lightRadius });

	switch (newPrefab.get<ProjectileType>()->type)
	{
		case PROJECTILE_TYPE::CANNONBALL:
		{
			newPrefab.override<Cannonball>();
			break;
		}
		case PROJECTILE_TYPE::LAZER:
		{
			newPrefab.override<Lazer>()
				.set_override<Offset>({ readCfg->at(prefabType).at("offset").as<float>() });
			break;
		}
		case PROJECTILE_TYPE::PEA:
		{
			newPrefab.override<Pea>();
			break;
		}
		case PROJECTILE_TYPE::TRAP:
		{
			newPrefab.override<Trap>();
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

bool GOG::ProjectileData::Unload(std::shared_ptr<flecs::world> _flecsWorld)
{
	_flecsWorld->defer_begin(); // required when removing while iterating!
	_flecsWorld->each([](flecs::entity _entity, Lazer&)
	{
		_entity.destruct();
	});
	_flecsWorld->each([](flecs::entity _entity, Cannonball&)
	{
		_entity.destruct();
	});
	_flecsWorld->defer_end(); // required when removing while iterating!

	UnregisterPrefab("ProjectilePrefab_1");
	UnregisterPrefab("ProjectilePrefab_2");
	UnregisterPrefab("ProjectilePrefab_3");

	return true;
}
