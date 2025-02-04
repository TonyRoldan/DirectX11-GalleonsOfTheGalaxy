#include "PlayerLogic.h"

#include "../Entities/Prefabs.h"

#include "../Components/Visuals.h"

#include "../Events/Playevents.h"

#include "../Utils/Macros.h"
#include "../Utils/SharedActorMethods.h"

using namespace GOG;
using namespace flecs;
using namespace GW;
using namespace INPUT;
using namespace AUDIO;
using namespace CORE;
using namespace MATH;
using namespace MATH2D;

// Connects logic to traverse any players and allow a controller to manipulate them
bool PlayerLogic::Init(std::shared_ptr<world> _flecsWorld,
	std::weak_ptr<const GameConfig> _gameConfig,
	GInput _keyboardMouseInput,
	GController _gamePadInput,
	GAudio _audioEngine,
	GEventGenerator _eventPusher)
{
	// Save handles to the ECS & game settings.

	flecsWorld = _flecsWorld;
	gameConfig = _gameConfig;
	keyboardMouseInput = _keyboardMouseInput;
	gamePadInput = _gamePadInput;
	audioEngine = _audioEngine;
	eventPusher = _eventPusher;

#pragma region Shared Queries

	playerQuery = flecsWorld->query<const Player, const Transform>();
	persistentStatsQuery = flecsWorld->query<const Player, const PersistentStats, Lives, Score, NukeDispenser>();
	enemyQuery = flecsWorld->query<const Enemy, const Transform, const Score, SoundClips>();
	projectileQuery = flecsWorld->query<const Projectile, const Transform>();

#pragma endregion

#pragma region Player Controller System

	playerControllerSystem = flecsWorld->system<Player, ControllerID, Transform, Acceleration, Velocity,
												PlayerMoveInfo, FlipInfo>().each(
			[this](entity _player, Player&, ControllerID& _controller, Transform& _transform, Acceleration& _accel,
			Velocity& _velocity, PlayerMoveInfo& _moveInfo, FlipInfo& _flipInfo)
			{
				// Received inputs
				float inputX = 0, inputY = 0, inputProjectile = 0, inputSmartBomb = 0;
				// Calculated input values.
				float xAxis = 0, yAxis = 0, inputProjectileAdditive = 0, inputSmartBombAdditive = 0;

				// Use the controller/keyboard to move the player around the screen
				if (_controller.index == 0)
				{
					bool isControllerConnected;
					gamePadInput.IsConnected(_controller.index, isControllerConnected);

					// Movement controls.
					if (isControllerConnected)
					{
						gamePadInput.GetState(_controller.index, G_LX_AXIS, inputX); xAxis += inputX;
						gamePadInput.GetState(_controller.index, G_LY_AXIS, inputY); yAxis += inputY;
					}
					keyboardMouseInput.GetState(G_KEY_LEFT, inputX); xAxis -= inputX;
					keyboardMouseInput.GetState(G_KEY_RIGHT, inputX); xAxis += inputX;
					keyboardMouseInput.GetState(G_KEY_UP, inputY); yAxis += inputY;
					keyboardMouseInput.GetState(G_KEY_DOWN, inputY); yAxis -= inputY;

					// Attack controls.
					if (isControllerConnected)
					{
						gamePadInput.GetState(_controller.index, G_RIGHT_TRIGGER_AXIS, inputProjectile);
						gamePadInput.GetState(_controller.index, G_LEFT_TRIGGER_AXIS, inputSmartBomb);
					}
					inputProjectileAdditive += inputProjectile;
					inputSmartBombAdditive += inputSmartBomb;
					keyboardMouseInput.GetState(G_KEY_SPACE, inputProjectile);
					keyboardMouseInput.GetState(G_KEY_B, inputSmartBomb);
					inputProjectileAdditive += inputProjectile;
					inputSmartBombAdditive += inputSmartBomb;
				}

				HandleMovementInput(xAxis, yAxis, _player, _accel, _velocity, _transform, _moveInfo, _flipInfo);
				HandleAttackInput(_player, _transform, _flipInfo, inputProjectileAdditive, inputSmartBombAdditive,
					_controller.index);
			});

#pragma endregion

#pragma region Event Responder

	scoreQuery = flecsWorld->query<const PersistentStats, Score>();

	eventResponder.Create([this](const GEvent& _event)
	{
		PLAY_EVENT event;
		PLAY_EVENT_DATA data;

		if (-_event.Read(event, data))
			return;

		switch (event)
		{
		case PLAY_EVENT::PLAYER_DESTROYED:
			{
				// Play lazer haptics.
				GEvent activatedPlayerDeathHaptic;
				PLAY_EVENT_DATA hapticData;
				hapticData.value = HAPTIC_TYPE::PLAYER_DEATH;
				hapticData.directive = 0;
				activatedPlayerDeathHaptic.Write(PLAY_EVENT::HAPTICS_ACTIVATED, hapticData);
				eventPusher.Push(activatedPlayerDeathHaptic);

				break;
			}
			case PLAY_EVENT::ENEMY_DESTROYED:
			{
				if (data.directive == UPDATE_SCORE_OK)
				{
					if (scoreQuery.count() > 0)
					{
						unsigned curScore = scoreQuery.first().get<Score>()->value;
						data.value += curScore;
						scoreQuery.first().set<Score>({ data.value });

						GEvent updateScore;
						updateScore.Write(PLAY_EVENT::UPDATE_SCORE, data);
						eventPusher.Push(updateScore);
					}
				}

				break;
			}
			case PLAY_EVENT::CIVILIAN_DESTROYED:
			{
				if (scoreQuery.count() > 0)
				{
					unsigned curScore = scoreQuery.first().get<Score>()->value;
					data.value += curScore;
					scoreQuery.first().set<Score>({ data.value });

					GEvent updateScore;
					updateScore.Write(PLAY_EVENT::UPDATE_SCORE, data);
					eventPusher.Push(updateScore);
				}

				break;
			}
			case PLAY_EVENT::SMART_BOMB_ACTIVATED:
			{
				if (playerQuery.first().is_alive())
				{
					playerQuery.first().remove<ChargingSmartBomb>();
					persistentStatsQuery.first().get_mut<NukeDispenser>()->bombs = data.value;
				}

				SoundClips clips = *playerQuery.first().get<SoundClips>();
				clips.sounds["SmartBomb"]->Play();
				break;
			}
			case PLAY_EVENT::HAPTICS_ACTIVATED:
			{
				if (playerQuery.count() <= 0)
					return;

				float pan = 0;
				float duration = 0;
				float strength = 0;

				switch (data.value)
				{
					case HAPTIC_TYPE::FIRE_LAZER:
					{
						pan = playerQuery.first().get<PlayerHaptics>()->lazerPan;
						duration = playerQuery.first().get<PlayerHaptics>()->lazerDuration;
						strength = playerQuery.first().get<PlayerHaptics>()->lazerStrength;

						break;
					}
					case HAPTIC_TYPE::DETONATE_SMART_BOMB:
					{
						pan = playerQuery.first().get<PlayerHaptics>()->smartBombPan;
						duration = playerQuery.first().get<PlayerHaptics>()->smartBombDuration;
						strength = playerQuery.first().get<PlayerHaptics>()->smartBombStrength;

						break;
					}
					case HAPTIC_TYPE::PLAYER_DEATH:
					{
						pan = playerQuery.first().get<PlayerHaptics>()->playerDeathPan;
						duration = playerQuery.first().get<PlayerHaptics>()->playerDeathDuration;
						strength = playerQuery.first().get<PlayerHaptics>()->playerDeathStrength;

						break;
					}
					default:
					{
						break;
					}
				}

				gamePadInput.StartVibration(data.directive, pan, duration, strength);

				break;
			}
			default:
			{
				break;
			}
		}
	});
	eventPusher.Register(eventResponder);

#pragma endregion

	return true;
}

#pragma region Movement Input

void PlayerLogic::HandleMovementInput(	float _xAxis,
										float _yAxis,
										entity _player,
										Acceleration& _acceleration,
										Velocity& _velocity,
										Transform& _transform,
										PlayerMoveInfo& _movementStats,
										FlipInfo& _flipInfo)
{
	// accelerate on player input
	_acceleration.value = { _xAxis, _yAxis };

	if (SIGN(_acceleration.value.x) == SIGN(_velocity.value.x))
		_acceleration.value.x *= _movementStats.hAccelRate;
	else												  
		_acceleration.value.x *= _movementStats.hFlipAccelRate;

	if (SIGN(_acceleration.value.y) == SIGN(_velocity.value.y))
		_acceleration.value.y *= _movementStats.vAccelRate;
	else												  
		_acceleration.value.y *= _movementStats.vFlipAccelRate;

	// deccelerate in absence of player input
	if (_xAxis == 0)
	{
		_acceleration.value.x = -_velocity.value.x
			* _movementStats.hDeccelRate;
	}
	if (_yAxis == 0)
	{
		_acceleration.value.y = -_velocity.value.y
			* _movementStats.vDeccelRate;
	}

	// Clamps velocity to not go over the player's max speed
	if (abs(_velocity.value.x) > _movementStats.hMaxSpeed)
	{
		_velocity.value.x = _movementStats.hMaxSpeed * SIGN(_velocity.value.x);
	}
	if (abs(_velocity.value.y) > _movementStats.vMaxSpeed)
	{
		_velocity.value.y = _movementStats.vMaxSpeed * SIGN(_velocity.value.y);
	}

	// Play acceleration sound
	if (_xAxis != 0 || _yAxis != 0)
	{
		LoopingClips clips = *_player.get<LoopingClips>();
		clips.sounds["Accel"].clip->SetVolume(clips.sounds["Accel"].volume);
		bool isPlaying; 
		clips.sounds["Accel"].clip->isPlaying(isPlaying);
		if (!isPlaying)
			clips.sounds["Accel"].clip->Play(true);
	}
	else {
		LoopingClips clips = *_player.get<LoopingClips>();
		clips.sounds["Accel"].clip->SetVolume(0);
		clips.sounds["Accel"].clip->Stop();
	}

	// Flip the ship
	SharedActorMethods::FlipEntity(	_player.delta_time(),
									_xAxis,
									_transform,
									_flipInfo);
}

#pragma endregion

#pragma region Attacking

void PlayerLogic::HandleAttackInput(	entity _player,
										Transform& _playerTransform,
										FlipInfo& _flipInfo,
										const float _projectileInput,
										const float _smartBombInput,
										unsigned int _controller)
{
	ChargeInfo* chargeInfo = _player.get_mut<ChargeInfo>();
	// Check if we are pressing or releasing the fire button.
	if (_projectileInput != 0 && _player.has<Charging>() == false)
	{
		// Start charging shot.
		_player.add<Charging>();
		chargeInfo->chargeStart = std::chrono::high_resolution_clock::now();
		//std::cout << "Charging shot!\n\n";
	}
	else if (_projectileInput == 0 && _player.has<Charging>())
	{
		_player.remove<Charging>();
		// Check if shot has charged long enough for a power shot.
		/*chargeInfo->chargeEnd = std::chrono::high_resolution_clock::now();
		auto elapsedNanos = chargeInfo->chargeEnd - chargeInfo->chargeStart;
		auto chargeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(elapsedNanos);*/

		//if (chargeElapsed >= chargeInfo->timeTillFullyCharged)
		//{
		//	// This is where we would fire a missile.
		//	std::cout << "Firing power shot!\n\n";
		//}
		//else
		//{
		//	// This is where we would fire a lazer.
		//	std::cout << "Firing regular shot!\n\n";
		//}

		std::string prefabName = "ProjectilePrefab_" + std::to_string(PROJECTILE_TYPE::LAZER);
		entity lazer{};
		if (RetreivePrefab(prefabName.c_str(), lazer))
		{
			GMATRIXF t = lazer.get<Transform>()->value;
			GVECTORF v = _playerTransform.value.row4;
			GMATRIXF transform = OrientProjectile(_flipInfo.isFacingRight,
				_playerTransform.value.row4,
				lazer.get<Transform>()->value,
				lazer.get<Offset>()->value);

			// Spawn the lazer.
			flecsWorld->entity().is_a(lazer)
				.add<Lazer>()
				.add<Alive>()
				.add<Collidable>()
				.set<Sender>({ SENDER::PLAYER })
				.set<Transform>({ transform });

			// Play lazer sounds.
			SoundClips clips = *lazer.get<SoundClips>();
			clips.sounds["Shoot"]->Play();

			// Play lazer haptics.
			GEvent activatedLazerHaptic;
			PLAY_EVENT_DATA hapticData;
			hapticData.value = HAPTIC_TYPE::FIRE_LAZER;
			hapticData.directive = _controller;
			activatedLazerHaptic.Write(PLAY_EVENT::HAPTICS_ACTIVATED, hapticData);
			eventPusher.Push(activatedLazerHaptic);
		}
	}
	// Check if we are pressing or releasing the smart bomb button.
	if (_smartBombInput != 0 && _player.has<ChargingSmartBomb>() == false)
	{
		unsigned int bombs = persistentStatsQuery.first().get<NukeDispenser>()->bombs;
		if (bombs > 0)
			_player.add<ChargingSmartBomb>();
	}
	else if (_smartBombInput == 0 && _player.has<ChargingSmartBomb>())
	{
		GVECTOR2F playerPos{};
		unsigned int smartBombCount = 0;
		unsigned int smartBombRange = 0;
		if (playerQuery.first().is_alive())
		{
			playerPos = {	playerQuery.first().get<Transform>()->value.row4.x,
							playerQuery.first().get<Transform>()->value.row4.y };
			smartBombCount = persistentStatsQuery.first().get<NukeDispenser>()->bombs;
			smartBombRange = persistentStatsQuery.first().get<NukeDispenser>()->range;
		}
		else
			return;
		// Destruct every enemy in smart bomb range.
		enemyQuery.each(
			[this, playerPos, smartBombRange]
			(entity _entity, const Enemy&, const Transform& _transform, const Score& _score,
				SoundClips& _sounds)
			{
				GVECTOR2F pos{ _transform.value.row4.x, _transform.value.row4.y };
				float distFromPlayer = DISTANCE_2D(pos.x, pos.y, playerPos.x, playerPos.y);
				if (distFromPlayer < smartBombRange)
				{
					GEvent enemyDestroyed;
					PLAY_EVENT_DATA data;
					data.value = _score.value;
					data.directive = DIRECTIVES::UPDATE_SCORE_OK;
					enemyDestroyed.Write(PLAY_EVENT::ENEMY_DESTROYED, data);
					eventPusher.Push(enemyDestroyed);
					_sounds.sounds["Death"]->Play();
					_entity.destruct();
				}
			});

		projectileQuery.each(
			[this, playerPos, smartBombRange]
			(entity& _entity, const Projectile&, const Transform& _transform)
			{
				GVECTOR2F pos{ _transform.value.row4.x, _transform.value.row4.y };
				float distFromPlayer = DISTANCE_2D(pos.x, pos.y, playerPos.x, playerPos.y);
				if (distFromPlayer < smartBombRange)
				{
					_entity.destruct();
				}
			});

			GEvent activateSmartBomb;
			PLAY_EVENT_DATA data;
			data.value = smartBombCount - 1;
			activateSmartBomb.Write(PLAY_EVENT::SMART_BOMB_ACTIVATED, data);
			eventPusher.Push(activateSmartBomb);

			// Play smart bomb haptics.
			GEvent activatedSmartBombHaptic;
			PLAY_EVENT_DATA hapticData;
			hapticData.value = HAPTIC_TYPE::DETONATE_SMART_BOMB;
			hapticData.directive = _controller;
			activatedSmartBombHaptic.Write(PLAY_EVENT::HAPTICS_ACTIVATED, hapticData);
			eventPusher.Push(activatedSmartBombHaptic);
	}
}

GMATRIXF PlayerLogic::OrientProjectile(	bool _playerIsFacingRight, 
										GVECTORF _playerPos,
										GMATRIXF _projectileTransform,
										float _projectileOffset)
{
	_projectileTransform.row4 = _playerPos;
	float rotDegree;
	float rotRadian;
	GVECTORF offset{ _projectileOffset, 0, 0, 0 };

	if (_playerIsFacingRight)
	{
		GMatrix::TranslateLocalF(_projectileTransform, offset, _projectileTransform);
	}
	else
	{
		rotDegree = 180;
		rotRadian = DEGREE_TO_RADIAN(rotDegree);
		GMatrix::RotateYGlobalF(_projectileTransform, rotRadian, _projectileTransform);
		GMatrix::TranslateLocalF(_projectileTransform, offset, _projectileTransform);
	}

	return _projectileTransform;
}
#pragma endregion

// Free any resources used to run this system
bool PlayerLogic::Shutdown()
{
	playerControllerSystem.destruct();
	flecsWorld.reset();
	gameConfig.reset();

	projectileQuery.destruct();
	playerQuery.destruct();
	persistentStatsQuery.destruct();
	enemyQuery.destruct();
	scoreQuery.destruct();

	return true;
}

// Toggle if a system's Logic is actively running
bool PlayerLogic::Activate(bool _runSystem)
{
	if (playerControllerSystem.is_alive())
	{
		(_runSystem) ? playerControllerSystem.enable() : playerControllerSystem.disable();
		return true;
	}
	return false;
}