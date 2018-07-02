#include <stdio.h>
#include <stdint.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <mmsystem.h>
#include <xaudio2.h>
#include "SoundManager.h"
#include "VoiceCallback.h"


#ifdef _DEBUG
#define OutputDebugStringEx( str, ... ) \
      { \
        TCHAR c[256]; \
        _stprintf_s( c, str, __VA_ARGS__ ); \
        OutputDebugString( c ); \
      }
#else
#    define OutputDebugString( str, ... ) // �����
#endif



int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	char* filePath = (char*)"Resources\\toujyo.wav";

	SoundManager soundManager;
	soundManager.Initialize();
	
	// Wave�t�@�C���I�[�v��
	if (!soundManager.OpenSoundFile(filePath)) {
		return -1;
	}

	if (!soundManager.Start()) {
		return -1;
	}

	Sleep(10000);

	return 0;
}
