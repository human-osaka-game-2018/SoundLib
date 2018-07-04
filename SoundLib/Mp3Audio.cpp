#include "Mp3Audio.h"
#include "Common.h"


namespace {
	// �r�b�g���[�g�̃e�[�u��
	const WORD BIT_RATE_TABLE[][16] = {
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

	// �T���v�����O���[�g�̃e�[�u��
	const WORD SAMPLE_RATE_TABLE[][4] = {
		{ 44100, 48000, 32000, -1 }, // MPEG1
		{ 22050, 24000, 16000, -1 }, // MPEG2
		{ 11025, 12000, 8000, -1 } // MPEG2.5
	};
}

Mp3Audio::Mp3Audio() : hFile(nullptr), has(nullptr), pAsh(nullptr), pos(0) {}

Mp3Audio::~Mp3Audio() {
	// ACM�̌�n��
	if (this->pAsh != nullptr) {
		acmStreamUnprepareHeader(this->has, this->pAsh, 0);
		// ���I�m�ۂ����f�[�^���J��
		delete[] this->pAsh->pbSrc;
		delete[] this->pAsh->pbDst;

		this->pAsh = nullptr;
	}

	if (this->has != nullptr) {
		acmStreamClose(this->has, 0);
		this->has = nullptr;
	}

	// �t�@�C�������
	if (this->hFile != nullptr) {
		CloseHandle(this->hFile);
		this->hFile = nullptr;
	}
}

bool Mp3Audio::Load(const char* pFilePath) {
	// �t�@�C�����J��
	this->hFile = CreateFile(
		pFilePath,
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (this->hFile == INVALID_HANDLE_VALUE)
		return FALSE; // �G���[

	// �t�@�C���T�C�Y�擾
	this->mp3DataSize = GetDataSize();

	// �t���[���w�b�_�m�F
	BYTE header[4];
	DWORD readSize;

	ReadFile(this->hFile, header, 4, &readSize, NULL);
	if (!(header[0] == 0xFF && (header[1] & 0xE0) == 0xE0))
		return FALSE;

	// MPWG�o�[�W�����擾
	BYTE version = (header[1] >> 3) & 0x03;

	//�@�r�b�g���[�g�擾
	WORD bitRate = GetBitRate(header, version);

	// �T���v�����[�g�擾
	WORD sampleRate = GetSampleRate(header, version);

	BYTE padding = header[2] >> 1 & 0x01;
	BYTE channel = header[3] >> 6;

	// �T�C�Y�擾
	WORD blockSize = ((144 * bitRate * 1000) / sampleRate) + padding;

	// �t�H�[�}�b�g�擾
	MPEGLAYER3WAVEFORMAT mf;
	mf.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
	mf.wfx.nChannels = channel == 3 ? 1 : 2;
	mf.wfx.nSamplesPerSec = sampleRate;
	mf.wfx.nAvgBytesPerSec = (bitRate * 1000) / 8;
	mf.wfx.nBlockAlign = 1;
	mf.wfx.wBitsPerSample = 0;
	mf.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;

	mf.wID = MPEGLAYER3_ID_MPEG;
	mf.fdwFlags = padding ? MPEGLAYER3_FLAG_PADDING_ON : MPEGLAYER3_FLAG_PADDING_OFF;
	mf.nBlockSize = blockSize;
	mf.nFramesPerBlock = 1;
	mf.nCodecDelay = 1393;

	// wav�t�H�[�}�b�g�擾
	this->waveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
	acmFormatSuggest(
		NULL,
		&mf.wfx,
		&this->waveFormatEx,
		sizeof(WAVEFORMATEX),
		ACM_FORMATSUGGESTF_WFORMATTAG
	);

	// ACM�X�g���[�����J��
	acmStreamOpen(&this->has, NULL, &mf.wfx, &this->waveFormatEx, NULL, 0, 0, 0);

	// WAV�ϊ���̃u���b�N�T�C�Y�擾
	DWORD mp3BlockSize = blockSize;
	DWORD waveBlockSize;
	acmStreamSize(this->has, mp3BlockSize, &waveBlockSize, ACM_STREAMSIZEF_SOURCE);

	this->pAsh = new ACMSTREAMHEADER();
	*(this->pAsh) = { 0 };
	this->pAsh->cbStruct = sizeof(ACMSTREAMHEADER);
	this->pAsh->pbSrc = new BYTE[mp3BlockSize];
	this->pAsh->cbSrcLength = mp3BlockSize;
	this->pAsh->pbDst = new BYTE[waveBlockSize];
	this->pAsh->cbDstLength = waveBlockSize;

	// �f�R�[�h����
	acmStreamPrepareHeader(this->has, this->pAsh, 0);

	return TRUE;
}

long Mp3Audio::Read(BYTE* pBuffer, DWORD bufSize) {
	DWORD bufRead = 0;	// �o�b�t�@��ǂݍ��񂾃T�C�Y

	while (this->mp3DataSize - this->pos >= this->pAsh->cbSrcLength) { // MP3�f�[�^�̏I�[�H
		// �P�u���b�N������MP3�f�[�^��ǂݍ���
		DWORD readSize;
		ReadFile(this->hFile, this->pAsh->pbSrc, this->pAsh->cbSrcLength, &readSize, NULL);
		acmStreamConvert(this->has, this->pAsh, ACM_STREAMCONVERTF_BLOCKALIGN);
		this->pos += readSize;

		if (bufSize - bufRead > this->pAsh->cbDstLengthUsed) {
			// WAVE�f�[�^���i�[����o�b�t�@�ɗ]�T������΁A
			// �f�R�[�h����WAVE�f�[�^�����̂܂܊i�[
			CopyMemory(pBuffer + bufRead, this->pAsh->pbDst, this->pAsh->cbDstLengthUsed);
			bufRead += this->pAsh->cbDstLengthUsed;
		} else {
			// WAVE�f�[�^���i�[����o�b�t�@�ɗ]�T���Ȃ���΁A
			// �o�b�t�@�̎c�蕪�����f�[�^����������
			CopyMemory(pBuffer + bufRead, this->pAsh->pbDst, bufSize - bufRead);
			bufRead += bufSize - bufRead;
			break;
		}
	}

	return bufRead;
}

const WAVEFORMATEX* Mp3Audio::GetWaveFormatEx() {
	return &this->waveFormatEx;
}

void Mp3Audio::Reset() {
	// �t�@�C���|�C���^��MP3�f�[�^�̊J�n�ʒu�Ɉړ�
	SetFilePointer(this->hFile, this->offset, NULL, FILE_BEGIN);
	this->pos = 0;
}

DWORD Mp3Audio::GetDataSize() {
	DWORD ret;
	DWORD fileSize = GetFileSize(this->hFile, NULL);

	BYTE header[10];
	DWORD readSize;

	// �w�b�_�̓ǂݍ���
	ReadFile(this->hFile, header, 10, &readSize, NULL);

	// �擪�R�o�C�g�̃`�F�b�N
	if (memcmp(header, "ID3", 3) == 0) {
		// �^�O�T�C�Y���擾
		DWORD tagSize = ((header[6] << 21) | (header[7] << 14) | (header[8] << 7) | (header[9])) + 10;

		// �f�[�^�̈ʒu�A�T�C�Y���v�Z
		this->offset = tagSize;
		ret = fileSize - offset;
	} else {
		// �����̃^�O�Ɉړ�
		BYTE tag[3];
		SetFilePointer(this->hFile, fileSize - 128, NULL, FILE_BEGIN);
		ReadFile(this->hFile, tag, 3, &readSize, NULL);

		// �f�[�^�̈ʒu�A�T�C�Y���v�Z
		this->offset = 0;
		if (memcmp(tag, "TAG", 3) == 0)
			ret = fileSize - 128; // �����̃^�O���Ȃ�
		else
			ret = fileSize; // �t�@�C���S�̂�MP3�f�[�^
	}

	// �t�@�C���|�C���^��MP3�f�[�^�̊J�n�ʒu�Ɉړ�
	SetFilePointer(this->hFile, this->offset, NULL, FILE_BEGIN);

	return ret;
}

WORD Mp3Audio::GetBitRate(BYTE header[], int version) {
	//�@���C���[���擾
	BYTE layer = (header[1] >> 1) & 0x03;

	INT index;
	if (version == 3) {
		index = 3 - layer;
	} else {
		if (layer == 3)
			index = 3;
		else
			index = 4;
	}

	return BIT_RATE_TABLE[index][header[2] >> 4];
}

WORD Mp3Audio::GetSampleRate(BYTE header[], int version) {
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

	return SAMPLE_RATE_TABLE[index][(header[2] >> 2) & 0x03];
}