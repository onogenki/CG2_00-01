#pragma once
#include <Windows.h>

class WinApp
{
public:
	//クライアント領域のサイズ
	static const uint32_t kClientWidth = 1280;
	static const uint32_t kClientHeight = 720;
	/// 静的メンバ関数
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM, LPARAM lparam);

	//メンバ関数
	void Initialize();

	void Update();

	//getter
	HWND GetHwnd() const { return hwnd; }

private:

	//ウィンドウハンドル
	HWND hwnd = nullptr;

};

