#pragma once
#include <Windows.h>

class WinApp
{
public:
	/// 静的メンバ関数
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM, LPARAM lparam);

	//メンバ関数
	void Initialize();

	void Update();

private:


};

