#include "TrapLogic.h"
#include "../Components/Gameplay.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"

using namespace GOG;
using namespace GW;
using namespace MATH;

bool TrapLogic::Init(std::shared_ptr<flecs::world> _game,
	std::weak_ptr<const GameConfig> _gameConfig)
{
	// save a handle to the ECS & game settings
	flecsWorld = _game;
	gameConfig = _gameConfig;

	TrapSystem = flecsWorld->system<const Trap, Transform>("TrapSystem")
		.each([](flecs::entity _entity, const Trap&, Transform& _transform)
			{
				GVECTORF translate{ 0, _entity.delta_time(),  0, 0 };
				GMatrix::TranslateLocalF(_transform.value, translate, _transform.value);
			});

	return true;
}

// Free any resources used to run this system
bool TrapLogic::Shutdown()
{
	TrapSystem.destruct();
	// invalidate the shared pointers
	flecsWorld.reset();
	gameConfig.reset();
	return true;
}

// Toggle if a system's Logic is actively running
bool TrapLogic::Activate(bool _runSystem)
{
	if (_runSystem)
	{
		TrapSystem.enable();
	}
	else
	{
		TrapSystem.disable();
	}
	return false;
}