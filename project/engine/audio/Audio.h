#pragma once

#include <vector>
#include <string>
#include <windows.h>
#include <cstdint>
#include <xaudio2.h>

//音声データ
struct SoundData {
	//波形フォーマット
	WAVEFORMATEX wfex;
	//バッファ
	std::vector<BYTE>buffer;
};

SoundData SoundLoadFile(const std::string& filename);
void SoundUnload(SoundData* soundData);
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData);
