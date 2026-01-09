#include "SpriteCommon.h"

void SpriteCommon::Initialize(DirectXCommon* dxCommon)
{
	//引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;

	//グラフィックスパイプラインの生成
	CreateGraphicsPipelineState();

	//ルートシグネチャをセットするコマンド
	CreateRootSignature();
	//グラフィックスパイプラインステートをセットするコマンド
	CreateGraphicsPipelineState();
	//プリミティブトポロジーをセットするコマンド
	SetCommonDrawSetting();
	
	SpriteCommon* spriteCommon = nullptr;

	spriteCommon = new SpriteCommon;
	spriteCommon->Initialize(dxCommon);

}


