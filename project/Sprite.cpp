#include "Sprite.h"
#include"SpriteCommon.h"

void Sprite::Initialize()
{
	Sprite* sprite = new Sprite();
	sprite->Initialize();

	//引数で受け取ってメンバ変数に記録する
	this->spriteCommon = spriteCommon;

	//頂点リソースを作る
	const Microsoft::WRL::ComPtr < ID3D12Resource>& vertexResource = CreateBufferResources(dxCommon->GetDevice(), sizeof(VertexData) * 4);

	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	//リリースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点3つ分のサイズ
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 4;
	//1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	//頂点リソースにデータを書き込む
	VertexData* vertexData = nullptr;
	//書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	////1枚目の三角形
	////左下
	//vertexData[0].position = { -0.5f,-0.5f,0.0f,1.0f };
	//vertexData[0].texcoord = { 0.0f,1.0f };
	////上
	//vertexData[1].position = { 0.0f, 0.5f, 0.0f, 1.0f };
	//vertexData[1].texcoord = { 0.5f,0.0f };
	////右下
	//vertexData[2].position = { 0.5f,-0.5f,0.0f,1.0f };
	//vertexData[2].texcoord = { 1.0f,1.0f };

	////2枚目の三角形
	////上2
	//vertexData[3].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	//vertexData[3].texcoord = { 0.5f,0.0f };


	//TransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	const Microsoft::WRL::ComPtr < ID3D12Resource>& transformationMatrixResource = CreateBufferResources(dxCommon->GetDevice(), sizeof(TransformationMatrix));

	//マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	const Microsoft::WRL::ComPtr < ID3D12Resource>& materialResource = CreateBufferResources(dxCommon->GetDevice(), sizeof(Material));
	//マテリアルにデータを書き込む
	Material* materialData = nullptr;
	//書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));



	//マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	const Microsoft::WRL::ComPtr < ID3D12Resource>& materialResourceSprite = CreateBufferResources(dxCommon->GetDevice(), sizeof(Material));
	//マテリアルにデータを書き込む
	Material* materialDataSprite = nullptr;
	//mapしてデータを書き込む色は白
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	//SpriteはLightingしないのでfalseに設定しておく
	materialDataSprite->enableLighting = false;
	materialDataSprite->uvTransform = MakeIdentity4x4();
	//今回は赤を書き込んでみる
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = true;
	materialData->uvTransform = MakeIdentity4x4();

	//WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	const Microsoft::WRL::ComPtr < ID3D12Resource>& wvpResource = CreateBufferResources(dxCommon->GetDevice(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* wvpData = nullptr;
	//書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列をかきこんでおく
	wvpData->WVP = MakeIdentity4x4();
	wvpData->World = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	//バッファリソース
	//座標変換行列リソース (constantBuffer)
	//バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData = nullptr;

	//座標変換行列リソースを作る
	TransformationMatrix CreateBufferResource();

	//座標変換行列リソースにデータを書き込むためのアドレスを取得してtransformationMatrixDataに割り当てる
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));
		//単位行列を書き込んでおく
	transformationMatrixData->WVP = MakeIdentity4x4();
	//単位行列を書き込んでおく
	transformationMatrixData->World = MakeIdentity4x4();


}

void Sprite::Updata()
{
	//Sprite用のリソースを作る
	const Microsoft::WRL::ComPtr < ID3D12Resource>& vertexResourceSprite = CreateBufferResources(dxCommon->GetDevice(), sizeof(VertexData) * 4);

	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	//1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	//スプライト用の頂点リソースにデータを書き込む
	VertexData* vertexDataSprite = nullptr;
	//書き込むためのアドレスを取得
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	//1枚目の三角形
	//左下
	//vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f };
	//vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	////上
	//vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	//vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	////右下
	//vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f };
	//vertexDataSprite[2].texcoord = { 1.0f,1.0f };
	//
	////2枚目の三角形
	////上2
	//vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	//vertexDataSprite[3].texcoord = { 1.0f,0.0f };

	//DepthStencilTextureをウィンドウのサイズで作成
	const Microsoft::WRL::ComPtr < ID3D12Resource>& depthStencilResource = CreateDepthStencilTextureResoource(dxCommon->GetDevice(), WinApp::kClientWidth, WinApp::kClientHeight);

	//スプライト用のTransform変数を作る
	Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	//Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	const Microsoft::WRL::ComPtr < ID3D12Resource>& transformationMatrixResourceSprite = CreateBufferResources(dxCommon->GetDevice(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	//書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	//単位行列を書き込んでおく
	transformationMatrixDataSprite->World = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);




	const Microsoft::WRL::ComPtr < ID3D12Resource>& indexResourceSprite = CreateBufferResources(dxCommon->GetDevice(), sizeof(uint32_t) * 6);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0;  indexDataSprite[1] = 1;   indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;  indexDataSprite[4] = 3;   indexDataSprite[5] = 2;




}

void Sprite::Draw()
{
	//VertexBufferViewを設定

	//IndexBufferViewを設定
	
	//マテリアルCBufferの場所を設定
	
	//座標変換行列CBufferの場所を設定

	//SRVのDescriptorTableの戦闘を設定
	
	//描画(DrawCall/ドローコール)
	

}