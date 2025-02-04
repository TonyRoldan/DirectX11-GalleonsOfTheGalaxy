// This class populates all player entities 
#pragma once

#include "../Utils/ActorData.h"
#include "../Utils/AudioData.h"
#include "../GameConfig.h"

// example space game (avoid name collisions)
namespace GOG
{
	class PlayerData
	{

	public:

		// Load required entities and/or prefabs into the ECS 
		bool Load(	std::shared_ptr<flecs::world> _flecsWorld,
					std::weak_ptr<const GameConfig> _gameConfig,
					unsigned int _playerCount,
					unsigned int _modelIndex,
					ActorData* _actorData,
					ActorData::Model& _model,
					AudioData& _audioData);

		// Unload the entities/prefabs from the ECS
		bool Unload(std::shared_ptr<flecs::world> _flecsWorld);
	};

};