//#include "BulletData.h"
//#include "../Components/Identification.h"
//#include "../Components/Visuals.h"
//#include "../Components/Physics.h"
//#include "Prefabs.h"
//#include "../Components/Gameplay.h"
//#include "../Components/AudioSource.h"
//
//bool GOG::BulletData::Load(	std::shared_ptr<flecs::world> _flecsWorld,
//							std::weak_ptr<const GameConfig> _gameConfig,
//							AudioData& _audioData)
//{
//	// Grab init settings for players
//	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();
//
//	// Create prefab for lazer weapon
//	// color
//	float red = (*readCfg).at("Lazers").at("red").as<float>();
//	float green = (*readCfg).at("Lazers").at("green").as<float>();
//	float blue = (*readCfg).at("Lazers").at("blue").as<float>();
//	// other attributes
//	float speed = (*readCfg).at("Lazers").at("speed").as<float>();
//	float xscale = (*readCfg).at("Lazers").at("xscale").as<float>();
//	float yscale = (*readCfg).at("Lazers").at("yscale").as<float>();
//	float zscale = (*readCfg).at("Lazers").at("zscale").as<float>();
//	float dmg = (*readCfg).at("Lazers").at("damage").as<float>();
//	float pcount = (*readCfg).at("Lazers").at("projectiles").as<float>();
//	float frate = (*readCfg).at("Lazers").at("firerate").as<float>();
//	std::string fireFX = (*readCfg).at("Lazers").at("fireFX").as<std::string>();
//
//	GW::AUDIO::GSound* fireFXClip = &_audioData.sounds[fireFX];
//	fireFXClip->SetVolume(0.025f);
//
//	// default projectile scale
//	GW::MATH::GMATRIXF world;
//	GW::MATH::GMatrix::ScaleLocalF(	GW::MATH::GIdentityMatrixF,
//									GW::MATH::GVECTORF{ xscale, yscale, zscale }, 
//									world);
//
//	// add prefab to ECS
//	auto lazerPrefab = _flecsWorld->prefab()
//		// .set<> in a prefab means components are shared (instanced)
//		.set<Material>({ red, green, blue })
//		.set<Acceleration>({ 0, 0 })
//		.set<Velocity>({ speed, 0 })
//		.set<SoundClips>({ { {"Shoot", fireFXClip}}})
//		.set_override<Transform>({ world })
//		.set_override<Damage>({ dmg })
//		//.set_override<ChargedShot>({ 2 })
//		.override<Bullet>() // Tag this prefab as a bullet (for queries/systems)
//		.override<Collidable>(); // can be collided with
//
//	//// register this prefab by name so other systems can use it
//	//RegisterPrefab("Lazer Bullet", lazerPrefab);
//
//	return true;
//}
//
//bool GOG::BulletData::Unload(std::shared_ptr<flecs::world> _game)
//{
//	// remove all bullets and their prefabs
//	_game->defer_begin(); // required when removing while iterating!
//	_game->each([](flecs::entity _entity, Bullet&) 
//	{
//		_entity.destruct(); // destroy this entitiy (happens at frame end)
//	});
//	_game->defer_end(); // required when removing while iterating!
//
//	// unregister this prefab by name
//	UnregisterPrefab("Lazer Bullet");
//
//	return true;
//}
