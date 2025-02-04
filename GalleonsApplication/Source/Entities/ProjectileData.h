#pragma once

#include "../GameConfig.h"
#include "../Utils/ActorData.h"
#include "../Utils/AudioData.h"

namespace GOG
{
	class ProjectileData
	{
	public:
		// Load prefabs into FLECS
		bool Load(	std::shared_ptr<flecs::world> _flecsWorld,
					std::weak_ptr<const GameConfig> _gameConfig,
					AudioData &_audioEngine,
					unsigned int _prefabCount,
					unsigned int _modelIndex,
					ActorData* _actorData,
					ActorData::Model &_model);
		// Remove all lazers, missiles, and their prefabs.
		bool Unload(std::shared_ptr<flecs::world> _flecsWorld);
	};

};