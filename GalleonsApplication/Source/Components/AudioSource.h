#ifndef AUDIO_SOURCE_H
#define AUDIO_SOURCE_H

namespace GOG
{
	struct SoundClips { std::map<std::string, GW::AUDIO::GSound*> sounds; };

	struct LoopingClip {
		GW::AUDIO::GMusic* clip;
		float volume;
	};

	struct LoopingClips { 
		std::map<std::string, LoopingClip> sounds; 
	};

}

#endif
