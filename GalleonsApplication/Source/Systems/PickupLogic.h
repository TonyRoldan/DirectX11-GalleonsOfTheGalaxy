#ifndef PICKUPLOGIC_H
#define PICKUPLOGIC_H

#include "../GameConfig.h"

namespace GOG
{
	class PickupLogic
	{
		// non-ownership handle to configuration settings
		std::weak_ptr<const GameConfig> gameConfig;
		// handle to events
		GW::CORE::GEventGenerator eventPusher;
		// shared connection to the main ECS engine
		std::shared_ptr<flecs::world> flecsWorld;
		flecs::system civiSystem;

	public:

		// attach the required logic to the ECS 
		bool Init(	std::shared_ptr<flecs::world> _flecsWorld,
					std::weak_ptr<const GameConfig> _gameConfig,
					GW::CORE::GEventGenerator _eventPusher);
		// control if the system is actively running
		bool Activate(bool _runSystem);
		// release any resources allocated by the system
		bool Shutdown();
	};

};

#endif