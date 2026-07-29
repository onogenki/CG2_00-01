#pragma once

#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Matrix4x4.h"

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
	void SetSmoothing(bool isSmoothing) { isSmoothing_ = isSmoothing; }
	void SetGaussianFilter(bool isGaussianFilter) { isGaussianFilter_ = isGaussianFilter; }
	void SetLuminanceBasedOutline(bool isLuminanceBasedOutline) { isLuminanceBasedOutline_ = isLuminanceBasedOutline; }
	void SetDepthBasedOutline(bool isDepthBasedOutline) { isDepthBasedOutline_ = isDepthBasedOutline; }
	void SetProjectionInverse(const Matrix4x4& projectionInverse);
	bool IsGrayscale() const { return isGrayscale_; }
	bool IsSepia() const { return isSepia_; }
	bool IsVignette() const { return isVignette_; }
	bool IsSmoothing() const { return isSmoothing_; }
	bool IsGaussianFilter() const { return isGaussianFilter_; }
	bool IsLuminanceBasedOutline() const { return isLuminanceBasedOutline_; }
	bool IsDepthBasedOutline() const { return isDepthBasedOutline_; }
	bool IsEnabled() const { return isGrayscale_ || isSepia_ || isVignette_ || isSmoothing_ || isGaussianFilter_ || isLuminanceBasedOutline_ || isDepthBasedOutline_; }

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
		LuminanceBasedOutline,
		DepthBasedOutline,
		GaussianFilterHorizontal,
		GaussianFilterVertical,
		Smoothing,
		Count
	};

	void CreateRootSignature();
	void CreateGraphicsPipeline();
	// 指定ピクセルシェーダーをコンパイルして、対応するPSOを作成する。
	void CreateGraphicsPipelineState(PipelineType type, const wchar_t* pixelShaderPath);
	void DrawWithPipeline(uint32_t sourceSrvIndex, PipelineType pipelineType);

	struct DepthBasedOutlineData
	{
		Matrix4x4 projectionInverse;
	};

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStates_[static_cast<int>(PipelineType::Count)];
	Microsoft::WRL::ComPtr<ID3D12Resource> depthBasedOutlineResource_;
	DepthBasedOutlineData* depthBasedOutlineData_ = nullptr;
	bool isGrayscale_ = false;
	bool isSepia_ = false;
	bool isVignette_ = false;
	bool isSmoothing_ = false;
	bool isGaussianFilter_ = false;
	bool isLuminanceBasedOutline_ = false;
	bool isDepthBasedOutline_ = false;
};
