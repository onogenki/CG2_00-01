#include "TextureManager.h"
#include "DirectXCommon.h"
#include <cassert>

TextureManager* TextureManager::instance = nullptr;

//ImGuiで0番を使用するため、1番から使用
//uint32_t TextureManager::kSRVIndexTop = 1;

TextureManager* TextureManager::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new TextureManager;
	}
	return instance;
}

void TextureManager::Initialize(DirectXCommon* dxCommon,SrvManager* srvManager)
{//srvの数と同数
	assert(dxCommon);
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	textureDatas.reserve(DirectXCommon::kMaxSRVCount);
}

void TextureManager::Finalize()
{
	delete instance;
	instance = nullptr;
}

void TextureManager::LoadTexture(const std::string& filePath)
{
	//読み込みテクスチャを検索
	if (textureDatas.contains(filePath))
	{
		//読み込み済みなら早朝return 
		return;
	}

	//テクスチャ枚数上限チェック
	assert(srvManager_->CanAllocate());

	//テクスチャを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring wFilePath = std::wstring(filePath.begin(), filePath.end());
	HRESULT hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));


	DirectX::ScratchImage mipImages{};
	//MipMapの作成
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//追加したテクスチャデータの参照を取得する
	TextureData& textureData = textureDatas[filePath];

	textureData.filePath = filePath; //ファイルパス
	textureData.metadata = mipImages.GetMetadata(); //テクスチャメタデータを取得
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);

		//テクスチャデータの要素数番号をSRVのインデックスとする
		//uint32_t srvIndex = static_cast<uint32_t>(textureDatas.size() - 1)+kSRVIndexTop;

		textureData.srvIndex = srvManager_->Allocate();
		textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
		textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);



		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

		srvDesc.Format = textureData.metadata.format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(textureData.metadata.mipLevels);

		//SRVの設定を行う
		//設定をもとにSRVの生成
		dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);

		//テクスチャデータの転送(GPUへのキックから完了待ち、リセットまでをDirectXCommonに全て任せる)
		dxCommon_->ExecuteTextureTransfer(textureData.resource, mipImages);
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
	//読み込み済みかチェック
	assert(textureDatas.contains(filePath));
	return textureDatas[filePath].metadata;
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath)
{
	// 読み込み済みかチェック
	assert(textureDatas.contains(filePath));
	return textureDatas[filePath].srvIndex;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
	// 読み込み済みかチェック
	assert(textureDatas.contains(filePath));
	return textureDatas[filePath].srvHandleGPU;
}