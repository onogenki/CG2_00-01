#pragma once
#include "DirectXCommon.h"
//スプライト共通部
class SpriteCommon
{
	public:

	void Initialize(DirectXCommon* dxCommon);

	ID3D12Device* GetDevice() const { return dxCommon_->GetDevice(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return dxCommon_->GetCommandList(); }

private:
	DirectXCommon* dxCommon_;

	//DirectXCommon* GetDxCommon() const { return dxCommon_; }


	//ルートシグネチャの作成
	void CreateRootSignature();
	//グラフィックスパイプラインの生成
	void CreateGraphicsPipelineState();

	void SetCommonDrawSetting();
};

