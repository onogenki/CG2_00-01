#pragma once
#include "DirectXCommon.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <string>

//3Dオブジェクト共通部
class Object3dCommon
{
public:

	void Initialize(DirectXCommon* dxCommon);


	//共通描画設定
	void SetCommonDrawSetting();

	///getter
	DirectXCommon* GetDxCommon() const {
		return dxCommon_;
	}

private:

	DirectXCommon* dxCommon_;

	// ルートシグネチャの生成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();

	// RootSignature
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	// PipelineState
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState;

};

