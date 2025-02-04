#include "PhysicsLogic.h"

#include "../Events/Playevents.h"

#include "../Components/AudioSource.h"

#include "../Utils/Macros.h"

using namespace flecs;
using namespace GOG;
using namespace GW;
using namespace MATH;
using namespace CORE;

bool GOG::PhysicsLogic::Init(	std::shared_ptr<world> _game, 
								std::weak_ptr<const GameConfig> _gameConfig,
								GEventGenerator _eventPusher)
{
	flecsWorld = _game;
	gameConfig = _gameConfig;
	eventPusher = _eventPusher;

	std::shared_ptr<const GameConfig> readCfg = gameConfig.lock();
	float projectileCullDist = readCfg->at("Game").at("projectileCullDist").as<float>();
	float worldBottomBoundry = readCfg->at("Game").at("worldBottomBoundry").as<float>();
	float worldTopBoundry = readCfg->at("Game").at("worldTopBoundry").as<float>();
	float worldWidth = readCfg->at("Game").at("worldWidth").as<float>();

#pragma region Shared Queries

	playerTransformQuery = flecsWorld->query<const Player, Transform>();
	enemyTransformQuery = flecsWorld->query<const Enemy, Transform>();
	projectileTransformQuery = flecsWorld->query<const Projectile, const Transform>();
	pickupTransformQuery = flecsWorld->query<const Pickup, Transform>();
	persistentStatsQuery = flecsWorld->query<const PersistentStats, Lives>();

#pragma endregion

	// **** MOVEMENT ****
	// update velocity by acceleration
	flecsWorld->system<Velocity, const Acceleration>("Acceleration System")
		.each([](entity e, Velocity& v, const Acceleration &a) 
		{
			GW::MATH::GVECTORF accel;
			GW::MATH::GVector::ScaleF(a.value, e.delta_time(), accel);
			GW::MATH::GVector::AddVectorF(accel, v.value, v.value);
		});
	// update position by velocity
	flecsWorld->system<Transform, BoundBox, const Velocity>("Translation System")
		.each([](entity _entity, Transform& _transform, BoundBox& _box, const Velocity& _velocity) 
		{
			GW::MATH::GVECTORF speed;
			GW::MATH::GVector::ScaleF(_velocity.value, _entity.delta_time(), speed);
			// adding is simple but doesn't account for orientation
			GW::MATH::GVector::AddVectorF(speed, _transform.value.row4, _transform.value.row4);
			// Update box collider positions to match transform positions.
			_box.collider.center = _transform.value.row4;
		});

#pragma region Update Collider Positions

	updateColliderPos = flecsWorld->system<const Transform, BoundBox>().each(
		[](entity _entity, const Transform& _transform, BoundBox& _box)
		{
			_box.collider.center = _transform.value.row4;
		});

#pragma endregion

#pragma region Out of Bounds Culling

	struct OutBoundsCulling {};
	flecsWorld->entity("OutBoundsCulling").add<OutBoundsCulling>();
	flecsWorld->system<OutBoundsCulling>().each([this, projectileCullDist, worldTopBoundry](OutBoundsCulling& _s)
	{
		// Will crash if player is not alive. Protect against this.
		if (playerTransformQuery.first().is_alive())
			playerPos = playerTransformQuery.first().get<Transform>()->value.row4;
		else
			return;

		projectileTransformQuery.each(
			[this, projectileCullDist](entity _projectile, const Projectile&, const Transform& _transform)
			{
				float projectile_x = _transform.value.row4.x, projectile_y = _transform.value.row4.y;
				float distanceFromPlayer = DISTANCE_2D(playerPos.x, playerPos.y, projectile_x, projectile_y);
				if (distanceFromPlayer > projectileCullDist)
					_projectile.destruct();
			});

		enemyTransformQuery.each([this, worldTopBoundry](entity _enemy, const Enemy&, const Transform& _transform)
		{
			if (_transform.value.row4.y > worldTopBoundry + 10)
			{
				EnemyOutBounds(_enemy);
			}
		});

		pickupTransformQuery.each([this, worldTopBoundry](entity _pickup, const Pickup&, const Transform& _transform)
		{
			if (_transform.value.row4.y > worldTopBoundry + 5)
			{
				_pickup.destruct();
			}
		});
	});

#pragma endregion

#pragma region World Boundaries

	struct WorldBoundrySystem{};
	flecsWorld->entity("WorldBoundrySystem").add<WorldBoundrySystem>();
	flecsWorld->system<WorldBoundrySystem>().each(
		[this, worldBottomBoundry, worldTopBoundry, worldWidth](WorldBoundrySystem& _s)  
		{
			playerTransformQuery.each(
				[this, worldBottomBoundry, worldTopBoundry, worldWidth](const Player&, Transform& _transform)
				{
					if (_transform.value.row4.y < worldBottomBoundry)
						_transform.value.row4.y = worldBottomBoundry;
					else if (_transform.value.row4.y > worldTopBoundry)
						_transform.value.row4.y = worldTopBoundry;
				});
			enemyTransformQuery.each(
				[this, worldBottomBoundry, worldTopBoundry, worldWidth](const Enemy&, Transform& _transform)
				{
					if (_transform.value.row4.y < worldBottomBoundry)
						_transform.value.row4.y = worldBottomBoundry;

					if (_transform.value.row4.x < playerPos.x - worldWidth)
						_transform.value.row4.x = playerPos.x + worldWidth;
					else if (_transform.value.row4.x > playerPos.x + worldWidth)
						_transform.value.row4.x = playerPos.x - worldWidth;
				});
			pickupTransformQuery.each(
				[this, worldBottomBoundry, worldWidth](const Pickup&, Transform& _transform)
				{
					if (_transform.value.row4.y < worldBottomBoundry)
						_transform.value.row4.y = worldBottomBoundry;

					if (_transform.value.row4.x < playerPos.x - worldWidth)
						_transform.value.row4.x = playerPos.x + worldWidth;
					else if (_transform.value.row4.x > playerPos.x + worldWidth)
						_transform.value.row4.x = playerPos.x - worldWidth;
				});
		});

#pragma endregion

#pragma region Collision Detection

	collidersQuery = flecsWorld->query<Collidable, const BoundBox>();
	struct CollisionSystem {};
	flecsWorld->entity("CollisionSystem").add<CollisionSystem>();
	flecsWorld->system<CollisionSystem>().each([this](CollisionSystem& _s)
	{
		collidersQuery.each([this](entity _entity, Collidable& _collidable, const BoundBox& _box)
		{
			Collider curCollider;
			curCollider.owner = _entity;
			curCollider.box = _box;
			colliders.push_back(curCollider);
		});

		for (int i = 0; i < colliders.size(); i += 1)
		{
			for (int j = i + 1; j < colliders.size(); j += 1)
			{
				GReturn returnCode;
				GCollision::GCollisionCheck collisionCheck{};
				returnCode = GCollision::TestOBBToOBBF(colliders[i].box.collider, colliders[j].box.collider, collisionCheck);
				if (collisionCheck == GCollision::GCollisionCheck::COLLISION)
				{
					// Projectiles hit enemies
					if (colliders[i].owner.has<Projectile>() && colliders[j].owner.has<Enemy>())
					{
						// Prohibit friendly fire
						if (colliders[i].owner.get<Sender>()->entityType == SENDER::ENEMY)
							continue;
						colliders[i].owner.destruct();
						EnemyDestroyed(colliders[j].owner);
						continue;
					}
					if (colliders[i].owner.has<Enemy>() && colliders[j].owner.has<Projectile>())
					{
						// Prohibit friendly fire
						if (colliders[j].owner.get<Sender>()->entityType == SENDER::ENEMY)
							continue;
						colliders[j].owner.destruct();
						EnemyDestroyed(colliders[i].owner);
						continue;
					}

					// Projectiles hit players
					if (colliders[i].owner.has<Projectile>() && colliders[j].owner.has<Player>())
					{
						// Prohibit friendly fire
						if (colliders[i].owner.get<Sender>()->entityType == SENDER::PLAYER)
							continue;
						colliders[i].owner.destruct();
						PlayerDestroyed(colliders[j].owner);
						continue;
					}
					if (colliders[i].owner.has<Player>() && colliders[j].owner.has<Projectile>())
					{
						// Prohibit friendly fire
						if (colliders[j].owner.get<Sender>()->entityType == SENDER::PLAYER)
							continue;
						colliders[j].owner.destruct();
						PlayerDestroyed(colliders[i].owner);
						continue;
					}

					// Player hits enemies
					if (colliders[i].owner.has<Player>() && colliders[j].owner.has<Enemy>())
					{
						PlayerDestroyed(colliders[i].owner);
						EnemyDestroyed(colliders[j].owner);
						continue;
					}
					else if (colliders[i].owner.has<Enemy>() && colliders[j].owner.has<Player>())
					{
						PlayerDestroyed(colliders[j].owner);
						EnemyDestroyed(colliders[i].owner);
						continue;
					}

					unsigned int curBombs = persistentStatsQuery.first().get<NukeDispenser>()->bombs;
					unsigned int maxBombs = persistentStatsQuery.first().get<NukeDispenser>()->maxCapacity;
					if (curBombs < maxBombs)
					{
						// Player collides with pickup
						if (colliders[i].owner.has<Player>() && colliders[j].owner.has<Pickup>())
						{
							if (colliders[j].owner.has<Civilian>() == true)
								continue;
							GetPickup(colliders[j].owner);
							continue;
						}
						else if (colliders[i].owner.has<Pickup>() && colliders[j].owner.has<Player>())
						{
							if (colliders[i].owner.has<Civilian>() == true)
								continue;
							GetPickup(colliders[i].owner);
							continue;
						}
					}

					// Lander collides with civilian
					if (colliders[i].owner.has<Lander>() && colliders[j].owner.has<Civilian>())
					{
						// If that civilian is already captured, then ignore it.
						if (colliders[j].owner.get<CaptureInfo>()->captured)
							continue;

						// Tell the lander they are capturing.
						colliders[i].owner.add<Capturing>();

						// Tell the civilian they are captured.
						CaptureInfo captured{ true, colliders[i].owner };
						colliders[j].owner.set<CaptureInfo>({ captured });

						continue;
					}
					else if (colliders[i].owner.has<Civilian>() && colliders[j].owner.has<Lander>())
					{
						// If that civilian is already captured, then ignore it.
						if (colliders[i].owner.get<CaptureInfo>()->captured)
							continue;

						// Tell the lander they are capturing.
						colliders[j].owner.add<Capturing>();

						// Tell the civilian they are captured.
						CaptureInfo captured{ true, colliders[j].owner };
						colliders[i].owner.set<CaptureInfo>({ captured });

						continue;
					}
				}
			}
		}

		colliders.clear();
	});

#pragma endregion

	return true;
}

void PhysicsLogic::PlayerDestroyed(entity& _entity)
{
	LoopingClips loopingClips = *_entity.get<LoopingClips>();
	loopingClips.sounds["Accel"].clip->SetVolume(0);
	loopingClips.sounds["Accel"].clip->Stop();
	SoundClips clips = *_entity.get<SoundClips>();
	clips.sounds["Death"]->Play();

	_entity.destruct();

	GEvent playerDestroyed;
	PLAY_EVENT_DATA livesData;
	livesData.value = persistentStatsQuery.first().get<Lives>()->count - 1;
	persistentStatsQuery.first().set<Lives>({ livesData.value });
	playerDestroyed.Write(PLAY_EVENT::PLAYER_DESTROYED, livesData);
	eventPusher.Push(playerDestroyed);
}

void PhysicsLogic::EnemyDestroyed(entity& _entity)
{
	SoundClips clips = *_entity.get<SoundClips>();
	clips.sounds["Death"]->Play();

	GW::GEvent enemyDestroyed;
	PLAY_EVENT_DATA scoreData;
	scoreData.value = _entity.get<Score>()->value;
	scoreData.directive = DIRECTIVES::UPDATE_SCORE_OK;
	enemyDestroyed.Write(PLAY_EVENT::ENEMY_DESTROYED, scoreData);
	eventPusher.Push(enemyDestroyed);
	_entity.destruct();
}

void PhysicsLogic::EnemyOutBounds(entity& _entity)
{
	SoundClips clips = *_entity.get<SoundClips>();
	clips.sounds["Death"]->Play();

	GW::GEvent onEnemyOutBounds;
	PLAY_EVENT_DATA nullData;
	onEnemyOutBounds.Write(PLAY_EVENT::ENEMY_DESTROYED, nullData);
	eventPusher.Push(onEnemyOutBounds);
	_entity.destruct();
}

void PhysicsLogic::GetPickup(flecs::entity _entity)
{
	SoundClips clips = *_entity.get<SoundClips>();
	clips.sounds["Pickup"]->Play();
	persistentStatsQuery.first().get_mut<NukeDispenser>()->bombs += 1;

	GW::GEvent onPickup;
	PLAY_EVENT_DATA pickupData;
	pickupData.value = persistentStatsQuery.first().get<NukeDispenser>()->bombs;
	onPickup.Write(GOG::PLAY_EVENT::SMART_BOMB_GRABBED, pickupData);
	eventPusher.Push(onPickup);
	_entity.destruct();
}

bool GOG::PhysicsLogic::Activate(bool _runSystem)
{
	if (_runSystem)
	{
		flecsWorld->entity("Acceleration System").enable();
		flecsWorld->entity("Translation System").enable();
		updateColliderPos.enable();
	}
	else 
	{
		flecsWorld->entity("Acceleration System").disable();
		flecsWorld->entity("Translation System").disable();
		updateColliderPos.disable();
	}

	return true;
}

bool GOG::PhysicsLogic::Shutdown()
{
	flecsWorld->entity("Acceleration System").destruct();
	flecsWorld->entity("Translation System").destruct();
	flecsWorld->entity("OutBoundsCulling").destruct();
	flecsWorld->entity("WorldBoundrySystem").destruct();
	flecsWorld->entity("CollisionSystem").destruct();
	updateColliderPos.destruct();
	collidersQuery.destruct();
	enemyTransformQuery.destruct();
	projectileTransformQuery.destruct();
	pickupTransformQuery.destruct();
	persistentStatsQuery.destruct();

	return true;
}
