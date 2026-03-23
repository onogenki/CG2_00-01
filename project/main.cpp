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
#include "Camera.h"
#include "CameraManager.h"


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
#include "ModelManager.h"
#include "SrvManager.h"
#include"ImGuiManager.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"

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

	SrvManager* srvManager = nullptr;
	//SRVマネージャの初期化
	srvManager = new SrvManager();
	srvManager->Initialize(dxCommon);

	TextureManager::GetInstance()->Initialize(dxCommon, srvManager);

	ImGuiManager* imGuiManager = new ImGuiManager();
	imGuiManager->Initialize(winApp, dxCommon, srvManager);


	ModelManager::GetInstance()->Initialize(dxCommon);

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

	//カメラマネージャ
	CameraManager* cameraManager = new CameraManager();

	//メインカメラ
	Camera* mainCamera = new Camera();
	mainCamera->SetRotate({ 0.0f,0.0f,0.0f });
	mainCamera->SetTranslate({ 0.0f,0.0f,-10.0f });
	cameraManager->AddCamera("MainCamera", mainCamera);

	//上アングルカメラ
	Camera* upCamera = new Camera();
	upCamera->SetRotate({ 0.785f,0.0f,0.0f });
	upCamera->SetTranslate({ 0.0f,5.0f,-5.0f });
	cameraManager->AddCamera("UpCamera", upCamera);

	//MainCameraをアクティブ
	cameraManager->SetActiveCamera("MainCamera");

	//共通部にはマネージャのアクティブカメラを渡す
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());

	//.objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("axis.obj");

	std::vector<Object3d*> objects;

	Object3d* objectPlane = new Object3d();
	objectPlane->Initialize(object3dCommon);
	objectPlane->SetModel("plane.obj");
	objectPlane->GetTransform().translate = { 1.0f, 0.0f, 0.0f }; // 左に配置
	objects.push_back(objectPlane); // リストに追加

	// --- 2つ目: Axis ---
	Object3d* objectAxis = new Object3d();
	objectAxis->Initialize(object3dCommon);
	objectAxis->SetModel("axis.obj");
	objectAxis->GetTransform().translate = { 2.0f, 0.0f, 0.0f }; // 右に配置
	objects.push_back(objectAxis); // リストに追加


	Object3d::DirectionalLight lightData;

	SpriteCommon* spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon);

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");//1枚目
	TextureManager::GetInstance()->LoadTexture("Resources/monsterBall.png");//2枚目
	TextureManager::GetInstance()->LoadTexture("Resources/circle.png");

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

	//パーティクル
	// マネージャの初期化（シングルトンなのでGetInstanceを使う）
	ParticleManager::GetInstance()->Initialize(dxCommon, srvManager);
	//座標、1回の発生数、発生頻度[秒]
	Transform emitterTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	//Circleパーティクル
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "Resources/circle.png");
	ParticleEmitter* emitterCircle = new ParticleEmitter("Circle", emitterTransform, 1, 0.1f);

	//四角形のパーティクル
	ParticleManager::GetInstance()->CreateParticleGroup("Plane", "Resources/uvChecker.png");
	ParticleEmitter* emitterPlane = new ParticleEmitter("Plane", emitterTransform, 1, 0.1f);

	//最初はcircleにする
	ParticleEmitter* activeEmitter = new ParticleEmitter("Circle", emitterTransform, 1, 0.1f);

	int selectedUI = 0;

	MSG msg{};
	//ウィンドウのxボタンが押されるまでループ
	while (true)
	{//ウィンドウにメッセージが来てたら最優先で処理される
		if (winApp->ProcessMessage())
		{
			break;
		}

		lightData = objectAxis->GetDirectionalLight();
		//ゲームの処理
		imGuiManager->Begin();
		/*ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();*/

		ImGui::Begin("test");
		if (ImGui::Button("UVTranslate")) {
			selectedUI = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("Object3d")) {
			selectedUI = 1;
		}
		ImGui::SameLine();
		if (ImGui::Button("Particle"))
		{
			selectedUI = 2;
		}


		ImGui::Separator();

		// UI選択用コンボボックス
		switch (selectedUI) {
		case 0: // Sprite
			ImGui::Text("Editing UVTranslate ( Sprite )");
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
			//リスト内のすべてのオブジェクトをImGuiで表示
			for (int i = 0; i < objects.size(); ++i) {
				ImGui::PushID(i); // IDを分けて干渉を防ぐ

				if (i == 0)
				{
					ImGui::Text("Plane");
				} else if (i == 1)
				{
					ImGui::Text("Axis");
				}

				// 各オブジェクトのTransformを取得して操作
				Transform& transform = objects[i]->GetTransform();

				ImGui::DragFloat3("Translate", &transform.translate.x, 0.01f);
				ImGui::DragFloat3("Rotate", &transform.rotate.x, 0.01f);
				ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);

				ImGui::Separator();
				ImGui::PopID();

			}
			break;
		case 2://Particle
			ImGui::Text("Editing Particle");
			static int particleType = 0;

			if (ImGui::Button("Circle Texture"))
			{
				delete activeEmitter;
				activeEmitter = new ParticleEmitter("Circle", emitterTransform, 5, 0.1f);
			}
			ImGui::SameLine();

			if (ImGui::Button("Plane Texture"))
			{
				delete activeEmitter;
				activeEmitter = new ParticleEmitter("Plane", emitterTransform, 5, 0.1f);
			}

			break;
		}

		ImGui::DragFloat3("DirectoinalLight:direction", &lightData.direction.x, 0.01f);//ハイライトの位置
		ImGui::DragFloat("DirectoinalLight:intensity", &lightData.intensity, 0.01f);//全体の明るさ
		ImGui::DragFloat3("DirectoinalLight:color", &lightData.color.x, 0.01f);
		
		ImGui::Separator();



		//カメラ
		Camera* activeCamera = cameraManager->GetActiveCamera();
		if (activeCamera)
		{//位置
			Vector3 cameraPos = activeCamera->GetTranslate();
			if (ImGui::DragFloat3("CameraTranslate", &cameraPos.x, 0.01f))
			{
				activeCamera->SetTranslate(cameraPos);
			}//角度
			Vector3 cameraRot = activeCamera->GetRotate();
			if (ImGui::DragFloat3("CameraRotate", &cameraRot.x, 0.01f))
			{
				activeCamera->SetRotate(cameraRot);
			}
		}
		ImGui::Text("Camera Control");
		if (ImGui::Button("Use MainCamera"))
		{//メインカメラ
			cameraManager->SetActiveCamera("MainCamera");
		}
		ImGui::SameLine();
		if (ImGui::Button("Use UpCamera"))
		{//上空カメラ
			cameraManager->SetActiveCamera("UpCamera");
		}
		ImGui::Separator();

		ImGui::End();
		//開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
		ImGui::ShowDemoWindow();
		imGuiManager->End();
		////ImGuiの内部コマンドを生成する
		//ImGui::Render();

		cameraManager->Update();


		//カメラのビュープロジェクション行列を渡して更新
		Matrix4x4 viewMatrix = cameraManager->GetActiveCamera()->GetViewMatrix();
		Matrix4x4 projectionMatrix = cameraManager->GetActiveCamera()->GetProjectionMatrix();
		Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

		//時間がきたら自動でパーティクル発生
		activeEmitter->Update();
		//パーティクル全体の更新
		ParticleManager::GetInstance()->Update(viewProjectionMatrix);

		//入力の更新
		input->Update();


		for (Object3d* object3d : objects) {
			//毎フレーム、マネージャから今のアクティブカメラをもらう
			object3d->SetCamera(cameraManager->GetActiveCamera());
			lightData.direction = Normalize(lightData.direction);
			object3d->SetDirectionalLight(lightData);//光を他のモデルにも分け与える

			object3d->Update();

		}

		for (Sprite* sprite : sprites)
		{
			sprite->Update();
		}

		////数字の0キーが押されていたら
		if (input->PushKey(DIK_0))
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
		srvManager->PreDraw();

		//3Dオブジェクトの描画準備3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
		object3dCommon->SetCommonDrawSetting();

		// Object のみ描画
		object3dCommon->SetCommonDrawSetting();
		for (Object3d* object3d : objects) {
			object3d->Draw();
		}

		//パーティクル描画
		ParticleManager::GetInstance()->Draw();

		// スプライト描画
		//Spriteの描画準備Spriteの描画に共通のグラフィックスコマンドを積む
		spriteCommon->SetCommonDrawSetting();
		for (Sprite* sprite : sprites)
		{
			sprite->Draw();
		}

		imGuiManager->Draw(dxCommon);
		////実際のcommandListのImGuiの描画コマンドを積む
		//ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());


		dxCommon->PostDraw();

	}
	//XAudio2解放
	xAudio2.Reset();
	//音声データ解放
	SoundUnload(&soundData1);

	//パーティクル全体解放
	delete activeEmitter;

	for (Sprite* sprite : sprites)
	{
		delete sprite;
	}
	sprites.clear();
	for (Object3d* object3d : objects) {
		delete object3d;
	}
	objects.clear();
	delete spriteCommon;
	delete object3dCommon;

	TextureManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();

	//ImGuiの終了処理
	imGuiManager->Finalize();
	/*ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();*/

	delete imGuiManager;
	delete srvManager;

#ifdef _DEBUG
#endif


	winApp->Finalize();

	return 0;
}