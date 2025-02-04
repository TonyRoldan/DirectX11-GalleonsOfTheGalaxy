#include <random>
#include "BulletLogic.h"
#include "../Components/Identification.h"
#include "../Components/Physics.h"
#include "../Components/Gameplay.h"

using namespace GOG; // Example Space Game

// Connects logic to traverse any players and allow a controller to manipulate them
bool GOG::BulletLogic::Init(	std::shared_ptr<flecs::world> _game,
								std::weak_ptr<const GameConfig> _gameConfig)
{
	// save a handle to the ECS & game settings
	game = _game;
	gameConfig = _gameConfig;

	// destroy any bullets that have the CollidedWith relationship
	//game->system<Bullet, Damage>("Bullet System")
	//	.each([](flecs::entity _entity, Bullet, Damage &_damage) 
	//	{
	//		// damage anything we come into contact with
	//		_entity.each<CollidedWith>([&_entity, _damage](flecs::entity _hit) 
	//		{
	//			if (_hit.has<Health>()) 
	//			{
	//				// If the bullet hit its own shooter, don't deal damage to the shooter
	//				if ((_entity.has<Player>() && _hit.has<Player>()) || 
	//					(_entity.has<Enemy>() && _hit.has<Enemy>()))
	//					return;

	//				int current = _hit.get<Health>()->value;
	//				_hit.set<Health>({ current - _damage.value, true });
	//				// reduce the amount of hits but the charged shot
	//				if (_entity.has<ChargedShot>() && _hit.get<Health>()->value <= 0)
	//				{
	//					int damage = _entity.get<ChargedShot>()->chargedDamage;
	//					_entity.set<ChargedShot>({ damage - 1.f, 1.5f, 0, 0 });
	//				}
	//			}
	//		});
	//		// if you have collidedWith realtionship then be destroyed
	//		_entity.each<CollidedWith>([&_entity, _damage](flecs::entity hit) 
	//		{
	//			if ((_entity.has<Player>() && hit.has<Player>()) || 
	//				(_entity.has<Enemy>() && hit.has<Enemy>()))
	//				return;

	//			if (_entity.has<CollidedWith>(flecs::Wildcard)) {

	//				if (_entity.has<ChargedShot>()) {

	//					if (_entity.get<ChargedShot>()->chargedDamage <= 0)
	//						_entity.destruct();
	//				}
	//				else {
	//					// play hit sound
	//					_entity.destruct();
	//				}
	//			}
	//		});
	//	});

	return true;
}

// Free any resources used to run this system
bool GOG::BulletLogic::Shutdown()
{
	//game->entity("Bullet System").destruct();
	// invalidate the shared pointers
	game.reset();
	gameConfig.reset();
	return true;
}

// Toggle if a system's Logic is actively running
bool GOG::BulletLogic::Activate(bool _runSystem)
{
	if (_runSystem) 
	{
		//game->entity("Bullet System").enable();
	}
	else 
	{
		//game->entity("Bullet System").disable();
	}
	return false;
}
