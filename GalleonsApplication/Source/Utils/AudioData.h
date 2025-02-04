#pragma once


class AudioData
{
	std::string soundFXFolderPath;
	std::string musicFolderPath;

	GW::AUDIO::GAudio* audioListener;

	std::vector<GW::AUDIO::GSound> soundInstances;
	int maxSoundInstances = 100;
	std::vector<GW::AUDIO::GMusic> loopingInstances;
	int maxLoopingInstances = 100;
public:
	std::map<std::string, GW::AUDIO::GMusic> music;

	void Init(const char* _soundFXFolderPath,
		const char* _musicFolderPath,
		GW::AUDIO::GAudio* _audioListener)
	{
		UnloadAudio();

		soundFXFolderPath = _soundFXFolderPath;
		musicFolderPath = _musicFolderPath;
		audioListener = _audioListener;

		soundInstances.reserve(maxSoundInstances);
		loopingInstances.reserve(maxLoopingInstances);

		ReadMusicFolder();
	}

	void UnloadAudio() {
		soundInstances.clear();
		music.clear();
	}

	bool ReadMusicFolder()
	{
		std::string folderPath = musicFolderPath;

		WIN32_FIND_DATAA findFileData;
		//finds the first dds file in the directory and stores it in @findHandle
		// the /*wav means that the path ends with '*' which means any text .wav
		HANDLE findHandle = FindFirstFileA((folderPath + "/*.wav").c_str(), &findFileData);

		//if the handle is valid, loop through every file in directory and add their path
		//to musicFolderPath
		if (findHandle != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					music.insert({ findFileData.cFileName, GW::AUDIO::GMusic() });
					music[findFileData.cFileName].Create((folderPath + "/" + findFileData.cFileName).c_str(), *audioListener, 1);
				}
			} while (FindNextFileA(findHandle, &findFileData) != 0);

			FindClose(findHandle);
		}
		else
		{
			return false;
		}

		return true;
	}

	GW::AUDIO::GSound* CreateSound(std::string name, float volume)
	{
		if (soundInstances.size() == maxSoundInstances)
			return nullptr;

		int index = soundInstances.size();
		soundInstances.push_back(GW::AUDIO::GSound());
		soundInstances[index].Create((soundFXFolderPath + "/" + name).c_str(), *audioListener, volume);

		return &soundInstances[index];
	}

	GW::AUDIO::GMusic* CreateSoundLooping(std::string name, float volume)
	{
		if (loopingInstances.size() == maxLoopingInstances)
			return nullptr;

		int index = loopingInstances.size();
		loopingInstances.push_back(GW::AUDIO::GMusic());
		loopingInstances[index].Create((soundFXFolderPath + "/" + name).c_str(), *audioListener, volume);

		return &loopingInstances[index];
	}
};