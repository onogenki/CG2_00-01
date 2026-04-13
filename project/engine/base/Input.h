#pragma once
#include<wrl.h>

#define DIRECTINPUT_VERSION 0x0800  //DirectInputのバージョン指定
#include<dinput.h>
#include"WinApp.h"

class Input
{
public:

	static Input* GetInstance()
	{
		static Input instance;
		return& instance;
	}

	Input(const Input&) = delete;
	Input& operator=(const Input&) = delete;

	//namespace省略
	template<class T>using ComPtr = Microsoft::WRL::ComPtr <T>;

	void Initialize(WinApp* winApp);

	void InitializeDXGIDevice();

	void Update();

	bool PushKey(BYTE keyNumber);

	bool TriggerKey(BYTE keyNumber);

private:

	Input() = default;
	~Input() = default;

	//DirectInputのインスタンス
	ComPtr<IDirectInput8>directInput;

	//キーボードのデバイス
	ComPtr<IDirectInputDevice8>keyboard;

	//今回の全キーの状態
	BYTE key[256] = {};

	//前回の全キーの状態
	BYTE keyPre[256] = {};

	//WindowsAPI
	WinApp* winApp_ = nullptr;
};

