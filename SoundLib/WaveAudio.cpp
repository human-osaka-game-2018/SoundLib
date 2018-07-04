#include "WaveAudio.h"
#include "Common.h"


WaveAudio::WaveAudio() : hMmio(nullptr), pos(0) {
	
}

WaveAudio::~WaveAudio() {
	if (this->hMmio != nullptr) {
		mmioClose(this->hMmio, 0);
		this->hMmio = nullptr;
	}
}

bool WaveAudio::Load(const char* pFilePath) {
	MMIOINFO mmioInfo;

	// Wave�t�@�C���I�[�v��
	memset(&mmioInfo, 0, sizeof(MMIOINFO));

	this->hMmio = mmioOpen((LPSTR)pFilePath, &mmioInfo, MMIO_READ);
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
	DWORD size = mmioRead(this->hMmio, (HPSTR)&this->waveFormatEx, fmsize);
	if (size != fmsize) {
		OutputDebugStringEx("error mmioRead(fmt) size=%d\n", size);
		mmioClose(this->hMmio, 0);
		return false;
	}

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

long WaveAudio::Read(BYTE* pBuffer, DWORD bufSize) {
	// �f�[�^�̕����i�[
	long size = mmioRead(this->hMmio, (HPSTR)pBuffer, bufSize);

	this->pos += size;   // �t�@�C���|�C���^�̃I�t�Z�b�g�l

	return size;
}

const WAVEFORMATEX* WaveAudio::GetWaveFormatEx() {
	return &this->waveFormatEx;
}

void WaveAudio::Reset() {
	mmioSeek(this->hMmio, -this->pos, SEEK_CUR);
	this->pos = 0;   // �t�@�C���|�C���^��擪�ɖ߂�
}
