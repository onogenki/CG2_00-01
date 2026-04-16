#pragma once

#include <vector>
#include <string>
#include <windows.h>
#include <cstdint>
#include <xaudio2.h>
#include <wrl.h>
#include <unordered_map>

//音声データ
struct SoundData {
	//波形フォーマット
	WAVEFORMATEX wfex{};
	//バッファ
	std::vector<BYTE>buffer{};
};

class Audio
{

public:

	// シングルトンインスタンスの取得
	static Audio* GetInstance() {
		static Audio instance;
		return &instance;
	}

	// コピーコンストラクタと代入演算子を禁止
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;

	void Initialize();

	void LoadFile(const std::string& filename);
	void PlayWave(const std::string& filename);
	void Unload();

private:
	Audio() = default;
	~Audio() = default;

	// xAudio2の本体をFrameworkから引っ越してきて、このクラスの持ち物にする
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	std::unordered_map<std::string, SoundData> soundDatas_;
};