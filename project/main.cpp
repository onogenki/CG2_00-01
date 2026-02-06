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
#include"Model.h"
#include"Audio.h"

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

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

void Log(std::ostream& os, const std::string& message)
{
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

Microsoft::WRL::ComPtr < ID3D12Resource> CreateBufferResources(Microsoft::WRL::ComPtr < ID3D12Device> device, size_t sizeInBytes)
{
	//頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;//UPloadHeapを使う
	//頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	//バッファリソース。テクスチャの場合はまた別の設定をする
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes;//リリースのサイズ。今回はVector4を3頂点分
	//バッファの場合はこれらは1にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	//バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//実際に頂点リソースを作る
	Microsoft::WRL::ComPtr < ID3D12Resource> vertexResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr));
	return vertexResource;
}

DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	//テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	//ミップマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//ミップマップ付きのデータを返す
	return mipImages;

}

Microsoft::WRL::ComPtr < ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr < ID3D12Device> device, const DirectX::TexMetadata& metadata)
{
	//metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);//textureの幅
	resourceDesc.Height = UINT(metadata.height);//Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);//mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);//奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;//TextureのFormat
	resourceDesc.SampleDesc.Count = 1;//TextureのFormat
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);//Textureの次元数。普段使っているのは2次元

	//利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties{};

	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//細かい設定を行う
	//heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//WriteBackポリシーでCPUアクセス可能
	//heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//プロセッサの近くに配置

	//Resourceの生成
	Microsoft::WRL::ComPtr < ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,//Heapの設定
		D3D12_HEAP_FLAG_NONE,//Heapの特殊な設定。特になし。
		&resourceDesc,//Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,//初回のResourceState。Textureは基本読むだけ
		nullptr,//Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource));//作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;

}

[[nodiscard]]
Microsoft::WRL::ComPtr < ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr < ID3D12Resource> texture, const DirectX::ScratchImage& mipImages, Microsoft::WRL::ComPtr < ID3D12Device> device,
	const Microsoft::WRL::ComPtr <ID3D12GraphicsCommandList>& commandList)
{
	std::vector<D3D12_SUBRESOURCE_DATA>subresources;
	DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	const Microsoft::WRL::ComPtr < ID3D12Resource>& intermediateResource = CreateBufferResources(device, intermediateSize);
	UpdateSubresources(commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());
	//Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);
	return intermediateResource;
}

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

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	MaterialData materialData;//構築するMaterialData
	std::string line;//ファイルから読んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//とりあえず開けなかったら止める
	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);//文字列を分解しながら読むためのクラス
		s >> identifier;//先頭の識別子を読む

		if (identifier == "map_Kd")
		{
			std::string textureFilename;
			s >> textureFilename;
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	ModelData modelData;//構築するModelData
	std::vector<Vector4>positions;//位置
	std::vector<Vector3>normals;//法線
	std::vector<Vector2>texcoords;//テクスチャ座標
	std::string line;//ファイルから読んだ1行を格納する

	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//とりあえず開けなかったら止める

	VertexData triangle[3];
	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);//文字列を分解しながら読むためのクラス
		s >> identifier;//先頭の識別子を読む

		if (identifier == "v")//頂点位置
		{
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt")//頂点テクスチャ座標
		{
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;

			texcoords.push_back(texcoord);
		} else if (identifier == "vn")//頂点法線
		{
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f")
		{//面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex)
			{
				std::string vertexDefinition;
				s >> vertexDefinition;
				//頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element)
				{
					std::string index;
					std::getline(v, index, '/');//区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				//要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				VertexData vertex = { position,texcoord,normal };
				modelData.vertices.push_back(vertex);
				triangle[faceVertex] = { position,texcoord,normal };
			}
			//頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib")
		{
			std::string materialFilename;
			s >> materialFilename;
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
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


bool useMonsterBall = true;
//Transform変数を作る
Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

Transform transformSphere{ {0.2f,0.2f,0.2f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

Transform uvTransformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

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
			////リソースリークチェック
			//Microsoft::WRL::ComPtr<IDXGIDebug1>debug;
			//if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
			//{
			//	debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			//	debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			//	debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
			//}
		}
	};
	//ログのディレクトリを用意
	std::filesystem::create_directory("logs");


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

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	const Microsoft::WRL::ComPtr < ID3D12Resource>& textureResource = CreateTextureResource(dxCommon->GetDevice(), metadata);
	const Microsoft::WRL::ComPtr < ID3D12Resource>& intermediateResource = UploadTextureData(textureResource, mipImages, dxCommon->GetDevice(), dxCommon->GetCommandList());

	//metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	//SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetSRVCPUDescriptorHandle(1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetSRVGPUDescriptorHandle(1);
	//SRVの生成
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;//0から始まる
	descriptorRange[0].NumDescriptors = 1;//数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//Offsetを自動計算

	//RootParameter作成。複数設定できるので配列
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;//レジスタ番号0とバインド

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;//VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0;//レジスタ番号0を使う

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;//Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);//Tableで利用する数

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1;//レジスタ番号1を使う

	descriptionRootSignature.pParameters = rootParameters;//ルートパレメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;// 0～1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;//ありったけのMi@mapを使う
	staticSamplers[0].ShaderRegister = 0;//レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr <IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice;

	HRESULT result;

	//XAudioエンジンのインスタンスを生成
	result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

	//マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);

	//音声読み込み
	SoundData soundData1 = SoundLoadWave("Resources/Alarm01.wav");

	//音声再生
	SoundPlayWave(xAudio2.Get(), soundData1);

	//ポインタ
	Input* input = nullptr;

	//入力の初期化
	input = new Input();
	input->Initialize(winApp);

	////マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	//const Microsoft::WRL::ComPtr < ID3D12Resource>& materialResource = CreateBufferResources(dxCommon->GetDevice(), sizeof(Material));
	////マテリアルにデータを書き込む
	//Material* materialData = nullptr;
	////書き込むためのアドレスを取得
	//materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	////今回は赤を書き込んでみる
	//materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	//materialData->enableLighting = true;
	//materialData->uvTransform = MakeIdentity4x4();

	////WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	//const Microsoft::WRL::ComPtr < ID3D12Resource>& wvpResource = CreateBufferResources(dxCommon->GetDevice(), sizeof(TransformationMatrix));
	////データを書き込む
	//TransformationMatrix* wvpData = nullptr;
	////書き込むためのアドレスを取得
	//wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	////単位行列をかきこんでおく
	//wvpData->WVP = MakeIdentity4x4();
	//wvpData->World = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);





	//シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr < ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr < ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	Microsoft::WRL::ComPtr < ID3D12RootSignature> rootSignature = nullptr;
	hr = dxCommon->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);
	//入力レイアウトの追加
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;


	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;

	//RasiterZerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//shaderをコンパイルする
	const Microsoft::WRL::ComPtr < IDxcBlob>& vertexShaderBlob = dxCommon->CompileShader(L"resources/shaders/Object3D.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	
	const Microsoft::WRL::ComPtr < IDxcBlob>& pixelShaderBlob = dxCommon->CompileShader(L"resources/shaders/Object3D.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	//PSOを作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();//RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;//InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };//PixelShader

	graphicsPipelineStateDesc.BlendState = blendDesc;//BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;//RasterizerState
	////書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	////利用するトポロジ(形状)のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	////どのように画面に色を打ち込むのかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	//DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//実際に生成
	Microsoft::WRL::ComPtr < ID3D12PipelineState> graphicsPipelineState = nullptr;
	hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	////頂点リソースを作る
	//const Microsoft::WRL::ComPtr < ID3D12Resource>& vertexResource = CreateBufferResources(dxCommon->GetDevice(), sizeof(VertexData) * 4);

	////頂点バッファビューを作成する
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	////リリースの先頭のアドレスから使う
	//vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	////使用するリソースのサイズは頂点3つ分のサイズ
	//vertexBufferView.SizeInBytes = sizeof(VertexData) * 4;
	////1頂点あたりのサイズ
	//vertexBufferView.StrideInBytes = sizeof(VertexData);

	////頂点リソースにデータを書き込む
	VertexData* vertexData = nullptr;
	////書き込むためのアドレスを取得
	//vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	////1枚目の三角形
	////左下
	vertexData[0].position = { -0.5f,-0.5f,0.0f,1.0f };
	vertexData[0].texcoord = { 0.0f,1.0f };
	//上
	vertexData[1].position = { 0.0f, 0.5f, 0.0f, 1.0f };
	vertexData[1].texcoord = { 0.5f,0.0f };
	//右下
	vertexData[2].position = { 0.5f,-0.5f,0.0f,1.0f };
	vertexData[2].texcoord = { 1.0f,1.0f };

	//2枚目の三角形
	//上2
	vertexData[3].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexData[3].texcoord = { 0.5f,0.0f };


	////TransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	//const Microsoft::WRL::ComPtr < ID3D12Resource>& transformationMatrixResource = CreateBufferResources(dxCommon->GetDevice(), sizeof(TransformationMatrix));
	////データを書き込む
	//TransformationMatrix* transformationMatrixData = nullptr;
	////書き込むためのアドレスを取得
	//transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));
	////単位行列を書き込んでおく
	//transformationMatrixData->WVP = MakeIdentity4x4();
	////単位行列を書き込んでおく
	//transformationMatrixData->World = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);


	////頂点バッファビューを作成する
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	////リソースの先頭のアドレスから使う
	//vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	////使用するリソースのサイズは頂点6つ分のサイズ
	//vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	////1頂点あたりのサイズ
	//vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	////スプライト用の頂点リソースにデータを書き込む
	VertexData* vertexDataSprite = nullptr;
	////書き込むためのアドレスを取得
	//vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	////1枚目の三角形
	////左下
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f };
	vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	//上
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	//右下
	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f };
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };

	//2枚目の三角形
	//上2
	vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[3].texcoord = { 1.0f,0.0f };

	////DepthStencilTextureをウィンドウのサイズで作成
	//const Microsoft::WRL::ComPtr < ID3D12Resource>& depthStencilResource = CreateDepthStencilTextureResoource(dxCommon->GetDevice(), WinApp::kClientWidth, WinApp::kClientHeight);

	////スプライト用のTransform変数を作る
	//Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	////データを書き込む
	//TransformationMatrix* transformationMatrixDataSprite = nullptr;
	////書き込むためのアドレスを取得
	//transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	//transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	////単位行列を書き込んでおく
	//transformationMatrixDataSprite->World = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);


	//SpriteCommon* spriteCommon = nullptr;

	//spriteCommon = new SpriteCommon;
	//spriteCommon->Initialize();

	//Sprite* sprite = new Sprite();
	//sprite->Initialize();




	
	//D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	////リソースの先頭のアドレスから使う
	//indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	////使用するリソースのサイズはインデックス6つ分のサイズ
	//indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	////インデックスはuint32_tとする
	//indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	//uint32_t* indexDataSprite = nullptr;
	//indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	//indexDataSprite[0] = 0;  indexDataSprite[1] = 1;   indexDataSprite[2] = 2;
	//indexDataSprite[3] = 1;  indexDataSprite[4] = 3;   indexDataSprite[5] = 2;




////マテリアルにデータを書き込む
	//Material* materialDataSprite = nullptr;
	////mapしてデータを書き込む色は白
	//materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	//materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	////SpriteはLightingしないのでfalseに設定しておく
	//materialDataSprite->enableLighting = false;

	const Microsoft::WRL::ComPtr < ID3D12Resource>& directionalLightResourceSprite = CreateBufferResources(dxCommon->GetDevice(), sizeof(DirectionalLight));

	DirectionalLight* directionalLightData = nullptr;
	directionalLightResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 1.0f,0.0f,0.0f };
	directionalLightData->intensity = 1.0f;
	directionalLightData->direction = Normalize(directionalLightData->direction);







	const uint32_t kSubdivision = 16;//分割数

	//経度分割1つ分の角度
	const float kLonEvery = std::numbers::pi_v<float> *2.0f / float(kSubdivision);
	//緯度分割1つ分のの角度
	const float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);

	//const uint32_t sphereVertexNum = (kSubdivision+1)*(kSubdivision+1);//頂点数
	//const uint32_t sphereIndexNum = (kSubdivision * kSubdivision) * 6;//インデックス数

	//モデル読み込み
	ModelData modelData = LoadObjFile("Resources", "plane.obj");

	//Sphere用の頂点リソースを作る
	const Microsoft::WRL::ComPtr < ID3D12Resource>& vertexResourceObject = CreateBufferResources(dxCommon->GetDevice(), sizeof(VertexData) * modelData.vertices.size());

	//バッファビューを作成
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewObj{};

	//リソースの先端アドレスから使う
	vertexBufferViewObj.BufferLocation = vertexResourceObject->GetGPUVirtualAddress();

	//使用するリソースのサイズは頂点3つ分サイズ
	vertexBufferViewObj.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());

	//1頂点当たりのサイズ
	vertexBufferViewObj.StrideInBytes = sizeof(VertexData);

	//リソースサイズデータに書き込む
	VertexData* vertexDataObj = nullptr;

	//書き込むためのアドレスを取得
	vertexResourceObject->Map(0, nullptr, reinterpret_cast<VOID**>(&vertexDataObj));
	//頂点データをリソースにコピー
	std::memcpy(vertexDataObj, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	//2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	const Microsoft::WRL::ComPtr < ID3D12Resource>& textureResource2 = CreateTextureResource(dxCommon->GetDevice(), metadata2);
	const Microsoft::WRL::ComPtr<ID3D12Resource>& intermediateResource2 = UploadTextureData(textureResource2, mipImages2, dxCommon->GetDevice(), dxCommon->GetCommandList());

	//metaDataを基に2枚目のSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	//Sphere用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	const Microsoft::WRL::ComPtr < ID3D12Resource>& transformationMatrixResourceSphere = CreateBufferResources(dxCommon->GetDevice(), sizeof(TransformationMatrix));

	//データを書き込む
	TransformationMatrix* transformationMatrixDataSphere = nullptr;

	//書き込むためのアドレスを取得
	transformationMatrixResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere));


	//単位行列を書き込んでおく
	transformationMatrixDataSphere->WVP = MakeIdentity4x4();
	//単位行列を書き込んでおく
	transformationMatrixDataSphere->World = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);



	const uint32_t sphereVertexNum = (kSubdivision + 1) * (kSubdivision + 1);//頂点数
	const uint32_t sphereIndexNum = (kSubdivision * kSubdivision) * 6;//インデックス数
	//Sphere用の頂点リソースを作る
	const Microsoft::WRL::ComPtr < ID3D12Resource> vertexResourceSphere = CreateBufferResources(dxCommon->GetDevice(), sizeof(VertexData) * sphereVertexNum);

	//Sphereバッファビューを作成
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};

	//リソースの先端アドレスから使う
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();

	//使用するリソースのサイズは頂点3つ分サイズ
	vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * sphereVertexNum;

	//1頂点当たりのサイズ
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	//球体リソースサイズデータに書き込む
	VertexData* vertexDataSphere = nullptr;

	//書き込むためのアドレスを取得
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<VOID**>(&vertexDataSphere));

	//緯度の方向に分割
	for (uint32_t latIndex = 0; latIndex < (kSubdivision + 1); ++latIndex)
	{
		float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex;
		//経度の方向に分割しながら線を描く
		for (uint32_t lonIndex = 0; lonIndex < (kSubdivision + 1); ++lonIndex)
		{

			float lon = lonIndex * kLonEvery;

			VertexData VertA = {
				{       //緯度   //経度
					std::cosf(lat) * std::cos(lon),
					std::sin(lat),
					std::cos(lat) * std::sinf(lon),
					1.0f
				},
				{                           //番号
					float(lonIndex) / float(kSubdivision),
					1.0f - float(latIndex) / float(kSubdivision)
				},
				{
					std::cosf(lat) * std::cos(lon),
					std::sin(lat),
					std::cos(lat) * std::sinf(lon)
				}
			};
			uint32_t start = (latIndex * (kSubdivision + 1) + lonIndex);
			vertexDataSphere[start] = VertA;
		}
	}

	//単位行列を書き込んでおく
	transformationMatrixDataSphere->WVP = MakeIdentity4x4();
	//単位行列を書き込んでおく
	transformationMatrixDataSphere->World = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);




	const Microsoft::WRL::ComPtr < ID3D12Resource> indexResourceSphere = CreateBufferResources(dxCommon->GetDevice(), sizeof(uint32_t) * sphereIndexNum);
	
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere{};
	//リソースの先頭のアドレスから使う
	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * sphereIndexNum;
	//インデックスはuint32_tとする
	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;
	
	uint32_t* indexDataSphere = nullptr;
	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSphere));

  //緯度の方向に分割
  for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex)
  {
  	
  	//経度の方向に分割しながら線を描く
  	for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex)
  	{
  
  		uint32_t ld = lonIndex + latIndex * (kSubdivision + 1);
  		uint32_t lt = lonIndex + (latIndex + 1) * (kSubdivision + 1);
  		uint32_t rd = (lonIndex + 1) + latIndex * (kSubdivision + 1);
  		uint32_t rt = (lonIndex + 1) + (latIndex + 1) * (kSubdivision + 1);
  
  		uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;
  		indexDataSphere[start + 0] = ld;
  		indexDataSphere[start + 1] = lt;
  		indexDataSphere[start + 2] = rd;
  		indexDataSphere[start + 3] = lt;
  		indexDataSphere[start + 4] = rt;
  		indexDataSphere[start + 5] = rd;

  	}
  }

  //マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
  const Microsoft::WRL::ComPtr < ID3D12Resource>& materialResourceSphere = CreateBufferResources(dxCommon->GetDevice(), sizeof(Material));
  //マテリアルにデータを書き込む
  Material* materialDataSphere = nullptr;
  //mapしてデータを書き込む色は白
  materialResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSphere));
  materialDataSphere->color = { 1.0f, 1.0f, 1.0f, 1.0f };
  //SpriteはLightingしないのでfalseに設定しておく
  materialDataSphere->enableLighting = false;
  materialDataSphere->uvTransform = MakeIdentity4x4();

  const Microsoft::WRL::ComPtr < ID3D12Resource>& directionalLightResourceSphere = CreateBufferResources(dxCommon->GetDevice(), sizeof(DirectionalLight));

 
  directionalLightResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
  directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
  directionalLightData->direction = { 1.0f,0.0f,0.0f };
  directionalLightData->intensity = 1.0f;
  directionalLightData->direction = Normalize(directionalLightData->direction);

	Transform cameraTransform{ { 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,-10.0f } };

	////SRVを作成するDescriptorHeapの場所を決める
	//D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	//D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	////先頭はImGuiが使ってるのでその次を使う
	//textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	MSG msg{};
	//ウィンドウのxボタンが押されるまでループ
	while (true)
	{//ウィンドウにメッセージが来てたら最優先で処理される
		if (winApp->ProcessMessage())
		{
			break;
		}
			static int selectedUI = 0;

			//ゲームの処理
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			ImGui::Begin("test");
			if (ImGui::Button("Sphere")) {
				selectedUI = 0;
			}
			ImGui::SameLine();
			if (ImGui::Button("Object")) {
				selectedUI = 1;
			}
			ImGui::SameLine();
			if (ImGui::Button("UVTranslate")) {
				selectedUI = 2;
			}

			ImGui::Separator();

			// UI選択用コンボボックス
			switch (selectedUI) {
			case 0: // Sphere
				ImGui::Text("Editing Sphere");
				ImGui::ColorEdit4("Sphere Color", &(materialDataSphere->color).x);
				ImGui::DragFloat3("Translate", &transformSphere.translate.x);
				ImGui::DragFloat3("Rotate", &transformSphere.rotate.x, 0.01f);
				ImGui::DragFloat3("Scale", &transformSphere.scale.x);
				break;

			case 1: // Object
				ImGui::Text("Editing Object");
				ImGui::ColorEdit4("Object Color", &(materialData->color).x);
				ImGui::DragFloat3("Translate", &transform.translate.x);
				ImGui::DragFloat3("Rotate", &transform.rotate.x, 0.01f);
				ImGui::DragFloat3("Scale", &transform.scale.x);
				break;

			case 2: // UVTranslate (Sprite)
				ImGui::Text("Editing UVTransform");
				ImGui::ColorEdit4("Sprite Color", &(materialDataSprite->color).x);
				ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
				ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
				break;
			}
			ImGui::DragFloat3("CameraTransform", &cameraTransform.translate.x);
			ImGui::Checkbox("useMonsterBall", &useMonsterBall);

			ImGui::End();
			//開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
			ImGui::ShowDemoWindow();

			//transform.rotate.y += 0.03f;
			Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
			Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
			wvpData->WVP = worldViewProjectionMatrix;
			wvpData->World = worldMatrix;

			//Sprite用のWorldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
			transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
			transformationMatrixDataSprite->World = worldMatrixSprite;

			Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
			materialDataSprite->uvTransform = uvTransformMatrix;

			//ImGuiの内部コマンドを生成する
			ImGui::Render();
			
			//入力の更新
			input->Update();

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

			//描画前処理
			dxCommon->PreDraw();

			//Spriteの描画準備Spriteの描画に共通のグラフィックスコマンドを積む
			spriteCommon->SetCommonDrawSetting();

			//RootSignatureを設定。PS0に設定しているけど別途設定が必要
			dxCommon->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
			dxCommon->GetCommandList()->SetPipelineState(graphicsPipelineState.Get());//PSOを設定

			// 描画開始（共通設定は済んでいる前提）
			if (selectedUI == 0) {
				// Sphere のみ描画
				dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
				dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferViewSphere);
				dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceSphere->GetGPUVirtualAddress());
				dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSphere->GetGPUVirtualAddress());
				dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU); // 例
				dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResourceSprite->GetGPUVirtualAddress());
				dxCommon->GetCommandList()->DrawIndexedInstanced(sphereIndexNum, 1, 0, 0, 0);
			} else if (selectedUI == 1) {
				// Object のみ描画
				dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewObj);
				dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
				dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
				dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
				dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResourceSprite->GetGPUVirtualAddress());
				dxCommon->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
			}




			//実際のcommandListのImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());


			
			dxCommon->PostDraw();

	}
	//XAudio2解放
	xAudio2.Reset();
	//音声データ解放
	SoundUnload(&soundData1);

	delete sprite;
	delete spriteCommon;


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