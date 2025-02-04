#include "LazerLogic.h"
#include "../Components/Gameplay.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"

using namespace GOG;
using namespace flecs;
using namespace GW;
using namespace MATH;

bool LazerLogic::Init(	std::shared_ptr<flecs::world> _game,
							std::weak_ptr<const GameConfig> _gameConfig)
{
	/* This should be a projectile logic class where all the systems for projectiles live. Right now the missile
	logic is outside of this class. Should be moved here in refactor. */

	// save a handle to the ECS & game settings
	flecsWorld = _game;
	gameConfig = _gameConfig;

	flecsWorld->system<Lazer, Transform, Speed>("LazerSystem")
		.iter([](flecs::iter _it, Lazer*, Transform* _transform, Speed* _speed) 
		{
			for (auto i : _it)
			{
				if (_it.entity(i).is_alive())
				{
					GVECTORF translate{ _speed[i].value * _it.delta_time(), 0,  0, 0 };
					GMatrix::TranslateLocalF(_transform[i].value, translate, _transform[i].value);
				}
			}
		});

	peaSystem = flecsWorld->system<const Pea, Transform, const Speed>().each(
		[](entity _pea, const Pea&, Transform& _transform, const Speed& _speed)
		{
			GVECTORF translate{ _speed.value * _pea.delta_time(), 0,  0, 0 };
			GMatrix::TranslateLocalF(_transform.value, translate, _transform.value);
		});

	return true;
}

// Free any resources used to run this system
bool LazerLogic::Shutdown()
{
	flecsWorld->entity("LazerSystem").destruct();
	// invalidate the shared pointers
	flecsWorld.reset();
	gameConfig.reset();
	return true;
}

// Toggle if a system's Logic is actively running
bool LazerLogic::Activate(bool _runSystem)
{
	if (_runSystem)
	{
		flecsWorld->entity("LazerSystem").enable();
		peaSystem.enable();
	}
	else
	{
		flecsWorld->entity("LazerSystem").disable();
		peaSystem.disable();
	}
	return false;
}
