#pragma once

#include "Audio.h"
#include <string>

// SceneがBGMを一曲だけ管理するための部品です。
// Audio本体は効果音を含む実際の再生を担当し、このクラスはBGMのループ・音量・フェードだけを担当します。
class BgmPlayer
{
public:
	// Audioは所有しません。Frameworkで初期化済みの共通Audioを渡します。
	void Initialize(Audio* audio);
	// 音源をループ再生します。fadeInSecondsを0より大きくすると無音から徐々に大きくなります。
	bool Play(const std::string& filename, float volume = 1.0f, float fadeInSeconds = 0.0f);
	// fadeOutSecondsを0より大きくすると、音量が0になった後で再生を停止します。
	void Stop(float fadeOutSeconds = 0.0f);
	void Pause();
	void Resume();
	// 次のUpdateを待たず、指定音量へ即座に変更します。
	void SetVolume(float volume);
	// フェード中の音量を更新します。SceneのUpdateから一度だけ呼びます。
	void Update(float deltaTime);

	bool IsPlaying() const;
	bool IsPaused() const { return isPaused_; }
	const std::string& GetFilename() const { return filename_; }

private:
	Audio* audio_ = nullptr;
	Audio::VoiceId voiceId_ = Audio::kInvalidVoiceId;
	std::string filename_;
	float currentVolume_ = 1.0f;
	float targetVolume_ = 1.0f;
	float fadeSpeed_ = 0.0f;
	bool stopWhenSilent_ = false;
	bool isPaused_ = false;

	// 曲固有の音量へAudioMixerのBGM全体音量を掛け、実際のVoiceへ設定します。
	void ApplyMixedVolume();
};
