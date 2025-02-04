// define all ECS components related to gameplay
#ifndef GAMEPLAY_H
#define GAMEPLAY_H

// example space game (avoid name collisions)
namespace GOG
{

	// gameplay tags (states)
	struct Firing {};
	struct Charging {};
	struct ChargingSmartBomb {};
	struct Capturing {};

	struct BatchSize { unsigned int min, max; };
	struct Lives { unsigned int	count; };
	struct Score { unsigned int value; };
	struct PlayerSpace { float offset; };

#pragma region Movement

	struct PlayerMoveInfo
	{
		float hAccelRate;
		float hDeccelRate;
		float hMaxSpeed;
		float hFlipAccelRate;
		float vAccelRate;
		float vDeccelRate;
		float vMaxSpeed;
		float vFlipAccelRate;
	};

	struct PlayerHaptics 
	{
		float lazerPan; 
		float lazerDuration; 
		float lazerStrength;
		float smartBombPan; 
		float smartBombDuration; 
		float smartBombStrength;
		float playerDeathPan;
		float playerDeathDuration;
		float playerDeathStrength;
	};

	struct BaiterMovementStats
	{
		float spawnDistFromPlayerMin;
		float spawnDistFromPlayerMax;
		float speed;
		float followDistance;
	};

	struct CiviMovementStats
	{
		bool isWalkingRight;
		float speed;
		unsigned int dirChangIntervalMin;
		unsigned int dirChangIntervalMax;
		std::chrono::milliseconds curDirChangeInterval;
		std::chrono::high_resolution_clock::time_point lastDirChange;
	};

	struct SpeedBoost
	{
		bool isBoosting;
		bool isMovingRight;
		float speed;
		std::chrono::milliseconds intervalMin;
		std::chrono::milliseconds intervalMax;
		std::chrono::milliseconds curInterval;
		std::chrono::milliseconds durationMin;
		std::chrono::milliseconds durationMax;
		std::chrono::milliseconds curDuration;
		std::chrono::high_resolution_clock::time_point lastBoost;
	};

	struct FlipInfo
	{
		bool isFacingRight;
		// how long in milliseconds a flip takes to perform
		int flipTime;
		// how far in degrees the entity has flipped toward the left
		float degreesFlipped;
		// -1 = left, 0 = no flip occuring, 1 = right
		int flipDir;
	};

#pragma endregion

#pragma region Weapons

	struct ChargeInfo
	{
		//float chargedDamage;
		std::chrono::milliseconds timeTillFullyCharged;
		std::chrono::high_resolution_clock::time_point chargeStart;
		std::chrono::high_resolution_clock::time_point chargeEnd;
	};

	struct NukeDispenser 
	{	
		unsigned int bombs; 
		float range; 
		unsigned int maxCapacity;
	};

	struct Cannon
	{
		float offset;
		float aimLeadScaler;
		std::chrono::milliseconds fireRate;
		std::chrono::high_resolution_clock::time_point prevFireTime;
	};

	struct PeaShooter
	{
		float offset;
		float range;
		std::chrono::milliseconds fireRate;
		std::chrono::high_resolution_clock::time_point prevFireTime;
	};

	struct BomberTrap
	{
		float offset;
		std::chrono::milliseconds fireRate;
		std::chrono::high_resolution_clock::time_point prevFireTime;
	};

#pragma endregion

#pragma region Pickup Behavior

	struct CaptureInfo { bool captured; flecs::entity captor; };

#pragma endregion

};

#endif