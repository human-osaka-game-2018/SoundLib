﻿#ifndef SOUNDS_MANAGER_H
#define SOUNDS_MANAGER_H

#include <unordered_map>
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <xaudio2.h>
#include "AudioHandler.h"
#include "ISoundsManagerDelegate.h"


namespace SoundLib {

class SoundsManager {
public:
	SoundsManager();
	~SoundsManager();
	bool Initialize();
	bool AddFile(const char* pFilePath, const char* pKey);
	bool Start(const char* pKey, bool isLoopPlayback = false);
	bool Start(const char* pKey, ISoundsManagerDelegate* pDelegate);
	bool Start(const char* pKey, void(*onPlayedToEndCallback)(const char* pKey));
	bool Stop(const char* pKey);
	bool Pause(const char* pKey);
	bool Resume(const char* pKey);
	PlayingStatus GetStatus(const char* pKey);

private:
	IAudio * pAudio;
	std::unordered_map<const char*, AudioHandler*> audioMap;
	IXAudio2* pXAudio2;

	bool ExistsKey(const char* pKey);
};
}
#endif
