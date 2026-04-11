#include <Windows.h>
#include "Game.h"

#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"mfplat.lib")
#pragma comment(lib,"Mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")

using namespace Microsoft::WRL;

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	//誰も補足しなかった場合に(Unhandled),補足する関数を登録

	struct D3DResourceLeakChecker
	{
		~D3DResourceLeakChecker(){}
	}leakChecker;
	
	//ゲームの生成と初期化
	Game* game = new Game();
	game->Initialize();

	//メインループ
	MSG msg{};
	//ウィンドウのxボタンが押されるまでループ
	while (true)
	{
		//更新処理
		game->Update();
		//終了リクエストが来ていたらループを抜ける
		if (game->IsEndRequest())
		{
			break;
		}
		//描画処理
		game->Draw();
	}

	//ゲームの終了処理
	game->Finalize();

	delete game;

	return 0;
}