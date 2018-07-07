﻿#include <stdint.h>
#include <windows.h>
#include "Common.h"
#include "SoundsManager.h"


SoundLib::SoundsManager soundsManager;

static void OnPlayedToEnd(const char* pKey);


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	const char* filePath1 = "Resources\\toujyo.wav";
	const char* filePath2 = "Resources\\musicbox.mp3";
	const char* filePath3 = "Resources\\jump03.mp3";

	//SoundsManager soundsManager;
	soundsManager.Initialize();
	
	// Waveファイルオープン
	if (!soundsManager.AddFile(filePath1, "wav")) {
		return -1;
	}

	// mp3ファイルオープン
	if (!soundsManager.AddFile(filePath2, "mp3")) {
		return -1;
	}

	// mp3ファイルオープン
	if (!soundsManager.AddFile(filePath3, "mp3SE")) {
		return -1;
	}

	SoundLib::PlayingStatus status = soundsManager.GetStatus("wav");
	OutputDebugStringEx("Status of the wave is %d\n", status);

	soundsManager.Start("wav", OnPlayedToEnd);
	Sleep(1000);
	OutputDebugStringEx("Status of the wave is %d\n", soundsManager.GetStatus("wav"));

	for (int i = 0; i < 1; ++i) {
		soundsManager.Start("mp3SE");
		Sleep(1500);
	}

	//Sleep(1000);
	soundsManager.Pause("wav");
	OutputDebugStringEx("Status of the wave is %d\n", soundsManager.GetStatus("wav"));
	Sleep(1000);


	soundsManager.Resume("wav");
	OutputDebugStringEx("Status of the wave is %d\n", soundsManager.GetStatus("wav"));
	Sleep(1000);

	//soundsManager.Stop("wav");
	OutputDebugStringEx("Status of the wave is %d\n", soundsManager.GetStatus("wav"));

	Sleep(5000);
	OutputDebugStringEx("Status of the wave is %d\n", soundsManager.GetStatus("wav"));

	return 0;
}

static void OnPlayedToEnd(const char* pKey) {
	soundsManager.Start("mp3");
}
