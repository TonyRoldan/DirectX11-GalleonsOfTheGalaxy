#include "MissileLogic.h"
#include "../Components/Gameplay.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"

using namespace GOG;
using namespace GW;
using namespace MATH;

bool MissileLogic::Init(std::shared_ptr<flecs::world> _game,
						std::weak_ptr<const GameConfig> _gameConfig)
{
	// save a handle to the ECS & game settings
	flecsWorld = _game;
	gameConfig = _gameConfig;

	missileSystem = flecsWorld->system<const Cannonball, Transform, const Speed>("MissileSystem")
		.each([](flecs::entity _entity, const Cannonball&, Transform& _transform, const Speed& _speed)
		{
			GVECTORF translate{ _speed.value * _entity.delta_time(), 0,   0, 0 };
			GMatrix::TranslateLocalF(_transform.value, translate, _transform.value);
		});

	return true;
}

// Free any resources used to run this system
bool MissileLogic::Shutdown()
{
	missileSystem.destruct();
	// invalidate the shared pointers
	flecsWorld.reset();
	gameConfig.reset();
	return true;
}

// Toggle if a system's Logic is actively running
bool MissileLogic::Activate(bool _runSystem)
{
	if (_runSystem)
	{
		missileSystem.enable();
	}
	else
	{
		missileSystem.disable();
	}
	return false;
}
