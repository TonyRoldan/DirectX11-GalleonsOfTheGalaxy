#ifndef RENDERER_H
#define RENDERER_H

#include <d3dcompiler.h> // required for compiling shaders on the fly, consider pre-compiling instead
#pragma comment(lib, "d3dcompiler.lib")
#include "../GameConfig.h"
#include "../Events/Playevents.h"
#include "../Utils/ActorData.h"
#include "../Utils/LevelData.h"
#include <DDSTextureLoader.h>
#include <SpriteFont.h>
#include <SimpleMath.h>
#include <WICTextureLoader.h>
#include <PostProcess.h>
#include "../Components/Identification.h"
#include "../Components/Visuals.h"
#include "../Components/Physics.h"
#include "../Components/UI.h"
#include "../Components/Lights.h"

namespace GOG
{
	struct MapModelTex
	{
		unsigned int modelType;
		unsigned int pad[3];
	};

	struct Quad
	{
		std::vector<H2B::Vertex> face;
		std::vector<unsigned int> indices;
	};
	struct PipelineHandles
	{
		ID3D11DeviceContext* context;
		ID3D11RenderTargetView* targetView;
		ID3D11DepthStencilView* depthStencil;
	};

	struct TextureData
	{
		unsigned width;
		unsigned height;
	};

	struct LightData
	{
		H2B::Light light;
	};

	struct TransformData
	{
		GW::MATH::GMATRIXF transform;
	};

	struct alignas(16) PerInstanceData
	{
		unsigned int transformStart;
		unsigned int materialStart;
		unsigned int pad[2];
		
	};

	struct alignas(16) SceneData
	{
		GW::MATH::GMATRIXF viewMatrix;
		GW::MATH::GMATRIXF projectionMatrix;
		GW::MATH::GVECTORF camPos;
		GW::MATH::GVECTORF dirLightDir, dirLightColor;
		GW::MATH::GVECTORF ambientTerm;
		GW::MATH::GVECTORF fogColor;
		float fogDensity;
		float fogStartDistance;
		float contrast;
		float saturation;
	};

	struct alignas(16) MeshData
	{
		GW::MATH::GMATRIXF worldMatrix;
		H2B::Attributes attribute;
		unsigned int texID;
		float offset;
		unsigned int padding[2];
		
	};

	struct RenderingSystem {};

	class DirectX11Renderer
	{
		std::chrono::steady_clock::time_point prevTime = std::chrono::steady_clock::now();

		GAME_STATES currState;

		//---------- Flecs ----------
		std::shared_ptr<flecs::world> flecsWorld;
		std::shared_ptr<flecs::world> uiWorld;
		flecs::world uiWorldAsync;

		flecs::system startDraw;
		flecs::system updateDraw;
		flecs::system completeDraw;

		std::weak_ptr<const GameConfig> gameConfig;

		flecs::query<Player, Transform> playerTransformsQuery;

		flecs::query<const Left, const ResizeOffset, Position, Scale> leftResizeQuery;
		flecs::query<const Right, const ResizeOffset, Position, Scale> rightResizeQuery;
		flecs::query<const Center, Position, Scale> centerResizeQuery;
		flecs::query<const Top, const ResizeOffset, Position, Scale> topResizeQuery;

		flecs::query<const LogoScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale> logoSpriteQuery;
		flecs::query<const DirectXScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale> directSpriteQuery;
		flecs::query<const GatewareScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale> gateSpriteQuery;
		flecs::query<const FlecsScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale> flecsSpriteQuery;
		flecs::query<const TitleScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale> titleSpriteQuery;

		flecs::query<const MainMenu, const Sprite, const SpriteCount, const Position, const Origin, const Scale> mainMenuSpriteQuery;
		flecs::query<const MainMenu, const Text, const TextColor, const Position, const Origin, const Scale, const ResizeOffset> mainMenuTextQuery;

		flecs::query<const PlayGame, const Sprite, const SpriteCount, const SpriteOffsets, const Position, const Origin, const Scale, const ResizeOffset> playGameSpriteQuery;
		flecs::query<const PlayGame, const Text, const TextColor, const Position, const Origin, const Scale, const ResizeOffset> playGameTextQuery;

		flecs::query<const PauseGame, const Text, const TextColor, const Position, const Origin, const Scale, const ResizeOffset> pauseGameTextQuery;

		flecs::query<const GameOverScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale> gameOverSpriteQuery;
		flecs::query<const GameOverScreen, const Text, const TextColor, const Position, const Origin, const Scale, const ResizeOffset> gameOverTextQuery;

		flecs::query<const Credits, const Text, const TextColor, const Position, const Origin, const Scale, const ResizeOffset> creditsTextQuery;



		//----------Gateware----------
		GW::CORE::GThreadShared uiWorldLock;

		GW::SYSTEM::GWindow window;
		GW::GRAPHICS::GDirectX11Surface d3d;
		GW::MATH::GVECTORF bgColor;

		GW::CORE::GEventResponder onEvent;
		GW::CORE::GEventResponder onWindowResize;
		GW::CORE::GEventGenerator eventPusherUI;

		GW::INPUT::GInput input;


		//----------Shaders----------
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> mapVertexShader;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> levelVertexShader;
		
		Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> mapPixelShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> mapModelsPixelShader;

		//----------Blobs----------
		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> vsLevelBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> vsMapBlob;

		Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> psMapBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> psMapModelsBlob;

		Microsoft::WRL::ComPtr<ID3DBlob> errors;

	
		//----------Input Layouts----------
		Microsoft::WRL::ComPtr<ID3D11InputLayout> vertexFormat;


		//----------Geometry----------
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> actorVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> mapVertexBuffer;

		Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> actorIndexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> mapIndexBuffer;
		std::vector<GW::MATH::GMATRIXF> playerTransforms;


		//----------Constant Buffers----------
		Microsoft::WRL::ComPtr<ID3D11Buffer> cMeshBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> cActorMeshBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> cSceneBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> cInstanceBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> cMapModelBuffer;


		//----------Structured Buffers----------
		Microsoft::WRL::ComPtr<ID3D11Buffer> sTransformBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> transformView;
		Microsoft::WRL::ComPtr<ID3D11Buffer> sActorTransformBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> actorTransformView;
		Microsoft::WRL::ComPtr<ID3D11Buffer> sLightBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> lightView;


		//----------Textures----------
		Microsoft::WRL::ComPtr<ID3D11SamplerState> texSampler;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> levelView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shipsView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bombView;
		unsigned int modelType;
		unsigned int texId;


		//----------Minimap----------
		D3D11_VIEWPORT mapViewPort;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> mapDepthStencil;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> mapDepthBuffer;		
		Microsoft::WRL::ComPtr<ID3D11Texture2D> targetTexMap;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> targetViewMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mapView;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> cullModeState;
		GW::MATH::GMATRIXF mapViewMatrix;
		GW::MATH::GVECTORF mapEye;
		GW::MATH::GVECTORF mapAt;
		GW::MATH::GVECTORF mapUp;
		GW::MATH::GMATRIXF mapProjMatrix;
		GW::MATH::GVECTORF mapModelScalar;
		MapModelTex mapModelData;
		Quad mapQuad;
		unsigned int zoomFactor;


		//----------Sprites----------
		std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;
		Microsoft::WRL::ComPtr<ID3D11BlendState> alphaBlend;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> teamLogoView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> teamLogoTex;
		DirectX::SimpleMath::Vector2 teamLogoPos;
		DirectX::SimpleMath::Vector2 teamLogoOrigin;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> directSplashView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> directSplashTex;
		DirectX::SimpleMath::Vector2 directSplashPos;
		DirectX::SimpleMath::Vector2 directSplashOrigin;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gateSplashView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> gateSplashTex;
		DirectX::SimpleMath::Vector2 gateSplashPos;
		DirectX::SimpleMath::Vector2 gateSplashOrigin;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> flecsSplashView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> flecsSplashTex;
		DirectX::SimpleMath::Vector2 flecsSplashPos;
		DirectX::SimpleMath::Vector2 flecsSplashOrigin;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> titleView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> titleTex;
		DirectX::SimpleMath::Vector2 titleSpritePos;
		DirectX::SimpleMath::Vector2 titleSpriteOrigin;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mainMenuView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> mainMenuTex;
		DirectX::SimpleMath::Vector2 mainMenuSpritePos;
		DirectX::SimpleMath::Vector2 mainMenuSpriteOrigin;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bombSpriteView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> bombSpriteTex;
		DirectX::SimpleMath::Vector2 bombSpritePos;
		DirectX::SimpleMath::Vector2 bombSpriteOrigin;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spaceshipView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> spaceshipTex;
		DirectX::SimpleMath::Vector2 shipSpritePos;
		DirectX::SimpleMath::Vector2 shipSpriteOrigin;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gameOverView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> gameOverTex;
		DirectX::SimpleMath::Vector2 gameOverPos;
		DirectX::SimpleMath::Vector2 gameOverOrigin;

		float splashAlpha;
		bool fadeInDone;


		//----------Text----------
		std::unique_ptr<DirectX::SpriteFont> ocraExtended;
		std::unique_ptr<DirectX::SpriteFont> ocraExtendedBold;
		std::unique_ptr<DirectX::SpriteFont> courierNew;
		DirectX::SimpleMath::Vector2 scoreNumPos;
		DirectX::SimpleMath::Vector2 waveTextPos;
		DirectX::SimpleMath::Vector2 waveNumTextPos;
		DirectX::XMVECTOR creditsPosCopy;
		float creditsOffset;

		unsigned int playerCurrScore;
		
		float pauseAlpha;
		bool maxReached;
		bool minReached;


		//----------Window Resize----------
		float uiScalar;
		unsigned int screenWidth;
		unsigned int screenHeight;
		unsigned int newHeight;
		unsigned int newWidth;


		//----------Level----------
		LevelData* levelData;
		ActorData* actorData;
		PerInstanceData instanceData;
		MeshData meshData;
		MeshData actorMeshData;
		SceneData sceneData;

		H2B::Attributes levelAttribute;
		H2B::Attributes actorAttribute;
		float levelSegmentWidth;

		GW::MATH::GMATRIXF worldMatrix;
		GW::MATH::GMATRIXF viewMatrix;
		GW::MATH::GMATRIXF projectionMatrix;
		GW::MATH::GMATRIXF cameraMatrix;
		GW::MATH::GVECTORF camPos;

		float fov;
		float currAspect;
		float nearPlane;
		float farPlane;

		GW::SYSTEM::GDaemon bombEffect;
		unsigned int updateBombEffectTime = 1;
		unsigned int bombEffectStartTime;
		unsigned int bombEffectTime = 500;

		//----------Background----------
		GW::MATH::GVECTORF black;
		GW::MATH::GVECTORF white;
		GW::MATH::GVECTORF skyBlue;
		GW::MATH::GVECTORF rust;
		bool isStateChanged;


		SceneData currentLevelSceneData;
		SceneData currentActorSceneData;
		GW::MATH::GVECTORF currentBgColorData;
		std::vector<SceneData> levelSceneData;
		std::vector<SceneData> actorSceneData;
		std::vector<GW::MATH::GVECTORF> bgColorData;

	public:

		bool Init(	GW::SYSTEM::GWindow _win, 
					GW::GRAPHICS::GDirectX11Surface _d3d,
					std::shared_ptr<flecs::world> _game,
					std::shared_ptr<flecs::world> _uiWorld,
					std::weak_ptr<const GameConfig> _gameConfig, LevelData* _lvl, 
					ActorData* _actors,
					GW::CORE::GEventGenerator _eventPusher);
		void RenderGameUI();
		void Resize(unsigned int height, unsigned int width);
		void UpdateCamera();
		void UpdateCamera(GW::MATH::GMATRIXF camWorld);
		void UpdateMiniMap();
		void UpdateGameState(GAME_STATES state);
		void UpdateStats(unsigned int _lives, unsigned int _score, unsigned int _smartBombs, unsigned int _waves);
		void UpdateLives(unsigned int _lives);
		void UpdateScore(unsigned int _score);
		void UpdateSmartBombs(unsigned int _smartBombs);
		void UpdateWaves(unsigned int _waves);
		void InitRendererSystems();
		bool Activate(bool runSystem);
		bool Shutdown();

	private:	
		bool LoadSceneData();
		bool LoadEventResponders();
		bool LoadShaders();
		bool LoadBuffers();
		bool LoadGeometry();
		bool LoadShaderResources();
		bool LoadTextures();
		bool Load2D();
		bool SetupPipeline();
		void Restore3DStates(PipelineHandles& handles);
		Quad CreateQuad();
		void InitCredits();
		
		std::string ReadFileIntoString(const char* _filePath);
		void PrintLabeledDebugString(const char* _label, const char* _toPrint);
		
	private:
		static constexpr unsigned int instanceMax = 1024;
		struct INSTANCE_TRANSFORMS
		{
			GW::MATH::GMATRIXF transforms[instanceMax];
			unsigned int modelNdxs[instanceMax];

		}instanceTransforms;

		int drawCounter = 0;

		INSTANCE_TRANSFORMS scaledMapModels;

		struct CreditsText
		{
			std::wstring text;
		}credits;
	};
}

#endif