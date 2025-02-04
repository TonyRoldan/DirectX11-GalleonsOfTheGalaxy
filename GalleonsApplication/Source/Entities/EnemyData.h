#pragma once

#include "../GameConfig.h"
#include "../Utils/ActorData.h"
#include "../Utils/AudioData.h"

// example space game (avoid name collisions)
namespace GOG
{
	class EnemyData
	{

	public:

		// Load required entities and/or prefabs into the ECS 
		bool Load(	std::shared_ptr<flecs::world> _game,
					std::weak_ptr<const GameConfig> _gameConfig,
					AudioData& _audioEngine,
					unsigned int _prefabCount,
					unsigned int _modelIndex,
					ActorData* _actorData,
					ActorData::Model &_model);
		// Unload the entities/prefabs from the ECS
		bool Unload(std::shared_ptr<flecs::world> _game);
	};

};