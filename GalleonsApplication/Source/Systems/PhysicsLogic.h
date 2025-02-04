// The level system is responsible for transitioning the various levels in the game
#ifndef PHYSICSLOGIC_H
#define PHYSICSLOGIC_H

// Contains our global game settings
#include "../GameConfig.h"

#include "../Components/Identification.h"
#include "../Components/Gameplay.h"
#include "../Components/Physics.h"

// example space game (avoid name collisions)
namespace GOG
{
	class PhysicsLogic
	{
	private:
		// shared connection to the main ECS engine
		std::shared_ptr<flecs::world> flecsWorld;
		// non-ownership handle to configuration settings
		std::weak_ptr<const GameConfig> gameConfig;

		flecs::query<const Player, Transform> playerTransformQuery;
		flecs::query<const Camera, Transform> cameraTransformQuery;
		flecs::query<const Enemy, Transform> enemyTransformQuery;
		flecs::query<const Pickup, Transform> pickupTransformQuery;
		flecs::query<const Projectile, const Transform> projectileTransformQuery;
		flecs::query<const PersistentStats, Lives> persistentStatsQuery;

		GW::MATH::GVECTORF playerPos;

		// Find all the colliders in the world.
		flecs::query<Collidable, const BoundBox> collidersQuery;
		// Local storage for collider information.
		struct Collider
		{
			flecs::entity owner;
			BoundBox box;
		};
		// All the current colliders in the world.
		std::vector<Collider> colliders;

		flecs::system updateColliderPos;

		GW::CORE::GEventGenerator eventPusher;

		void PlayerDestroyed(flecs::entity& _entity);
		void EnemyDestroyed(flecs::entity& _entity);
		void EnemyOutBounds(flecs::entity& _entity);
		void PickupOutBounds(flecs::entity& _entity);
		void GetPickup(flecs::entity _entity);

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