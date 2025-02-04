// The Enemy system is responsible for enemy behaviors
#ifndef ENEMYLOGIC_H
#define ENEMYLOGIC_H

#include "../GameConfig.h"

#include "../Components/Identification.h"
#include "../Components/Gameplay.h"
#include "../Components/Physics.h"

#include <random>

namespace GOG
{
	class EnemyLogic
	{
		// non-ownership handle to configuration settings
		std::weak_ptr<const GameConfig> gameConfig;
		// handle to events
		GW::CORE::GEventGenerator eventPusher;
		// shared connection to the main ECS engine
		std::shared_ptr<flecs::world> flecsWorld;

		flecs::query<const Player, const Transform, const Velocity> playerMovementQuery;
		float playerPos_x;
		float playerPos_y;
		float playerVelocity_x;
		float playerVelocity_y;
		flecs::query<const Baiter, const BaiterMovementStats, SpeedBoost, Transform, FlipInfo, Cannon> baiterQuery;
		flecs::query<const Civilian, CaptureInfo, const Transform> civiQuery;
		flecs::system landerSystem;


		void BaiterMovement(float _deltaTime, 
							const BaiterMovementStats& _movementStats, 
							SpeedBoost& _speedBoost, 
							Transform& _transform,
							FlipInfo& _flipInfo);
		void LanderMovement(flecs::entity _lander, Transform& _landerTransform, const Speed& _speed);
		void FireCannon(Transform& _transform, Cannon& _cannon);

		flecs::query<const Bomber, 
					Transform, 
					Velocity, 
					FlipInfo, BomberTrap> bomberQuery;
		void BomberMovement(float _deltaTime, float _speed, 
							/*float _topBound, float _bottomBound,*/
							flecs::entity _entity,
							Transform& _transform,
							Velocity& _velocity,
							FlipInfo& _flipInfo, float _bottom, float _top);

		void SpawnBomberTrap(Transform& _transform, Velocity& _velocity, BomberTrap& _trap);

	public:

		// attach the required logic to the ECS 
		bool Init(	std::shared_ptr<flecs::world> _game,
					std::weak_ptr<const GameConfig> _gameConfig,
					GW::CORE::GEventGenerator _eventPusher);
		// control if the system is actively running
		bool Activate(bool _runSystem);
		// release any resources allocated by the system
		bool Shutdown();
	};

};

#endif