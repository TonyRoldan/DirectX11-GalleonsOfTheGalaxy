#ifndef CAMERA_LOGIC_H
#define CAMERA_LOGIC_H
#include "../GameConfig.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"
#include "../Systems/Renderer.h"
#include "../Components/Gameplay.h"

namespace GOG
{
	class CameraLogic
	{
		std::shared_ptr<flecs::world> flecsWorld;

		std::weak_ptr<const GameConfig> gameConfig;

		DirectX11Renderer* renderer;

		flecs::system movementSystem;

		flecs::query<Player, Transform, FlipInfo> queryCache;

		float smoothing;
		float zOffset;

		float leadDistance;
		bool isFacingRight;
		bool isSwitchingDir;

		int switchDirTime;
		int switchDirStart;

		GW::MATH::GMATRIXF defaultPosition;

	public:
		bool Init(std::shared_ptr<flecs::world> _flecsWorld,
			std::weak_ptr<const GameConfig> _gameConfig, DirectX11Renderer* _renderer);
		void Reset();
		bool Activate(bool runSystem);
		bool Shutdown();
	};
}
#endif