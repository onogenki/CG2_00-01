#pragma once

#include "DirectXCommon.h"
#include "SrvManager.h"

// RenderTextureを全画面三角形へ貼り、SwapChainへ描画する。
// グレースケールとセピアは同じ入力テクスチャに対して切り替える。
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

	// 共通のルートシグネチャと各ポストエフェクト用のPSOを生成する。
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	// RenderTextureをそのままSwapChainへ転送する。
	void Draw();
	// 指定SRVを描画する。useEffectがfalseなら通常の転送だけを行う。
	void Draw(uint32_t sourceSrvIndex, bool useEffect);
	// GaussianFilterの縦方向のぼかしを描画する。
	void DrawGaussianVertical(uint32_t sourceSrvIndex);
	void SetGrayscale(bool isGrayscale) { isGrayscale_ = isGrayscale; }
	void SetSepia(bool isSepia) { isSepia_ = isSepia; }
	void SetVignette(bool isVignette) { isVignette_ = isVignette; }
	void SetGaussianFilter(bool isGaussianFilter) { isGaussianFilter_ = isGaussianFilter; }
	void SetBoxFilter(bool isBoxFilter) { isBoxFilter_ = isBoxFilter; }
	bool IsGrayscale() const { return isGrayscale_; }
	bool IsSepia() const { return isSepia_; }
	bool IsVignette() const { return isVignette_; }
	bool IsGaussianFilter() const { return isGaussianFilter_; }
	bool IsBoxFilter() const { return isBoxFilter_; }
	bool IsEnabled() const { return isGrayscale_ || isSepia_ || isVignette_ || isGaussianFilter_ || isBoxFilter_; }

private:
	PostEffect() = default;
	~PostEffect() = default;

	// 描画時に選択するピクセルシェーダーの種類。
	enum class PipelineType
	{
		Fullscreen,
		Grayscale,
		Sepia,
		Vignette,
		GaussianFilterHorizontal,
		GaussianFilterVertical,
		BoxFilter,
		Count
	};

	void CreateRootSignature();
	void CreateGraphicsPipeline();
	// 指定ピクセルシェーダーをコンパイルして、対応するPSOを作成する。
	void CreateGraphicsPipelineState(PipelineType type, const wchar_t* pixelShaderPath);
	void DrawWithPipeline(uint32_t sourceSrvIndex, PipelineType pipelineType);

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStates_[static_cast<int>(PipelineType::Count)];
	bool isGrayscale_ = false;
	bool isSepia_ = false;
	bool isVignette_ = false;
	bool isGaussianFilter_ = false;
	bool isBoxFilter_ = false;
};
