#include "TextureManager.h"
#include "DirectXCommon.h"
#include <cassert>

TextureManager* TextureManager::instance = nullptr;

//ImGuiで0番を使用するため、1番から使用
uint32_t TextureManager::kSRVIndexTop = 1;

TextureManager* TextureManager::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new TextureManager;
	}
	return instance;
}

void TextureManager::Initialize(DirectXCommon* dxCommon)
{//srvの数と同数
	assert(dxCommon);
	dxCommon_ = dxCommon;
	textureDatas.reserve(DirectXCommon::kMaxSRVCount);
}

void TextureManager::Finalize()
{
	delete instance;
	instance = nullptr;
}

void TextureManager::LoadTexture(const std::string& filePath)
{
	//読み込み済みテクスチャを検索
	auto it = std::find_if(
		textureDatas.begin(),
		textureDatas.end(),
		[&](TextureData& textureData) { return textureData.filePath == filePath; }
	);
	if (it != textureDatas.end())
	{
		//読み込み済みなら早朝return 
		return;
	}

	//テクスチャ枚数上限チェック
	assert(textureDatas.size() + kSRVIndexTop < DirectXCommon::kMaxSRVCount);

	//テクスチャを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring wFilePath = std::wstring(filePath.begin(), filePath.end());
	HRESULT hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));


	DirectX::ScratchImage mipImages{};
	//MipMapの作成
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//テクスチャデータを追加
	textureDatas.resize(textureDatas.size() + 1);
	//追加したテクスチャデータの参照を取得する
	TextureData& textureData = textureDatas.back();

	textureData.filePath = filePath; //ファイルパス
	textureData.metadata = mipImages.GetMetadata(); //テクスチャメタデータを取得
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);

		//テクスチャデータの要素数番号をSRVのインデックスとする
		uint32_t srvIndex = static_cast<uint32_t>(textureDatas.size() - 1)+kSRVIndexTop;

		textureData.srvHandleCPU = dxCommon_->GetSRVCPUDescriptorHandle(srvIndex);
		textureData.srvHandleGPU = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

		srvDesc.Format = textureData.metadata.format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(textureData.metadata.mipLevels);

		//SRVの設定を行う
		//設定をもとにSRVの生成
		dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);

		//テクスチャデータ転送
		Microsoft::WRL::ComPtr< ID3D12Resource > intermediateResource = dxCommon_->UploadTextureData(textureData.resource, mipImages);
		
		//CommandListをCloseし、commandQueue->ExecuteCommandListを使いキックする。 
		dxCommon_->GetCommandList()->Close();
		ID3D12CommandList* commandLists[] = { dxCommon_->GetCommandList() };
		dxCommon_->GetCommandQueue()->ExecuteCommandLists(1, commandLists);

		// //実行を待つ。
		Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
		uint64_t fenceVal = 0;
		hr = dxCommon_->GetDevice()->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		assert(SUCCEEDED(hr));

		HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		assert(fenceEvent != nullptr);

		// シグナルを送る
		fenceVal++;
		dxCommon_->GetCommandQueue()->Signal(fence.Get(), fenceVal);

		// 完了していなければ待つ
		if (fence->GetCompletedValue() < fenceVal)
		{
			fence->SetEventOnCompletion(fenceVal, fenceEvent);
			WaitForSingleObject(fenceEvent, INFINITE);
		}
		CloseHandle(fenceEvent);

		//実行が完了したので、allocatorとcommandListをResetsして次のコマンドを積めるようにする。 
		hr = dxCommon_->GetCommandAllocator()->Reset(); // ※要GetCommandAllocator()
		assert(SUCCEEDED(hr));
		hr = dxCommon_->GetCommandList()->Reset(dxCommon_->GetCommandAllocator(), nullptr);
		assert(SUCCEEDED(hr));

		//ここまできたら転送は終わってるので、intermediateResourceはReleaseしても良い。 
		intermediateResource.Reset();

}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{

	auto it = std::find_if(
	textureDatas.begin(),
		textureDatas.end(),
		[&](const TextureData& textureData) {return textureData.filePath == filePath; }
	);//テクスチャデータ検索;

		if (it != textureDatas.end())
		{
			//読み込み済みなら要素番号を返す
			uint32_t textureIndex = static_cast<uint32_t>(std::distance(textureDatas.begin(), it));
			return textureIndex + kSRVIndexTop;
		}

		assert(0);
		return 0;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex)
{
	uint32_t dataIndex = textureIndex - kSRVIndexTop;
	//範囲外指定違反チェック
	assert(dataIndex < textureDatas.size());

	TextureData& textureData = textureDatas[dataIndex];

	return dxCommon_->GetSRVGPUDescriptorHandle(textureIndex);
}
