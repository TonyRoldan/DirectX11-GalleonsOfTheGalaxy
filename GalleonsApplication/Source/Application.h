#ifndef APPLICATION_H
#define APPLICATION_H

// include events
#include "Events/Playevents.h"
// Contains our global game settings
#include "GameConfig.h"

#include "Utils/AudioData.h"
#include "Utils/ActorData.h"
#include "Utils/LevelData.h"

// Load all entities+prefabs used by the game 

#include "Entities/PlayerData.h"
#include "Entities/EnemyData.h"
#include "Entities/ProjectileData.h"
#include "Entities/PickupData.h"

// Include all systems used by the game and their associated components
#include "Systems/Renderer.h"

#include "Systems/GameLogic.h" // must be included after all other systems

// Allocates and runs all sub-systems essential to operating the game
class Application 
{
	// gateware libs used to access operating system
	GW::SYSTEM::GWindow window; // gateware multi-platform window
	
	GW::GRAPHICS::GDirectX11Surface d3d11;
	GW::CORE::GEventResponder messages;
	// third-party gameplay & utility libraries
	std::shared_ptr<flecs::world> flecsWorld; // ECS database for gameplay
	std::shared_ptr<flecs::world> uiWorld;
	std::shared_ptr<GameConfig> gameConfig; // .ini file game settings
	GW::SYSTEM::GLog log;
	// ECS Entities and Prefabs that need to be loaded

	GOG::EnemyData	enemyData;
	GOG::PlayerData playerData;
	GOG::ProjectileData projectileData;
	GOG::PickupData pickupData;

	// specific ECS systems used to run the game
	GOG::DirectX11Renderer d3d11RenderingSystem;
	GOG::GameLogic gameLogic;

	// EventGenerator for Game Events
	GW::CORE::GEventGenerator eventPusher;
	GW::AUDIO::GAudio audioEngine; // can create music & sound effects
	AudioData audioData;

	std::unique_ptr<ActorData>	actorData;
	std::unique_ptr<LevelData>	levelData;

public:
	bool Init();
	bool Run();
	bool Shutdown();

private:
	bool InitWindow();
	bool InitGraphics(ActorData* _actorData, LevelData* _levelData);
	bool InitActorPrefabs(ActorData* _actorData);
	bool InitAudio(GW::SYSTEM::GLog _log);
	bool InitSystems();
	bool GameLoop();
};




#endif 