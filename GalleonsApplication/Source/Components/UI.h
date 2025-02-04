#ifndef UI_H
#define UI_H

namespace GOG
{
	enum GAME_STATES
	{
		LOGO_SCREEN = 0,
		DIRECTX_SCREEN,
		GATEWARE_SCREEN,
		FLECS_SCREEN,
		TITLE_SCREEN,
		MAIN_MENU,
		PLAY_GAME,
		PAUSE_GAME,
		GAME_OVER_SCREEN,
		HIGH_SCORES,
		CREDITS,
	};

	struct LogoScreen {};
	struct DirectXScreen {};
	struct GatewareScreen {};
	struct FlecsScreen {};
	struct TitleScreen {};
	struct MainMenu {};
	struct PlayGame {};
	struct WaveScreen {};
	struct GameOverScreen {};
	struct PauseGame {};
	struct HighScores {};
	struct Credits {};

	struct Sprite { Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view; };
	struct SpriteCount { unsigned int numSprites; };
	struct SpriteOffsets { DirectX::SimpleMath::Vector2 values; };

	struct Text { std::wstring value;  };
	struct TextColor { DirectX::XMVECTOR value; };
	
	struct Position { DirectX::SimpleMath::Vector2 value; };
	struct Origin { DirectX::SimpleMath::Vector2 value; };
	struct Scale { float value; };

	struct ResizeOffset { DirectX::SimpleMath::Vector2 value; };
	struct Left {};
	struct Right{};
	struct Center{};
	struct Top{};
	
}

#endif