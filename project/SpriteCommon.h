#pragma once
#include "DirectXCommon.h"
//スプライト共通部
class SpriteCommon
{

	void Initialize(DirectXCommon* dxCommon);


	//共通描画設定
	void SetCommonDrawSetting();

private:
	DirectXCommon* dxCommon_;

	DirectXCommon* GetDxCommon() const { return dxCommon_; }


	//ルートシグネチャの作成
	void CreateRootSignature();
	//グラフィックスパイプラインの生成
	void CreateGraphicsPipelineState();
};

