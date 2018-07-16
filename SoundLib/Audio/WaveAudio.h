﻿#ifndef WAVE_AUDIO_H
#define WAVE_AUDIO_H

#include "IAudio.h"


namespace SoundLib {
namespace Audio {

class WaveAudio : public IAudio {
public:
	WaveAudio();
	~WaveAudio();

	const WAVEFORMATEX* GetWaveFormatEx() const;
	TString GetFormatName() const;
	int GetChannelCount() const;
	int GetSamplingRate() const;
	int GetBitsPerSample() const;
	bool HasReadToEnd() const;

	bool Load(TString filePath);
	long Read(BYTE* pBuffer, DWORD bufSize);
	void Reset();

private:
	HMMIO hMmio;
	WAVEFORMATEX waveFormatEx;
	long offset;
	long dataSize;
	long restSize;
	bool hasReadToEnd;
};

}
}
#endif
