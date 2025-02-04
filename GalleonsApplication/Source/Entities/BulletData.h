//// This class creates all types of bullet prefabs
//#ifndef BULLETDATA_H
//#define BULLETDATA_H
//
//// Contains our global game settings
//#include "../GameConfig.h"
//#include "../Utils/AudioData.h"
//
//// example space game (avoid name collisions)
//namespace GOG
//{
//	class BulletData
//	{
//	public:
//		// Load required entities and/or prefabs into the ECS 
//		bool Load(	std::shared_ptr<flecs::world> _flecsWorld,
//					std::weak_ptr<const GameConfig> _gameConfig,
//					AudioData& _audioEngine);
//		// Unload the entities/prefabs from the ECS
//		bool Unload(std::shared_ptr<flecs::world> _flecsWorld);
//	};
//
//};
//
//#endif