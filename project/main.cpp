#include <Windows.h>
#include<string>
#include<filesystem>
#include<chrono>
#include<fstream>
#include <dbghelp.h>
#include <strsafe.h>
#include<dxgidebug.h>
#include<dxcapi.h>
#include<vector>
#include <numbers>
#include<sstream>
#include<xaudio2.h>
#include <format>
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"



#include "externals/DirectXTex/DirectXTex.h"
#include"externals/DirectXTex/d3dx12.h"

#include"externals/imgui/imgui_impl_dx12.h"
#include"externals/imgui/imgui_impl_win32.h"
#include "SpriteCommon.h"
#include "Sprite.h"

#include "Vector2.h"
#include"Vector3.h"
#include"Vector4.h"
#include"Matrix4x4.h"
#include"Transform.h"
#include"MyMath.h"
#include "ModelCommon.h"
#include"Model.h"
#include"Audio.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include "Object3d.h"

#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")


using namespace Microsoft::WRL;
using namespace MyMath;

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception)
{
	//時刻を取得して、時刻を名前に入れたファイルを作成。
	// Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	//processId(このexeのID)とクラッシュ(例外)の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	//設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	//Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
	//他に関連づけられていSEH例外ハンドラあれば実行。通常はプロセスを終了する
	return EXCEPTION_EXECUTE_HANDLER;
}

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

void Log(std::ostream& os, const std::string& message)
{
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

//Microsoft::WRL::ComPtr < ID3D12Resource> CreateBufferResources(Microsoft::WRL::ComPtr < ID3D12Device> device, size_t sizeInBytes)
//{
//	//頂点リソース用のヒープの設定
//	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
//	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;//UPloadHeapを使う
//	//頂点リソースの設定
//	D3D12_RESOURCE_DESC vertexResourceDesc{};
//	//バッファリソース。テクスチャの場合はまた別の設定をする
//	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
//	vertexResourceDesc.Width = sizeInBytes;//リリースのサイズ。今回はVector4を3頂点分
//	//バッファの場合はこれらは1にする決まり
//	vertexResourceDesc.Height = 1;
//	vertexResourceDesc.DepthOrArraySize = 1;
//	vertexResourceDesc.MipLevels = 1;
//	vertexResourceDesc.SampleDesc.Count = 1;
//	//バッファの場合はこれにする決まり
//	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
//	//実際に頂点リソースを作る
//	Microsoft::WRL::ComPtr < ID3D12Resource> vertexResource = nullptr;
//	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
//		&vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
//		IID_PPV_ARGS(&vertexResource));
//	assert(SUCCEEDED(hr));
//	return vertexResource;
//}

Microsoft::WRL::ComPtr < ID3D12Resource> CreateDepthStencilTextureResoource(Microsoft::WRL::ComPtr < ID3D12Device> device, int32_t width, int32_t height)
{
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;//Textureの幅
	resourceDesc.Height = height;//Textureの高さ
	resourceDesc.MipLevels = 1;//mipmapの数
	resourceDesc.DepthOrArraySize = 1;//奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//DepthStencilとして使う通知

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//VRAM上に作る

	//深度数のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;//1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//フォーマット。Resourceと合わせる

	//Resourceの生成
	Microsoft::WRL::ComPtr < ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

SoundData SoundLoadWave(const char* filename)
{

	//ファイル入力ストリームのインスタンス
	std::ifstream file;
	//.wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);
	//ファイルオープン失敗を検出する
	assert(file.is_open());

	//RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));
	//ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0)
	{
		assert(0);
	}//タイプがWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0)
	{
		assert(0);
	}

	//Formatチャンクの読み込み
	FormatChunk format = {};
	//チャンクヘッダーの確認
	file.read((char*)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0)
	{
		assert(0);
	}

	//チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);

	//Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));
	//JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0)
	{//読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		//再読み込み
		file.read((char*)&data, sizeof(data));
	}
	if (strncmp(data.id, "data", 4) != 0)
	{
		assert(0);
	}
	//Dataチャンクのデータ部(波形データ)の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);
	//Waveファイルを閉じる
	file.close();

	//returnする為の音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}


//音声データ解放
void SoundUnload(SoundData* soundData)
{
	//バッファのメモリを解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}


//音声再生
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData)
{
	HRESULT result;

	//波形フォーマットを先にSourceVoiceの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	//再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	//波形データの再生

	result = pSourceVoice->SubmitSourceBuffer(&buf);
	result = pSourceVoice->Start();

}

HRESULT Present(
	UINT SyncInterval,
	UINT Flags
);

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	//誰も補足しなかった場合に(Unhandled),補足する関数を登録
	//main関数はじまってすぐに登録すると良い
	SetUnhandledExceptionFilter(ExportDump);

	struct D3DResourceLeakChecker
	{
		~D3DResourceLeakChecker()
		{
			D3DResourceLeakChecker leakCheck;
			Microsoft::WRL::ComPtr<IDXGIFactory7>dxgiFactory;
			Microsoft::WRL::ComPtr<ID3D12Device>device;
		}
	};
	//ログのディレクトリを用意
	std::filesystem::create_directory("logs");
//Transform変数を作る

//Transform transformSphere{ {0.2f,0.2f,0.2f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	//ここからファイルを作成し
	//現在時刻を取得(UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	//ログファイルの名前にコンマ何秒はいらないので削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	//日本時間(pcの設定時間)に変換
	std::chrono::zoned_time localTime{ std::chrono::current_zone(),nowSeconds };
	//formatを使って年月日_時分秒の文字列に変換
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	//時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + dateString + ".log";
	//ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);

	//ポインタ
	WinApp* winApp = nullptr;

	winApp = new WinApp();
	winApp->Initialize();

	DirectXCommon* dxCommon = nullptr;
	//DirectXの初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	TextureManager::GetInstance()->Initialize(dxCommon);

	//入力の初期化
	Input* input = new Input();
	input->Initialize(winApp);

	//Audio初期化
	Microsoft::WRL::ComPtr <IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice;
	//XAudioエンジンのインスタンスを生成
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	//マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);
	//音声読み込み
	SoundData soundData1 = SoundLoadWave("Resources/Alarm01.wav");
	//音声再生
	SoundPlayWave(xAudio2.Get(), soundData1);

	//3Dオブジェクト共通部の初期化
	Object3dCommon* object3dCommon = new Object3dCommon;
	object3dCommon->Initialize(dxCommon);

	ModelCommon* modelCommon = new ModelCommon();
	modelCommon->Initialize(dxCommon);

	SpriteCommon* spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon);

	Model* model = new Model();
	model->Initialize(modelCommon, "Resources", "plane.obj");

	Object3d* object3d = new Object3d();
	object3d->Initialize(object3dCommon);
	object3d->SetModel(model);

	Transform& objTransform = object3d->GetTransform();

	Transform cameraTransform{ { 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,-10.0f } };

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");//1枚目
	TextureManager::GetInstance()->LoadTexture("Resources/monsterBall.png");//2枚目

	//////SRVを作成するDescriptorHeapの場所を決める
	//D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetSRVCPUDescriptorHandle(1);
	//D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetSRVGPUDescriptorHandle(1);

	//const uint32_t kSubdivision = 16;//分割数

	////経度分割1つ分の角度
	//const float kLonEvery = std::numbers::pi_v<float> *2.0f / float(kSubdivision);
	////緯度分割1つ分のの角度
	//const float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);

	//Sphere用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	//const Microsoft::WRL::ComPtr < ID3D12Resource>& transformationMatrixResourceSphere = CreateBufferResources(dxCommon->GetDevice(), sizeof(TransformationMatrix));

	//データを書き込む
	//TransformationMatrix* transformationMatrixDataSphere = nullptr;

	//書き込むためのアドレスを取得
	//transformationMatrixResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere));


	//単位行列を書き込んでおく
	//transformationMatrixDataSphere->WVP = MakeIdentity4x4();
	//単位行列を書き込んでおく
	//transformationMatrixDataSphere->World = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);



	//const uint32_t sphereVertexNum = (kSubdivision + 1) * (kSubdivision + 1);//頂点数
	//const uint32_t sphereIndexNum = (kSubdivision * kSubdivision) * 6;//インデックス数
	//Sphere用の頂点リソースを作る
	//const Microsoft::WRL::ComPtr < ID3D12Resource> vertexResourceSphere = CreateBufferResources(dxCommon->GetDevice(), sizeof(VertexData) * sphereVertexNum);

	////Sphereバッファビューを作成
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};

	////リソースの先端アドレスから使う
	//vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();

	////使用するリソースのサイズは頂点3つ分サイズ
	//vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * sphereVertexNum;

	////1頂点当たりのサイズ
	//vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	////球体リソースサイズデータに書き込む
	//VertexData* vertexDataSphere = nullptr;

	//書き込むためのアドレスを取得
	//vertexResourceSphere->Map(0, nullptr, reinterpret_cast<VOID**>(&vertexDataSphere));

	////緯度の方向に分割
	//for (uint32_t latIndex = 0; latIndex < (kSubdivision + 1); ++latIndex)
	//{
	//	float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex;
	//	//経度の方向に分割しながら線を描く
	//	for (uint32_t lonIndex = 0; lonIndex < (kSubdivision + 1); ++lonIndex)
	//	{

	//		float lon = lonIndex * kLonEvery;

	//		VertexData VertA = {
	//			{       //緯度   //経度
	//				std::cosf(lat) * std::cos(lon),
	//				std::sin(lat),
	//				std::cos(lat) * std::sinf(lon),
	//				1.0f
	//			},
	//			{                           //番号
	//				float(lonIndex) / float(kSubdivision),
	//				1.0f - float(latIndex) / float(kSubdivision)
	//			},
	//			{
	//				std::cosf(lat) * std::cos(lon),
	//				std::sin(lat),
	//				std::cos(lat) * std::sinf(lon)
	//			}
	//		};
	//		uint32_t start = (latIndex * (kSubdivision + 1) + lonIndex);
	//		vertexDataSphere[start] = VertA;
	//	}
	//}

	//const Microsoft::WRL::ComPtr < ID3D12Resource> indexResourceSphere = CreateBufferResources(dxCommon->GetDevice(), sizeof(uint32_t) * sphereIndexNum);
	//
	//D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere{};
	////リソースの先頭のアドレスから使う
	//indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	////使用するリソースのサイズはインデックス6つ分のサイズ
	//indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * sphereIndexNum;
	////インデックスはuint32_tとする
	//indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;
	//
	//uint32_t* indexDataSphere = nullptr;
	//indexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSphere));

  //緯度の方向に分割
//  for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex)
//  {
//  	
//  	//経度の方向に分割しながら線を描く
//  	for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex)
//  	{
//  
//  		uint32_t ld = lonIndex + latIndex * (kSubdivision + 1);
//  		uint32_t lt = lonIndex + (latIndex + 1) * (kSubdivision + 1);
//  		uint32_t rd = (lonIndex + 1) + latIndex * (kSubdivision + 1);
//  		uint32_t rt = (lonIndex + 1) + (latIndex + 1) * (kSubdivision + 1);
//  
//  		uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;
//  		indexDataSphere[start + 0] = ld;
//  		indexDataSphere[start + 1] = lt;
//  		indexDataSphere[start + 2] = rd;
//  		indexDataSphere[start + 3] = lt;
//  		indexDataSphere[start + 4] = rt;
//  		indexDataSphere[start + 5] = rd;
//
//  	}
//  }

  //マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
 // const Microsoft::WRL::ComPtr < ID3D12Resource>& materialResourceSphere = CreateBufferResources(dxCommon->GetDevice(), sizeof(Material));
  //マテリアルにデータを書き込む
  //Material* materialDataSphere = nullptr;
  //mapしてデータを書き込む色は白
  //materialResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSphere));
  //materialDataSphere->color = { 1.0f, 1.0f, 1.0f, 1.0f };
  ////SpriteはLightingしないのでfalseに設定しておく
  //materialDataSphere->enableLighting = false;
  //materialDataSphere->uvTransform = MakeIdentity4x4();

  //const Microsoft::WRL::ComPtr < ID3D12Resource>& directionalLightResourceSphere = CreateBufferResources(dxCommon->GetDevice(), sizeof(DirectionalLight));

  //DirectionalLight* directionalLightDataSphere = nullptr;
 
 /* directionalLightResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightDataSphere));
  directionalLightDataSphere->color = { 1.0f, 1.0f, 1.0f, 1.0f };
  directionalLightDataSphere->direction = { 1.0f,0.0f,0.0f };
  directionalLightDataSphere->intensity = 1.0f;
  directionalLightDataSphere->direction = Normalize(directionalLightDataSphere->direction);*/

	std::vector<Sprite*>sprites;
	for (uint32_t i = 0; i < 1; ++i)
	{
		Sprite* sprite = new Sprite();
		sprite->Initialize(spriteCommon, "Resources/monsterBall.png");
		//sprite->SetTexture(textureSrvHandleGPU);

		if (i % 2 == 0) {
			// 偶数にモンスターボールpng
			sprite->SetTexture("Resources/uvChecker.png");
		}

		Vector2 pos = { 0.0f + i * 0.0f,0.0f + i * 50.0f };
		sprite->SetPosition(pos);
		sprites.push_back(sprite);
		
		//切り取り
		//sprite->SetTextureLeftTop({ 0.0f, 0.0f });
		//sprite->SetTextureSize({ 100.0f, 100.0f });
	}

	bool useMonsterBall = true;
	int selectedUI = 0;

	MSG msg{};
	//ウィンドウのxボタンが押されるまでループ
	while (true)
	{//ウィンドウにメッセージが来てたら最優先で処理される
		if (winApp->ProcessMessage())
		{
			break;
		}

		//ゲームの処理
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("test");
		if (ImGui::Button("UVTranslate")) {
			selectedUI = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("Object3d")) {
			selectedUI = 1;
		}

		ImGui::Separator();

		// UI選択用コンボボックス
		switch (selectedUI) {
		case 0: // Sprite
			ImGui::Text("Editing UVTranslate");
			// std::vector の全要素に対して処理
			for (int i = 0; i < sprites.size(); ++i)
			{
				// IDをプッシュ（これが無いと全部のスプライトが同時に動いてしまう）
				ImGui::PushID(i);
				ImGui::Text("Sprite %d", i); // 番号表示
				//座標
				Vector2 pos = sprites[i]->GetPosition();
				if (ImGui::DragFloat2("Pos", &pos.x, 1.0f)) {
					sprites[i]->SetPosition(pos);
				}//回転
				float rot = sprites[i]->GetRotation();
				if (ImGui::DragFloat("Rot", &rot, 0.01f)) {
					sprites[i]->SetRotation(rot);
				}//サイズ
				Vector2 size = sprites[i]->GetSize();
				if (ImGui::DragFloat2("Size", &size.x, 1.0f)) {
					sprites[i]->SetSize(size);
				}//アンカーポイント
				Vector2 anchor = sprites[i]->GetAnchorPoint();
				// 0.0～1.0 の範囲で動かす
				if (ImGui::DragFloat2("Anchor", &anchor.x, 0.01f, 0.0f, 1.0f)) {
					sprites[i]->SetAnchorPoint(anchor);
				}//左右反転
				bool isFlipX = sprites[i]->GetIsFlipX();
				if (ImGui::Checkbox("isFlipX", &isFlipX)) {
					sprites[i]->SetIsFlipX(isFlipX);
				}
				//上下反転
				bool isFlipY = sprites[i]->GetIsFlipY();
				if (ImGui::Checkbox("isFlipY", &isFlipY)) {
					sprites[i]->SetIsFlipY(isFlipY);
				}//左上座標
				Vector2 texBase = sprites[i]->GetTextureLeftTop();
				if (ImGui::DragFloat2("TexLeftTop", &texBase.x, 1.0f)) {
					sprites[i]->SetTextureLeftTop(texBase);
				}//切り出しサイズ
				Vector2 texSize = sprites[i]->GetTextureSize();
				if (ImGui::DragFloat2("TexSize", &texSize.x, 1.0f)) {
					sprites[i]->SetTextureSize(texSize);
				}

				ImGui::Separator();
				ImGui::PopID(); // IDをポップ
			}
			break;
			case 1: // Object
				ImGui::Text("Editing Object");
				ImGui::DragFloat3("Model Translate", &objTransform.translate.x, 0.01f); // 位置
				ImGui::DragFloat3("Model Rotate", &objTransform.rotate.x, 0.01f);    // 回転
				ImGui::DragFloat3("Model Scale", &objTransform.scale.x, 0.01f);
			
				break;
				ImGui::Separator();

			//case 2: // Sphere
			//	ImGui::Text("Editing Sphere");
			//	ImGui::ColorEdit4("Sphere Color", &(materialDataSphere->color).x);
			//	ImGui::DragFloat3("Translate", &transformSphere.translate.x);
			//	ImGui::DragFloat3("Rotate", &transformSphere.rotate.x, 0.01f);
			//	ImGui::DragFloat3("Scale", &transformSphere.scale.x);
			//	break;
			}
			ImGui::DragFloat3("CameraTransform", &cameraTransform.translate.x);
			ImGui::Checkbox("useMonsterBall", &useMonsterBall);

			ImGui::End();
			//開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
			ImGui::ShowDemoWindow();
			//ImGuiの内部コマンドを生成する
			ImGui::Render();
			
			//入力の更新
			input->Update();

			object3d->GetCameraTransform() = cameraTransform;

			object3d->Update();
			
			for (Sprite* sprite : sprites)
			{
				sprite->Update();
			}

			////数字の0キーが押されていたら
			if(input->PushKey(DIK_0))
			{
				OutputDebugStringA("Hit 0\n");//出力ウィンドウに「Hit 0」と表示
			}

			////数字の0キーが押されていたら
			if (input->TriggerKey(DIK_P))
			{
				OutputDebugStringA("Hit p\n");//出力ウィンドウに「Hit p」と表示
			}

			///
			///デバック確認
			///
			
			////現在の座標を変数で受ける
			//Vector2 position = sprite->GetPosition();
			////座標を変更する
			//position += Vector2{ 0.1f,0.1f };
			////変更を反映する
			//sprite->SetPosition(position);
			//
			////回転
			//float rotation = sprite->GetRotation();
			//rotation += 0.01f;
			//sprite->SetRotation(rotation);
			//
			////色
			//Vector4 color = sprite->GetColor();
			//color.x += 0.01f;
			//if (color.x > 1.0f)
			//{
			//	color.x -= 1.0f;
			//}
			//sprite->SetColor(color);
			//
			//サイズ
			//Vector2 size = sprite->GetSize();
			//size.x += 0.1f;
			//size.y += 0.1f;
			//sprite->SetSize(size);


			///
			///
			///

			//描画前処理
			dxCommon->PreDraw();

			//3Dオブジェクトの描画準備3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
			//object3dCommon->SetCommonDrawSetting();

			//RootSignatureを設定。PS0に設定しているけど別途設定が必要

			// スプライト描画
				//Spriteの描画準備Spriteの描画に共通のグラフィックスコマンドを積む
				spriteCommon->SetCommonDrawSetting();
				for (Sprite* sprite : sprites)
				{
					sprite->Draw();
				}
				// Object のみ描画
				object3d->Draw();

			//3Dモデル描画
			object3d->Draw();

			//実際のcommandListのImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());

			
			dxCommon->PostDraw();

	}
	//XAudio2解放
	xAudio2.Reset();
	//音声データ解放
	SoundUnload(&soundData1);

	for (Sprite* sprite : sprites)
	{
		delete sprite;
	}
	sprites.clear();
	TextureManager::GetInstance()->Finalize();
	delete spriteCommon;
	delete object3dCommon;
	delete object3d;
	delete model;
	delete modelCommon;

	//ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	//dxcUtils->Release();
	//dxcCompiler->Release();
	//includeHandler->Release();
	//wvpResource->Release();
	//materialResource->Release();
	//vertexResource->Release();
	//graphicsPipelineState->Release();
	//signatureBlob->Release();
	//if (errorBlob)
	//{
	//	errorBlob->Release();
	//}
	//rootSignature->Release();
	//pixelShaderBlob->Release();
	//vertexShaderBlob->Release();

	//textureResource->Release();
	//textureResource2->Release();

	//fence->Release();
	//vertexResourceObject->Release();

	//transformationMatrixResource->Release();

	//intermediateResource->Release();
	//intermediateResource2->Release();

	//directionalLightResourceSprite->Release();

	//depthStencilResource->Release();

	//materialResourceSprite->Release();

	//dsvDescriptorHeap->Release();

	//vertexResourceSprite->Release();
	//transformationMatrixResourceSprite->Release();
	// 
	//transformationMatrixResourceSphere->Release();

	//indexResourceSprite->Release();
	//rtvDescriptorHeap->Release();
	//srvDescriptorHeap->Release();
	//swapChainResources[0]->Release();
	//swapChainResources[1]->Release();
	//swapChain->Release();
	//commandList->Release();
	//commandAllocator->Release();
	//commandQueue->Release();
	//device->Release();
	//useAdapter->Release();
	//dxgiFactory->Release();


#ifdef _DEBUG
	//debugController->Release();
#endif


		winApp->Finalize();

	return 0;
}