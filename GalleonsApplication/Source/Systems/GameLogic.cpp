#include "GameLogic.h"

bool GOG::GameLogic::Init(
	std::shared_ptr<flecs::world> _game,
	DirectX11Renderer* _d3d11RenderingSystem,
	GW::SYSTEM::GWindow _win,
	std::weak_ptr<GameConfig> _gameConfig,
	GW::AUDIO::GAudio* _audioEngine,
	AudioData* _audioData,
	GW::CORE::GEventGenerator _eventPusher)
{
	flecsWorld = _game;
	flecsWorldAsync = flecsWorld->async_stage();
	flecsWorldLock.Create();

	d3d11RenderingSystem = _d3d11RenderingSystem;
	window = _win;
	audioEngine = _audioEngine;
	audioData = _audioData;
	gameConfig = _gameConfig;
	eventPusher = _eventPusher;

	wasEscapePressed = false;
	wasPausePressed = false;
	wasPausePressed = false;
	wasCreditsPressed = false;

	/*keyboardMouseInput.Create(window);
	gamePads.Create();*/
	
	currState = GAME_STATES::LOGO_SCREEN;

	LoadHighScores(_gameConfig);

	struct MergeAsyncStages {}; // local definition so we control iteration counts
	flecsWorld->entity("MergeAsyncStages").add<MergeAsyncStages>();
	// only happens once per frame at the very start of the frame
	flecsWorld->system<MergeAsyncStages>()
		.kind(flecs::OnLoad) // first defined phase
		.each([this](flecs::entity _entity, MergeAsyncStages& _mergeAsyncChanges)
			{
				// merge any waiting changes from the last frame that happened on other threads
				flecsWorldLock.LockSyncWrite();
				flecsWorldAsync.merge();
				flecsWorldLock.UnlockSyncWrite();
			});

	if (InitAudio() == false)
		return false;
	if (InitInput() == false)
		return false;
	if (InitEvents() == false)
		return false;

	return true;
}

bool GOG::GameLogic::InitInput()
{
	if (-gamePads.Create())
		return false;
	if (-keyboardMouseInput.Create(window))
		return false;
	if (-bufferedInput.Create(window))
		return false;
	return true;
}

bool GOG::GameLogic::InitEvents()
{
	onEvent.Create([this](const GW::GEvent& _event)
		{
			PLAY_EVENT eventTag;
			PLAY_EVENT_DATA data;

			if (+_event.Read(eventTag, data))
			{
				if (eventTag == PLAY_EVENT::GAME_OVER)
				{
					gameOverFX->Play();
					PlayMusic(nullptr);
					PauseSystems();
					currState = GAME_STATES::GAME_OVER_SCREEN;
				}
			}
		}); 
	eventPusher.Register(onEvent);

	return true;
}

bool GOG::GameLogic::InitAudio()
{
	std::shared_ptr<const GameConfig> readCfg = gameConfig.lock();

	std::string menuMusicName = (*readCfg).at("Game").at("menuMusic").as<std::string>();
	std::string creditsMusicName = (*readCfg).at("Game").at("creditsMusic").as<std::string>();
	std::string gameMusicName = (*readCfg).at("Game").at("gameMusic").as<std::string>();
	float menuMusicVolume = (*readCfg).at("Game").at("menuMusicVolume").as<float>();
	float creditsMusicVolume = (*readCfg).at("Game").at("creditsMusicVolume").as<float>();
	float gameMusicVolume = (*readCfg).at("Game").at("gameMusicVolume").as<float>();

	menuMusic = audioData->music[menuMusicName];
	creditsMusic = audioData->music[creditsMusicName];
	gameMusic = audioData->music[gameMusicName];

	menuMusic.SetVolume(menuMusicVolume);
	creditsMusic.SetVolume(creditsMusicVolume);
	gameMusic.SetVolume(gameMusicVolume);

	std::string pauseFXName = (*readCfg).at("UI").at("pauseFX").as<std::string>();
	std::string menuClickFXName = (*readCfg).at("UI").at("menuClickFX").as<std::string>();
	std::string gameOverFXName = (*readCfg).at("Game").at("gameOverFX").as<std::string>();
	float pauseVolume = (*readCfg).at("UI").at("pauseVolume").as<float>();
	float menuClickVolume = (*readCfg).at("UI").at("menuClickVolume").as<float>();
	float gameOverVolume = (*readCfg).at("Game").at("gameOverVolume").as<float>();

	pauseFX = audioData->CreateSound(pauseFXName, pauseVolume);
	menuClickFX = audioData->CreateSound(menuClickFXName, menuClickVolume);
	gameOverFX = audioData->CreateSound(gameOverFXName, gameOverVolume);

	return true;
}

void GOG::GameLogic::PlayMusic(GW::AUDIO::GMusic* _music)
{
	menuMusic.Stop();
	creditsMusic.Stop();
	gameMusic.Stop();
	gameOverMusic.Stop();

	if (_music != nullptr)
		_music->Play(true);
}

void GOG::GameLogic::CheckInput()
{
	// Received input
	float pauseInput = 0, escInput = 0, enterInput = 0, cInput = 0;
	// Calculated input values.
	float pauseValue = 0, escValue = 0, enterValue = 0, cValue = 0;
	keyboardMouseInput.GetState(G_KEY_P, pauseInput); pauseValue += pauseInput;
	keyboardMouseInput.GetState(G_KEY_ESCAPE, escInput); escValue += escInput;
	keyboardMouseInput.GetState(G_KEY_ENTER, enterInput); enterValue += enterInput;
	keyboardMouseInput.GetState(G_KEY_C, cInput); cValue += cInput;
	gamePads.GetState(0, G_START_BTN, pauseInput); pauseValue += pauseInput;
	gamePads.GetState(0, G_EAST_BTN, escInput); escValue += escInput;
	gamePads.GetState(0, G_SOUTH_BTN, enterInput); enterValue += enterInput;
	gamePads.GetState(0, G_SELECT_BTN, cInput); cValue += cInput;

	auto timer = std::chrono::system_clock::now().time_since_epoch();
	unsigned int now = std::chrono::duration_cast<std::chrono::milliseconds>(timer).count();

	switch (currState)
	{
		case GAME_STATES::LOGO_SCREEN:
		{
			if (splashScreenStart == -1)
				splashScreenStart = now;

			if ((enterValue && !wasEnterPressed) || (now - splashScreenStart > splashScreenTime))
			{
				if (enterValue)
					menuClickFX->Play();
				splashScreenStart = now;
				FadeInEvent();
				currState = GAME_STATES::DIRECTX_SCREEN;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::DIRECTX_SCREEN);
			}
			break;
		}
		case GAME_STATES::DIRECTX_SCREEN:
		{
			if ((enterValue && !wasEnterPressed) || (now - splashScreenStart > splashScreenTime))
			{
				if (enterValue)
					menuClickFX->Play();
				splashScreenStart = now;
				FadeInEvent();
				currState = GAME_STATES::GATEWARE_SCREEN;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::GATEWARE_SCREEN);
			}
			break;
		}
		case GAME_STATES::GATEWARE_SCREEN:
		{
			if ((enterValue && !wasEnterPressed) || (now - splashScreenStart > splashScreenTime))
			{
				if (enterValue)
					menuClickFX->Play();
				splashScreenStart = now;
				FadeInEvent();
				currState = GAME_STATES::FLECS_SCREEN;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::FLECS_SCREEN);
			}
			break;
		}
		case GAME_STATES::FLECS_SCREEN:
		{
			if ((enterValue && !wasEnterPressed) || (now - splashScreenStart > splashScreenTime))
			{
				if (enterValue)
					menuClickFX->Play();
				splashScreenStart = now;
				FadeInEvent();
				currState = GAME_STATES::TITLE_SCREEN;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::TITLE_SCREEN);
			}
			break;
		}
		case GAME_STATES::TITLE_SCREEN:
		{
			if ((enterValue && !wasEnterPressed) || (now - splashScreenStart > splashScreenTime))
			{
				if (enterValue)
					menuClickFX->Play();
				PlayMusic(&menuMusic);
				FadeInEvent();
				currState = GAME_STATES::MAIN_MENU;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::MAIN_MENU);
			}
			break;
		}
		case GAME_STATES::MAIN_MENU:
		{
			if (enterValue && !wasEnterPressed)
			{
				menuClickFX->Play();
				PlayMusic(&gameMusic);
				FadeInEvent();
				GameplayStart();
				currState = GAME_STATES::PLAY_GAME;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::PLAY_GAME);
			}

			if (cValue && !wasCreditsPressed)
			{
				pauseFX->Play();
				PlayMusic(&creditsMusic);
				FadeInEvent();
				currState = GAME_STATES::CREDITS;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::CREDITS);
			}

			break;
		}
		case GAME_STATES::PLAY_GAME:
		{
			if (pauseValue && !wasPausePressed)
			{
				pauseFX->Play();
				PauseSystems();
				currState = GAME_STATES::PAUSE_GAME;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::PAUSE_GAME);
			}

			break;
		}
		case GAME_STATES::PAUSE_GAME:
		{
			if ((enterValue && !wasEnterPressed) || (pauseValue && !wasPausePressed))
			{
				menuClickFX->Play();
				PlaySystems();
				currState = GAME_STATES::PLAY_GAME;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::PLAY_GAME);
			}

			if (escValue && !wasEscapePressed)
			{
				menuClickFX->Play();
				PlayMusic(&menuMusic);
				currState = GAME_STATES::MAIN_MENU;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::MAIN_MENU);
				GameplayStop();
			}

			break;
		}
		case GAME_STATES::GAME_OVER_SCREEN:
		{
			if (enterValue && !wasEnterPressed)
			{
				menuClickFX->Play();
				PlayMusic(&gameMusic);
				GameplayStop();
				FadeInEvent();
				GameplayStart();
				currState = GAME_STATES::PLAY_GAME;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::PLAY_GAME);
				
			}

			if (escValue && !wasEscapePressed)
			{
				menuClickFX->Play();
				PlayMusic(&menuMusic);
				currState = GAME_STATES::MAIN_MENU;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::MAIN_MENU);
				GameplayStop();
			}

			break;
		}
		case GAME_STATES::HIGH_SCORES:
			break;
		case GAME_STATES::CREDITS:
		{
			if (escValue && !wasEscapePressed)
			{
				menuClickFX->Play();
				PlayMusic(&menuMusic);
				FadeInEvent();
				currState = GAME_STATES::MAIN_MENU;
				d3d11RenderingSystem->UpdateGameState(GAME_STATES::MAIN_MENU);
			}

			break;
		}
			
		default:
			break;
	}

	if (pauseValue)
		wasPausePressed = true;
	else
		wasPausePressed = false;

	if (escValue)
		wasEscapePressed = true;
	else
		wasEscapePressed = false;

	if (enterValue)
		wasEnterPressed = true;
	else
		wasEnterPressed = false;

	if (cValue)
		wasCreditsPressed = true;
	else
		wasCreditsPressed = false;
}

bool::GOG::GameLogic::GameplayStart()
{
	if (systemsInitialized)
	{
		PlaySystems();
		levelLogic.Reset();
		cameraLogic.Reset();
		return true;
	}

	if (playerLogic.Init(flecsWorld,
		gameConfig,
		keyboardMouseInput,
		gamePads,
		*audioEngine,
		*eventPusher) == false)
		return false;
	if (levelLogic.Init(flecsWorld, gameConfig, *audioData, *eventPusher) == false)
		return false;
	if (physicsLogic.Init(flecsWorld, gameConfig, *eventPusher) == false)
		return false;
	if (lazerLogic.Init(flecsWorld, gameConfig) == false)
		return false;
	if (missileLogic.Init(flecsWorld, gameConfig) == false)
		return false;
	if (trapLogic.Init(flecsWorld, gameConfig) == false)
		return false;
	if (enemyLogic.Init(flecsWorld, gameConfig, *eventPusher) == false)
		return false;
	if (pickupLogic.Init(flecsWorld, gameConfig, *eventPusher) == false)
		return false;
	if (cameraLogic.Init(flecsWorld, gameConfig, d3d11RenderingSystem) == false)
		return false;

	systemsInitialized = true;

	return true;
}

void GOG::GameLogic::GameplayStop()
{
	PauseSystems();

	std::shared_ptr<const GameConfig> readCfg = gameConfig.lock();

	unsigned int defaultLives = readCfg->at("PlayerPrefab_1").at("lives").as<unsigned int>();
	d3d11RenderingSystem->UpdateStats(defaultLives, 0, 0, 1);

	flecsWorldLock.LockSyncWrite();

	UpdateHighScores(gameConfig, flecsWorld->entity("Persistent Player Stats").get<Score>()->value);

	flecsWorldAsync.entity("Persistent Player Stats")
		.set<Lives>({ defaultLives })
		.set<Score>({ 0 })
		.set<NukeDispenser>(
			{
				0,
				readCfg->at("NukeDispenser").at("range").as<float>(),
				readCfg->at("NukeDispenser").at("maxCapacity").as<unsigned int>()
			});


	flecsWorldAsync.each([this](flecs::entity _entity, Transform&)
		{
			if (!_entity.has<Camera>())
				_entity.destruct();
		});

	flecsWorldLock.UnlockSyncWrite();

}

void GOG::GameLogic::PauseSystems()
{
	playerLogic.Activate(false);	
	levelLogic.Activate(false);
	physicsLogic.Activate(false);	
	lazerLogic.Activate(false);	
	missileLogic.Activate(false);
	trapLogic.Activate(false);
	enemyLogic.Activate(false);
	pickupLogic.Activate(false);
	cameraLogic.Activate(false);
}

void GOG::GameLogic::PlaySystems()
{
	playerLogic.Activate(true);
	levelLogic.Activate(true);
	physicsLogic.Activate(true);
	lazerLogic.Activate(true);
	missileLogic.Activate(true);
	trapLogic.Activate(true);
	enemyLogic.Activate(true);
	pickupLogic.Activate(true);
	cameraLogic.Activate(true);
}

void GOG::GameLogic::LoadHighScores(std::weak_ptr<const GameConfig> _gameConfig)
{
	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();

	unsigned int highScoreCount = (readCfg->at("HighScores").at("highScoreCount").as<unsigned int>());

	for (int i = 0; i < highScoreCount; i++)
	{
		highScores.push_back((readCfg->at("HighScores").at("highScore" + std::to_string(i)).as<unsigned int>()));
	}
}

void GOG::GameLogic::UpdateHighScores(std::weak_ptr<GameConfig> _gameConfig, unsigned int newScore)
{
	std::shared_ptr<GameConfig> readCfg = _gameConfig.lock();

	bool isNewHighScore = false;

	for (int i = 0; i < highScores.size(); i++)
	{
		if (newScore > highScores[i])
		{
			isNewHighScore = true;
			highScores.insert(highScores.begin() + i, newScore);
			break;
		}
	}

	if (isNewHighScore)
	{
		highScores.pop_back();

		for (int i = 0; i < highScores.size(); i++)
		{
			(*readCfg)["HighScores"]["highScore" + std::to_string(i)] = highScores[i];
		}
	}
}

void GOG::GameLogic::FadeInEvent()
{
	GW::GEvent stateChanged;
	PLAY_EVENT_DATA data;
	data.value = 0;
	stateChanged.Write(PLAY_EVENT::STATE_CHANGED, data);
	eventPusher.Push(stateChanged);
}

bool GOG::GameLogic::Shutdown()
{
	if (playerLogic.Shutdown() == false)
		return false;
	if (levelLogic.Shutdown() == false)
		return false;
	if (physicsLogic.Shutdown() == false)
		return false;	
	if (lazerLogic.Shutdown() == false)
		return false;
	if (missileLogic.Shutdown() == false)
		return false;
	if (trapLogic.Shutdown() == false)
		return false;
	if (enemyLogic.Shutdown() == false)
		return false;
	if (pickupLogic.Shutdown() == false)
		return false;
	if (cameraLogic.Shutdown() == false)
		return false;

	flecsWorld->entity("MergeAsyncStages").destruct();

	return true;
}