#include <Windows.h>
#include "Game.h"
#include <memory>
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"dxguid.lib")

using namespace Microsoft::WRL;

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	struct D3DResourceLeakChecker
	{
		~D3DResourceLeakChecker(){}
	}leakChecker;
	
	//インスタンス生成
	std::unique_ptr<Framework> game = std::make_unique<Game>();

	//実行
	game->Run();

	return 0;
}