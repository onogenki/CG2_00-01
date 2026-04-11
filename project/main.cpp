#include <Windows.h>
#include "Game.h"

#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"dxguid.lib")

using namespace Microsoft::WRL;

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	//誰も補足しなかった場合に(Unhandled),補足する関数を登録

	struct D3DResourceLeakChecker
	{
		~D3DResourceLeakChecker(){}
	}leakChecker;
	
	//インスタンス生成
	Framework* game = new Game();

	//実行
	game->Run();

	//削除
	delete game;

	return 0;
}