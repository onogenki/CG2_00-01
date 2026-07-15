#include "CaptureManager.h"

#include "DirectXCommon.h"
#include "PostEffect.h"
#include "SceneManager.h"

#include <Windows.h>
#include <wincodec.h>
#include <shellapi.h>
#include <vfw.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "vfw32.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace {
std::string GetEnvironmentString(const char* name)
{
	char* value = nullptr;
	size_t size = 0;
	if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
		return {};
	}
	std::string result(value);
	std::free(value);
	return result;
}

bool OpenFolderInExplorer(const std::filesystem::path& directory)
{
	std::error_code errorCode;
	std::filesystem::create_directories(directory, errorCode);
	if (errorCode) {
		return false;
	}

	const HINSTANCE result = ShellExecuteW(
		nullptr,
		L"open",
		directory.wstring().c_str(),
		nullptr,
		nullptr,
		SW_SHOWNORMAL);
	return reinterpret_cast<INT_PTR>(result) > 32;
}
}

CaptureManager* CaptureManager::GetInstance()
{
	static CaptureManager instance;
	return &instance;
}

void CaptureManager::Initialize()
{
	if (initialized_) {
		return;
	}
	initialized_ = true;
	photoRequested_ = false;
	photoCapturePending_ = false;
	recordingStartRequested_ = false;
	recordingStartCapturePending_ = false;
	recordingGeneration_ = 0;
	pendingCaptureRequests_.clear();
	droppedCaptureFrames_ = 0;
	CreateCaptureDirectories();
	smokeEnabled_ = GetEnvironmentString("CG2_CAPTURE_SMOKE") == "1";
	performanceBaselineEnabled_ = GetEnvironmentString("CG2_CAPTURE_PERF_BASELINE") == "1";
	if (performanceBaselineEnabled_) {
		replayBufferEnabled_ = false;
	}
	smokeFinished_ = false;
	smokeFrame_ = 0;
	smokeGamePlayFrame_ = 0;
	smokeCaptureAttempts_ = 0;
	smokeVisibleCaptures_ = 0;
	smokeSavedPhotos_ = 0;
	smokeVideoFrameCount_ = 0;
	smokePhotoPaths_.clear();
	smokeFirstVideoFrame_.clear();
	smokeLastVideoFrame_.clear();
	smokeFirstReplayFrame_.clear();
	smokeLastReplayFrame_.clear();
	smokeVideoWidth_ = 0;
	smokeVideoHeight_ = 0;
	smokeReplayFrameCount_ = 0;
	smokeReplayDroppedFrames_ = 0;
	smokePerformanceFrameCount_ = 0;
	smokePerformanceTime_ = 0.0f;
	smokeMaximumDeltaTime_ = 0.0f;
	smokeReadbackMilliseconds_ = 0.0;
	smokeVideoMilliseconds_ = 0.0;
	smokeReplayMilliseconds_ = 0.0;
	smokeVideoWorkerMilliseconds_ = 0.0;
	smokeReadbackSamples_ = 0;
	smokeVideoSamples_ = 0;
	smokeReplaySamples_ = 0;
	smokeVideoWorkerSamples_ = 0;
	smokeMainVideoPath_.clear();
	smokeReplayPath_.clear();
	smokeTrackActiveVideo_ = false;
	smokeLogPath_ = std::filesystem::path("logs") / "capture_manager_smoke.log";
	const std::string requestedFormat = GetEnvironmentString("CG2_CAPTURE_VIDEO_FORMAT");
	if (requestedFormat == "AVI" || requestedFormat == "avi") {
		selectedVideoFormat_ = VideoFormat::Avi;
	} else if (requestedFormat == "MP4" || requestedFormat == "mp4") {
		selectedVideoFormat_ = VideoFormat::Mp4;
	}
	if (smokeEnabled_) {
		std::ofstream log(smokeLogPath_, std::ios::trunc);
		if (log) {
			log << "START common capture smoke\n";
		}
	}
#ifdef USE_IMGUI
	StartVideoEncoder();
	StartReplayEncoder();
#endif
}

void CaptureManager::Finalize()
{
	if (!initialized_) {
		return;
	}
	WaitForReplaySave();
	ProcessCompletedCaptures(true);
	if (isRecording_) {
		StopRecording("Recording stopped when the game closed.");
	} else {
		EndActiveVideo();
	}
	StopVideoEncoder();
	StopReplayEncoder();
	ClearReplayBuffer();
	photoRequested_ = false;
	photoCapturePending_ = false;
	recordingStartRequested_ = false;
	recordingStartCapturePending_ = false;
	pendingCaptureRequests_.clear();
	initialized_ = false;
}

bool CaptureManager::IsVisiblyNonBlack(
	const std::vector<unsigned char>& pixels,
	int width,
	int height) const
{
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	if (width <= 0 || height <= 0 || pixels.size() < pixelCount * 4) {
		return false;
	}

	size_t visiblePixels = 0;
	for (size_t index = 0; index < pixelCount; ++index) {
		const size_t offset = index * 4;
		const unsigned int brightness =
			static_cast<unsigned int>(pixels[offset + 0]) +
			static_cast<unsigned int>(pixels[offset + 1]) +
			static_cast<unsigned int>(pixels[offset + 2]);
		if (brightness >= 24) {
			++visiblePixels;
		}
	}
	return visiblePixels >= pixelCount / 100;
}

void CaptureManager::UpdateSmokeBeforeCapture()
{
	if (!smokeEnabled_ || smokeFinished_) {
		return;
	}

	++smokeFrame_;
	const std::string& sceneName = SceneManager::GetInstance()->GetCurrentSceneName();
	if (sceneName == "TITLE") {
		if (smokeFrame_ == 5) {
			++recordingGeneration_;
			recordingStartRequested_ = true;
		}
		if (smokeFrame_ >= 6 && smokeFrame_ <= 13) {
			photoRequested_ = true;
		}
		if (smokeFrame_ == 20 && !SceneManager::GetInstance()->ChangeScene("GAMEPLAY")) {
			FinishSmoke(false, "Could not request TITLE to GAMEPLAY scene change.");
		}
		return;
	}

	if (sceneName == "GAMEPLAY") {
		++smokeGamePlayFrame_;
		if (smokeGamePlayFrame_ <= 8) {
			photoRequested_ = true;
		}
		if (smokeGamePlayFrame_ == 45 && isRecording_) {
			smokeVideoFrameCount_ = recordingFrameIndex_;
			StopRecording("Capture smoke stopped the cross-scene video.");
			smokeMainVideoPath_ = lastVideoPath_;
		}
		if (smokeGamePlayFrame_ == 48) {
			if (!SaveReplayClip()) {
				FinishSmoke(false, "Could not save the shared replay buffer.");
				return;
			}
		}
		if (smokeGamePlayFrame_ == 55) {
			WaitForReplaySave();
			smokeReplayPath_ = lastReplayPath_;
			std::error_code errorCode;
			bool filesExist = smokePhotoPaths_.size() == 16;
			for (const std::filesystem::path& path : smokePhotoPaths_) {
				filesExist = filesExist &&
					path.extension() == ".png" &&
					std::filesystem::exists(path, errorCode) &&
					!errorCode;
				errorCode.clear();
			}
			const bool videoExists =
				!smokeMainVideoPath_.empty() &&
				std::filesystem::exists(smokeMainVideoPath_, errorCode) &&
				!errorCode &&
				std::filesystem::file_size(smokeMainVideoPath_, errorCode) > 1024 &&
				!errorCode;
			errorCode.clear();
			const bool replayExists =
				!smokeReplayPath_.empty() &&
				std::filesystem::exists(smokeReplayPath_, errorCode) &&
				!errorCode &&
				std::filesystem::file_size(smokeReplayPath_, errorCode) > 1024 &&
				!errorCode;
			bool mp4ContentValid = true;
			double directError = 0.0;
			double flippedError = 0.0;
			double swappedError = 0.0;
			if (videoExists && smokeMainVideoPath_.extension() == ".mp4") {
				std::vector<unsigned char> decodedFirst;
				std::vector<unsigned char> decodedLast;
				int decodedWidth = 0;
				int decodedHeight = 0;
				mp4ContentValid = DecodeMp4FirstAndLastFrame(
					smokeMainVideoPath_, decodedFirst, decodedLast, decodedWidth, decodedHeight);
				if (mp4ContentValid) {
					const bool dimensionsMatch =
						decodedWidth == smokeVideoWidth_ && decodedHeight == smokeVideoHeight_;
					directError = (
						CalculateFrameError(smokeFirstVideoFrame_, decodedFirst, decodedWidth, decodedHeight, false, false) +
						CalculateFrameError(smokeLastVideoFrame_, decodedLast, decodedWidth, decodedHeight, false, false)) * 0.5;
					flippedError = (
						CalculateFrameError(smokeFirstVideoFrame_, decodedFirst, decodedWidth, decodedHeight, true, false) +
						CalculateFrameError(smokeLastVideoFrame_, decodedLast, decodedWidth, decodedHeight, true, false)) * 0.5;
					swappedError = (
						CalculateFrameError(smokeFirstVideoFrame_, decodedFirst, decodedWidth, decodedHeight, false, true) +
						CalculateFrameError(smokeLastVideoFrame_, decodedLast, decodedWidth, decodedHeight, false, true)) * 0.5;
					mp4ContentValid =
						dimensionsMatch &&
						directError < 55.0 &&
						directError < flippedError * 0.85 &&
						directError < swappedError * 0.95;

					const std::filesystem::path decodedFirstPath =
						GetCaptureDirectory("Screenshots") / "CG2_mp4_decoded_first.bmp";
					const std::filesystem::path decodedLastPath =
						GetCaptureDirectory("Screenshots") / "CG2_mp4_decoded_last.bmp";
					mp4ContentValid = mp4ContentValid &&
						SavePixelsAsBmp(decodedFirstPath, decodedFirst, decodedWidth, decodedHeight) &&
						SavePixelsAsBmp(decodedLastPath, decodedLast, decodedWidth, decodedHeight);
				}
			}
			bool replayContentValid = true;
			double replayDirectError = 0.0;
			double replayFlippedError = 0.0;
			double replaySwappedError = 0.0;
			int decodedReplayFrameCount = 0;
			if (replayExists && smokeReplayPath_.extension() == ".mp4") {
				std::vector<unsigned char> decodedReplayFirst;
				std::vector<unsigned char> decodedReplayLast;
				int decodedReplayWidth = 0;
				int decodedReplayHeight = 0;
				replayContentValid = DecodeMp4FirstAndLastFrame(
					smokeReplayPath_,
					decodedReplayFirst,
					decodedReplayLast,
					decodedReplayWidth,
					decodedReplayHeight,
					&decodedReplayFrameCount);
				if (replayContentValid) {
					replayDirectError = (
						CalculateFrameError(smokeFirstReplayFrame_, decodedReplayFirst, decodedReplayWidth, decodedReplayHeight, false, false) +
						CalculateFrameError(smokeLastReplayFrame_, decodedReplayLast, decodedReplayWidth, decodedReplayHeight, false, false)) * 0.5;
					replayFlippedError = (
						CalculateFrameError(smokeFirstReplayFrame_, decodedReplayFirst, decodedReplayWidth, decodedReplayHeight, true, false) +
						CalculateFrameError(smokeLastReplayFrame_, decodedReplayLast, decodedReplayWidth, decodedReplayHeight, true, false)) * 0.5;
					replaySwappedError = (
						CalculateFrameError(smokeFirstReplayFrame_, decodedReplayFirst, decodedReplayWidth, decodedReplayHeight, false, true) +
						CalculateFrameError(smokeLastReplayFrame_, decodedReplayLast, decodedReplayWidth, decodedReplayHeight, false, true)) * 0.5;
					replayContentValid =
						decodedReplayFrameCount == smokeReplayFrameCount_ &&
						smokeReplayFrameCount_ >= 20 &&
						replayDirectError < 55.0 &&
						replayDirectError < replayFlippedError * 0.85 &&
						replayDirectError < replaySwappedError * 0.95;

					const std::filesystem::path replayFirstPath =
						GetCaptureDirectory("Screenshots") / "CG2_replay_decoded_first.bmp";
					const std::filesystem::path replayLastPath =
						GetCaptureDirectory("Screenshots") / "CG2_replay_decoded_last.bmp";
					replayContentValid = replayContentValid &&
						SavePixelsAsBmp(replayFirstPath, decodedReplayFirst, decodedReplayWidth, decodedReplayHeight) &&
						SavePixelsAsBmp(replayLastPath, decodedReplayLast, decodedReplayWidth, decodedReplayHeight);
				}
			}
			const float averageFps = smokePerformanceTime_ > 0.0f
				? static_cast<float>(smokePerformanceFrameCount_) / smokePerformanceTime_
				: 0.0f;
			const bool success =
				filesExist &&
				videoExists &&
				replayExists &&
				mp4ContentValid &&
				replayContentValid &&
				smokeReplayDroppedFrames_ == 0 &&
				averageFps >= 30.0f &&
				smokeSavedPhotos_ == 16 &&
				smokeCaptureAttempts_ == 16 &&
				smokeVisibleCaptures_ == smokeCaptureAttempts_ &&
				smokeVideoFrameCount_ >= 2;
			std::ostringstream message;
			message << "scene=GAMEPLAY photos=" << smokeSavedPhotos_
				<< " visible=" << smokeVisibleCaptures_ << '/' << smokeCaptureAttempts_
				<< " videoFrames=" << smokeVideoFrameCount_
				<< " directError=" << directError
				<< " flippedError=" << flippedError
				<< " swappedError=" << swappedError
				<< " replayFrames=" << smokeReplayFrameCount_ << '/' << decodedReplayFrameCount
				<< " replayDropped=" << smokeReplayDroppedFrames_
				<< " replayDirectError=" << replayDirectError
				<< " replayFlippedError=" << replayFlippedError
				<< " replaySwappedError=" << replaySwappedError
				<< " averageFps=" << averageFps
				<< " maxFrameMs=" << smokeMaximumDeltaTime_ * 1000.0f
				<< " readbackMs=" << (smokeReadbackSamples_ > 0 ? smokeReadbackMilliseconds_ / smokeReadbackSamples_ : 0.0)
				<< " videoQueueMs=" << (smokeVideoSamples_ > 0 ? smokeVideoMilliseconds_ / smokeVideoSamples_ : 0.0)
				<< " videoWorkerMs=" << (smokeVideoWorkerSamples_ > 0 ? smokeVideoWorkerMilliseconds_ / smokeVideoWorkerSamples_ : 0.0)
				<< " replayMs=" << (smokeReplaySamples_ > 0 ? smokeReplayMilliseconds_ / smokeReplaySamples_ : 0.0)
				<< " video=" << smokeMainVideoPath_.string()
				<< " replay=" << smokeReplayPath_.string();
			FinishSmoke(success, message.str());
		}
	}
}

void CaptureManager::UpdateSmokeAfterCapture(bool captured, bool visiblyNonBlack)
{
	if (!smokeEnabled_ || smokeFinished_) {
		return;
	}
	++smokeCaptureAttempts_;
	if (captured && visiblyNonBlack) {
		++smokeVisibleCaptures_;
	}
}

void CaptureManager::FinishSmoke(bool success, const std::string& message)
{
	if (smokeFinished_) {
		return;
	}
	smokeFinished_ = true;
	std::ofstream log(smokeLogPath_, std::ios::app);
	if (log) {
		log << (success ? "SUCCESS: " : "FAILURE: ") << message << '\n';
	}
	PostQuitMessage(success ? 0 : 1);
}

std::filesystem::path CaptureManager::GetCaptureDirectory(const char* folderName) const
{
	std::error_code errorCode;
	const std::filesystem::path resourceDirectory = std::filesystem::absolute("resources", errorCode);
	return (errorCode ? std::filesystem::path("resources") : resourceDirectory) / "Captures" / folderName;
}

bool CaptureManager::CreateCaptureDirectories() const
{
	std::error_code errorCode;
	for (const char* folderName : { "Screenshots", "Videos", "Replays" }) {
		std::filesystem::create_directories(GetCaptureDirectory(folderName), errorCode);
		if (errorCode) {
			return false;
		}
	}
	return true;
}

std::string CaptureManager::MakeTimestampString() const
{
	const auto now = std::chrono::system_clock::now();
	const std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
	std::tm localTime{};
	localtime_s(&localTime, &currentTime);
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::ostringstream stream;
	stream << std::put_time(&localTime, "%Y%m%d_%H%M%S")
		<< '_' << std::setw(3) << std::setfill('0') << milliseconds.count();
	return stream.str();
}

bool CaptureManager::CapturePixels(std::vector<unsigned char>& pixels, int& width, int& height) const
{
	return DirectXCommon::GetInstance()->CaptureGameTexturePixels(
		pixels,
		width,
		height,
		PostEffect::GetInstance()->IsEnabled());
}

bool CaptureManager::SavePixelsAsBmp(
	const std::filesystem::path& filePath,
	const std::vector<unsigned char>& pixels,
	int width,
	int height) const
{
	if (pixels.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4 || width <= 0 || height <= 0) {
		return false;
	}

	std::error_code errorCode;
	std::filesystem::create_directories(filePath.parent_path(), errorCode);
	if (errorCode) {
		return false;
	}

	const DWORD imageSize = static_cast<DWORD>(width * height * 4);
	BITMAPFILEHEADER fileHeader{};
	fileHeader.bfType = 0x4D42;
	fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	fileHeader.bfSize = fileHeader.bfOffBits + imageSize;

	BITMAPINFOHEADER bitmapHeader{};
	bitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapHeader.biWidth = width;
	bitmapHeader.biHeight = -height;
	bitmapHeader.biPlanes = 1;
	bitmapHeader.biBitCount = 32;
	bitmapHeader.biCompression = BI_RGB;

	std::ofstream file(filePath, std::ios::binary);
	if (!file) {
		return false;
	}
	file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
	file.write(reinterpret_cast<const char*>(&bitmapHeader), sizeof(bitmapHeader));
	file.write(reinterpret_cast<const char*>(pixels.data()), imageSize);
	return file.good();
}

bool CaptureManager::SavePixelsAsPng(
	const std::filesystem::path& filePath,
	const std::vector<unsigned char>& pixels,
	int width,
	int height) const
{
	if (width <= 0 || height <= 0 ||
		pixels.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
		return false;
	}

	std::error_code errorCode;
	std::filesystem::create_directories(filePath.parent_path(), errorCode);
	if (errorCode) {
		return false;
	}
	std::vector<unsigned char> opaquePixels = pixels;
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	for (size_t index = 0; index < pixelCount; ++index) {
		opaquePixels[index * 4 + 3] = 255;
	}

	DirectX::Image image{};
	image.width = static_cast<size_t>(width);
	image.height = static_cast<size_t>(height);
	image.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	image.rowPitch = static_cast<size_t>(width) * 4;
	image.slicePitch = image.rowPitch * static_cast<size_t>(height);
	image.pixels = opaquePixels.data();
	return SUCCEEDED(DirectX::SaveToWICFile(
		image,
		DirectX::WIC_FLAGS_FORCE_SRGB,
		GUID_ContainerFormatPng,
		filePath.wstring().c_str(),
		&GUID_WICPixelFormat32bppBGRA));
}

bool CaptureManager::BeginRecordingAvi(const std::filesystem::path& filePath, int width, int height, int frameRate)
{
	if (width <= 0 || height <= 0 || frameRate <= 0) {
		return false;
	}
	EndRecordingAvi();

	std::error_code errorCode;
	std::filesystem::create_directories(filePath.parent_path(), errorCode);
	if (errorCode) {
		return false;
	}

	AVIFileInit();
	aviInitialized_ = true;
	PAVIFILE aviFile = nullptr;
	HRESULT result = AVIFileOpenW(&aviFile, filePath.wstring().c_str(), OF_WRITE | OF_CREATE, nullptr);
	if (FAILED(result)) {
		EndRecordingAvi();
		return false;
	}

	AVISTREAMINFOW streamInfo{};
	streamInfo.fccType = streamtypeVIDEO;
	streamInfo.dwScale = 1;
	streamInfo.dwRate = static_cast<DWORD>(frameRate);
	streamInfo.dwSuggestedBufferSize = static_cast<DWORD>(width * height * 4);
	SetRect(&streamInfo.rcFrame, 0, 0, width, height);

	PAVISTREAM aviStream = nullptr;
	result = AVIFileCreateStreamW(aviFile, &aviStream, &streamInfo);
	if (FAILED(result)) {
		AVIFileRelease(aviFile);
		EndRecordingAvi();
		return false;
	}

	BITMAPINFOHEADER bitmapHeader{};
	bitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapHeader.biWidth = width;
	bitmapHeader.biHeight = height;
	bitmapHeader.biPlanes = 1;
	bitmapHeader.biBitCount = 32;
	bitmapHeader.biCompression = BI_RGB;
	bitmapHeader.biSizeImage = static_cast<DWORD>(width * height * 4);
	result = AVIStreamSetFormat(aviStream, 0, &bitmapHeader, sizeof(bitmapHeader));
	if (FAILED(result)) {
		AVIStreamRelease(aviStream);
		AVIFileRelease(aviFile);
		EndRecordingAvi();
		return false;
	}

	recordingAviFile_ = aviFile;
	recordingAviStream_ = aviStream;
	recordingVideoPath_ = filePath;
	recordingVideoWidth_ = width;
	recordingVideoHeight_ = height;
	return true;
}

bool CaptureManager::AppendRecordingFrame(
	const std::vector<unsigned char>& pixels,
	int width,
	int height,
	int frameIndex)
{
	if (!recordingAviStream_ || width <= 0 || height <= 0 || pixels.empty()) {
		return false;
	}

	const std::vector<unsigned char>* framePixels = &pixels;
	std::vector<unsigned char> resizedPixels;
	if (width != recordingVideoWidth_ || height != recordingVideoHeight_) {
		resizedPixels = ResizePixels(pixels, width, height, recordingVideoWidth_, recordingVideoHeight_);
		framePixels = &resizedPixels;
		width = recordingVideoWidth_;
		height = recordingVideoHeight_;
	}
	if (framePixels->size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
		return false;
	}

	const size_t rowSize = static_cast<size_t>(width) * 4;
	std::vector<unsigned char> bottomUpPixels(framePixels->size());
	for (int y = 0; y < height; ++y) {
		const unsigned char* source = framePixels->data() + static_cast<size_t>(height - 1 - y) * rowSize;
		unsigned char* destination = bottomUpPixels.data() + static_cast<size_t>(y) * rowSize;
		std::memcpy(destination, source, rowSize);
	}

	const HRESULT result = AVIStreamWrite(
		static_cast<PAVISTREAM>(recordingAviStream_),
		frameIndex,
		1,
		bottomUpPixels.data(),
		static_cast<LONG>(bottomUpPixels.size()),
		AVIIF_KEYFRAME,
		nullptr,
		nullptr);
	return SUCCEEDED(result);
}

void CaptureManager::EndRecordingAvi()
{
	if (recordingAviStream_) {
		AVIStreamRelease(static_cast<PAVISTREAM>(recordingAviStream_));
		recordingAviStream_ = nullptr;
	}
	if (recordingAviFile_) {
		AVIFileRelease(static_cast<PAVIFILE>(recordingAviFile_));
		recordingAviFile_ = nullptr;
	}
	if (aviInitialized_) {
		AVIFileExit();
		aviInitialized_ = false;
	}
	recordingVideoWidth_ = 0;
	recordingVideoHeight_ = 0;
}

bool CaptureManager::BeginRecordingMp4(
	const std::filesystem::path& filePath,
	int width,
	int height,
	int frameRate)
{
	if (width <= 0 || height <= 0 || frameRate <= 0) {
		return false;
	}
	EndRecordingMp4();

	std::error_code errorCode;
	std::filesystem::create_directories(filePath.parent_path(), errorCode);
	if (errorCode) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFAttributes> writerAttributes;
	HRESULT result = MFCreateAttributes(&writerAttributes, 2);
	if (SUCCEEDED(result)) {
		writerAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		writerAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
	}

	Microsoft::WRL::ComPtr<IMFSinkWriter> sinkWriter;
	if (SUCCEEDED(result)) {
		result = MFCreateSinkWriterFromURL(
			filePath.wstring().c_str(), nullptr, writerAttributes.Get(), &sinkWriter);
	}
	if (FAILED(result)) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaType> outputType;
	result = MFCreateMediaType(&outputType);
	if (SUCCEEDED(result)) result = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (SUCCEEDED(result)) result = outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	const UINT32 bitRate = static_cast<UINT32>((std::clamp)(
		static_cast<unsigned long long>(width) * static_cast<unsigned long long>(height) * 10ULL,
		8'000'000ULL,
		40'000'000ULL));
	if (SUCCEEDED(result)) result = outputType->SetUINT32(MF_MT_AVG_BITRATE, bitRate);
	if (SUCCEEDED(result)) result = outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	if (SUCCEEDED(result)) result = MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, width, height);
	if (SUCCEEDED(result)) result = MFSetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, frameRate, 1);
	if (SUCCEEDED(result)) result = MFSetAttributeRatio(outputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

	DWORD streamIndex = 0;
	if (SUCCEEDED(result)) result = sinkWriter->AddStream(outputType.Get(), &streamIndex);

	Microsoft::WRL::ComPtr<IMFMediaType> inputType;
	if (SUCCEEDED(result)) result = MFCreateMediaType(&inputType);
	if (SUCCEEDED(result)) result = inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (SUCCEEDED(result)) result = inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	if (SUCCEEDED(result)) result = inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	if (SUCCEEDED(result)) result = MFSetAttributeSize(inputType.Get(), MF_MT_FRAME_SIZE, width, height);
	if (SUCCEEDED(result)) result = MFSetAttributeRatio(inputType.Get(), MF_MT_FRAME_RATE, frameRate, 1);
	if (SUCCEEDED(result)) result = MFSetAttributeRatio(inputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if (SUCCEEDED(result)) result = inputType->SetUINT32(MF_MT_DEFAULT_STRIDE, static_cast<UINT32>(width * 4));
	if (SUCCEEDED(result)) result = sinkWriter->SetInputMediaType(streamIndex, inputType.Get(), nullptr);
	if (SUCCEEDED(result)) result = sinkWriter->BeginWriting();
	if (FAILED(result)) {
		std::ofstream errorLog(std::filesystem::path("logs") / "capture_mp4_error.log", std::ios::trunc);
		if (errorLog) {
			errorLog << "BeginRecordingMp4 HRESULT=0x"
				<< std::hex << static_cast<unsigned long>(result)
				<< " width=" << std::dec << width
				<< " height=" << height
				<< " fps=" << frameRate
				<< '\n';
		}
		return false;
	}

	recordingMp4SinkWriter_ = sinkWriter.Detach();
	recordingMp4StreamIndex_ = streamIndex;
	recordingMp4SampleDuration_ = 10'000'000LL / frameRate;
	recordingVideoPath_ = filePath;
	recordingVideoWidth_ = width;
	recordingVideoHeight_ = height;
	return true;
}

bool CaptureManager::AppendRecordingFrameMp4(
	const std::vector<unsigned char>& pixels,
	int width,
	int height,
	int frameIndex)
{
	IMFSinkWriter* sinkWriter = static_cast<IMFSinkWriter*>(recordingMp4SinkWriter_);
	if (!sinkWriter || pixels.empty() || width <= 0 || height <= 0) {
		return false;
	}

	const std::vector<unsigned char>* framePixels = &pixels;
	std::vector<unsigned char> resizedPixels;
	if (width != recordingVideoWidth_ || height != recordingVideoHeight_) {
		resizedPixels = ResizePixels(pixels, width, height, recordingVideoWidth_, recordingVideoHeight_);
		framePixels = &resizedPixels;
		width = recordingVideoWidth_;
		height = recordingVideoHeight_;
	}
	const DWORD frameBytes = static_cast<DWORD>(width * height * 4);
	if (framePixels->size() < frameBytes) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
	HRESULT result = MFCreateMemoryBuffer(frameBytes, &mediaBuffer);
	BYTE* destination = nullptr;
	DWORD maximumLength = 0;
	if (SUCCEEDED(result)) result = mediaBuffer->Lock(&destination, &maximumLength, nullptr);
	if (SUCCEEDED(result) && maximumLength >= frameBytes) {
		std::memcpy(destination, framePixels->data(), frameBytes);
	} else if (SUCCEEDED(result)) {
		result = E_FAIL;
	}
	if (destination) {
		mediaBuffer->Unlock();
	}
	if (SUCCEEDED(result)) result = mediaBuffer->SetCurrentLength(frameBytes);

	Microsoft::WRL::ComPtr<IMFSample> sample;
	if (SUCCEEDED(result)) result = MFCreateSample(&sample);
	if (SUCCEEDED(result)) result = sample->AddBuffer(mediaBuffer.Get());
	if (SUCCEEDED(result)) result = sample->SetSampleTime(
		static_cast<LONGLONG>(frameIndex) * recordingMp4SampleDuration_);
	if (SUCCEEDED(result)) result = sample->SetSampleDuration(recordingMp4SampleDuration_);
	if (SUCCEEDED(result)) result = sinkWriter->WriteSample(recordingMp4StreamIndex_, sample.Get());
	return SUCCEEDED(result);
}

void CaptureManager::EndRecordingMp4()
{
	IMFSinkWriter* sinkWriter = static_cast<IMFSinkWriter*>(recordingMp4SinkWriter_);
	if (sinkWriter) {
		sinkWriter->Finalize();
		sinkWriter->Release();
		recordingMp4SinkWriter_ = nullptr;
	}
	recordingMp4StreamIndex_ = 0;
	recordingMp4SampleDuration_ = 0;
	recordingVideoWidth_ = 0;
	recordingVideoHeight_ = 0;
}

bool CaptureManager::BeginSelectedVideo(
	const std::filesystem::path& directory,
	const std::string& filePrefix,
	int width,
	int height,
	int frameRate)
{
	EndActiveVideo();
	const std::string timestamp = MakeTimestampString();
	if (selectedVideoFormat_ == VideoFormat::Mp4) {
		smokeTrackActiveVideo_ = smokeEnabled_ && filePrefix == "CG2_";
		recordingVideoPath_ = directory / (filePrefix + timestamp + ".mp4");
		const int mp4Width = (std::max)(2, width & ~1);
		const int mp4Height = (std::max)(2, height & ~1);
		if (BeginRecordingMp4(recordingVideoPath_, mp4Width, mp4Height, frameRate)) {
			activeVideoWriter_ = ActiveVideoWriter::Mp4;
			recordingFramesPerSecond_ = frameRate;
			return true;
		}

		recordingVideoPath_ = directory / (filePrefix + timestamp + ".avi");
		const int aviFallbackFrameRate = (std::min)(frameRate, kAviRecordingFramesPerSecond_);
		if (BeginRecordingAvi(recordingVideoPath_, width, height, aviFallbackFrameRate)) {
			activeVideoWriter_ = ActiveVideoWriter::Avi;
			recordingFramesPerSecond_ = aviFallbackFrameRate;
			lastMessage_ = "MP4 was unavailable, so recording automatically switched to AVI (Stable).";
			return true;
		}
		return false;
	}

	recordingVideoPath_ = directory / (filePrefix + timestamp + ".avi");
	smokeTrackActiveVideo_ = smokeEnabled_ && filePrefix == "CG2_";
	if (!BeginRecordingAvi(recordingVideoPath_, width, height, frameRate)) {
		return false;
	}
	activeVideoWriter_ = ActiveVideoWriter::Avi;
	recordingFramesPerSecond_ = frameRate;
	return true;
}

bool CaptureManager::AppendActiveVideoFrame(
	const std::vector<unsigned char>& pixels,
	int width,
	int height,
	int frameIndex)
{
	bool success = false;
	if (activeVideoWriter_ == ActiveVideoWriter::Mp4) {
		success = AppendRecordingFrameMp4(pixels, width, height, frameIndex);
	} else if (activeVideoWriter_ == ActiveVideoWriter::Avi) {
		success = AppendRecordingFrame(pixels, width, height, frameIndex);
	}
	if (success && smokeTrackActiveVideo_) {
		std::vector<unsigned char> normalized =
			ResizePixels(pixels, width, height, recordingVideoWidth_, recordingVideoHeight_);
		if (!normalized.empty()) {
			if (smokeFirstVideoFrame_.empty()) {
				smokeFirstVideoFrame_ = normalized;
			}
			smokeLastVideoFrame_ = std::move(normalized);
			smokeVideoWidth_ = recordingVideoWidth_;
			smokeVideoHeight_ = recordingVideoHeight_;
		}
	}
	return success;
}

void CaptureManager::StartVideoEncoder()
{
	if (videoEncoderThread_.joinable()) {
		StopVideoEncoder();
	}
	{
		std::lock_guard lock(videoEncodeMutex_);
		videoEncodeJobs_.clear();
		videoEncoderStopping_ = false;
		videoEncoderActiveJobs_ = 0;
	}
	videoEncodeFailed_.store(false);
	videoEncoderThread_ = std::thread(&CaptureManager::VideoEncoderWorker, this);
}

void CaptureManager::StopVideoEncoder()
{
	if (!videoEncoderThread_.joinable()) {
		return;
	}
	WaitForVideoEncoder();
	{
		std::lock_guard lock(videoEncodeMutex_);
		videoEncoderStopping_ = true;
	}
	videoEncodeWorkCondition_.notify_all();
	videoEncoderThread_.join();
	{
		std::lock_guard lock(videoEncodeMutex_);
		videoEncodeJobs_.clear();
		videoEncoderActiveJobs_ = 0;
	}
}

void CaptureManager::WaitForVideoEncoder()
{
	std::unique_lock lock(videoEncodeMutex_);
	videoEncodeIdleCondition_.wait(lock, [this] {
		return videoEncodeJobs_.empty() && videoEncoderActiveJobs_ == 0;
	});
}

bool CaptureManager::QueueVideoFrame(
	std::shared_ptr<const std::vector<unsigned char>> pixels,
	int width,
	int height,
	int frameIndex)
{
	if (!pixels || pixels->empty() || width <= 0 || height <= 0 || videoEncodeFailed_.load()) {
		return false;
	}
	{
		std::lock_guard lock(videoEncodeMutex_);
		if (videoEncoderStopping_ || videoEncodeJobs_.size() >= kVideoMaximumQueuedFrames_) {
			return false;
		}
		videoEncodeJobs_.push_back(VideoEncodeJob{
			std::move(pixels), width, height, frameIndex });
	}
	videoEncodeWorkCondition_.notify_one();
	return true;
}

void CaptureManager::VideoEncoderWorker()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	while (true) {
		VideoEncodeJob job{};
		{
			std::unique_lock lock(videoEncodeMutex_);
			videoEncodeWorkCondition_.wait(lock, [this] {
				return videoEncoderStopping_ || !videoEncodeJobs_.empty();
			});
			if (videoEncoderStopping_ && videoEncodeJobs_.empty()) {
				break;
			}
			job = std::move(videoEncodeJobs_.front());
			videoEncodeJobs_.pop_front();
			++videoEncoderActiveJobs_;
		}

		const auto encodeStart = std::chrono::steady_clock::now();
		const bool success = AppendActiveVideoFrame(
			*job.pixels, job.width, job.height, job.frameIndex);
		if (smokeEnabled_) {
			smokeVideoWorkerMilliseconds_ += std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - encodeStart).count();
			++smokeVideoWorkerSamples_;
		}

		{
			std::lock_guard lock(videoEncodeMutex_);
			if (!success) {
				videoEncodeFailed_.store(true);
				videoEncodeJobs_.clear();
			}
			--videoEncoderActiveJobs_;
			if (videoEncodeJobs_.empty() && videoEncoderActiveJobs_ == 0) {
				videoEncodeIdleCondition_.notify_all();
			}
		}
	}
	if (SUCCEEDED(comResult)) {
		CoUninitialize();
	}
}

void CaptureManager::EndActiveVideo()
{
	if (activeVideoWriter_ == ActiveVideoWriter::Mp4) {
		EndRecordingMp4();
	} else if (activeVideoWriter_ == ActiveVideoWriter::Avi) {
		EndRecordingAvi();
	} else {
		EndRecordingMp4();
		EndRecordingAvi();
	}
	activeVideoWriter_ = ActiveVideoWriter::None;
	smokeTrackActiveVideo_ = false;
}

bool CaptureManager::DecodeMp4FirstAndLastFrame(
	const std::filesystem::path& filePath,
	std::vector<unsigned char>& firstFrame,
	std::vector<unsigned char>& lastFrame,
	int& width,
	int& height,
	int* frameCount) const
{
	Microsoft::WRL::ComPtr<IMFAttributes> readerAttributes;
	HRESULT result = MFCreateAttributes(&readerAttributes, 1);
	if (SUCCEEDED(result)) {
		result = readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
	}
	if (FAILED(result)) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFSourceReader> reader;
	result = MFCreateSourceReaderFromURL(filePath.wstring().c_str(), readerAttributes.Get(), &reader);
	if (FAILED(result)) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaType> requestedType;
	result = MFCreateMediaType(&requestedType);
	if (SUCCEEDED(result)) result = requestedType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (SUCCEEDED(result)) result = requestedType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	if (SUCCEEDED(result)) result = reader->SetCurrentMediaType(
		MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, requestedType.Get());
	if (FAILED(result)) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaType> actualType;
	result = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &actualType);
	UINT32 frameWidth = 0;
	UINT32 frameHeight = 0;
	if (SUCCEEDED(result)) result = MFGetAttributeSize(actualType.Get(), MF_MT_FRAME_SIZE, &frameWidth, &frameHeight);
	if (FAILED(result) || frameWidth == 0 || frameHeight == 0) {
		return false;
	}
	width = static_cast<int>(frameWidth);
	height = static_cast<int>(frameHeight);

	UINT32 rawStride = frameWidth * 4;
	actualType->GetUINT32(MF_MT_DEFAULT_STRIDE, &rawStride);
	const LONG stride = static_cast<LONG>(rawStride);
	const size_t absoluteStride = static_cast<size_t>(stride < 0 ? -stride : stride);
	const size_t tightRowSize = static_cast<size_t>(width) * 4;
	if (absoluteStride < tightRowSize) {
		return false;
	}

	int decodedFrameCount = 0;
	while (true) {
		DWORD flags = 0;
		Microsoft::WRL::ComPtr<IMFSample> sample;
		result = reader->ReadSample(
			MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			0,
			nullptr,
			&flags,
			nullptr,
			&sample);
		if (FAILED(result)) {
			return false;
		}
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
			break;
		}
		if (!sample) {
			continue;
		}
		++decodedFrameCount;

		Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
		result = sample->ConvertToContiguousBuffer(&buffer);
		BYTE* data = nullptr;
		DWORD currentLength = 0;
		if (SUCCEEDED(result)) result = buffer->Lock(&data, nullptr, &currentLength);
		if (FAILED(result) || !data || currentLength < absoluteStride * static_cast<size_t>(height)) {
			if (data) buffer->Unlock();
			return false;
		}

		std::vector<unsigned char> decoded(tightRowSize * static_cast<size_t>(height));
		const BYTE* topRow = stride >= 0
			? data
			: data + absoluteStride * static_cast<size_t>(height - 1);
		for (int y = 0; y < height; ++y) {
			const BYTE* sourceRow = topRow + static_cast<ptrdiff_t>(y) * static_cast<ptrdiff_t>(stride);
			std::memcpy(decoded.data() + static_cast<size_t>(y) * tightRowSize, sourceRow, tightRowSize);
		}
		buffer->Unlock();
		if (firstFrame.empty()) {
			firstFrame = decoded;
		}
		lastFrame = std::move(decoded);
	}
	if (frameCount) {
		*frameCount = decodedFrameCount;
	}
	return !firstFrame.empty() && !lastFrame.empty();
}

double CaptureManager::CalculateFrameError(
	const std::vector<unsigned char>& reference,
	const std::vector<unsigned char>& decoded,
	int width,
	int height,
	bool flipVertically,
	bool swapRedBlue) const
{
	const size_t expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
	if (width <= 0 || height <= 0 || reference.size() < expectedSize || decoded.size() < expectedSize) {
		return 1.0e9;
	}

	double error = 0.0;
	for (int y = 0; y < height; ++y) {
		const int decodedY = flipVertically ? height - 1 - y : y;
		for (int x = 0; x < width; ++x) {
			const size_t referenceOffset =
				(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
			const size_t decodedOffset =
				(static_cast<size_t>(decodedY) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
			for (size_t channel = 0; channel < 3; ++channel) {
				const size_t decodedChannel = swapRedBlue ? 2 - channel : channel;
				error += std::abs(
					static_cast<int>(reference[referenceOffset + channel]) -
					static_cast<int>(decoded[decodedOffset + decodedChannel]));
			}
		}
	}
	return error / static_cast<double>(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
}

std::vector<unsigned char> CaptureManager::ResizePixels(
	const std::vector<unsigned char>& pixels,
	int sourceWidth,
	int sourceHeight,
	int targetWidth,
	int targetHeight) const
{
	if (sourceWidth <= 0 || sourceHeight <= 0 || targetWidth <= 0 || targetHeight <= 0 ||
		pixels.size() < static_cast<size_t>(sourceWidth) * static_cast<size_t>(sourceHeight) * 4) {
		return {};
	}
	if (sourceWidth == targetWidth && sourceHeight == targetHeight) {
		return pixels;
	}

	std::vector<unsigned char> resized(static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight) * 4);
	std::vector<int> sourceColumns(static_cast<size_t>(targetWidth));
	for (int x = 0; x < targetWidth; ++x) {
		sourceColumns[static_cast<size_t>(x)] =
			(static_cast<long long>(x) * sourceWidth) / targetWidth;
	}
	for (int y = 0; y < targetHeight; ++y) {
		const int sourceY = static_cast<int>(
			(static_cast<long long>(y) * sourceHeight) / targetHeight);
		const unsigned char* sourceRow =
			pixels.data() + static_cast<size_t>(sourceY) * static_cast<size_t>(sourceWidth) * 4;
		unsigned char* targetRow =
			resized.data() + static_cast<size_t>(y) * static_cast<size_t>(targetWidth) * 4;
		for (int x = 0; x < targetWidth; ++x) {
			std::memcpy(
				targetRow + static_cast<size_t>(x) * 4,
				sourceRow + static_cast<size_t>(sourceColumns[static_cast<size_t>(x)]) * 4,
				4);
		}
	}
	return resized;
}

void CaptureManager::UpdateReplayBuffer(
	std::shared_ptr<const std::vector<unsigned char>> pixels,
	int width,
	int height)
{
	if (!pixels || pixels->empty() || width <= 0 || height <= 0) {
		return;
	}

	const float scale = (std::min)(
		1.0f,
		(std::min)(
			static_cast<float>(kReplayWidth_) / static_cast<float>(width),
			static_cast<float>(kReplayHeight_) / static_cast<float>(height)));
	const int targetWidth = (std::max)(2, static_cast<int>(static_cast<float>(width) * scale) & ~1);
	const int targetHeight = (std::max)(2, static_cast<int>(static_cast<float>(height) * scale) & ~1);
	ReplayEncodeJob job{};
	job.sourceWidth = width;
	job.sourceHeight = height;
	job.width = targetWidth;
	job.height = targetHeight;
	job.pixels = std::move(pixels);

	{
		std::lock_guard lock(replayMutex_);
		if (replayTargetWidth_ != targetWidth || replayTargetHeight_ != targetHeight) {
			++replayGeneration_;
			replayFrames_.clear();
			replayEncodeJobs_.clear();
			replayTargetWidth_ = targetWidth;
			replayTargetHeight_ = targetHeight;
		}
		job.generation = replayGeneration_;
		if (replayEncodeJobs_.size() >= kReplayMaximumQueuedFrames_) {
			replayEncodeJobs_.pop_front();
			++replayDroppedFrames_;
		}
		replayEncodeJobs_.push_back(std::move(job));
	}
	replayWorkCondition_.notify_one();
}

void CaptureManager::StartReplayEncoder()
{
	StopReplayEncoder();
	{
		std::lock_guard lock(replayMutex_);
		replayEncoderStopping_ = false;
		replayActiveJobs_ = 0;
	}
	replayEncoderThread_ = std::thread(&CaptureManager::ReplayEncoderWorker, this);
}

void CaptureManager::StopReplayEncoder()
{
	{
		std::lock_guard lock(replayMutex_);
		replayEncoderStopping_ = true;
		replayEncodeJobs_.clear();
	}
	replayWorkCondition_.notify_all();
	if (replayEncoderThread_.joinable()) {
		replayEncoderThread_.join();
	}
}

void CaptureManager::ReplayEncoderWorker()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	while (true) {
		ReplayEncodeJob job{};
		{
			std::unique_lock lock(replayMutex_);
			replayWorkCondition_.wait(lock, [this] {
				return replayEncoderStopping_ || !replayEncodeJobs_.empty();
			});
			if (replayEncoderStopping_) {
				break;
			}
			job = std::move(replayEncodeJobs_.front());
			replayEncodeJobs_.pop_front();
			++replayActiveJobs_;
		}

		ReplayFrame frame{};
		const bool encoded = EncodeReplayFrame(job, frame);
		{
			std::lock_guard lock(replayMutex_);
			if (encoded && job.generation == replayGeneration_) {
				replayFrames_.push_back(std::move(frame));
				const size_t maximumFrameCount = static_cast<size_t>(kReplaySeconds_ * kReplayFramesPerSecond_);
				while (replayFrames_.size() > maximumFrameCount) {
					replayFrames_.pop_front();
				}
			}
			--replayActiveJobs_;
			if (replayEncodeJobs_.empty() && replayActiveJobs_ == 0) {
				replayIdleCondition_.notify_all();
			}
		}
	}
	if (SUCCEEDED(comResult)) {
		CoUninitialize();
	}
}

bool CaptureManager::EncodeReplayFrame(const ReplayEncodeJob& job, ReplayFrame& frame) const
{
	if (job.width <= 0 || job.height <= 0 || job.sourceWidth <= 0 || job.sourceHeight <= 0 ||
		!job.pixels || job.pixels->empty()) {
		return false;
	}
	std::vector<unsigned char> resizedPixels = ResizePixels(
		*job.pixels,
		job.sourceWidth,
		job.sourceHeight,
		job.width,
		job.height);
	if (resizedPixels.empty()) {
		return false;
	}

	DirectX::Image image{};
	image.width = static_cast<size_t>(job.width);
	image.height = static_cast<size_t>(job.height);
	image.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	image.rowPitch = static_cast<size_t>(job.width) * 4;
	image.slicePitch = image.rowPitch * static_cast<size_t>(job.height);
	image.pixels = resizedPixels.data();

	DirectX::Blob blob;
	const auto setQuality = [](IPropertyBag2* propertyBag) {
		PROPBAG2 option{};
		option.pstrName = const_cast<wchar_t*>(L"ImageQuality");
		VARIANT value{};
		VariantInit(&value);
		value.vt = VT_R4;
		value.fltVal = 0.94f;
		propertyBag->Write(1, &option, &value);
		VariantClear(&value);
	};
	const HRESULT result = DirectX::SaveToWICMemory(
		image,
		DirectX::WIC_FLAGS_NONE,
		GUID_ContainerFormatJpeg,
		blob,
		&GUID_WICPixelFormat24bppBGR,
		setQuality);
	if (FAILED(result) || blob.GetBufferSize() == 0) {
		return false;
	}

	frame.width = job.width;
	frame.height = job.height;
	const unsigned char* encoded = static_cast<const unsigned char*>(blob.GetBufferPointer());
	frame.encodedPixels.assign(encoded, encoded + blob.GetBufferSize());
	return true;
}

bool CaptureManager::DecodeReplayFrame(const ReplayFrame& frame, std::vector<unsigned char>& pixels) const
{
	if (frame.width <= 0 || frame.height <= 0 || frame.encodedPixels.empty()) {
		return false;
	}

	DirectX::ScratchImage decoded;
	HRESULT result = DirectX::LoadFromWICMemory(
		frame.encodedPixels.data(),
		frame.encodedPixels.size(),
		DirectX::WIC_FLAGS_NONE,
		nullptr,
		decoded);
	if (FAILED(result) || !decoded.GetImage(0, 0, 0)) {
		return false;
	}

	const DirectX::Image* source = decoded.GetImage(0, 0, 0);
	DirectX::ScratchImage converted;
	if (source->format != DXGI_FORMAT_B8G8R8A8_UNORM) {
		result = DirectX::Convert(
			*source,
			DXGI_FORMAT_B8G8R8A8_UNORM,
			DirectX::TEX_FILTER_DEFAULT,
			DirectX::TEX_THRESHOLD_DEFAULT,
			converted);
		if (FAILED(result) || !converted.GetImage(0, 0, 0)) {
			return false;
		}
		source = converted.GetImage(0, 0, 0);
	}

	const size_t tightRowPitch = static_cast<size_t>(frame.width) * 4;
	pixels.resize(tightRowPitch * static_cast<size_t>(frame.height));
	for (int y = 0; y < frame.height; ++y) {
		std::memcpy(
			pixels.data() + static_cast<size_t>(y) * tightRowPitch,
			source->pixels + static_cast<size_t>(y) * source->rowPitch,
			tightRowPitch);
	}
	return true;
}

void CaptureManager::ClearReplayBuffer()
{
	std::lock_guard lock(replayMutex_);
	++replayGeneration_;
	replayFrames_.clear();
	replayEncodeJobs_.clear();
	replayTargetWidth_ = 0;
	replayTargetHeight_ = 0;
	replayFrameTimer_ = 0.0f;
}

bool CaptureManager::SaveReplayClip()
{
	FinishReplaySaveIfReady();
	if (isRecording_ || recordingStartRequested_ || replaySaveInProgress_.load() || !CreateCaptureDirectories()) {
		return false;
	}

	std::vector<ReplayFrame> frames;
	{
		std::unique_lock lock(replayMutex_);
		replayIdleCondition_.wait(lock, [this] {
			return replayEncodeJobs_.empty() && replayActiveJobs_ == 0;
		});
		if (replayFrames_.empty()) {
			return false;
		}
		frames.assign(replayFrames_.begin(), replayFrames_.end());
		if (smokeEnabled_) {
			smokeReplayFrameCount_ = static_cast<int>(frames.size());
			smokeReplayDroppedFrames_ = replayDroppedFrames_;
		}
	}
	if (smokeEnabled_ &&
		(!DecodeReplayFrame(frames.front(), smokeFirstReplayFrame_) ||
		 !DecodeReplayFrame(frames.back(), smokeLastReplayFrame_))) {
		return false;
	}

	replaySaveSucceeded_ = false;
	replaySaveInProgress_.store(true);
	replaySaveThread_ = std::thread([this, frames = std::move(frames)]() mutable {
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		replaySaveSucceeded_ = SaveReplayFrames(std::move(frames));
		replaySaveInProgress_.store(false);
		if (SUCCEEDED(comResult)) {
			CoUninitialize();
		}
	});
	return true;
}

bool CaptureManager::SaveReplayFrames(std::vector<ReplayFrame> frames)
{
	if (frames.empty()) {
		return false;
	}

	const ReplayFrame& firstFrame = frames.front();
	recordingFrameIndex_ = 0;
	if (!BeginSelectedVideo(
		GetCaptureDirectory("Replays"),
		"CG2_replay_",
		firstFrame.width,
		firstFrame.height,
		kReplayFramesPerSecond_)) {
		return false;
	}

	bool success = true;
	std::vector<unsigned char> decodedPixels;
	for (const ReplayFrame& frame : frames) {
		if (!DecodeReplayFrame(frame, decodedPixels) ||
			!AppendActiveVideoFrame(decodedPixels, frame.width, frame.height, recordingFrameIndex_)) {
			success = false;
			break;
		}
		++recordingFrameIndex_;
	}
	EndActiveVideo();
	if (!success) {
		return false;
	}

	lastReplayPath_ = recordingVideoPath_;
	lastVideoPath_ = recordingVideoPath_;
	return true;
}

void CaptureManager::WaitForReplaySave()
{
	if (replaySaveThread_.joinable()) {
		replaySaveThread_.join();
	}
	replaySaveInProgress_.store(false);
}

void CaptureManager::FinishReplaySaveIfReady()
{
	if (replaySaveInProgress_.load() || !replaySaveThread_.joinable()) {
		return;
	}
	replaySaveThread_.join();
	lastMessage_ = replaySaveSucceeded_
		? "Saved replay: " + lastReplayPath_.string()
		: "Replay save failed.";
}

void CaptureManager::ProcessCompletedCaptures(bool waitForGpu)
{
	if (pendingCaptureRequests_.empty()) {
		return;
	}
	if (waitForGpu) {
		DirectXCommon::GetInstance()->WaitForGPU();
	}

	while (!pendingCaptureRequests_.empty()) {
		std::vector<unsigned char> pixels;
		int width = 0;
		int height = 0;
		const auto readbackStart = std::chrono::steady_clock::now();
		const bool captureReady =
			DirectXCommon::GetInstance()->TryGetGameTextureCapturePixels(pixels, width, height);
		if (!captureReady) {
			break;
		}
		if (smokeEnabled_) {
			smokeReadbackMilliseconds_ += std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - readbackStart).count();
			++smokeReadbackSamples_;
		}
		const PendingCaptureRequest request = pendingCaptureRequests_.front();
		pendingCaptureRequests_.pop_front();
		ProcessCapturedFrame(request, std::move(pixels), width, height);
	}
}

void CaptureManager::ProcessCapturedFrame(
	const PendingCaptureRequest& request,
	std::vector<unsigned char> pixels,
	int width,
	int height)
{
	const bool validPixels =
		width > 0 && height > 0 &&
		pixels.size() >= static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
	if (request.photo) {
		photoCapturePending_ = false;
		photoRequested_ = false;
		const bool visiblyNonBlack = validPixels && IsVisiblyNonBlack(pixels, width, height);
		UpdateSmokeAfterCapture(validPixels, visiblyNonBlack);
		if (validPixels) {
			const std::filesystem::path capturePath =
				GetCaptureDirectory("Screenshots") / ("CG2_" + MakeTimestampString() + ".png");
			if (SavePixelsAsPng(capturePath, pixels, width, height)) {
				lastScreenshotPath_ = capturePath;
				if (smokeEnabled_) {
					++smokeSavedPhotos_;
					smokePhotoPaths_.push_back(capturePath);
				}
				lastMessage_ = "Saved screenshot: " + capturePath.string();
			} else {
				lastMessage_ = "Screenshot failed: could not write PNG to " + capturePath.string();
			}
		} else {
			lastMessage_ = "Screenshot failed: Game View pixels could not be captured.";
		}
	}
	std::shared_ptr<const std::vector<unsigned char>> sharedPixels;
	if (validPixels && (request.startRecording || request.recordingFrame || request.replayFrame)) {
		sharedPixels = std::make_shared<std::vector<unsigned char>>(std::move(pixels));
	}
	if (request.replayFrame && replayBufferEnabled_ && validPixels) {
		const auto replayStart = std::chrono::steady_clock::now();
		UpdateReplayBuffer(sharedPixels, width, height);
		if (smokeEnabled_) {
			smokeReplayMilliseconds_ += std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - replayStart).count();
			++smokeReplaySamples_;
		}
	}

	if (request.startRecording) {
		recordingStartCapturePending_ = false;
		const bool requestStillCurrent =
			recordingStartRequested_ && request.recordingGeneration == recordingGeneration_;
		recordingStartRequested_ = false;
		if (requestStillCurrent) {
			videoEncodeFailed_.store(false);
			recordingFrameIndex_ = 0;
			const int selectedFrameRate = selectedVideoFormat_ == VideoFormat::Mp4
				? kMp4RecordingFramesPerSecond_
				: kAviRecordingFramesPerSecond_;
			const auto videoStart = std::chrono::steady_clock::now();
			const bool videoStarted = validPixels && BeginSelectedVideo(
				GetCaptureDirectory("Videos"),
				"CG2_",
				width,
				height,
				selectedFrameRate) && AppendActiveVideoFrame(*sharedPixels, width, height, 0);
			if (smokeEnabled_) {
				smokeVideoMilliseconds_ += std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - videoStart).count();
				++smokeVideoSamples_;
			}
			if (videoStarted) {
				isRecording_ = true;
				recordingTime_ = 0.0f;
				recordingFrameTimer_ = 0.0f;
				++recordingFrameIndex_;
				lastMessage_ = "Recording video: " + recordingVideoPath_.string();
			} else {
				StopRecording("Recording failed: MP4 and AVI could not be started.");
			}
		}
	} else if (request.recordingFrame &&
		isRecording_ && request.recordingGeneration == recordingGeneration_) {
		const auto videoStart = std::chrono::steady_clock::now();
		const bool frameQueued = validPixels && QueueVideoFrame(
			sharedPixels, width, height, recordingFrameIndex_);
		if (smokeEnabled_) {
			smokeVideoMilliseconds_ += std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - videoStart).count();
			++smokeVideoSamples_;
		}
		if (frameQueued) {
			++recordingFrameIndex_;
		} else if (videoEncodeFailed_.load() || !validPixels) {
			StopRecording("Recording stopped because the video frame could not be written.");
		} else {
			++droppedCaptureFrames_;
			++recordingFrameIndex_;
		}
	}
}

void CaptureManager::StopRecording(const std::string& message)
{
	++recordingGeneration_;
	isRecording_ = false;
	recordingStartRequested_ = false;
	recordingStartCapturePending_ = false;
	WaitForVideoEncoder();
	EndActiveVideo();
	videoEncodeFailed_.store(false);
	lastVideoPath_ = recordingVideoPath_;
	lastMessage_ = message;
}

void CaptureManager::DrawImGui()
{
#ifdef USE_IMGUI
	const std::filesystem::path screenshotDirectory = GetCaptureDirectory("Screenshots");
	const std::filesystem::path videoDirectory = GetCaptureDirectory("Videos");
	const std::filesystem::path replayDirectory = GetCaptureDirectory("Replays");
	const bool replaySaving = replaySaveInProgress_.load();
	if (!CreateCaptureDirectories()) {
		lastMessage_ = "Could not create capture folders under resources/Captures.";
	}

	ImGui::TextUnformatted("Capture");
	ImGui::SameLine();
	ImGui::TextDisabled("PNG / MP4 / All scenes");
	const int totalSeconds = static_cast<int>(recordingTime_);
	const int minutes = totalSeconds / 60;
	const float seconds = recordingTime_ - static_cast<float>(minutes * 60);
	char recordingLabel[48];
	snprintf(
		recordingLabel,
		sizeof(recordingLabel),
		(isRecording_ || recordingStartRequested_) ? "REC %02d:%04.1f" : "Video %02d:%04.1f",
		minutes,
		seconds);
	const float recordingLabelWidth = ImGui::CalcTextSize(recordingLabel).x;
	ImGui::SameLine();
	ImGui::SetCursorPosX((std::max)(
		ImGui::GetCursorPosX(),
		ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - recordingLabelWidth - 180.0f));
	ImGui::BeginDisabled(replaySaving);
	if (isRecording_ || recordingStartRequested_) {
		ImGui::TextColored(ImVec4(1.0f, 0.12f, 0.12f, 1.0f), "%s", recordingLabel);
	} else {
		ImGui::TextDisabled("%s", recordingLabel);
	}

	ImGui::BeginDisabled(photoRequested_);
	if (ImGui::Button("Photo")) {
		photoRequested_ = true;
		lastMessage_ = "Taking photo after this frame...";
	}
	ImGui::EndDisabled();
	ImGui::SameLine();

	if (isRecording_ || recordingStartRequested_) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.05f, 0.07f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.08f, 0.10f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.62f, 0.02f, 0.04f, 1.0f));
		if (ImGui::Button("Stop Video")) {
			StopRecording("Saved video: " + recordingVideoPath_.string());
		}
		ImGui::PopStyleColor(3);
	} else if (ImGui::Button("Video")) {
		++recordingGeneration_;
		recordingStartRequested_ = true;
		lastMessage_ = "Starting video after this frame...";
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::SmallButton("open Photos File")) {
		lastMessage_ = OpenFolderInExplorer(screenshotDirectory)
			? "Opened screenshot folder: " + screenshotDirectory.string()
			: "Could not open screenshot folder: " + screenshotDirectory.string();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("open Videos File")) {
		lastMessage_ = OpenFolderInExplorer(videoDirectory)
			? "Opened video folder: " + videoDirectory.string()
			: "Could not open video folder: " + videoDirectory.string();
	}

	if (ImGui::Checkbox("Keep 30s Replay", &replayBufferEnabled_) && !replayBufferEnabled_) {
		ClearReplayBuffer();
	}
	ImGui::SameLine();
	size_t replayFrameCount = 0;
	size_t replayBytes = 0;
	size_t queuedFrames = 0;
	size_t droppedFrames = 0;
	{
		std::lock_guard lock(replayMutex_);
		replayFrameCount = replayFrames_.size();
		queuedFrames = replayEncodeJobs_.size() + replayActiveJobs_;
		droppedFrames = replayDroppedFrames_;
		for (const ReplayFrame& frame : replayFrames_) {
			replayBytes += frame.encodedPixels.size();
		}
	}
	ImGui::BeginDisabled(
		isRecording_ || recordingStartRequested_ || replaySaving || !replayBufferEnabled_ || replayFrameCount == 0);
	if (ImGui::Button("Save Last 30s")) {
		lastMessage_ = SaveReplayClip()
			? "Saving replay in background..."
			: "Replay save could not be started.";
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::SmallButton("open Replays File")) {
		lastMessage_ = OpenFolderInExplorer(replayDirectory)
			? "Opened replay folder: " + replayDirectory.string()
			: "Could not open replay folder: " + replayDirectory.string();
	}

	const float replaySeconds = static_cast<float>(replayFrameCount) / static_cast<float>(kReplayFramesPerSecond_);
	ImGui::TextDisabled(
		"Replay: %.1f / %d sec (960x540, 30 FPS, %.1f MB, queue %zu, dropped %zu)",
		replaySeconds,
		kReplaySeconds_,
		static_cast<double>(replayBytes) / (1024.0 * 1024.0),
		queuedFrames,
		droppedFrames);
	if (replaySaving) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Saving MP4...");
	}
	if (!lastMessage_.empty()) {
		ImGui::TextWrapped("%s", lastMessage_.c_str());
	}
#endif
}

void CaptureManager::UpdateAfterDraw()
{
#ifdef USE_IMGUI
	if (!initialized_) {
		return;
	}
	if (performanceBaselineEnabled_) {
		if (SceneManager::GetInstance()->GetCurrentSceneName() == "GAMEPLAY") {
			++smokeGamePlayFrame_;
			const float deltaTime = DirectXCommon::GetInstance()->GetDeltaTime();
			if (smokeGamePlayFrame_ > 8 && smokeGamePlayFrame_ < 69) {
				++smokePerformanceFrameCount_;
				smokePerformanceTime_ += deltaTime;
				smokeMaximumDeltaTime_ = (std::max)(smokeMaximumDeltaTime_, deltaTime);
			}
			if (smokeGamePlayFrame_ == 69) {
				const float averageFps = smokePerformanceTime_ > 0.0f
					? static_cast<float>(smokePerformanceFrameCount_) / smokePerformanceTime_
					: 0.0f;
				std::ofstream log(std::filesystem::path("logs") / "capture_performance_baseline.log", std::ios::trunc);
				if (log) {
					log << "BASELINE: averageFps=" << averageFps
						<< " maxFrameMs=" << smokeMaximumDeltaTime_ * 1000.0f << '\n';
				}
				smokeFinished_ = true;
				PostQuitMessage(0);
			}
		}
		return;
	}
	FinishReplaySaveIfReady();
	if (isRecording_ && videoEncodeFailed_.load()) {
		StopRecording("Recording stopped because the MP4 encoder rejected a frame.");
	}
	ProcessCompletedCaptures();
	UpdateSmokeBeforeCapture();
	if (smokeFinished_) {
		return;
	}

	const float deltaTime = DirectXCommon::GetInstance()->GetDeltaTime();
	if (smokeEnabled_ &&
		SceneManager::GetInstance()->GetCurrentSceneName() == "GAMEPLAY" &&
		smokeGamePlayFrame_ > 8 &&
		smokeGamePlayFrame_ < 45) {
		++smokePerformanceFrameCount_;
		smokePerformanceTime_ += deltaTime;
		smokeMaximumDeltaTime_ = (std::max)(smokeMaximumDeltaTime_, deltaTime);
	}
	bool recordingFrameDue = false;
	if (isRecording_) {
		recordingTime_ += deltaTime;
		recordingFrameTimer_ += deltaTime;
		const float interval = 1.0f / static_cast<float>(recordingFramesPerSecond_);
		if (recordingFrameTimer_ >= interval) {
			while (recordingFrameTimer_ >= interval) {
				recordingFrameTimer_ -= interval;
			}
			recordingFrameDue = true;
		}
	}

	bool replayFrameDue = false;
	if (replayBufferEnabled_) {
		replayFrameTimer_ += deltaTime;
		const float interval = 1.0f / static_cast<float>(kReplayFramesPerSecond_);
		if (replayFrameTimer_ >= interval) {
			while (replayFrameTimer_ >= interval) {
				replayFrameTimer_ -= interval;
			}
			replayFrameDue = true;
		}
	}

	PendingCaptureRequest request{};
	request.photo = photoRequested_ && !photoCapturePending_;
	request.startRecording = recordingStartRequested_ && !recordingStartCapturePending_;
	request.recordingFrame = recordingFrameDue && isRecording_;
	request.replayFrame = replayFrameDue && replayBufferEnabled_;
	request.recordingGeneration = recordingGeneration_;
	if (!request.photo && !request.startRecording && !request.recordingFrame && !request.replayFrame) {
		return;
	}

	if (!DirectXCommon::GetInstance()->QueueGameTextureCapture(
		PostEffect::GetInstance()->IsEnabled())) {
		if (request.recordingFrame || request.replayFrame) {
			++droppedCaptureFrames_;
		}
		return;
	}
	pendingCaptureRequests_.push_back(request);
	if (request.photo) {
		photoCapturePending_ = true;
	}
	if (request.startRecording) {
		recordingStartCapturePending_ = true;
	}
#endif
}
