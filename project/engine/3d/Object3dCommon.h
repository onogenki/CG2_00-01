#pragma once
#include "DirectXCommon.h"
#include "Camera.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <string>

//3Dオブジェクト共通部
class Object3dCommon
{
public:

	void Initialize(DirectXCommon* dxCommon);


	///getter
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	Camera* GetDefaultCamera() const { return defaultCamera_; }

	//共通描画設定
	void SetCommonDrawSetting();

	//setter
	void SetDefaultCamera(Camera* camera) { this->defaultCamera_ = camera; }

private:

	DirectXCommon* dxCommon_ = nullptr;

	Camera* defaultCamera_ = nullptr;

	// ルートシグネチャの生成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();

	// RootSignature
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	// PipelineState
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

};

