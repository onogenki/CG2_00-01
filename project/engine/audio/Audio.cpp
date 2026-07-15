#include "Audio.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl.h>
#include <cassert>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

using namespace Microsoft::WRL;

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

void Audio::Initialize()
{
	//初期化処理
	HRESULT result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(result)) {
		return;
	}

	result = xAudio2_->CreateMasteringVoice(&masterVoice_);
	if (FAILED(result)) {
		xAudio2_.Reset();
		return;
	}

	result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	if (FAILED(result)) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
		xAudio2_.Reset();
	}
}

//音声データの読み込み
bool Audio::LoadFile(const std::string& filename)
{

	// 既に読み込み済みなら何もしない（二重読み込み防止）
	if (soundDatas_.find(filename) != soundDatas_.end()) {
		return true;
	}

	HRESULT result;

	// ファイルパスをワイド文字列に変換
	std::wstring filePathW = ConvertString(filename);

	//SourceReader作成
	ComPtr<IMFSourceReader> pReader;
	result = MFCreateSourceReaderFromURL(filePathW.c_str(), nullptr, &pReader);
	if (FAILED(result)) {
		return false;
	}

	//PCM形式にフォーマット指定する
	ComPtr<IMFMediaType> pPCMType;
	result = MFCreateMediaType(&pPCMType);
	if (FAILED(result)) {
		return false;
	}
	pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	result = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPCMType.Get());
	if (FAILED(result)) {
		return false;
	}

	//実際にセットされたメディアタイプを取得する
	ComPtr<IMFMediaType>pOutType;
	result = pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pOutType);
	if (FAILED(result)) {
		return false;
	}

	//Waveフォーマットを取得する
	WAVEFORMATEX* waveFormat = nullptr;
	result = MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);
	if (FAILED(result) || !waveFormat) {
		return false;
	}

	//コンテナに格納する音声データ
	SoundData soundData = {};
	soundData.wfex = *waveFormat;
	//生成したWaveフォーマットを開放
	CoTaskMemFree(waveFormat);

	while (true)
	{
		ComPtr<IMFSample> pSample;
		DWORD stramIndex = 0, flags = 0;
		LONGLONG llTimeStamp = 0;
		//サンプルを読み込む
		result = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &stramIndex, &flags, &llTimeStamp, &pSample);
		if (FAILED(result)) {
			return false;
		}
		//ストリームの末尾に達したら抜ける
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)break;

		if (pSample)
		{
			ComPtr<IMFMediaBuffer> pBuffer;
			//サンプルに含まれるサウンドデータのバッファを一繋ぎにして取得
			pSample->ConvertToContiguousBuffer(&pBuffer);

			BYTE* pData = nullptr;//データ読み取りポインタ
			DWORD maxLength = 0, currentLength = 0;
			//バッファの読み込み時にロック
			pBuffer->Lock(&pData, &maxLength, &currentLength);
			//バッファの末尾にデータを追加
			soundData.buffer.insert(soundData.buffer.end(), pData, pData + currentLength);
			pBuffer->Unlock();
		}
	}

	soundDatas_[filename] = soundData;
	return true;
}

//音声データ解放
void Audio::Unload()
{
	for (IXAudio2SourceVoice* voice : sourceVoices_) {
		voice->Stop();
		voice->DestroyVoice();
	}
	sourceVoices_.clear();
	//バッファのメモリを解放
	soundDatas_.clear();
}

//音声再生
bool Audio::PlayWave(const std::string& filename)
{
	HRESULT result;
	ReleaseFinishedVoices();

	// 本棚から音声データを探し出す
	auto soundIt = soundDatas_.find(filename);
	if (!xAudio2_ || soundIt == soundDatas_.end()) {
		return false;
	}
	SoundData& soundData = soundIt->second;

	//波形フォーマットを先にSourceVoiceの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	if (FAILED(result)) {
		return false;
	}

	//再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.buffer.data();
	buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
	buf.Flags = XAUDIO2_END_OF_STREAM;

	//波形データの再生
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	if (FAILED(result)) {
		pSourceVoice->DestroyVoice();
		return false;
	}
	result = pSourceVoice->Start();
	if (FAILED(result)) {
		pSourceVoice->DestroyVoice();
		return false;
	}
	sourceVoices_.push_back(pSourceVoice);
	return true;
}

void Audio::ReleaseFinishedVoices()
{
	for (auto it = sourceVoices_.begin(); it != sourceVoices_.end();) {
		XAUDIO2_VOICE_STATE state{};
		(*it)->GetState(&state);
		if (state.BuffersQueued == 0) {
			(*it)->DestroyVoice();
			it = sourceVoices_.erase(it);
		} else {
			++it;
		}
	}
}
