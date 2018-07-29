﻿//----------------------------------------------------------
// <filename>Mp3Audio.cpp</filename>
// <author>Masami Sugao</author>
// <date>2018/07/16</date>
//----------------------------------------------------------
#include "Mp3Audio.h"
#include "../Common.h"


namespace {
// ビットレートのテーブル
const int BIT_RATE_TABLE[][16] = {
	// MPEG1, Layer1
	{ 0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,-1 },
	// MPEG1, Layer2
	{ 0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,-1 },
	// MPEG1, Layer3
	{ 0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,-1 },
	// MPEG2/2.5, Layer1,2
	{ 0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,-1 },
	// MPEG2/2.5, Layer3
	{ 0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,-1 }
};

// サンプリングレートのテーブル
const int SAMPLE_RATE_TABLE[][4] = {
	{ 44100, 48000, 32000, -1 }, // MPEG1
	{ 22050, 24000, 16000, -1 }, // MPEG2
	{ 11025, 12000, 8000, -1 } // MPEG2.5
};
}

namespace SoundLib {
namespace Audio {
/* Constructor / Destructor ------------------------------------------------------------------------- */
Mp3Audio::Mp3Audio() : hFile(nullptr), has(nullptr), hasReadToEnd(false) {
	this->ash = { 0 };
}

Mp3Audio::~Mp3Audio() {
	// ACMの後始末
	if ((this->ash.fdwStatus & ACMSTREAMHEADER_STATUSF_PREPARED) == ACMSTREAMHEADER_STATUSF_PREPARED) {
		acmStreamUnprepareHeader(this->has, &this->ash, 0);
		// 動的確保したデータを開放
		delete[] this->ash.pbSrc;
		this->ash.pbSrc = nullptr;
		delete[] this->ash.pbDst;
		this->ash.pbDst = nullptr;
	}

	if (this->has != nullptr) {
		acmStreamClose(this->has, 0);
		this->has = nullptr;
	}

	// ファイルを閉じる
	if (this->hFile != nullptr) {
		CloseHandle(this->hFile);
		this->hFile = nullptr;
	}
}


/* Getters / Setters -------------------------------------------------------------------------------- */
const WAVEFORMATEX* Mp3Audio::GetWaveFormatEx() const {
	return &this->waveFormatEx;
}

std::string Mp3Audio::GetFormatName() const {
	return "mp3";
}

int Mp3Audio::GetChannelCount() const {
	return this->channelCount;
}

int Mp3Audio::GetSamplingRate() const {
	return this->sampleRate;
}

int Mp3Audio::GetBitsPerSample() const {
	return this->bitsPerSample;
}

bool Mp3Audio::HasReadToEnd() const {
	return this->hasReadToEnd;
}


/* Public Functions  -------------------------------------------------------------------------------- */
bool Mp3Audio::Load(std::string filePath) {
	// ファイルを開く
	this->hFile = CreateFileA(
		filePath.c_str(),
		GENERIC_READ,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (this->hFile == INVALID_HANDLE_VALUE) {
		Common::OutputDebugString("CreateFile resutns INVALID_HANDLE_VALUE filePath=%s\n", filePath.c_str());
		return false;
	}

	// ファイルサイズ取得
	this->mp3DataSize = GetDataSize();

	// フレームヘッダ確認
	BYTE pHeader[4];
	DWORD readSize;

	ReadFile(this->hFile, pHeader, 4, &readSize, nullptr);
	if (!(pHeader[0] == 0xFF && (pHeader[1] & 0xE0) == 0xE0)) {
		Common::OutputDebugString("invalid file filePath=%s\n", filePath.c_str());
		return false;
	}

	// MPWGバージョン取得
	BYTE version = (pHeader[1] >> 3) & 0x03;

	//　ビットレート取得
	WORD bitRatePerMilliSec = GetBitRate(pHeader, version);

	// サンプルレート取得
	this->sampleRate = GetSampleRate(pHeader, version);

	BYTE padding = pHeader[2] >> 1 & 0x01;
	BYTE channel = pHeader[3] >> 6;
	this->channelCount = (channel == 3 ? 1 : 2);

	// サイズ取得
	WORD blockSize = ((144 * bitRatePerMilliSec * 1000) / this->sampleRate) + padding;

	// フォーマット取得
	MPEGLAYER3WAVEFORMAT mf;
	mf.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
	mf.wfx.nChannels = this->channelCount;
	mf.wfx.nSamplesPerSec = this->sampleRate;
	mf.wfx.nAvgBytesPerSec = (bitRatePerMilliSec * 1000) / 8;
	mf.wfx.nBlockAlign = 1;
	mf.wfx.wBitsPerSample = 0;
	mf.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
	this->bitsPerSample = (bitRatePerMilliSec * 1000) / this->sampleRate;

	mf.wID = MPEGLAYER3_ID_MPEG;
	mf.fdwFlags = padding ? MPEGLAYER3_FLAG_PADDING_ON : MPEGLAYER3_FLAG_PADDING_OFF;
	mf.nBlockSize = blockSize;
	mf.nFramesPerBlock = 1;
	mf.nCodecDelay = 1393;

	// wavフォーマット取得
	this->waveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
	acmFormatSuggest(
		nullptr,
		&mf.wfx,
		&this->waveFormatEx,
		sizeof(WAVEFORMATEX),
		ACM_FORMATSUGGESTF_WFORMATTAG
	);

	// ACMストリームを開く
	acmStreamOpen(&this->has, nullptr, &mf.wfx, &this->waveFormatEx, nullptr, 0, 0, 0);

	// WAV変換後サイズに対応する変換前サイズ取得
	DWORD mp3AvgBytesPerSec;
	acmStreamSize(this->has, this->waveFormatEx.nAvgBytesPerSec, &mp3AvgBytesPerSec, ACM_STREAMSIZEF_DESTINATION);

	this->ash.cbStruct = sizeof(ACMSTREAMHEADER);
	this->ash.pbSrc = new BYTE[mp3AvgBytesPerSec];
	this->ash.cbSrcLength = mp3AvgBytesPerSec;
	this->ash.pbDst = new BYTE[this->waveFormatEx.nAvgBytesPerSec];
	this->ash.cbDstLength = this->waveFormatEx.nAvgBytesPerSec;

	// デコード準備
	acmStreamPrepareHeader(this->has, &this->ash, 0);

	return true;
}

long Mp3Audio::Read(BYTE* pBuffer, long bufSize) {
	// WAV変換後サイズに対応する変換前サイズ取得
	DWORD mp3Bytes;
	acmStreamSize(this->has, bufSize, &mp3Bytes, ACM_STREAMSIZEF_DESTINATION);

	// ファイル読み込み
	DWORD readSize;
	ReadFile(this->hFile, this->ash.pbSrc, mp3Bytes, &readSize, NULL);
	if (readSize == 0) {
		this->hasReadToEnd = true;
		return 0;
	}
	this->ash.cbSrcLength = readSize;

	// デコード
	MMRESULT result = acmStreamConvert(this->has, &this->ash, ACM_STREAMCONVERTF_BLOCKALIGN);
	if (result != 0) {
		return 0;
	}

	if (this->ash.cbDstLengthUsed == 0) {
		this->hasReadToEnd = true;
	} else {
		// デコードしたWAVEデータを格納
		CopyMemory(pBuffer, this->ash.pbDst, this->ash.cbDstLengthUsed);
	}

	return this->ash.cbDstLengthUsed;
}

void Mp3Audio::Reset() {
	// ファイルポインタをMP3データの開始位置に移動
	SetFilePointer(this->hFile, this->offset, NULL, FILE_BEGIN);
	this->hasReadToEnd = false;
}

/* Private Functions  ------------------------------------------------------------------------------- */
DWORD Mp3Audio::GetDataSize() {
	DWORD ret;
	DWORD fileSize = GetFileSize(this->hFile, NULL);

	BYTE header[10];
	DWORD readSize;

	// ヘッダの読み込み
	ReadFile(this->hFile, header, 10, &readSize, NULL);

	// 先頭３バイトのチェック
	if (memcmp(header, _T("ID3"), 3) == 0) {
		// タグサイズを取得
		DWORD tagSize = ((header[6] << 21) | (header[7] << 14) | (header[8] << 7) | (header[9])) + 10;

		// データの位置、サイズを計算
		this->offset = tagSize;
		ret = fileSize - offset;
	} else {
		// 末尾のタグに移動
		BYTE tag[3];
		SetFilePointer(this->hFile, fileSize - 128, NULL, FILE_BEGIN);
		ReadFile(this->hFile, tag, 3, &readSize, NULL);

		// データの位置、サイズを計算
		this->offset = 0;
		if (memcmp(tag, _T("TAG"), 3) == 0)
			ret = fileSize - 128; // 末尾のタグを省く
		else
			ret = fileSize; // ファイル全体がMP3データ
	}

	// ファイルポインタをMP3データの開始位置に移動
	SetFilePointer(this->hFile, this->offset, NULL, FILE_BEGIN);

	return ret;
}

WORD Mp3Audio::GetBitRate(BYTE* pHeader, int version) const {
	//　レイヤー数取得
	BYTE layer = (pHeader[1] >> 1) & 0x03;

	INT index;
	if (version == 3) {
		index = 3 - layer;
	} else {
		if (layer == 3)
			index = 3;
		else
			index = 4;
	}

	return BIT_RATE_TABLE[index][pHeader[2] >> 4];
}

WORD Mp3Audio::GetSampleRate(BYTE* pHeader, int version) const {
	int index;
	switch (version) {
	case 0:
		index = 2;
		break;
	case 2:
		index = 1;
		break;
	case 3:
		index = 0;
		break;
	}

	return SAMPLE_RATE_TABLE[index][(pHeader[2] >> 2) & 0x03];
}

}
}
