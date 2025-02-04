#include "EnemyLogic.h"

#include "../Entities/Prefabs.h"

#include "../Components/AudioSource.h"

#include "../Events/Playevents.h"

#include "../Utils/Macros.h"
#include "../Utils/SharedActorMethods.h"

using namespace GOG;
using namespace flecs;
using namespace GW;
using namespace MATH;
using namespace MATH2D;

// Connects logic to traverse any players and allow a controller to manipulate them
bool EnemyLogic::Init(	std::shared_ptr<world> _flecsWorld,
							std::weak_ptr<const GameConfig> _gameConfig,
							CORE::GEventGenerator _eventPusher)
{
	// save a handle to the ECS & game settings
	flecsWorld = _flecsWorld;
	gameConfig = _gameConfig;
	eventPusher = _eventPusher;
	playerMovementQuery = flecsWorld->query<const Player, const Transform, const Velocity>();
	baiterQuery = flecsWorld->query<const Baiter, const BaiterMovementStats, SpeedBoost, Transform, FlipInfo, Cannon>();
	civiQuery = flecsWorld->query<const Civilian, CaptureInfo, const Transform>();

#pragma region SharedEntityValues

	std::random_device rd;
	std::mt19937 gen(rd());

	std::uniform_real_distribution<float> dirDist(-1, 1);

#pragma endregion

#pragma region Bomber
	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();

	bomberQuery = flecsWorld->query<const Bomber, Transform, Velocity, FlipInfo, BomberTrap>();
	float speedBomber = readCfg->at("EnemyPrefab_1").at("speed").as<float>();
	float topBound = readCfg->at("Game").at("worldTopBoundry").as<float>();
	float bottomBound = readCfg->at("Game").at("worldBottomBoundry").as<float>();

	struct BomberSystem {};
	flecsWorld->entity("BomberSystem").add<BomberSystem>();
	flecsWorld->system<BomberSystem>().each([this, gen, dirDist, speedBomber, bottomBound, topBound](BomberSystem& _b)
		{

			bomberQuery.each(
				[this, gen, dirDist, speedBomber, bottomBound, topBound]
				(entity _entity, const Bomber& _bomber, Transform& _transform, 
				Velocity& _velocity, FlipInfo& _flipInfo, BomberTrap& _bomberTrap)
				{
					BomberMovement(_entity.delta_time(), speedBomber, _entity, _transform, _velocity, _flipInfo, bottomBound, topBound);
					SpawnBomberTrap(_transform, _velocity, _bomberTrap);
				});
		});

#pragma endregion

#pragma region Baiter

	struct BaiterSystem {};
	flecsWorld->entity("BaiterSystem").add<BaiterSystem>();
	flecsWorld->system<BaiterSystem>().each(
		[this](BaiterSystem& _s)
		{
			if (playerMovementQuery.first().is_alive())
			{
				playerPos_x = playerMovementQuery.first().get<Transform>()->value.row4.x;
				playerPos_y = playerMovementQuery.first().get<Transform>()->value.row4.y;
				playerVelocity_x = playerMovementQuery.first().get<Velocity>()->value.x;
				playerVelocity_y = playerMovementQuery.first().get<Velocity>()->value.y;
			}
			else
				return;

			baiterQuery.each(
				[this]
				(entity _entity, const Baiter& _baiter, const BaiterMovementStats& _movementStats, SpeedBoost& _speedBoost,
				Transform& _transform, FlipInfo& _flipInfo, Cannon& _missileLauncher)
				{
					BaiterMovement(_entity.delta_time(), _movementStats, _speedBoost, _transform, _flipInfo);
					FireCannon(_transform, _missileLauncher);
				});
		});


#pragma endregion

#pragma region Lander

	landerSystem = flecsWorld->system<const Lander, Transform, const Speed, PeaShooter>().each(
		[this]
		(entity _lander, const Lander&, Transform& _landerTransform, const Speed& _speed, PeaShooter& _peaShooter)
		{
			if (playerMovementQuery.first().is_alive())
			{
				playerPos_x = playerMovementQuery.first().get<Transform>()->value.row4.x;
				playerPos_y = playerMovementQuery.first().get<Transform>()->value.row4.y;
				playerVelocity_x = playerMovementQuery.first().get<Velocity>()->value.x;
				playerVelocity_y = playerMovementQuery.first().get<Velocity>()->value.y;
			}

			LanderMovement(_lander, _landerTransform, _speed);

			float landerPos_x = _landerTransform.value.row4.x, landerPos_y = _landerTransform.value.row4.y;
			float distToPlayer = DISTANCE_2D(landerPos_x, landerPos_y, playerPos_x, playerPos_y);
			if (distToPlayer < _peaShooter.range)
			{
				std::chrono::milliseconds timeSinceLastPea;
				timeSinceLastPea = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::high_resolution_clock::now() - _peaShooter.prevFireTime);
				if (timeSinceLastPea < _peaShooter.fireRate)
					return;
				_peaShooter.prevFireTime = std::chrono::high_resolution_clock::now();

				entity pea{};
				std::string peaPrefab = "ProjectilePrefab_" + std::to_string(PROJECTILE_TYPE::PEA);
				if (RetreivePrefab(peaPrefab.c_str(), pea))
				{
					float delta_x = playerPos_x - _landerTransform.value.row4.x;
					float delta_y = playerPos_y - _landerTransform.value.row4.y;

					// Rotate the projectile towards target.

					float targetAngleRad = atan2(delta_y, delta_x);
					GMATRIXF transform{ pea.get<Transform>()->value };
					transform.row4 = _landerTransform.value.row4;
					GMatrix::RotateZGlobalF(transform,
											targetAngleRad,
											transform);

					// Offset the missile spawn in the target direction.

					GVECTORF startOffset{ delta_x, delta_y, 0, 1 };
					GVector::NormalizeF(startOffset, startOffset);
					GVector::ScaleF(startOffset, _peaShooter.offset, startOffset);
					GMatrix::TranslateGlobalF(transform, startOffset, transform);

					flecsWorld->entity().is_a(pea)
						.add<Projectile>()
						.add<Pea>()
						.add<Alive>()
						.add<Collidable>()
						.set<Sender>({ SENDER::ENEMY })
						.set<Transform>({ transform });
				}
			}
		});

#pragma endregion

	return true;
}

void EnemyLogic::BaiterMovement(float _deltaTime,
								const BaiterMovementStats& _movementStats,
								SpeedBoost& _speedBoost,
								Transform& _transform,
								FlipInfo& _flipInfo)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	float dirMoving = 0;

	if (_speedBoost.isBoosting)
	{
		switch (_speedBoost.isMovingRight)
		{
			case false:
			{
				_transform.value.row4.x -= (_speedBoost.speed * _deltaTime);
				break;
			}
			case true:
			{
				_transform.value.row4.x += (_speedBoost.speed * _deltaTime);
				break;
			}
		}

		dirMoving = (playerPos_x < _transform.value.row4.x ? -1 : 1);
		SharedActorMethods::FlipEntity(_deltaTime, dirMoving, _transform, _flipInfo);

		/* Check if the baiter has been boosting for its alloted duration, and if it has, then generate new
		boost stats, and stop the boost. */

		std::chrono::nanoseconds timeSinceLastBoostNano;
		timeSinceLastBoostNano = std::chrono::high_resolution_clock::now() - _speedBoost.lastBoost;
		std::chrono::milliseconds timeSinceLastBoostMilli;
		timeSinceLastBoostMilli = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceLastBoostNano);
		if (timeSinceLastBoostMilli > _speedBoost.curDuration)
		{
			_speedBoost.lastBoost = std::chrono::high_resolution_clock::now();
			std::uniform_int_distribution<int> intervalRange(_speedBoost.intervalMin.count(),
				_speedBoost.intervalMax.count());
			_speedBoost.curInterval = std::chrono::milliseconds(intervalRange(gen));
			std::uniform_int_distribution<int> boostDurationRange(_speedBoost.durationMin.count(),
				_speedBoost.durationMax.count());
			_speedBoost.curDuration = std::chrono::milliseconds(boostDurationRange(gen));
			_speedBoost.isBoosting = false;
		}

		return;
	}

	/* Check how long it has been since the baiter's last boost. If enough time has passed, initiate a boost. */

	std::chrono::nanoseconds timeSinceLastBoostNano;
	timeSinceLastBoostNano = std::chrono::high_resolution_clock::now() - _speedBoost.lastBoost;
	std::chrono::milliseconds timeSinceLastBoostMilli;
	timeSinceLastBoostMilli = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceLastBoostNano);
	if (timeSinceLastBoostMilli > _speedBoost.curInterval)
	{
		_speedBoost.isBoosting = true;
		std::uniform_int_distribution<int> leftRight(0, 1);
		_speedBoost.isMovingRight = leftRight(gen);
		_speedBoost.lastBoost = std::chrono::high_resolution_clock::now();
		return;
	}

	/* Regular movement. If the baiter is not withing an acceptable range of the player, then move towards the player
	on the x and y axis at a steady pace. */

	float distFromPlayer_x = abs(playerPos_x) - abs(_transform.value.row4.x);
	float distFromPlayer_y = abs(playerPos_y) - abs(_transform.value.row4.y);
	if (abs(distFromPlayer_x) > _movementStats.followDistance)
	{
		if (playerPos_x < _transform.value.row4.x)
			_transform.value.row4.x -= (_movementStats.speed * _deltaTime);
		else
			_transform.value.row4.x += (_movementStats.speed * _deltaTime);
		dirMoving = (playerPos_x < _transform.value.row4.x ? -1 : 1);
	}
	if (abs(distFromPlayer_y) > _movementStats.followDistance)
	{
		if (playerPos_y < _transform.value.row4.y)
			_transform.value.row4.y -= (_movementStats.speed * _deltaTime);
		else
			_transform.value.row4.y += (_movementStats.speed * _deltaTime);
	}
	
	SharedActorMethods::FlipEntity(_deltaTime, dirMoving, _transform, _flipInfo);
}

void EnemyLogic::LanderMovement(entity _lander, Transform& _landerTransform, const Speed& _speed)
{
	// Check if lander currently has a civilian captured.
	switch (_lander.has<Capturing>())
	{
		// If not, then go find one.
		case false:
		{
			unsigned int civisCaptured = 0;
			float closestCivi = FLT_MAX;
			civiQuery.each(
				[&](entity _civi, const Civilian&, CaptureInfo _captureInfo, const Transform& _civiTransform)
				{
					// Don't want to chase a civi that is already captured.
					if (_captureInfo.captured == true)
					{
						civisCaptured += 1;
						return;
					}

					GVECTOR2F landerPos{ _landerTransform.value.row4.x, _landerTransform.value.row4.y };
					GVECTOR2F civiPos{ _civiTransform.value.row4.x, _civiTransform.value.row4.y };
					float distFromCivi = DISTANCE_2D(landerPos.x, landerPos.y, civiPos.x, civiPos.y);
					if (distFromCivi <= closestCivi)
					{
						closestCivi = distFromCivi;
						float delta_x = civiPos.x - landerPos.x;
						float delta_y = civiPos.y - landerPos.y;
						GVECTORF towardsCivi{ delta_x, delta_y, 0, 0 };
						GVector::NormalizeF(towardsCivi, towardsCivi);
						float magnitude = _lander.delta_time() * _speed.value;
						GVector::ScaleF(towardsCivi, magnitude, towardsCivi);
						GMatrix::TranslateGlobalF(_landerTransform.value, towardsCivi, _landerTransform.value);
					}
				});

			// If all the civis were taken, then fly towards the player instead.
			if (civisCaptured == civiQuery.count())
			{
				if (playerPos_x < _landerTransform.value.row4.x)
					_landerTransform.value.row4.x -= _lander.delta_time() * _speed.value;
				else
					_landerTransform.value.row4.x += _lander.delta_time() * _speed.value;
			}

			break;
		}
		// If the lander does have a civilian, then just fly straight up with it.
		case true:
		{
			GVECTORF up{ 0, 1, 0, 0 };
			GVector::NormalizeF(up, up);
			float magnitude = _lander.delta_time() * _speed.value;
			GVector::ScaleF(up, magnitude, up);
			GMatrix::TranslateGlobalF(_landerTransform.value, up, _landerTransform.value);
			break;
		}
	}
}

void EnemyLogic::BomberMovement(float _deltaTime, float _speed,
								flecs::entity _entity, Transform& _transform,
								Velocity& _velocity, FlipInfo& _flipInfo,
								float _bottom, float _top)
{
	float DirMoving = 0;
	if (_velocity.value.x < 0)
	{
		DirMoving = -1;
	}
	if(_velocity.value.x > 0)
	{
		DirMoving = 1;
	}

	if (_transform.value.row4.y <= _bottom)
	{
		_velocity.value.y = -_velocity.value.y;
	}

	if (_transform.value.row4.y >= _top)
	{
		_velocity.value.y = -_velocity.value.y;
	}

	SharedActorMethods::FlipEntity(_deltaTime, DirMoving, _transform, _flipInfo);
}

void EnemyLogic::FireCannon(Transform& _enemyTransform, Cannon& _cannon)
{
	std::chrono::milliseconds timeSinceLastCannonball;
	timeSinceLastCannonball = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - _cannon.prevFireTime);
	if (timeSinceLastCannonball < _cannon.fireRate)
		return;
	_cannon.prevFireTime = std::chrono::high_resolution_clock::now();

	entity cannonBall{};
	std::string cannonballPrefab = "ProjectilePrefab_" + std::to_string(PROJECTILE_TYPE::CANNONBALL);
	if (RetreivePrefab(cannonballPrefab.c_str(), cannonBall))
	{
		/* Add an offset to how we're measuring the player's position based off its velocity, so that the
		Baiter can lead the player with its shots. */

		float delta_x = (playerPos_x + playerVelocity_x * _cannon.aimLeadScaler) - _enemyTransform.value.row4.x;
		float delta_y = (playerPos_y + playerVelocity_y * _cannon.aimLeadScaler) - _enemyTransform.value.row4.y;

		// Rotate the projectile towards target.

		float targetAngleRad = atan2(delta_y, delta_x);
		GMATRIXF transform{ cannonBall.get<Transform>()->value };
		transform.row4 = _enemyTransform.value.row4;
		GMatrix::RotateZGlobalF(transform,
								targetAngleRad,
								transform);

		// Offset the missile spawn in the target direction.

		GVECTORF startOffset{ delta_x, delta_y, 0, 1 };
		GVector::NormalizeF(startOffset, startOffset);
		GVector::ScaleF(startOffset, _cannon.offset, startOffset);
		GMatrix::TranslateGlobalF(transform, startOffset, transform);

		flecsWorld->entity().is_a(cannonBall)
			.add<Projectile>()
			.add<Cannonball>()
			.add<Alive>()
			.add<Collidable>()
			.set<Sender>({ SENDER::ENEMY })
			.set<Transform>({ transform });
	}

	SoundClips clips = *cannonBall.get<SoundClips>();
	clips.sounds["Shoot"]->Play();
}

void GOG::EnemyLogic::SpawnBomberTrap(Transform& _transform, Velocity& _velocity, BomberTrap& _trap)
{
	std::chrono::milliseconds sinceLastTrap = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - _trap.prevFireTime);
	if (sinceLastTrap < _trap.fireRate)
		return;
	_trap.prevFireTime = std::chrono::high_resolution_clock::now();

	entity trap{};
	std::string trapPrefab = "ProjectilePrefab_" + std::to_string(PROJECTILE_TYPE::TRAP);

	if (RetreivePrefab(trapPrefab.c_str(), trap))
	{
		flecsWorld->entity().is_a(trap)
			.add<Projectile>()
			.add<Trap>()
			.add<Alive>()
			.add<Collidable>()
			.set<Sender>({ SENDER::ENEMY })
			.set<Transform>({ _transform })
			.set_override<Velocity>({ -_velocity.value.x, -_velocity.value.y });
	}

	SoundClips clips = *trap.get<SoundClips>();
	clips.sounds["Shoot"]->Play();
}

// Free any resources used to run this system
bool EnemyLogic::Shutdown()
{
	flecsWorld->entity("BaiterSystem").destruct();
	flecsWorld->entity("BomberSystem").destruct();
	landerSystem.destruct();
	playerMovementQuery.destruct();
	baiterQuery.destruct();
	civiQuery.destruct();
	
	// invalidate the shared pointers
	flecsWorld.reset();
	gameConfig.reset();
	return true;
}

// Toggle if a system's Logic is actively running
bool EnemyLogic::Activate(bool runSystem)
{
	if (runSystem)
	{
		landerSystem.enable();
		flecsWorld->entity("BaiterSystem").enable();
		flecsWorld->entity("BomberSystem").enable();
	}
	else
	{
		landerSystem.disable();
		flecsWorld->entity("BaiterSystem").disable();
		flecsWorld->entity("BomberSystem").disable();
	}

	return false;
}
