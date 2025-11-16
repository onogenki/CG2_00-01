#include "DirectXCommon.h"
#include<cassert>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

using namespace Microsoft::WRL;

void DirectXCommon::Initialize(WinApp* winApp)
{
	//NULL検出
	assert(winApp);

	//メンバ変数に記録
	this->winApp = winApp;


	//デバイスの生成();
	//コマンド関連の初期化();
	//スワップチェーンの生成();
	//深度バッファの生成();
	//各種デスクリプタヒープの生成();
	//レンダーターゲットビューの初期化();
	//深度ステンシルビューの初期化();
	//フェンスの初期化();
	//ビューポート矩形の初期化();
	//シザリング矩形の初期化();
	//DXCコンパイラの生成();
	//ImGuiの初期化();

}

void DirectXCommon:://デバイスの初期化()
{
	HRESULT hr;

	//デバックレイヤーをオンに
	//DXGIファクトリの生成
	//アダプターの列挙
	//デバイス生成
	//エラー時にブレークを発生させる設定
}

void DirectXCommon:: ()
{
	//コマンドアロケータ
}

void DirectXCommon:: ()
{
	//スワップチェーン
}

void DirectXCommon::()
{
	//深度バッファの生成
}

void DirectXCommon::()
{
	//RTV,SRV,DSVデスクリプタヒープの生成

}

void DirectXCommon::()
{
	//スワップチェーンからリソースを引っ張ってくる
	//RTV用の設定
	for (uint32_t i = 0; i < ; ++i)
	{
		//RTVハンドルを取得
	}
}

void DIrectXCommon::()
{
	//深度用の設定
}

void DirectXCommon::()
{
	//フェンス生成
}

void DirectXCommon::()
{
	//ビューポート矩形の設定
}

void DirectXCommon::()
{
	//シザリング矩形の設定
}

void DirectXCommon::()
{
	//DXCコンパイラの生成
}

void DirectXCommon::()
{
	//IMGUIの初期化
}



D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index)
{
	return GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}