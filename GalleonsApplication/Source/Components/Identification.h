// define all ECS components related to identification
#ifndef IDENTIFICATION_H
#define IDENTIFICATION_H

// example space game (avoid name collisions)
namespace GOG
{
	enum ENEMY_TYPE
	{
		LANDER = 1,
		BOMBER,
		BAITER
	};

	enum PROJECTILE_TYPE
	{
		CANNONBALL = 1,
		LAZER,
		PEA,
		TRAP
	};

	enum SENDER
	{
		PLAYER,
		ENEMY
	};

	enum PICKUP_TYPE
	{
		SMART_BOMB = 1,
		CIVILIAN
	};

	enum HAPTIC_TYPE
	{
		FIRE_LAZER,
		DETONATE_SMART_BOMB,
		PLAYER_DEATH
	};

	struct EnemyType { unsigned int type; };
	struct ProjectileType { unsigned int type; };
	struct PickupType { unsigned int type; };
	struct Sender { unsigned int entityType; };
	struct ControllerID { unsigned index = 0; };

	struct PersistentStats {};
	struct Player {};
	struct Enemy {};
	struct Bomber {};
	struct Baiter {};
	struct Lander {};
	struct Projectile {};
	struct Lazer {};
	struct Cannonball {};
	struct Pea {};
	struct Pickup {};
	struct Trap {};
	struct SmartBomb {};
	struct Civilian {};
	struct Camera {};
	struct Alive {};
};

#endif