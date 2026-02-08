#include "Object3dCommon.h"
#include <cassert>
#include <vector>

void Object3dCommon::Initialize(DirectXCommon* dxCommon)
{
	assert(dxCommon);
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	CreateRootSignature();

	// グラフィックスパイプラインの生成
	CreateGraphicsPipeline();
}

void Object3dCommon::SetCommonDrawSetting()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	//ルートシグネチャをセットするコマンド
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	//グラフィックスパイプラインステートをセットするコマンド
	commandList->SetPipelineState(graphicsPipelineState.Get());
	//プリミティブトポロジーをセットするコマンド
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}