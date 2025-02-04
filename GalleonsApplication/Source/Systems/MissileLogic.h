#pragma once

#include "../GameConfig.h"

// example space game (avoid name collisions)
namespace GOG
{
	class MissileLogic
	{
	private:
		// Shared connection to the main ECS engine.
		std::shared_ptr<flecs::world> flecsWorld;
		// Non-ownership handle to configuration settings in the .ini files.
		std::weak_ptr<const GameConfig> gameConfig;
		flecs::system missileSystem;

	public:
		// Attach the required logic to the ECS.
		bool Init(std::shared_ptr<flecs::world> _flecsWorld,
			std::weak_ptr<const GameConfig> _gameConfig);
		// Control if the system is actively running.
		bool Activate(bool _runSystem);
		// Release any resources allocated by the system.
		bool Shutdown();
	};

};