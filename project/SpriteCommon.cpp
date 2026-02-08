#include "SpriteCommon.h"

void SpriteCommon::Initialize(DirectXCommon* dxCommon)
{
	//引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;

}

//グラフィックスパイプラインの生成
void CreateGraphicsPipelineState()
{
	// ラスタライザの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};

	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//カリングしない(裏面も表示させる)
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
}

	//ルートシグネチャをセットするコマンド
void CreateRootSignature() {};

	//プリミティブトポロジーをセットするコマンド
void SetCommonDrawSetting() {};

