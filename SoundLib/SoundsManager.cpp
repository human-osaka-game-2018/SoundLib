﻿#include <typeinfo>
#include "SoundsManager.h"
#include "Common.h"
#include "WaveAudio.h"
#include "Mp3Audio.h"


namespace SoundLib {
SoundsManager::SoundsManager() : pXAudio2(nullptr) {}

SoundsManager::~SoundsManager() {
	for (auto& rAudioPair : this->audioMap) {
		delete rAudioPair.second;
	}
	this->audioMap.clear();

	if (this->pXAudio2 != nullptr) {
		this->pXAudio2->Release();
		this->pXAudio2 = nullptr;
	}
	CoUninitialize();
}

bool SoundsManager::Initialize() {
	HRESULT ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(ret)) {
		OutputDebugStringEx(_T("error CoInitializeEx ret=%d\n"), ret);
		return false;
	}

	ret = XAudio2Create(
		&this->pXAudio2
		// UINT32 Flags = 0,
		// XAUDIO2_PROCESSOR XAudio2Processor = XAUDIO2_DEFAULT_PROCESSOR
	);
	if (FAILED(ret)) {
		OutputDebugStringEx(_T("error XAudio2Create ret=%d\n"), ret);
		return false;
	}

	IXAudio2MasteringVoice* master = NULL;
	ret = this->pXAudio2->CreateMasteringVoice(
		&master
		// UINT32 InputChannels = XAUDIO2_DEFAULT_CHANNELS,
		// UINT32 InputSampleRate = XAUDIO2_DEFAULT_SAMPLERATE,
		// UINT32 Flags = 0,
		// UINT32 DeviceIndex = 0,
		// const XAUDIO2_EFFECT_CHAIN *pEffectChain = NULL
	);
	if (FAILED(ret)) {
		OutputDebugStringEx(_T("error CreateMasteringVoice ret=%d\n"), ret);
		return false;
	}

	return true;
}

bool SoundsManager::AddFile(const TCHAR* pFilePath, const TCHAR* pKey) {
	if (ExistsKey(pKey)) {
		OutputDebugStringEx(_T("キー%sは既に登録済み。\n"), pKey);
		return false;
	}
	
	const TCHAR* pExtension = _tcsstr(pFilePath, _T("."));

	IAudio* pAudio;
	if (pExtension != nullptr && strcmp(pExtension, _T(".wav")) == 0) {
		pAudio = new WaveAudio();
	} else {
		pAudio = new Mp3Audio();
	} 

	if (!pAudio->Load(pFilePath)) {
		// 他の形式で読めるか試してみる
		if (typeid(*pAudio) == typeid(WaveAudio)) {
			delete pAudio;
			pAudio = new Mp3Audio();
		} else {
			delete pAudio;
			pAudio = new WaveAudio();
		}

		if (!pAudio->Load(pFilePath)) {
			OutputDebugStringEx(_T("%sはWAVE又MP3形式ではありません。\n"), pFilePath);
			delete pAudio;
			return false;
		}
	}

	const WAVEFORMATEX* pFormat = pAudio->GetWaveFormatEx();
	OutputDebugStringEx(_T("-----------------キー%sのオーディオ情報--------------------\n"), pKey);
	OutputDebugStringEx(_T("foramt    =%d\n"), pFormat->wFormatTag);
	OutputDebugStringEx(_T("channel   =%d\n"), pFormat->nChannels);
	OutputDebugStringEx(_T("sampling  =%dHz\n"), pFormat->nSamplesPerSec);
	OutputDebugStringEx(_T("bit/sample=%d\n"), pFormat->wBitsPerSample);
	OutputDebugStringEx(_T("byte/sec  =%d\n"), pFormat->nAvgBytesPerSec);
	OutputDebugStringEx(_T("-----------------------------------------------------------\n"));

	this->audioMap[pKey] = new AudioHandler(pKey, pAudio);
	return this->audioMap[pKey]->Prepare(this->pXAudio2);
}

bool SoundsManager::Start(const TCHAR* pKey, bool isLoopPlayback) {
	if (!ExistsKey(pKey)) {
		OutputDebugStringEx(_T("キー%sは存在しません。\n"), pKey);
		return false;
	}

	this->audioMap[pKey]->Start(isLoopPlayback);
	return true;
}

bool SoundsManager::Start(const TCHAR* pKey, ISoundsManagerDelegate* pDelegate) {
	if (!ExistsKey(pKey)) {
		OutputDebugStringEx(_T("キー%sは存在しません。\n"), pKey);
		return false;
	}
	
	this->audioMap[pKey]->Start(pDelegate);
	return true;
}

bool SoundsManager::Start(const TCHAR* pKey, void(*onPlayedToEndCallback)(const TCHAR* pKey)) {
	if (!ExistsKey(pKey)) {
		OutputDebugStringEx(_T("キー%sは存在しません。\n"), pKey);
		return false;
	}
	
	this->audioMap[pKey]->Start(onPlayedToEndCallback);
	return true;
}

bool SoundsManager::Stop(const TCHAR* pKey) {
	if (!ExistsKey(pKey)) {
		OutputDebugStringEx(_T("キー%sは存在しません。\n"), pKey);
		return false;
	}
	
	this->audioMap[pKey]->Stop();
	return true;
}

bool SoundsManager::Pause(const TCHAR* pKey) {
	if (!ExistsKey(pKey)) {
		OutputDebugStringEx(_T("キー%sは存在しません。\n"), pKey);
		return false;
	}
	
	this->audioMap[pKey]->Pause();
	return true;
}

bool SoundsManager::Resume(const TCHAR* pKey) {
	if (!ExistsKey(pKey)) {
		OutputDebugStringEx(_T("キー%sは存在しません。\n"), pKey);
		return false;
	}
	
	this->audioMap[pKey]->Resume();
	return true;
}

PlayingStatus SoundsManager::GetStatus(const TCHAR* pKey) {
	if (!ExistsKey(pKey)) {
		OutputDebugStringEx(_T("キー%sは存在しません。\n"), pKey);
		throw std::invalid_argument(_T("キーが存在しません。"));
	}
	
	return this->audioMap[pKey]->GetStatus();
}

bool SoundsManager::ExistsKey(const TCHAR* pKey) {
	auto itr = this->audioMap.find(pKey);
	return (itr != this->audioMap.end());
}
}
