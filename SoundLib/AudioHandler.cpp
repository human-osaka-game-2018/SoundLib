﻿//----------------------------------------------------------
// <filename>AudioHandler.cpp</filename>
// <author>Masami Sugao</author>
// <date>2018/07/16</date>
//----------------------------------------------------------
#include "AudioHandler.h"
#include <typeinfo>
#include "Common.h"


namespace SoundLib {
/* Constructor / Destructor ------------------------------------------------------------------------- */
template <typename T>
AudioHandler<T>::AudioHandler(std::basic_string<T> name, Audio::IAudio* pAudio) :
	name(name), 
	pAudio(pAudio), 
	pVoice(nullptr), 
	pDelegate(nullptr), 
	onPlayedToEndCallback(nullptr), 
	status(PlayingStatus::Stopped) {
		this->pVoiceCallback = new VoiceCallback(this);
		this->xAudioBuffer = { 0 };
		this->pReadBuffers = new BYTE*[BUF_COUNT];
}

template <typename T>
AudioHandler<T>::~AudioHandler() {
	Stop();

	if (this->pVoice != nullptr) {
		this->pVoice->DestroyVoice();
		this->pVoice = nullptr;
	}

	delete this->pAudio;
	this->pAudio = nullptr;

	delete this->pVoiceCallback;
	this->pVoiceCallback = nullptr;

	delete[] this->pReadBuffers;
	this->pReadBuffers = nullptr;
}

/* Getters / Setters -------------------------------------------------------------------------------- */
template <typename T>
PlayingStatus AudioHandler<T>::GetStatus() const{
	return this->status;
}

template <typename T>
float AudioHandler<T>::GetVolume() const {
	float volume;
	this->pVoice->GetVolume(&volume);
	return volume;
}

template <typename T>
bool AudioHandler<T>::SetVolume(float volume) {
	HRESULT result = this->pVoice->SetVolume(volume);
	if (FAILED(result)) {
		Common::OutputDebugString("error SetVolume resule=%d\n", result);
		return false;
	}
	return true;
}

template <typename T>
float AudioHandler<T>::GetFrequencyRatio() const {
	float ratio;
	this->pVoice->GetFrequencyRatio(&ratio);
	return ratio;
}

template <typename T>
bool AudioHandler<T>::SetFrequencyRatio(float ratio) {
	HRESULT result = this->pVoice->SetFrequencyRatio(ratio);
	if (FAILED(result)) {
		Common::OutputDebugString("error SetFrequencyRatio resule=%d\n", result);
		return false;
	}
	return true;
}

/* Public Functions  -------------------------------------------------------------------------------- */
template <typename T>
bool AudioHandler<T>::Prepare(IXAudio2& rXAudio2) {
	HRESULT ret = rXAudio2.CreateSourceVoice(&this->pVoice, this->pAudio->GetWaveFormatEx(), 0, static_cast<float>(MAX_FREQENCY_RATIO), this->pVoiceCallback);
	if (FAILED(ret)) {
		Common::OutputDebugString("error CreateSourceVoice ret=%d\n", ret);
		return false;
	}
	return true;
}

template <typename T>
void AudioHandler<T>::Start(bool isLoopPlayback) {
	if (this->status == PlayingStatus::Pausing) {
		Stop(true);
	}
	if (this->status == PlayingStatus::Stopped) {
		this->isLoopPlayback = isLoopPlayback;
		Start();
	}
}

template <typename T>
void AudioHandler<T>::Start(IAudioHandlerDelegate<T>* pDelegate) {
	if(this->status == PlayingStatus::Pausing) {
		Stop(true);
	}
	if (this->status == PlayingStatus::Stopped) {
		this->pDelegate = pDelegate;
		this->isLoopPlayback = false;
		Start();
	}
}

template <typename T>
void AudioHandler<T>::Start(void(*onPlayedToEndCallback)(const T* pName)) {
	if(this->status == PlayingStatus::Pausing) {
		Stop(true);
	}
	if (this->status == PlayingStatus::Stopped) {
		this->onPlayedToEndCallback = onPlayedToEndCallback;
		this->isLoopPlayback = false;
		Start();
	}
}

template <typename T>
void AudioHandler<T>::Stop() {
	if (this->status == PlayingStatus::Playing) {
		Stop(true);
	}
}

template <typename T>
void AudioHandler<T>::Pause() {
	if (this->status == PlayingStatus::Playing) {
		this->pVoice->Stop();
		this->status = PlayingStatus::Pausing;
	}
}

template <typename T>
void AudioHandler<T>::Resume() {
	if (this->status == PlayingStatus::Pausing) {
		this->status = PlayingStatus::Playing;
		this->pVoice->Start();
	}
}

template <typename T>
void AudioHandler<T>::BufferEndCallback() {
	Push();
}

/* Private Functions  ------------------------------------------------------------------------------- */
template <typename T>
void AudioHandler<T>::Push() {
	if (this->status == PlayingStatus::Stopped) {
		return;
	}

	if (this->pAudio->HasReadToEnd()) {
		if (this->isLoopPlayback) {
			this->pAudio->Reset();
		} else {
			Stop(false);
			if (this->pDelegate != nullptr) {
				this->pDelegate->OnPlayedToEnd(this->name);
				this->pDelegate = nullptr;
			} else if (this->onPlayedToEndCallback != nullptr) {
				this->onPlayedToEndCallback(this->name.c_str());
				this->onPlayedToEndCallback = nullptr;
			}
			return;
		}
	}

	// 音データ格納
	memset(this->pReadBuffers[this->currentBufNum], 0, this->bufferSize);
	long readLength = this->pAudio->Read(this->pReadBuffers[this->currentBufNum], this->bufferSize);

	if (readLength <= 0) {
		if (this->pAudio->HasReadToEnd()) {
			// ファイル末尾まで再生した後の処理
			Push();
		} else {
			// エラー発生による停止
			//this->Stop(true);

			// ファイル形式によりデコード不要なデータをデコードしてエラーとなるパターンがあるので
			// エラーが発生した場合も続きから読み込み直す
			Push();
		}
		return;
	}

	this->xAudioBuffer.AudioBytes = readLength;
	this->xAudioBuffer.pAudioData = this->pReadBuffers[this->currentBufNum];
	HRESULT ret = this->pVoice->SubmitSourceBuffer(&this->xAudioBuffer);
	if (FAILED(ret)) {
		Common::OutputDebugString("error SubmitSourceBuffer HRESULT=%d\n", ret);
		return;
	}

	if (BUF_COUNT <= ++this->currentBufNum) {
		this->currentBufNum = 0;
	}
}

template <typename T>
void AudioHandler<T>::Start() {
	this->xAudioBuffer.Flags = 0;
	this->bufferSize = this->pAudio->GetWaveFormatEx()->nAvgBytesPerSec;
	for (int i = 0; i < BUF_COUNT; ++i) {
		this->pReadBuffers[i] = new BYTE[this->bufferSize];
	}

	this->currentBufNum = 0;
	this->status = PlayingStatus::Playing;
	this->Push();
	this->pVoice->Start();
}

template <typename T>
void AudioHandler<T>::Stop(bool clearsCallback) {
	this->pVoice->Stop();
	this->status = PlayingStatus::Stopped;

	for (int i = 0; i < BUF_COUNT; i++) {
		delete[] this->pReadBuffers[i];
		this->pReadBuffers[i] = nullptr;
	}

	if (!this->pAudio->HasReadToEnd()) {
		this->pVoice->FlushSourceBuffers();
	}

	this->pAudio->Reset();
	if (clearsCallback) {
		this->pDelegate = nullptr;
		this->onPlayedToEndCallback = nullptr;
	}
}

template class AudioHandler<char>;
template class AudioHandler<wchar_t>;

}
