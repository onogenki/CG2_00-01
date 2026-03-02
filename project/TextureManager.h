#pragma once
#include "SrvManager.h"
#include <string>
#include <unordered_map>
#include <dxgi1_6.h>

#include "externals/DirectXTex/d3dx12.h"
#include "externals/DirectXTex/DirectXTex.h"

// 前方宣言
class DirectXCommon;
class SrvManager;

//テクスチャマネージャー
class TextureManager
{
public:
	//シングルトンインスタンスの取得
	static TextureManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	void Finalize();

	void LoadTexture(const std::string& filePath);

	//メタデータを取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);
	//SRVインデックスの取得
	uint32_t GetSrvIndex(const std::string& filePath);
	//GPUハンドルの取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

private:

	static TextureManager* instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

	//SRVインデックスの開始番号
	//static uint32_t kSRVIndexTop;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	//テクスチャ1枚分のデータ
	struct TextureData
	{
		std::string filePath;
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	//テクスチャデータ
	std::unordered_map<std::string, TextureData>textureDatas;

};

