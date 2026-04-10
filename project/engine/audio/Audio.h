#pragma once
#include <cstdint>
#include <xaudio2.h>

// チャンクヘッダ
struct ChunkHeader {
	//チャンク毎のID
	char id[4];
	//チャンクサイズ
	int32_t size;
};

//RIFFヘッダチャンク
struct RiffHeader {
	//RIFF
	ChunkHeader chunk;
	//WAVE
	char type[4];
};

//FMTチャンク
struct FormatChunk {
	//fmt
	ChunkHeader chunk;
	//波形フォーマット
	WAVEFORMATEX fmt;
};

//音声データ
struct SoundData {
	//波形フォーマット
	WAVEFORMATEX wfex;
	//バッファの先頭アドレス
	BYTE* pBuffer;
	//バッファのサイズ
	unsigned int bufferSize;
};
