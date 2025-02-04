// The player system is responsible for allowing control over the main ship(s)
#ifndef PLAYERLOGIC_H
#define PLAYERLOGIC_H

// Contains our global game settings
#include "../GameConfig.h"

#include "../Components/AudioSource.h"
#include "../Components/Gameplay.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"

// example space game (avoid name collisions)
namespace GOG 
{
	class PlayerLogic 
	{
	private:
		std::shared_ptr<flecs::world> flecsWorld;
		std::weak_ptr<const GameConfig> gameConfig;
		// handle to our running ECS system
		flecs::system playerControllerSystem;
		//flecs::system healthSystem;
		// permananent handles input systems
		GW::INPUT::GInput keyboardMouseInput;
		GW::INPUT::GController gamePadInput;
		//GW::INPUT::GBufferedInput bufferedInput;

		GW::AUDIO::GAudio audioEngine;
		// key press event cache (saves input events)
		// we choose cache over responder here for better ECS compatibility
		//GW::CORE::GEventCache pressEvents;

		GW::CORE::GEventGenerator eventPusher;
		GW::CORE::GEventResponder eventResponder;

		flecs::query<const PersistentStats, Score> scoreQuery;
		flecs::query<const Player, const Transform> playerQuery;
		flecs::query<const Player, const PersistentStats, Lives, Score, NukeDispenser> persistentStatsQuery;
		flecs::query<const Enemy, const Transform, const Score, SoundClips> enemyQuery;
		flecs::query<const Projectile, const Transform> projectileQuery;


		void HandleMovementInput(	float _xAxis,
									float _yAxis,
									flecs::entity _entity,
									Acceleration& _acceleration,
									Velocity& _velocity,
									Transform& _transform,
									PlayerMoveInfo& _movementStats,
									FlipInfo& _flipInfo);
		void HandleAttackInput(	flecs::entity _entity,
								Transform& _playerTransform,
								FlipInfo& _flipInfo,
								const float _fireInput,
								const float _smartBombInput,
								unsigned int _controller);
		GW::MATH::GMATRIXF OrientProjectile(bool _playerIsFacingRight, 
											GW::MATH::GVECTORF _playerPos,
											GW::MATH::GMATRIXF _projectileTransform,
											float _projectileOffset);

	public:

		// attach the required logic to the ECS 
		bool Init(	std::shared_ptr<flecs::world> _game,
					std::weak_ptr<const GameConfig> _gameConfig,
					GW::INPUT::GInput _keyboardMouseInput,
					GW::INPUT::GController _gamePadInput,
					//GW::INPUT::GBufferedInput _bufferedInput,
					GW::AUDIO::GAudio _audioEngine,
					GW::CORE::GEventGenerator _eventPusher);
		// control if the system is actively running
		bool Activate(bool runSystem);
		// release any resources allocated by the system
		bool Shutdown(); 
	private:
		// how big the input cache can be each frame
		static constexpr unsigned int Max_Frame_Events = 32;
		// helper routines
		//bool ProcessInputEvents(flecs::iter _i, unsigned int _entityIndex);
		//bool FireLasers(flecs::world& _flecsWorld, Transform _origin);
	};

};

#endif