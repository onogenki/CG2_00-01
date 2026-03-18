#pragma once
#include <Windows.h>
#include <cstdint>

class WinApp
{
public:
	//クライアント領域のサイズ
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;
	/// 静的メンバ関数
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM, LPARAM lparam);

	//メンバ関数
	void Initialize();

	void Update();
	
	//終了
	void Finalize();

	//getter
	HINSTANCE GetHInstance() const { return wc.hInstance; }

	//getter
	HWND GetHwnd() const { return hwnd; }

	//メッセージの処理
	bool ProcessMessage();

private:

	//ウィンドウハンドル
	HWND hwnd = nullptr;
	//ウィンドウクラスの設定
	WNDCLASS wc{};
};

