#ifndef PLAYEVENTS_H
#define PLAYEVENTS_H

// example space game (avoid name collisions)
namespace GOG
{
	enum PLAY_EVENT 
	{
		ENEMY_DESTROYED,
		CIVILIAN_DESTROYED,
		WAVE_CLEARED,
		PLAYER_RESPAWNED,
		PLAYER_DESTROYED,
		UPDATE_SCORE,
		EVENT_COUNT,
		SMART_BOMB_ACTIVATED,
		SMART_BOMB_GRABBED,
		GAME_OVER,
		HAPTICS_ACTIVATED,
		STATE_CHANGED
	};

	enum DIRECTIVES
	{
		UPDATE_SCORE_OK = 1
	};

	struct PLAY_EVENT_DATA 
	{
		flecs::id entityId; // which entity was affected?
		unsigned int value;
		unsigned int directive;
	};
}

#endif