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

	static Object3dCommon* GetInstance() {
		static Object3dCommon instance;
		return &instance;
	}

	Object3dCommon(const Object3dCommon&) = delete;
	Object3dCommon& operator=(const Object3dCommon&) = delete;

	void Initialize(DirectXCommon* dxCommon);


	///getter
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	Camera* GetDefaultCamera() const { return defaultCamera_; }
	const std::string& GetEnvironmentTexturePath() const { return environmentTexturePath_; }

	//共通描画設定
	void SetCommonDrawSetting();
	void SetSkinningComputeSetting();

	//setter
	void SetDefaultCamera(Camera* camera) { this->defaultCamera_ = camera; }
	void SetEnvironmentTexturePath(const std::string& path) {
		environmentTexturePath_ = path;
	}

	//スキニング用の共通描画設定
	void SetSkinningCommonDrawSetting();

	// Skybox用のルートシグネチャとパイプラインを取得するゲッター
	ID3D12RootSignature* GetSkyboxRootSignature() const { return skyboxRootSignature_.Get(); }
	ID3D12PipelineState* GetSkyboxPipelineState() const { return skyboxGraphicsPipelineState_.Get(); }

private:

	Object3dCommon() = default;
	~Object3dCommon() = default;

	DirectXCommon* dxCommon_ = nullptr;

	Camera* defaultCamera_ = nullptr;

	std::string environmentTexturePath_ = "";

	// ルートシグネチャの生成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();

	// RootSignature
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	// PipelineState
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// スキニング用のルートシグネチャとパイプライン
	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningGraphicsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningComputeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningComputePipelineState_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> skyboxRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skyboxGraphicsPipelineState_;

	// スキニング用の生成関数
	void CreateSkinningRootSignature();
	void CreateSkinningGraphicsPipeline();
	void CreateSkinningComputeRootSignature();
	void CreateSkinningComputePipeline();

	//Skybox用生成関数
	void CreateSkyboxRootSignature();
	void CreateSkyboxPipeline();

};

