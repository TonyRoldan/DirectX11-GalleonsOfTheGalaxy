#include "Application.h"

// open some Gateware namespaces for conveinence 
// NEVER do this in a header file!
using namespace GW;
using namespace CORE;
using namespace SYSTEM;
using namespace GRAPHICS;

bool Application::Init() 
{
	eventPusher.Create();

	// load all game settigns
	gameConfig = std::make_shared<GameConfig>(); 
	// create the ECS system
	flecsWorld = std::make_shared<flecs::world>();
	uiWorld = std::make_shared<flecs::world>();

	//GW::SYSTEM::GLog log;
	actorData = std::make_unique<ActorData>();
	levelData = std::make_unique<LevelData>();

	if (actorData->LoadActors("../GameModels/ActorModels/Models/", log) == false)
	{
		return false;
	}
	
	if (levelData->LoadLevel(
		"../GameModels/Levels/SpaceLevel/GameLevel.txt",
		"../GameModels/Levels/SpaceLevel/Models",

		/*"../GameModels/Levels/GameLevel/GameLevel.txt",
		"../GameModels/Levels/GameLevel/Models",*/

		/*"../GameModels/Levels/Test/GameLevel.txt",
		"../GameModels/Levels/test/Models",*/
		log) == false)
	{
		return false;
	}

	if (InitWindow() == false) 
		return false;
	if (InitGraphics(actorData.get(), levelData.get()) == false)
		return false;
	if (InitAudio(log) == false)
		return false;
	if (InitActorPrefabs(actorData.get()) == false)
		return false;
	if (InitSystems() == false)
		return false;
	return true;
}

bool Application::Run()
{
	bool winClosed = false;
	GW::CORE::GEventResponder winHandler;
	winHandler.Create([&winClosed](GW::GEvent _gEvent) 
	{
		GW::SYSTEM::GWindow::Events ev;
		if (+_gEvent.Read(ev) && ev == GW::SYSTEM::GWindow::Events::DESTROY)
			winClosed = true;
	});

	while (+window.ProcessWindowEvents())
	{
		if (winClosed == true)
			return true;

		IDXGISwapChain* swapChain;
		ID3D11DeviceContext* context;
		ID3D11RenderTargetView* targetView;
		ID3D11DepthStencilView* depthStencil;

		if (+d3d11.GetImmediateContext((void**)&context) &&
			+d3d11.GetRenderTargetView((void**)&targetView) &&
			+d3d11.GetDepthStencilView((void**)&depthStencil) &&
			+d3d11.GetSwapchain((void**)&swapChain))
		{

			gameLogic.CheckInput();
			if (GameLoop() == false)
				return false;
			d3d11RenderingSystem.UpdateMiniMap();
			//d3d11RenderingSystem.UpdateCamera();
			swapChain->Present(1, 0);
			// release incremented COM reference counts
			if (swapChain != nullptr)			
				swapChain->Release();			
			if (targetView != nullptr)
				targetView->Release();
			if (depthStencil != nullptr)
				depthStencil->Release();
			if (context != nullptr)
				context->Release();			
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool Application::Shutdown() 
{
	// disconnect systems from global ECS
	if (d3d11RenderingSystem.Shutdown() == false)
		return false;
	if (gameLogic.Shutdown() == false)
		return false;

	actorData.reset();
	levelData.reset();

	return true;
}

bool Application::InitWindow()
{
	// grab settings
	int width = gameConfig->at("Window").at("width").as<int>();
	int height = gameConfig->at("Window").at("height").as<int>();
	int xstart = gameConfig->at("Window").at("xstart").as<int>();
	int ystart = gameConfig->at("Window").at("ystart").as<int>();
	std::string title = gameConfig->at("Window").at("title").as<std::string>();
	// open window
	if (+window.Create(xstart, ystart, width, height, GWindowStyle::WINDOWEDBORDERED) &&
		+window.SetWindowName(title.c_str())) 
	{
		float clr[] = { 0.1f, 0.1f, 0.1f, 1 };
		messages.Create([&](const GW::GEvent& e) {
			GW::SYSTEM::GWindow::Events q;
			if (+e.Read(q) && q == GWindow::Events::RESIZE)
				clr[2] += 0.01f; // move towards a cyan as they resize
			});
		window.Register(messages);
		return true;
	}
	return false;
}



bool Application::InitGraphics(ActorData* _actorData, LevelData* _levelData)
{
	//const char* debugLayers[] = {
	//	"VK_LAYER_KHRONOS_validation", // standard validation layer
	//	//"VK_LAYER_RENDERDOC_Capture" // add this if you have installed RenderDoc
	//};
	if (+d3d11.Create(window, GW::GRAPHICS::DEPTH_BUFFER_SUPPORT))
		return true;

	return false;
}

bool Application::InitActorPrefabs(ActorData* _actorData)
{
	unsigned int playerPrefabCount	= 0;
	unsigned int enemyPrefabCount = 0;
	unsigned int projectilePrefabCount = 0;
	unsigned int pickupPrefabCount = 0;
	unsigned int modelIndex = 0;

	// Iterate through all the actor models we have in game files.
	for (auto& i : _actorData->models)
	{	
		std::string fileName(i.fileName);

		// If the actor is a player...
		if (fileName.find("Player") != std::string::npos)
		{
			// Make a prefab for the new type of player we found.
			playerPrefabCount += 1;
			playerData.Load(flecsWorld, gameConfig, playerPrefabCount, modelIndex, _actorData, i, audioData);
		}
		else if (fileName.find("Enemy") != std::string::npos)
		{
			enemyPrefabCount += 1;
			enemyData.Load(flecsWorld, gameConfig, audioData, enemyPrefabCount, modelIndex, _actorData, i);
		}
		else if (fileName.find("Projectile") != std::string::npos)
		{
			projectilePrefabCount += 1;
			projectileData.Load(flecsWorld, gameConfig, audioData, projectilePrefabCount, modelIndex, _actorData, i);
		}
		else if (fileName.find("Pickup") != std::string::npos)
		{
			pickupPrefabCount += 1;
			pickupData.Load(flecsWorld, gameConfig, audioData, pickupPrefabCount, modelIndex, _actorData, i);
		}

		modelIndex += 1;
	}


	return true;
}

bool Application::InitAudio(GW::SYSTEM::GLog _log)
{
	if (-audioEngine.Create())
		return false;

	audioData.Init("../SoundFX", "../Music", &audioEngine);
	return true;
}

bool Application::InitSystems()
{
	if (d3d11RenderingSystem.Init(	window,
									d3d11,
									flecsWorld,
									uiWorld,
									gameConfig,
									levelData.get(),
									actorData.get(),
									eventPusher) == false)
	{
		return false;
	}
	if (gameLogic.Init(	flecsWorld, 
						&d3d11RenderingSystem,
						window,
						gameConfig,
						&audioEngine,
						&audioData,
						eventPusher) == false)
		return false;
	return true;
}

bool Application::GameLoop()
{
	// compute delta time and pass to the ECS system
	static auto startTime = std::chrono::steady_clock::now();
	double elapsedTime = std::chrono::duration<double>(
		std::chrono::steady_clock::now() - startTime).count();
	startTime = std::chrono::steady_clock::now();
	// let the ECS system run
	uiWorld->progress(static_cast<float>(elapsedTime));
	return flecsWorld->progress(static_cast<float>(elapsedTime)); 
}
