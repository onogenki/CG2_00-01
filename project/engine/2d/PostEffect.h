#pragma once

#include "DirectXCommon.h"
#include "SrvManager.h"

// RenderTextureを全画面三角形へ貼り、SwapChainへ描画する
class PostEffect
{
public:
	static PostEffect* GetInstance()
	{
		static PostEffect instance;
		return &instance;
	}

	PostEffect(const PostEffect&) = delete;
	PostEffect& operator=(const PostEffect&) = delete;

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Draw();

private:
	PostEffect() = default;
	~PostEffect() = default;

	void CreateRootSignature();
	void CreateGraphicsPipeline();

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
};
