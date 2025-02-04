#ifndef GAMELOGIC_H
#define GAMELOGIC_H

#include "../Systems/PlayerLogic.h"
#include "../Systems/EnemyLogic.h"
#include "../Systems/LazerLogic.h"
#include "../Systems/LevelLogic.h"
#include "../Systems/PhysicsLogic.h"
#include "../Systems/CameraLogic.h"
#include "../Systems/PickupLogic.h"
#include "../Systems/Renderer.h"
#include "../Systems/MissileLogic.h"
#include "../Systems/TrapLogic.h"


namespace GOG
{
	class GameLogic
	{
		GW::INPUT::GController gamePads; // controller support
		GW::INPUT::GInput keyboardMouseInput; // twitch keybaord/mouse
		GW::INPUT::GBufferedInput bufferedInput; // event keyboard/mouse

		GW::AUDIO::GAudio* audioEngine; // can create music & sound effects
		AudioData* audioData;
		
		GW::CORE::GEventResponder onEvent;
		GW::CORE::GEventGenerator eventPusher;
		std::weak_ptr<GameConfig> gameConfig; // .ini file game settings

		GOG::EnemyLogic enemyLogic;
		GOG::LazerLogic lazerLogic;
		GOG::MissileLogic missileLogic;
		GOG::TrapLogic trapLogic;
		GOG::PickupLogic pickupLogic;
		GOG::LevelLogic levelLogic;
		GOG::PhysicsLogic physicsLogic;
		GOG::PlayerLogic playerLogic;
		GOG::CameraLogic cameraLogic;

		std::shared_ptr<flecs::world> flecsWorld;
		DirectX11Renderer* d3d11RenderingSystem;

		flecs::world flecsWorldAsync;
		GW::CORE::GThreadShared flecsWorldLock;

		bool systemsInitialized = false;

		unsigned int splashScreenTime = 4000;
		unsigned int splashScreenStart = -1;
	
		bool wasEnterPressed;
		bool wasEscapePressed;
		bool wasPausePressed;
		bool wasCreditsPressed;
		unsigned int currState;
		GW::SYSTEM::GWindow window;

		GW::AUDIO::GSound* pauseFX;
		GW::AUDIO::GSound* menuClickFX;
		GW::AUDIO::GSound* gameOverFX;

		GW::AUDIO::GMusic menuMusic;
		GW::AUDIO::GMusic creditsMusic;
		GW::AUDIO::GMusic gameMusic;
		GW::AUDIO::GMusic gameOverMusic;

	public:

		std::vector<unsigned int> highScores;

		bool Init(std::shared_ptr<flecs::world> _game,
			DirectX11Renderer* _d3d11RenderingSystem,
			GW::SYSTEM::GWindow _win,
			std::weak_ptr<GameConfig> _gameConfig,
			GW::AUDIO::GAudio* _audioEngine,
			AudioData* _audioData,
			GW::CORE::GEventGenerator _eventPusher);

		void LoadHighScores(std::weak_ptr<const GameConfig> _gameConfig);
		void UpdateHighScores(std::weak_ptr<GameConfig> _gameConfig, unsigned int newScore);
		void CheckInput();	
		bool Shutdown();

	private:
		void PauseSystems();
		void PlaySystems();
		bool GameplayStart();
		void GameplayStop();
		bool InitInput();
		bool InitEvents();
		bool InitAudio();
		void PlayMusic(GW::AUDIO::GMusic* _music);
		void FadeInEvent();
	};
};

#endif