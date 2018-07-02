#include "SoundManager.h"
#include <stdio.h>
#include <tchar.h>


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


SoundManager::SoundManager() : audio(nullptr), hMmio(nullptr), pVoice(nullptr), buf(nullptr) {
	this->pCallback = new VoiceCallback(this);
}

SoundManager::~SoundManager() {
	delete this->pCallback;

	if (this->hMmio != nullptr) {
		this->Stop();
		mmioClose(this->hMmio, 0);
		this->hMmio = nullptr;
	}

	if (this->audio != nullptr) {
		this->audio->Release();
	}
	CoUninitialize();
}

bool SoundManager::Initialize() {
	HRESULT ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(ret)) {
		OutputDebugStringEx("error CoInitializeEx ret=%d\n", ret);
		return false;
	}

	ret = XAudio2Create(
		&this->audio
		// UINT32 Flags = 0,
		// XAUDIO2_PROCESSOR XAudio2Processor = XAUDIO2_DEFAULT_PROCESSOR
	);
	if (FAILED(ret)) {
		OutputDebugStringEx("error XAudio2Create ret=%d\n", ret);
		return false;
	}

	IXAudio2MasteringVoice* master = NULL;
	ret = this->audio->CreateMasteringVoice(
		&master
		// UINT32 InputChannels = XAUDIO2_DEFAULT_CHANNELS,
		// UINT32 InputSampleRate = XAUDIO2_DEFAULT_SAMPLERATE,
		// UINT32 Flags = 0,
		// UINT32 DeviceIndex = 0,
		// const XAUDIO2_EFFECT_CHAIN *pEffectChain = NULL
	);
	if (FAILED(ret)) {
		OutputDebugStringEx("error CreateMasteringVoice ret=%d\n", ret);
		return false;
	}

	return true;
}

bool SoundManager::OpenSoundFile(char* filePath) {
	MMIOINFO mmioInfo;

	// Wave�t�@�C���I�[�v��
	memset(&mmioInfo, 0, sizeof(MMIOINFO));

	this->hMmio = mmioOpen(filePath, &mmioInfo, MMIO_READ);
	if (!this->hMmio) {
		// �t�@�C���I�[�v�����s
		OutputDebugStringEx("error mmioOpen\n");
		return false;
	}

	// RIFF�`�����N����
	MMRESULT mmRes;
	MMCKINFO riffChunk;
	riffChunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	mmRes = mmioDescend(this->hMmio, &riffChunk, NULL, MMIO_FINDRIFF);
	if (mmRes != MMSYSERR_NOERROR) {
		OutputDebugStringEx("error mmioDescend(wave) ret=%d\n", mmRes);
		mmioClose(this->hMmio, 0);
		return false;
	}

	// �t�H�[�}�b�g�`�����N����
	MMCKINFO formatChunk;
	formatChunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
	mmRes = mmioDescend(this->hMmio, &formatChunk, &riffChunk, MMIO_FINDCHUNK);
	if (mmRes != MMSYSERR_NOERROR) {
		mmioClose(this->hMmio, 0);
		return false;
	}

	// WAVEFORMATEX�\���̊i�[
	DWORD fmsize = formatChunk.cksize;
	DWORD size = mmioRead(this->hMmio, (HPSTR)&(this->waveFormatEx), fmsize);
	if (size != fmsize) {
		OutputDebugStringEx("error mmioRead(fmt) size=%d\n", size);
		mmioClose(this->hMmio, 0);
		return false;
	}

	OutputDebugStringEx("foramt    =%d\n", this->waveFormatEx.wFormatTag);
	OutputDebugStringEx("channel   =%d\n", this->waveFormatEx.nChannels);
	OutputDebugStringEx("sampling  =%dHz\n", this->waveFormatEx.nSamplesPerSec);
	OutputDebugStringEx("bit/sample=%d\n", this->waveFormatEx.wBitsPerSample);
	OutputDebugStringEx("byte/sec  =%d\n", this->waveFormatEx.nAvgBytesPerSec);

	// WAVEFORMATEX�\���̊i�[
	mmioAscend(this->hMmio, &formatChunk, 0);

	// �f�[�^�`�����N����
	MMCKINFO dataChunk;
	dataChunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
	mmRes = mmioDescend(this->hMmio, &dataChunk, &riffChunk, MMIO_FINDCHUNK);
	if (mmRes != MMSYSERR_NOERROR) {
		OutputDebugStringEx("error mmioDescend(data) ret=%d\n", mmRes);
		mmioClose(this->hMmio, 0);
		return false;
	}

	return true;
}

bool SoundManager::Start(bool isLoopPlayback = false) {
	this->isLoopPlayback = isLoopPlayback;
	HRESULT ret = this->audio->CreateSourceVoice(
		&this->pVoice,
		&waveFormatEx,
		0,                          // UINT32 Flags = 0,
		XAUDIO2_DEFAULT_FREQ_RATIO, // float MaxFrequencyRatio = XAUDIO2_DEFAULT_FREQ_RATIO,
		this->pCallback                   // IXAudio2VoiceCallback *pCallback = NULL,
									// const XAUDIO2_VOICE_SENDS *pSendList = NULL,
									// const XAUDIO2_EFFECT_CHAIN *pEffectChain = NULL
	);
	if (FAILED(ret)) {
		OutputDebugStringEx("error CreateSourceVoice ret=%d\n", ret);
		return false;
	}
	this->pVoice->Start();

	this->buffer = { 0 };
	this->buf = new BYTE*[BUF_LEN];
	this->len = this->waveFormatEx.nAvgBytesPerSec;
	for (int i = 0; i < BUF_LEN; i++) {
		this->buf[i] = new BYTE[this->len];
	}

	this->buf_cnt = 0;

	this->Push();

	return true;
}

void SoundManager::Stop() {
	if (this->pVoice != nullptr) {
		this->pVoice->Stop();
		this->pVoice->DestroyVoice();
		this->pVoice = nullptr;
	}

	if (this->buf != nullptr) {
		for (int i = 0; i < BUF_LEN; i++) {
			delete[] this->buf[i];
		}
		delete this->buf;
		this->buf = nullptr;
	}
}

void SoundManager::Pause() {
	this->pVoice->Stop();
}

void SoundManager::Resume() {
	this->pVoice->Start();
}

void SoundManager::BufferEndCallback() {
	this->Push();
}

long SoundManager::ReadSoundData() {
	static int curOffset = 0;

	// �f�[�^�̕����i�[
	long size = mmioRead(this->hMmio, (HPSTR)this->buf[this->buf_cnt], this->len);
	if (size <= 0 && this->isLoopPlayback) {
		// �Ō�܂œǂݍ��񂾏ꍇ�͍ŏ��ɖ߂�
		mmioSeek(this->hMmio, -curOffset, SEEK_CUR);
		curOffset = 0;   // �t�@�C���|�C���^��擪�ɖ߂�
		size = mmioRead(this->hMmio, (HPSTR)this->buf[this->buf_cnt], this->len);
	}

	curOffset += size;   // �t�@�C���|�C���^�̃I�t�Z�b�g�l

	return size;
}

void SoundManager::Push() {
	// ���f�[�^�i�[
	long size = ReadSoundData();
	if (size <= 0) {
		this->Stop();
		return;
	}
	this->buffer.AudioBytes = size;
	this->buffer.pAudioData = this->buf[this->buf_cnt];
	HRESULT ret = this->pVoice->SubmitSourceBuffer(&this->buffer);
	if (FAILED(ret)) {
		OutputDebugStringEx("error SubmitSourceBuffer ret=%d\n", ret);
		return;
	}
	if (BUF_LEN <= ++this->buf_cnt) {
		this->buf_cnt = 0;
	}
}


