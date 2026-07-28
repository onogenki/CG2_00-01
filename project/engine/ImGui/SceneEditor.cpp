#include "SceneEditor.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "TextureManager.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <numbers>
#include <unordered_map>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#endif

namespace {
	struct CachedModelBounds
	{
		Vector3 minimum{};
		Vector3 maximum{};
	};

	Vector3 TransformEditorPoint(const Vector3& point, const Matrix4x4& matrix)
	{
		return {
			point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0],
			point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1],
			point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2],
		};
	}

	bool ProjectEditorPoint(const Vector3& point, const Matrix4x4& viewProjection, const ImVec2& imageMin, const ImVec2& imageSize, ImVec2& screen)
	{
		const float x = point.x * viewProjection.m[0][0] + point.y * viewProjection.m[1][0] + point.z * viewProjection.m[2][0] + viewProjection.m[3][0];
		const float y = point.x * viewProjection.m[0][1] + point.y * viewProjection.m[1][1] + point.z * viewProjection.m[2][1] + viewProjection.m[3][1];
		const float w = point.x * viewProjection.m[0][3] + point.y * viewProjection.m[1][3] + point.z * viewProjection.m[2][3] + viewProjection.m[3][3];
		if (w <= 0.0001f) {
			return false;
		}
		screen = {
			imageMin.x + (x / w + 1.0f) * 0.5f * imageSize.x,
			imageMin.y + (1.0f - y / w) * 0.5f * imageSize.y,
		};
		return true;
	}

	float EditorDistanceToSegment(const ImVec2& point, const ImVec2& start, const ImVec2& end)
	{
		const ImVec2 segment(end.x - start.x, end.y - start.y);
		const ImVec2 fromStart(point.x - start.x, point.y - start.y);
		const float lengthSquared = segment.x * segment.x + segment.y * segment.y;
		const float amount = lengthSquared > 0.0001f
			? std::clamp((fromStart.x * segment.x + fromStart.y * segment.y) / lengthSquared, 0.0f, 1.0f)
			: 0.0f;
		const float dx = point.x - (start.x + segment.x * amount);
		const float dy = point.y - (start.y + segment.y * amount);
		return std::sqrt(dx * dx + dy * dy);
	}

	bool BuildEditorWorldBounds(const Object3d& object, Vector3& minimum, Vector3& maximum)
	{
		Model* model = object.GetModel();
		if (!model || model->GetModelData().vertices.empty()) {
			return false;
		}

		static std::unordered_map<const Model*, CachedModelBounds> cachedBounds;
		auto cached = cachedBounds.find(model);
		if (cached == cachedBounds.end()) {
			CachedModelBounds bounds{};
			bounds.minimum = {
				(std::numeric_limits<float>::max)(),
				(std::numeric_limits<float>::max)(),
				(std::numeric_limits<float>::max)(),
			};
			bounds.maximum = {
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
			};
			for (const Model::VertexData& vertex : model->GetModelData().vertices) {
				const Vector3 point{ vertex.position.x, vertex.position.y, vertex.position.z };
				bounds.minimum.x = (std::min)(bounds.minimum.x, point.x);
				bounds.minimum.y = (std::min)(bounds.minimum.y, point.y);
				bounds.minimum.z = (std::min)(bounds.minimum.z, point.z);
				bounds.maximum.x = (std::max)(bounds.maximum.x, point.x);
				bounds.maximum.y = (std::max)(bounds.maximum.y, point.y);
				bounds.maximum.z = (std::max)(bounds.maximum.z, point.z);
			}
			cached = cachedBounds.emplace(model, bounds).first;
		}

		const CachedModelBounds& local = cached->second;
		const Transform& transform = object.GetTransform();
		const Matrix4x4 world = MyMath::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		const std::array<Vector3, 8> corners{ {
			{ local.minimum.x, local.minimum.y, local.minimum.z },
			{ local.maximum.x, local.minimum.y, local.minimum.z },
			{ local.minimum.x, local.maximum.y, local.minimum.z },
			{ local.maximum.x, local.maximum.y, local.minimum.z },
			{ local.minimum.x, local.minimum.y, local.maximum.z },
			{ local.maximum.x, local.minimum.y, local.maximum.z },
			{ local.minimum.x, local.maximum.y, local.maximum.z },
			{ local.maximum.x, local.maximum.y, local.maximum.z },
		} };

		minimum = {
			(std::numeric_limits<float>::max)(),
			(std::numeric_limits<float>::max)(),
			(std::numeric_limits<float>::max)(),
		};
		maximum = {
			std::numeric_limits<float>::lowest(),
			std::numeric_limits<float>::lowest(),
			std::numeric_limits<float>::lowest(),
		};
		for (const Vector3& corner : corners) {
			const Vector3 point = TransformEditorPoint(corner, world);
			minimum.x = (std::min)(minimum.x, point.x);
			minimum.y = (std::min)(minimum.y, point.y);
			minimum.z = (std::min)(minimum.z, point.z);
			maximum.x = (std::max)(maximum.x, point.x);
			maximum.y = (std::max)(maximum.y, point.y);
			maximum.z = (std::max)(maximum.z, point.z);
		}
		return true;
	}

bool IsShelfModelFile(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return extension == ".obj" || extension == ".gltf" || extension == ".glb" || extension == ".fbx";
}

bool IsShelfTextureFile(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp";
}

Vector3 ToShelfVector3(const aiVector3D& value)
{
	return { value.x, value.y, value.z };
}

std::string MakeAddMessage(const std::string& sceneLabel, const char* kind, const std::string& fileName, bool success)
{
	return std::string(success ? "Added " : "Could not add ") + sceneLabel + " " + kind + ": " + fileName;
}

std::string MakePreviewMessage(const char* kind, const std::string& fileName, bool success)
{
	return std::string(success ? "Previewing " : "Could not preview ") + kind + ": " + fileName;
}
}

void SceneEditor::ScanResourceShelf(ShelfState& state)
{
	state.entries.clear();

	const std::filesystem::path resourceDirectory = "resources";
	std::error_code errorCode;
	if (!std::filesystem::exists(resourceDirectory, errorCode)) {
		state.selectedEntry.clear();
		return;
	}

	for (std::filesystem::recursive_directory_iterator it(resourceDirectory, std::filesystem::directory_options::skip_permission_denied, errorCode), end;
		it != end;
		it.increment(errorCode)) {
		if (errorCode) {
			errorCode.clear();
			continue;
		}
		if (!it->is_regular_file(errorCode) || errorCode) {
			errorCode.clear();
			continue;
		}

		const bool isModelFile = IsShelfModelFile(it->path());
		const bool isTextureFile = IsShelfTextureFile(it->path());
		if (!isModelFile && !isTextureFile) {
			continue;
		}

		std::filesystem::path relativePath = std::filesystem::relative(it->path(), resourceDirectory, errorCode);
		if (errorCode) {
			relativePath = it->path().filename();
			errorCode.clear();
		}
		std::filesystem::path displayPath = relativePath;
		displayPath.replace_extension();

		ShelfEntry shelfEntry{};
		shelfEntry.fileName = relativePath.generic_string();
		shelfEntry.displayName = displayPath.generic_string();

		if (isTextureFile) {
			shelfEntry.isTexture = true;
			shelfEntry.textureFilePath = (std::filesystem::path("Resources") / relativePath).generic_string();
			TextureManager* textureManager = TextureManager::GetInstance();
			if (!textureManager->Contains(shelfEntry.textureFilePath)) {
				textureManager->LoadTexture(shelfEntry.textureFilePath);
			}
			shelfEntry.textureSrvIndex = textureManager->GetSrvIndex(shelfEntry.textureFilePath);
			const DirectX::TexMetadata& metadata = textureManager->GetMetaData(shelfEntry.textureFilePath);
			shelfEntry.textureSize = {
				static_cast<float>(metadata.width),
				static_cast<float>(metadata.height)
			};
			state.entries.push_back(std::move(shelfEntry));
			continue;
		}

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(it->path().string(), aiProcess_Triangulate | aiProcess_GenNormals);
		if (scene) {
			shelfEntry.hasMesh = scene->HasMeshes();
			shelfEntry.hasAnimation = scene->mNumAnimations > 0;

			Vector3 minPoint{
				(std::numeric_limits<float>::max)(),
				(std::numeric_limits<float>::max)(),
				(std::numeric_limits<float>::max)()
			};
			Vector3 maxPoint{
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest()
			};
			bool hasVertex = false;
			bool hasDrawableFace = false;
			for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
				const aiMesh* mesh = scene->mMeshes[meshIndex];
				if (!mesh || !mesh->HasPositions()) {
					continue;
				}
				hasDrawableFace = hasDrawableFace || mesh->HasFaces();
				for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
					const Vector3 position = ToShelfVector3(mesh->mVertices[vertexIndex]);
					minPoint.x = (std::min)(minPoint.x, position.x);
					minPoint.y = (std::min)(minPoint.y, position.y);
					minPoint.z = (std::min)(minPoint.z, position.z);
					maxPoint.x = (std::max)(maxPoint.x, position.x);
					maxPoint.y = (std::max)(maxPoint.y, position.y);
					maxPoint.z = (std::max)(maxPoint.z, position.z);
					hasVertex = true;
				}
			}

			if (hasVertex) {
				shelfEntry.thumbnailCenter = {
					(minPoint.x + maxPoint.x) * 0.5f,
					(minPoint.y + maxPoint.y) * 0.5f,
					(minPoint.z + maxPoint.z) * 0.5f
				};
				const Vector3 size{
					maxPoint.x - minPoint.x,
					maxPoint.y - minPoint.y,
					maxPoint.z - minPoint.z
				};
				shelfEntry.thumbnailRadius = (std::max)({ size.x, size.y, size.z, 0.001f }) * 0.5f;
			}
			shelfEntry.canLoad = shelfEntry.hasMesh && hasVertex && hasDrawableFace;

			constexpr size_t maxThumbnailLines = 96;
			constexpr size_t maxThumbnailTriangles = 72;
			for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes && shelfEntry.thumbnailLines.size() < maxThumbnailLines; ++meshIndex) {
				const aiMesh* mesh = scene->mMeshes[meshIndex];
				if (!mesh || !mesh->HasPositions() || !mesh->HasFaces()) {
					continue;
				}
				for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
					const aiFace& face = mesh->mFaces[faceIndex];
					if (face.mNumIndices < 2) {
						continue;
					}
					if (face.mNumIndices >= 3 && shelfEntry.thumbnailTriangles.size() < maxThumbnailTriangles) {
						for (uint32_t index = 1; index + 1 < face.mNumIndices && shelfEntry.thumbnailTriangles.size() < maxThumbnailTriangles; ++index) {
							const uint32_t index0 = face.mIndices[0];
							const uint32_t index1 = face.mIndices[index];
							const uint32_t index2 = face.mIndices[index + 1];
							if (index0 < mesh->mNumVertices && index1 < mesh->mNumVertices && index2 < mesh->mNumVertices) {
								shelfEntry.thumbnailTriangles.push_back({
									ToShelfVector3(mesh->mVertices[index0]),
									ToShelfVector3(mesh->mVertices[index1]),
									ToShelfVector3(mesh->mVertices[index2])
								});
							}
						}
					}
					for (uint32_t index = 0; index < face.mNumIndices && shelfEntry.thumbnailLines.size() < maxThumbnailLines; ++index) {
						const uint32_t startIndex = face.mIndices[index];
						const uint32_t endIndex = face.mIndices[(index + 1) % face.mNumIndices];
						if (startIndex >= mesh->mNumVertices || endIndex >= mesh->mNumVertices) {
							continue;
						}
						shelfEntry.thumbnailLines.emplace_back(
							ToShelfVector3(mesh->mVertices[startIndex]),
							ToShelfVector3(mesh->mVertices[endIndex]));
					}
				}
			}
		}

		state.entries.push_back(std::move(shelfEntry));
	}

	std::sort(state.entries.begin(), state.entries.end(), [](const ShelfEntry& lhs, const ShelfEntry& rhs) {
		return lhs.fileName < rhs.fileName;
	});
	if (!state.selectedEntry.empty()) {
		const bool selectionStillExists = std::any_of(state.entries.begin(), state.entries.end(), [&](const ShelfEntry& entry) {
			return entry.fileName == state.selectedEntry;
		});
		if (!selectionStillExists) {
			state.selectedEntry.clear();
		}
	}
}

void SceneEditor::DrawModelShelf(ShelfState& state, const ShelfCallbacks& callbacks)
{
#ifdef USE_IMGUI
	// リソース棚は配置作業専用なので、Game Viewでは生成しない。
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}

	if (!ImGui::Begin("Model Shelf")) {
		ImGui::End();
		return;
	}

	const size_t loadableModelCount = static_cast<size_t>(std::count_if(state.entries.begin(), state.entries.end(), [](const ShelfEntry& entry) {
		return !entry.isTexture && entry.canLoad;
	}));
	const size_t animationModelCount = static_cast<size_t>(std::count_if(state.entries.begin(), state.entries.end(), [](const ShelfEntry& entry) {
		return entry.canLoad && entry.hasAnimation;
	}));
	const size_t textureCount = static_cast<size_t>(std::count_if(state.entries.begin(), state.entries.end(), [](const ShelfEntry& entry) {
		return entry.isTexture;
	}));
	const size_t unsupportedModelCount = static_cast<size_t>(std::count_if(state.entries.begin(), state.entries.end(), [](const ShelfEntry& entry) {
		return !entry.isTexture && !entry.canLoad;
	}));
	auto findSelectedEntry = [&state]() -> const ShelfEntry* {
		const auto selectedIt = std::find_if(state.entries.begin(), state.entries.end(), [&](const ShelfEntry& entry) {
			return entry.fileName == state.selectedEntry;
		});
		return selectedIt != state.entries.end() ? &(*selectedIt) : nullptr;
	};
	const ShelfEntry* selectedEntry = findSelectedEntry();
	bool hasLoadableSelection = selectedEntry != nullptr && (selectedEntry->canLoad || selectedEntry->isTexture);

	ImGui::Text("%s Resources (%zu)", callbacks.sceneLabel.c_str(), state.entries.size());
	ImGui::SameLine();
	if (ImGui::SmallButton("Refresh")) {
		ScanResourceShelf(state);
		selectedEntry = findSelectedEntry();
		hasLoadableSelection = selectedEntry != nullptr && (selectedEntry->canLoad || selectedEntry->isTexture);
		state.message = "Refreshed " + callbacks.sceneLabel + " resources: " + std::to_string(state.entries.size()) + " shelf item(s) found.";
	}
	if (callbacks.drawExtraToolbar) {
		callbacks.drawExtraToolbar();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(callbacks.addedModelCount == 0 && callbacks.addedTextureCount == 0 || !callbacks.clearAdded);
	if (ImGui::SmallButton(("Clear " + callbacks.sceneLabel + " Added").c_str())) {
		callbacks.clearAdded();
		state.message = "Cleared " + callbacks.sceneLabel + " added models and textures.";
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("Added: %zu (Textures: %zu)", callbacks.addedModelCount + callbacks.addedTextureCount, callbacks.addedTextureCount);
	ImGui::TextDisabled(
		"Models: %zu | Animation: %zu | 2D Textures: %zu | Unsupported: %zu",
		loadableModelCount,
		animationModelCount,
		textureCount,
		unsupportedModelCount);
	if (!state.message.empty()) {
		ImGui::TextWrapped("%s", state.message.c_str());
	}
	if (callbacks.drawExtraStatus) {
		callbacks.drawExtraStatus();
	}

	if (callbacks.previewEntry) {
		ImGui::BeginDisabled(!hasLoadableSelection);
		if (ImGui::SmallButton("Preview Selected")) {
			const bool success = callbacks.previewEntry(*selectedEntry);
			state.message = MakePreviewMessage(selectedEntry->isTexture ? "2D Texture" : "model", selectedEntry->isTexture ? selectedEntry->textureFilePath : selectedEntry->fileName, success);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
	}
	ImGui::BeginDisabled(!hasLoadableSelection);
	if (ImGui::SmallButton(("Add Selected to " + callbacks.sceneLabel).c_str())) {
		if (selectedEntry->isTexture) {
			const bool success = callbacks.addTexture && callbacks.addTexture(selectedEntry->textureFilePath);
			state.message = MakeAddMessage(callbacks.sceneLabel, "2D Texture", selectedEntry->textureFilePath, success);
		} else {
			const bool success = callbacks.addModel && callbacks.addModel(selectedEntry->fileName);
			state.message = MakeAddMessage(callbacks.sceneLabel, "model", selectedEntry->fileName, success);
		}
		if (callbacks.afterAdd) {
			callbacks.afterAdd();
		}
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip(hasLoadableSelection ? "Add the selected card to the scene." : "Select a loadable model or texture card first.");
	}
	ImGui::Separator();

	constexpr int cardsPerRow = 6;
	ImGui::BeginChild("SceneEditorModelShelfGrid", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoMove);
	const float shelfSpacingX = ImGui::GetStyle().ItemSpacing.x;
	const float shelfAvailableWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
	const float calculatedCardWidth = (shelfAvailableWidth - shelfSpacingX * static_cast<float>(cardsPerRow - 1)) / static_cast<float>(cardsPerRow);
	const ImVec2 cardSize((std::max)(118.0f, (std::min)(150.0f, calculatedCardWidth)), 118.0f);

	if (state.entries.empty()) {
		ImGui::TextDisabled("No supported model or texture files were found under resources.");
	}

	size_t cardIndex = 0;
	for (const ShelfEntry& entry : state.entries) {
		if (cardIndex > 0 && cardIndex % cardsPerRow != 0) {
			ImGui::SameLine();
		}
		ImGui::PushID(entry.fileName.c_str());
		const ImVec2 cardMin = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("SceneEditorModelCard", cardSize);
		const bool hovered = ImGui::IsItemHovered();
		const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		const bool selected = state.selectedEntry == entry.fileName;

		if (clicked) {
			state.selectedEntry = entry.fileName;
		}
		if (doubleClicked && entry.isTexture) {
			state.selectedEntry = entry.fileName;
			if (callbacks.previewOnDoubleClick && callbacks.previewEntry) {
				const bool success = callbacks.previewEntry(entry);
				state.message = MakePreviewMessage("2D Texture", entry.textureFilePath, success);
			} else {
				const bool success = callbacks.addTexture && callbacks.addTexture(entry.textureFilePath);
				state.message = MakeAddMessage(callbacks.sceneLabel, "2D Texture", entry.textureFilePath, success);
				if (callbacks.afterAdd) {
					callbacks.afterAdd();
				}
			}
		} else if (doubleClicked && entry.canLoad) {
			state.selectedEntry = entry.fileName;
			if (callbacks.previewOnDoubleClick && callbacks.previewEntry) {
				const bool success = callbacks.previewEntry(entry);
				state.message = MakePreviewMessage("model", entry.fileName, success);
			} else {
				const bool success = callbacks.addModel && callbacks.addModel(entry.fileName);
				state.message = MakeAddMessage(callbacks.sceneLabel, "model", entry.fileName, success);
				if (callbacks.afterAdd) {
					callbacks.afterAdd();
				}
			}
		}

		if (entry.isTexture && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("TEXTURE_FILE", entry.textureFilePath.c_str(), entry.textureFilePath.size() + 1);
			ImGui::Text("Add 2D Texture %s", entry.textureFilePath.c_str());
			ImGui::EndDragDropSource();
		} else if (entry.canLoad && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("MODEL_FILE", entry.fileName.c_str(), entry.fileName.size() + 1);
			ImGui::Text("Add %s", entry.fileName.c_str());
			ImGui::EndDragDropSource();
		}

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 cardMax(cardMin.x + cardSize.x, cardMin.y + cardSize.y);
		const ImU32 backgroundColor = selected ? IM_COL32(64, 88, 130, 255) : (hovered ? IM_COL32(50, 50, 56, 255) : IM_COL32(34, 34, 38, 255));
		const ImU32 borderColor = entry.isTexture
			? IM_COL32(120, 210, 170, 255)
			: (entry.canLoad ? IM_COL32(120, 150, 210, 255) : IM_COL32(120, 80, 80, 255));
		drawList->AddRectFilled(cardMin, cardMax, backgroundColor, 8.0f);
		drawList->AddRect(cardMin, cardMax, borderColor, 8.0f, 0, selected ? 3.0f : 1.5f);

		const ImVec2 previewMin(cardMin.x + 10.0f, cardMin.y + 10.0f);
		const ImVec2 previewMax(cardMax.x - 10.0f, cardMin.y + 76.0f);
		drawList->AddRectFilled(previewMin, previewMax, IM_COL32(18, 18, 22, 255), 5.0f);
		drawList->AddRect(previewMin, previewMax, IM_COL32(78, 78, 92, 255), 5.0f);

		const float centerX = (previewMin.x + previewMax.x) * 0.5f;
		const float centerY = (previewMin.y + previewMax.y) * 0.5f;
		const float animationPulse = entry.hasAnimation && hovered ? (std::sin(static_cast<float>(ImGui::GetTime()) * 8.0f) * 0.5f + 0.5f) : 0.0f;
		const ImU32 modelColor = entry.hasAnimation
			? IM_COL32(255, static_cast<int>(190.0f + 50.0f * animationPulse), 70, 255)
			: IM_COL32(120, 180, 255, 255);
		if (entry.isTexture) {
			const float previewWidth = previewMax.x - previewMin.x;
			const float previewHeight = previewMax.y - previewMin.y;
			const float textureWidth = (std::max)(entry.textureSize.x, 1.0f);
			const float textureHeight = (std::max)(entry.textureSize.y, 1.0f);
			const float imageScale = (std::min)(previewWidth / textureWidth, previewHeight / textureHeight);
			const ImVec2 imageSize(textureWidth * imageScale, textureHeight * imageScale);
			const ImVec2 imageMin(previewMin.x + (previewWidth - imageSize.x) * 0.5f, previewMin.y + (previewHeight - imageSize.y) * 0.5f);
			const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
			D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(entry.textureFilePath);
			drawList->PushClipRect(previewMin, previewMax, true);
			drawList->AddImage(ImTextureRef(static_cast<ImTextureID>(textureHandle.ptr)), imageMin, imageMax);
			drawList->PopClipRect();
			drawList->AddText(ImVec2(previewMax.x - 32.0f, previewMin.y + 8.0f), IM_COL32(130, 255, 190, 230), "2D");
		} else if (!entry.thumbnailLines.empty() || !entry.thumbnailTriangles.empty()) {
			const float yaw = entry.hasAnimation && hovered ? static_cast<float>(ImGui::GetTime()) * 2.2f : 0.65f;
			const float cosYaw = std::cos(yaw);
			const float sinYaw = std::sin(yaw);
			const float previewWidth = previewMax.x - previewMin.x;
			const float previewHeight = previewMax.y - previewMin.y;
			const float previewScale = (std::min)(previewWidth, previewHeight) * (0.35f + 0.035f * animationPulse) / entry.thumbnailRadius;
			const auto projectThumbnailPoint = [&](const Vector3& position) {
				const Vector3 local{
					position.x - entry.thumbnailCenter.x,
					position.y - entry.thumbnailCenter.y,
					position.z - entry.thumbnailCenter.z
				};
				const float animationTime = static_cast<float>(ImGui::GetTime());
				const float hoverWave = entry.hasAnimation && hovered
					? std::sin(animationTime * 7.0f + local.x * 2.3f + local.z * 1.4f) * entry.thumbnailRadius * 0.08f
					: 0.0f;
				const float hoverStride = entry.hasAnimation && hovered
					? std::sin(animationTime * 5.0f + local.y * 3.0f) * entry.thumbnailRadius * 0.04f
					: 0.0f;
				const float animatedX = local.x + hoverStride;
				const float animatedY = local.y + hoverWave;
				const float rotatedX = animatedX * cosYaw + local.z * sinYaw;
				const float rotatedZ = -animatedX * sinYaw + local.z * cosYaw;
				return ImVec2(centerX + rotatedX * previewScale, centerY - animatedY * previewScale + rotatedZ * previewScale * 0.18f);
			};

			drawList->PushClipRect(previewMin, previewMax, true);
			const ImU32 fillColor = entry.hasAnimation
				? IM_COL32(255, static_cast<int>(150.0f + 45.0f * animationPulse), 45, 95)
				: IM_COL32(80, 145, 255, 95);
			for (const auto& triangle : entry.thumbnailTriangles) {
				drawList->AddTriangleFilled(projectThumbnailPoint(triangle[0]), projectThumbnailPoint(triangle[1]), projectThumbnailPoint(triangle[2]), fillColor);
			}
			for (const auto& line : entry.thumbnailLines) {
				drawList->AddLine(projectThumbnailPoint(line.first), projectThumbnailPoint(line.second), modelColor, 1.15f);
			}
			if (entry.hasAnimation) {
				drawList->AddCircleFilled(ImVec2(previewMax.x - 10.0f, previewMin.y + 10.0f), 3.0f + 1.5f * animationPulse, IM_COL32(255, 210, 80, 230));
			}
			drawList->PopClipRect();
		} else {
			drawList->AddTriangleFilled(ImVec2(centerX, centerY - 18.0f - 4.0f * animationPulse), ImVec2(centerX - 24.0f, centerY + 18.0f), ImVec2(centerX + 24.0f, centerY + 18.0f), modelColor);
		}

		const auto drawCardText = [&](const ImVec2& position, ImU32 color, const char* text) {
			const ImVec2 clipMin(cardMin.x + 8.0f, position.y);
			const ImVec2 clipMax(cardMax.x - 8.0f, position.y + ImGui::GetTextLineHeight());
			drawList->PushClipRect(clipMin, clipMax, true);
			drawList->AddText(position, color, text);
			drawList->PopClipRect();
		};
		drawCardText(ImVec2(cardMin.x + 10.0f, cardMin.y + 82.0f), IM_COL32(235, 235, 235, 255), entry.displayName.c_str());
		const char* typeLabel = entry.isTexture ? "TEXTURE" : (entry.hasAnimation ? "ANIM" : "MODEL");
		const ImU32 typeColor = entry.isTexture
			? IM_COL32(140, 255, 190, 255)
			: (entry.canLoad ? IM_COL32(160, 210, 255, 255) : IM_COL32(255, 140, 140, 255));
		drawCardText(ImVec2(cardMin.x + 10.0f, cardMin.y + 100.0f), typeColor, entry.canLoad || entry.isTexture ? typeLabel : "UNSUPPORTED");
		if (!entry.canLoad && !entry.isTexture) {
			drawList->AddText(ImVec2(previewMin.x + 10.0f, centerY - 7.0f), IM_COL32(255, 120, 120, 230), "Cannot load");
		}

		if (hovered) {
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(entry.fileName.c_str());
			if (entry.isTexture) {
				ImGui::Text("2D Texture preview");
				ImGui::Text("Size: %.0f x %.0f", entry.textureSize.x, entry.textureSize.y);
				ImGui::Text("Double click: add to scene");
			} else if (entry.canLoad) {
				ImGui::Text("Double click: add to scene");
				ImGui::Text("Right Inspector: adjust after adding");
			} else {
				ImGui::Text("This file was found, but no drawable mesh could be loaded.");
			}
			if (entry.hasAnimation) {
				ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "Animation model");
			}
			if (!entry.canLoad && !entry.isTexture) {
				ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "No mesh found");
			}
			ImGui::EndTooltip();
		}

		ImGui::PopID();
		++cardIndex;
	}

	ImGui::EndChild();
	ImGui::End();
#else
	(void)state;
	(void)callbacks;
#endif
}

void SceneEditor::HandleShelfDropOnEditView(ShelfState& state, const ShelfCallbacks& callbacks)
{
#ifdef USE_IMGUI
	// Game Viewへの誤配置を防ぎ、Edit Viewだけをドロップ先として扱う。
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}

	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(x, y, width, height)) {
		return;
	}

	const ImRect gameViewRect(ImVec2(x, y), ImVec2(x + width, y + height));
	const std::string dropTargetId = callbacks.sceneLabel + "EditViewShelfDropTarget";
	if (ImGui::BeginDragDropTargetCustom(gameViewRect, ImGui::GetID(dropTargetId.c_str()))) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_FILE")) {
			std::string fileName(static_cast<const char*>(payload->Data), payload->DataSize);
			if (!fileName.empty() && fileName.back() == '\0') {
				fileName.pop_back();
			}
			const bool success = callbacks.addModel && callbacks.addModel(fileName);
			state.message = MakeAddMessage(callbacks.sceneLabel, "model", fileName, success);
			if (callbacks.afterAdd) {
				callbacks.afterAdd();
			}
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_FILE")) {
			std::string textureFilePath(static_cast<const char*>(payload->Data), payload->DataSize);
			if (!textureFilePath.empty() && textureFilePath.back() == '\0') {
				textureFilePath.pop_back();
			}
			const bool success = callbacks.addTexture && callbacks.addTexture(textureFilePath);
			state.message = MakeAddMessage(callbacks.sceneLabel, "2D Texture", textureFilePath, success);
			if (callbacks.afterAdd) {
				callbacks.afterAdd();
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::IsDragDropActive() && gameViewRect.Contains(ImGui::GetMousePos())) {
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->AddRect(gameViewRect.Min, gameViewRect.Max, IM_COL32(80, 180, 255, 255), 0.0f, 0, 4.0f);
		const std::string dropLabel = "Drop into " + callbacks.sceneLabel;
		drawList->AddText(ImVec2(gameViewRect.Min.x + 16.0f, gameViewRect.Min.y + 16.0f), IM_COL32(180, 230, 255, 255), dropLabel.c_str());
	}
#else
	(void)state;
	(void)callbacks;
#endif
}

void SceneEditor::DrawViewportEditor(ViewportState& state, const ViewportOptions& options)
{
#ifdef USE_IMGUI
	if (!ImGuiManager::GetInstance()->IsEditViewActive() || !options.camera) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}
	const ImVec2 imageMin(rectX, rectY);
	const ImVec2 imageSize(rectWidth, rectHeight);
	const ImRect imageRect(imageMin, ImVec2(rectX + rectWidth, rectY + rectHeight));
	const Matrix4x4& viewProjection = options.camera->GetViewProjectionMatrix();

	if (state.selectedIndex >= static_cast<int>(options.objects.size()) ||
		(state.selectedIndex >= 0 && !options.objects[state.selectedIndex].object)) {
		state.selectedIndex = -1;
		state.isDragging = false;
		state.activeAxis = -1;
	}

	// Edit View左上へ、Blenderのツール切替に相当する小さな操作パネルを重ねる。
	ImGui::SetNextWindowPos(ImVec2(rectX + 10.0f, rectY + 10.0f), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.88f);
	const ImGuiWindowFlags toolbarFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking;
	ImGui::Begin("Edit View Tools", nullptr, toolbarFlags);
	const auto drawToolButton = [&](const char* label, TransformTool tool) {
		if (state.tool == tool) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.48f, 0.82f, 1.0f));
		}
		if (ImGui::Button(label)) {
			state.tool = tool;
			state.isDragging = false;
			state.activeAxis = -1;
		}
		if (state.tool == tool) {
			ImGui::PopStyleColor();
		}
	};
	drawToolButton("Move", TransformTool::Move);
	ImGui::SameLine();
	drawToolButton("Rotate", TransformTool::Rotate);
	ImGui::SameLine();
	drawToolButton("Scale", TransformTool::Scale);
	if (state.selectedIndex >= 0) {
		ImGui::Text("Selected: %s", options.objects[state.selectedIndex].label.c_str());
	} else {
		ImGui::TextDisabled("Left click an object to select it.");
	}
	ImGui::TextDisabled("RMB: rotate camera | MMB: pan | Wheel: zoom");
	if (ImGui::TreeNode("Camera Transform")) {
		Vector3 cameraPosition = options.camera->GetTranslate();
		if (ImGui::DragFloat3("Camera Position", &cameraPosition.x, 0.05f)) {
			options.camera->SetTranslate(cameraPosition);
		}
		constexpr float kRadianToDegree = 180.0f / std::numbers::pi_v<float>;
		constexpr float kDegreeToRadian = std::numbers::pi_v<float> / 180.0f;
		const Vector3 cameraRotation = options.camera->GetRotate();
		float cameraRotationDegrees[3]{
			cameraRotation.x * kRadianToDegree,
			cameraRotation.y * kRadianToDegree,
			cameraRotation.z * kRadianToDegree,
		};
		if (ImGui::DragFloat3("Camera Rotation (deg)", cameraRotationDegrees, 1.0f)) {
			options.camera->SetRotate({
				cameraRotationDegrees[0] * kDegreeToRadian,
				cameraRotationDegrees[1] * kDegreeToRadian,
				cameraRotationDegrees[2] * kDegreeToRadian,
			});
		}
		ImGui::TreePop();
	}
	const bool toolbarHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
	ImGui::End();

	Object3d* selectedObject = state.selectedIndex >= 0
		? options.objects[state.selectedIndex].object
		: nullptr;
	bool gizmoHovered = false;
	if (selectedObject) {
		Vector3 boundsMin{};
		Vector3 boundsMax{};
		if (BuildEditorWorldBounds(*selectedObject, boundsMin, boundsMax)) {
			const Vector3 center{
				(boundsMin.x + boundsMax.x) * 0.5f,
				(boundsMin.y + boundsMax.y) * 0.5f,
				(boundsMin.z + boundsMax.z) * 0.5f,
			};
			const Vector3 boundsSize{
				boundsMax.x - boundsMin.x,
				boundsMax.y - boundsMin.y,
				boundsMax.z - boundsMin.z,
			};
			const float axisLength = (std::max)({ boundsSize.x, boundsSize.y, boundsSize.z, 1.0f }) * 0.85f;
			const std::array<Vector3, 6> axisEnds{ {
				{ center.x + axisLength, center.y, center.z },
				{ center.x - axisLength, center.y, center.z },
				{ center.x, center.y + axisLength, center.z },
				{ center.x, center.y - axisLength, center.z },
				{ center.x, center.y, center.z + axisLength },
				{ center.x, center.y, center.z - axisLength },
			} };
			const std::array<Vector3, 6> worldDirections{ {
				{ 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
				{ 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f },
				{ 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f },
			} };
			ImVec2 centerScreen{};
			std::array<ImVec2, 6> endScreens{};
			if (ProjectEditorPoint(center, viewProjection, imageMin, imageSize, centerScreen)) {
				const ImVec2 mouse = ImGui::GetMousePos();
				int hoveredAxisEnd = -1;
				float bestDistance = 11.0f;
				for (int axisEndIndex = 0; axisEndIndex < 6; ++axisEndIndex) {
					if (!ProjectEditorPoint(axisEnds[axisEndIndex], viewProjection, imageMin, imageSize, endScreens[axisEndIndex])) {
						continue;
					}
					const float distance = EditorDistanceToSegment(mouse, centerScreen, endScreens[axisEndIndex]);
					if (distance <= bestDistance) {
						bestDistance = distance;
						hoveredAxisEnd = axisEndIndex;
					}
				}
				gizmoHovered = hoveredAxisEnd >= 0;

				if (gizmoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					const ImVec2 direction(
						endScreens[hoveredAxisEnd].x - centerScreen.x,
						endScreens[hoveredAxisEnd].y - centerScreen.y);
					const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
					state.dragScreenDirectionX = length > 0.0001f ? direction.x / length : 0.0f;
					state.dragScreenDirectionY = length > 0.0001f ? direction.y / length : 0.0f;
					state.dragWorldDirection = worldDirections[hoveredAxisEnd];
					state.activeAxis = hoveredAxisEnd / 2;
					state.isDragging = true;
				}
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					state.isDragging = false;
					state.activeAxis = -1;
				}

				if (state.isDragging && state.activeAxis >= 0) {
					const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
					const float dragAmount =
						mouseDelta.x * state.dragScreenDirectionX +
						mouseDelta.y * state.dragScreenDirectionY;
					if (std::abs(dragAmount) > 0.0001f) {
						Transform& transform = selectedObject->GetTransform();
						const Transform beforeEdit = transform;
						if (state.tool == TransformTool::Move) {
							const float amount = dragAmount * (std::max)(axisLength, 1.0f) * 0.006f;
							transform.translate.x += state.dragWorldDirection.x * amount;
							transform.translate.y += state.dragWorldDirection.y * amount;
							transform.translate.z += state.dragWorldDirection.z * amount;
						} else if (state.tool == TransformTool::Rotate) {
							const float directionSign = state.dragWorldDirection.x + state.dragWorldDirection.y + state.dragWorldDirection.z;
							if (state.activeAxis == 0) transform.rotate.x += dragAmount * directionSign * 0.01f;
							if (state.activeAxis == 1) transform.rotate.y += dragAmount * directionSign * 0.01f;
							if (state.activeAxis == 2) transform.rotate.z += dragAmount * directionSign * 0.01f;
						} else {
							if (state.activeAxis == 0) transform.scale.x = (std::max)(0.01f, transform.scale.x + dragAmount * 0.01f);
							if (state.activeAxis == 1) transform.scale.y = (std::max)(0.01f, transform.scale.y + dragAmount * 0.01f);
							if (state.activeAxis == 2) transform.scale.z = (std::max)(0.01f, transform.scale.z + dragAmount * 0.01f);
						}
						selectedObject->SetTranslate(transform.translate);
						selectedObject->SetRotate(transform.rotate);
						selectedObject->SetScale(transform.scale);
						selectedObject->RecordTransformEdit(beforeEdit);
						if (options.onTransformChanged) {
							options.onTransformChanged(state.selectedIndex, transform);
						}
					}
				}

				ImDrawList* drawList = ImGui::GetForegroundDrawList();
				drawList->PushClipRect(imageRect.Min, imageRect.Max, true);
				const std::array<ImU32, 3> axisColors{
					IM_COL32(255, 75, 75, 255),
					IM_COL32(70, 235, 110, 255),
					IM_COL32(70, 145, 255, 255),
				};
				for (int axisEndIndex = 0; axisEndIndex < 6; ++axisEndIndex) {
					const int axis = axisEndIndex / 2;
					ImU32 color = axisColors[axis];
					if (state.isDragging && state.activeAxis == axis) color = IM_COL32(255, 255, 255, 255);
					if (!state.isDragging && hoveredAxisEnd == axisEndIndex) color = IM_COL32(255, 225, 90, 255);
					drawList->AddLine(centerScreen, endScreens[axisEndIndex], color, axisEndIndex % 2 == 0 ? 4.0f : 2.0f);
					if (state.tool == TransformTool::Scale) {
						drawList->AddRectFilled(
							ImVec2(endScreens[axisEndIndex].x - 5.0f, endScreens[axisEndIndex].y - 5.0f),
							ImVec2(endScreens[axisEndIndex].x + 5.0f, endScreens[axisEndIndex].y + 5.0f),
							color);
					} else {
						drawList->AddCircleFilled(endScreens[axisEndIndex], 6.0f, color);
					}
				}
				const char* toolLabel = state.tool == TransformTool::Move
					? "Move XYZ"
					: (state.tool == TransformTool::Rotate ? "Rotate XYZ" : "Scale XYZ");
				drawList->AddText(ImVec2(centerScreen.x + 10.0f, centerScreen.y + 10.0f), IM_COL32(255, 255, 255, 240), toolLabel);
				drawList->PopClipRect();
			}
		}
	}

	// ギズモ以外を左クリックした時だけ、新しいオブジェクトを選択する。
	if (!state.isDragging && !gizmoHovered && !toolbarHovered &&
		imageRect.Contains(ImGui::GetMousePos()) &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		const ImVec2 mouse = ImGui::GetMousePos();
		float bestScore = (std::numeric_limits<float>::max)();
		int bestIndex = -1;
		for (int index = 0; index < static_cast<int>(options.objects.size()); ++index) {
			Object3d* object = options.objects[index].object;
			Vector3 boundsMin{};
			Vector3 boundsMax{};
			if (!object || !BuildEditorWorldBounds(*object, boundsMin, boundsMax)) {
				continue;
			}
			const std::array<Vector3, 8> corners{ {
				{ boundsMin.x, boundsMin.y, boundsMin.z }, { boundsMax.x, boundsMin.y, boundsMin.z },
				{ boundsMin.x, boundsMax.y, boundsMin.z }, { boundsMax.x, boundsMax.y, boundsMin.z },
				{ boundsMin.x, boundsMin.y, boundsMax.z }, { boundsMax.x, boundsMin.y, boundsMax.z },
				{ boundsMin.x, boundsMax.y, boundsMax.z }, { boundsMax.x, boundsMax.y, boundsMax.z },
			} };
			ImVec2 screenMin((std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)());
			ImVec2 screenMax(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
			int visibleCount = 0;
			for (const Vector3& corner : corners) {
				ImVec2 screen{};
				if (!ProjectEditorPoint(corner, viewProjection, imageMin, imageSize, screen)) continue;
				screenMin.x = (std::min)(screenMin.x, screen.x);
				screenMin.y = (std::min)(screenMin.y, screen.y);
				screenMax.x = (std::max)(screenMax.x, screen.x);
				screenMax.y = (std::max)(screenMax.y, screen.y);
				++visibleCount;
			}
			if (visibleCount == 0 || !ImRect(screenMin, screenMax).Contains(mouse)) continue;
			const float centerX = (screenMin.x + screenMax.x) * 0.5f;
			const float centerY = (screenMin.y + screenMax.y) * 0.5f;
			const float dx = mouse.x - centerX;
			const float dy = mouse.y - centerY;
			const float area = (std::max)(1.0f, (screenMax.x - screenMin.x) * (screenMax.y - screenMin.y));
			const float score = dx * dx + dy * dy + area * 0.01f;
			if (score < bestScore) {
				bestScore = score;
				bestIndex = index;
			}
		}
		if (state.selectedIndex != bestIndex) {
			state.selectedIndex = bestIndex;
			if (options.onSelectionChanged) {
				options.onSelectionChanged(bestIndex);
			}
		}
	}

	// 左クリックは選択・ギズモ専用とし、カメラは右・中ボタンへ分離して誤操作を防ぐ。
	UpdateViewportCamera(options.camera, state.isDragging || toolbarHovered);
#else
	(void)state;
	(void)options;
#endif
}

void SceneEditor::DrawSpriteViewportEditor(SpriteViewportState& state, const SpriteViewportOptions& options)
{
#ifdef USE_IMGUI
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}
	const float clientWidth = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
	if (clientWidth <= 0.0f || clientHeight <= 0.0f) {
		return;
	}

	if (state.selectedIndex >= static_cast<int>(options.sprites.size()) ||
		(state.selectedIndex >= 0 && !options.sprites[state.selectedIndex].sprite)) {
		state.selectedIndex = -1;
		state.isDragging = false;
		state.activeAxis = -1;
	}

	const ImRect imageRect(
		ImVec2(rectX, rectY),
		ImVec2(rectX + rectWidth, rectY + rectHeight));
	const float screenScaleX = rectWidth / clientWidth;
	const float screenScaleY = rectHeight / clientHeight;
	const auto toScreen = [&](const Vector2& point) {
		return ImVec2(
			rectX + point.x * screenScaleX,
			rectY + point.y * screenScaleY);
	};

	// 3Dツールと重ならない位置に、2D専用の小さな操作パネルを表示します。
	ImGui::SetNextWindowPos(ImVec2(rectX + 10.0f, rectY + 135.0f), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.88f);
	const ImGuiWindowFlags toolbarFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking;
	ImGui::Begin("2D Edit Tools", nullptr, toolbarFlags);
	const auto drawToolButton = [&](const char* label, TransformTool tool) {
		if (state.tool == tool) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.48f, 0.82f, 1.0f));
		}
		if (ImGui::Button(label)) {
			state.tool = tool;
			state.isDragging = false;
			state.activeAxis = -1;
		}
		if (state.tool == tool) {
			ImGui::PopStyleColor();
		}
	};
	ImGui::TextUnformatted("2D Sprite");
	drawToolButton("Move##Sprite", TransformTool::Move);
	ImGui::SameLine();
	drawToolButton("Rotate##Sprite", TransformTool::Rotate);
	ImGui::SameLine();
	drawToolButton("Size##Sprite", TransformTool::Scale);
	if (state.selectedIndex >= 0) {
		ImGui::Text("Selected: %s", options.sprites[state.selectedIndex].label.c_str());
	} else {
		ImGui::TextDisabled("Left click a 2D texture to select it.");
	}
	const bool toolbarHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
	ImGui::End();

	Sprite* selectedSprite = state.selectedIndex >= 0
		? options.sprites[state.selectedIndex].sprite
		: nullptr;
	bool gizmoHovered = false;
	if (selectedSprite) {
		const Vector2 position = selectedSprite->GetPosition();
		const Vector2 size = selectedSprite->GetSize();
		const Vector2 anchor = selectedSprite->GetAnchorPoint();
		const float rotation = selectedSprite->GetRotation();
		const float cosine = std::cos(rotation);
		const float sine = std::sin(rotation);
		const auto rotatePoint = [&](const Vector2& local) {
			return Vector2{
				position.x + local.x * cosine - local.y * sine,
				position.y + local.x * sine + local.y * cosine,
			};
		};
		const std::array<Vector2, 4> localCorners{ {
			{ -size.x * anchor.x, -size.y * anchor.y },
			{ size.x * (1.0f - anchor.x), -size.y * anchor.y },
			{ size.x * (1.0f - anchor.x), size.y * (1.0f - anchor.y) },
			{ -size.x * anchor.x, size.y * (1.0f - anchor.y) },
		} };
		std::array<ImVec2, 4> corners{};
		for (size_t index = 0; index < corners.size(); ++index) {
			corners[index] = toScreen(rotatePoint(localCorners[index]));
		}

		const Vector2 worldCenter = rotatePoint({
			size.x * (0.5f - anchor.x),
			size.y * (0.5f - anchor.y),
		});
		const ImVec2 center = toScreen(worldCenter);
		const ImVec2 mouse = ImGui::GetMousePos();
		constexpr float axisLength = 58.0f;
		const ImVec2 xHandle(center.x + axisLength, center.y);
		const ImVec2 yHandle(center.x, center.y - axisLength);
		const float halfScreenWidth = std::abs(size.x * screenScaleX) * 0.5f;
		const float halfScreenHeight = std::abs(size.y * screenScaleY) * 0.5f;
		const float rotationRadius = (std::max)({ halfScreenWidth, halfScreenHeight, 34.0f }) + 18.0f;
		const float centerDistance = std::sqrt(
			(mouse.x - center.x) * (mouse.x - center.x) +
			(mouse.y - center.y) * (mouse.y - center.y));

		int hoveredAxis = -1;
		if (state.tool == TransformTool::Rotate) {
			if (std::abs(centerDistance - rotationRadius) <= 10.0f) {
				hoveredAxis = 2;
			}
		} else {
			float bestDistance = 11.0f;
			const float xDistance = EditorDistanceToSegment(mouse, center, xHandle);
			if (xDistance <= bestDistance) {
				bestDistance = xDistance;
				hoveredAxis = 0;
			}
			const float yDistance = EditorDistanceToSegment(mouse, center, yHandle);
			if (yDistance <= bestDistance) {
				hoveredAxis = 1;
			}
		}
		gizmoHovered = hoveredAxis >= 0;

		if (gizmoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			state.activeAxis = hoveredAxis;
			state.isDragging = true;
			state.previousMouseAngle = std::atan2(mouse.y - center.y, mouse.x - center.x);
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			state.activeAxis = -1;
			state.isDragging = false;
		}

		if (state.isDragging && state.activeAxis >= 0) {
			const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
			Vector2 editedPosition = selectedSprite->GetPosition();
			Vector2 editedSize = selectedSprite->GetSize();
			float editedRotation = selectedSprite->GetRotation();
			if (state.tool == TransformTool::Move) {
				if (state.activeAxis == 0) {
					editedPosition.x += mouseDelta.x / screenScaleX;
				}
				if (state.activeAxis == 1) {
					editedPosition.y += mouseDelta.y / screenScaleY;
				}
				selectedSprite->SetPosition(editedPosition);
			} else if (state.tool == TransformTool::Rotate) {
				const float mouseAngle = std::atan2(mouse.y - center.y, mouse.x - center.x);
				float angleDelta = mouseAngle - state.previousMouseAngle;
				if (angleDelta > std::numbers::pi_v<float>) angleDelta -= std::numbers::pi_v<float> * 2.0f;
				if (angleDelta < -std::numbers::pi_v<float>) angleDelta += std::numbers::pi_v<float> * 2.0f;
				editedRotation += angleDelta;
				state.previousMouseAngle = mouseAngle;
				selectedSprite->SetRotation(editedRotation);
			} else {
				if (state.activeAxis == 0) {
					editedSize.x = (std::max)(1.0f, editedSize.x + mouseDelta.x / screenScaleX);
				}
				if (state.activeAxis == 1) {
					editedSize.y = (std::max)(1.0f, editedSize.y - mouseDelta.y / screenScaleY);
				}
				selectedSprite->SetSize(editedSize);
			}
			if (options.onTransformChanged) {
				options.onTransformChanged(
					state.selectedIndex,
					selectedSprite->GetPosition(),
					selectedSprite->GetRotation(),
					selectedSprite->GetSize());
			}
		}

		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->PushClipRect(imageRect.Min, imageRect.Max, true);
		for (size_t index = 0; index < corners.size(); ++index) {
			drawList->AddLine(
				corners[index],
				corners[(index + 1) % corners.size()],
				IM_COL32(255, 225, 95, 255),
				2.0f);
		}
		if (state.tool == TransformTool::Rotate) {
			drawList->AddCircle(
				center,
				rotationRadius,
				gizmoHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 190, 70, 255),
				48,
				3.0f);
			drawList->AddText(
				ImVec2(center.x + rotationRadius + 8.0f, center.y - 8.0f),
				IM_COL32(255, 220, 120, 255),
				"Rotate");
		} else {
			const ImU32 xColor = state.activeAxis == 0
				? IM_COL32(255, 255, 255, 255)
				: IM_COL32(255, 75, 75, 255);
			const ImU32 yColor = state.activeAxis == 1
				? IM_COL32(255, 255, 255, 255)
				: IM_COL32(70, 235, 110, 255);
			drawList->AddLine(center, xHandle, xColor, 4.0f);
			drawList->AddLine(center, yHandle, yColor, 4.0f);
			if (state.tool == TransformTool::Scale) {
				drawList->AddRectFilled(
					ImVec2(xHandle.x - 6.0f, xHandle.y - 6.0f),
					ImVec2(xHandle.x + 6.0f, xHandle.y + 6.0f),
					xColor);
				drawList->AddRectFilled(
					ImVec2(yHandle.x - 6.0f, yHandle.y - 6.0f),
					ImVec2(yHandle.x + 6.0f, yHandle.y + 6.0f),
					yColor);
			} else {
				drawList->AddCircleFilled(xHandle, 7.0f, xColor);
				drawList->AddCircleFilled(yHandle, 7.0f, yColor);
			}
			drawList->AddText(ImVec2(xHandle.x + 8.0f, xHandle.y - 8.0f), xColor, "X");
			drawList->AddText(ImVec2(yHandle.x + 8.0f, yHandle.y - 8.0f), yColor, "Y");
		}
		drawList->PopClipRect();
	}

	// ギズモ以外を左クリックした時だけ、手前に描かれたスプライトから順に選択します。
	if (!state.isDragging &&
		!gizmoHovered &&
		!toolbarHovered &&
		imageRect.Contains(ImGui::GetMousePos()) &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		const ImVec2 mouse = ImGui::GetMousePos();
		const Vector2 mouseInClient{
			(mouse.x - rectX) / screenScaleX,
			(mouse.y - rectY) / screenScaleY,
		};
		int selectedIndex = -1;
		for (int index = static_cast<int>(options.sprites.size()) - 1; index >= 0; --index) {
			Sprite* sprite = options.sprites[index].sprite;
			if (!sprite) {
				continue;
			}
			const Vector2 position = sprite->GetPosition();
			const Vector2 size = sprite->GetSize();
			const Vector2 anchor = sprite->GetAnchorPoint();
			const float rotation = sprite->GetRotation();
			const float cosine = std::cos(-rotation);
			const float sine = std::sin(-rotation);
			const Vector2 delta{
				mouseInClient.x - position.x,
				mouseInClient.y - position.y,
			};
			const Vector2 local{
				delta.x * cosine - delta.y * sine,
				delta.x * sine + delta.y * cosine,
			};
			const float minimumX = -size.x * anchor.x;
			const float minimumY = -size.y * anchor.y;
			if (local.x >= minimumX &&
				local.x <= minimumX + size.x &&
				local.y >= minimumY &&
				local.y <= minimumY + size.y) {
				selectedIndex = index;
				break;
			}
		}
		if (state.selectedIndex != selectedIndex) {
			state.selectedIndex = selectedIndex;
			if (options.onSelectionChanged) {
				options.onSelectionChanged(selectedIndex);
			}
		}
	}
#else
	(void)state;
	(void)options;
#endif
}

void SceneEditor::UpdateViewportCamera(Camera* camera, bool inputBlocked)
{
#ifdef USE_IMGUI
	if (!camera || inputBlocked) {
		return;
	}
	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}
	const ImRect imageRect(
		ImVec2(rectX, rectY),
		ImVec2(rectX + rectWidth, rectY + rectHeight));
	if (!imageRect.Contains(ImGui::GetMousePos()) || ImGui::IsAnyItemActive()) {
		return;
	}

	Vector3 cameraPosition = camera->GetTranslate();
	Vector3 cameraRotation = camera->GetRotate();
	const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
	bool cameraChanged = false;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		cameraRotation.y += mouseDelta.x * 0.005f;
		cameraRotation.x = std::clamp(cameraRotation.x + mouseDelta.y * 0.005f, -1.45f, 1.45f);
		cameraChanged = true;
	}
	if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		const Vector3 right{ std::cos(cameraRotation.y), 0.0f, -std::sin(cameraRotation.y) };
		cameraPosition.x -= right.x * mouseDelta.x * 0.01f;
		cameraPosition.z -= right.z * mouseDelta.x * 0.01f;
		cameraPosition.y += mouseDelta.y * 0.01f;
		cameraChanged = true;
	}
	if (std::abs(ImGui::GetIO().MouseWheel) > 0.0001f) {
		const float cosinePitch = std::cos(cameraRotation.x);
		const Vector3 forward{
			std::sin(cameraRotation.y) * cosinePitch,
			-std::sin(cameraRotation.x),
			std::cos(cameraRotation.y) * cosinePitch,
		};
		const float zoomAmount = ImGui::GetIO().MouseWheel * 0.6f;
		cameraPosition.x += forward.x * zoomAmount;
		cameraPosition.y += forward.y * zoomAmount;
		cameraPosition.z += forward.z * zoomAmount;
		cameraChanged = true;
	}
	if (cameraChanged) {
		camera->SetTranslate(cameraPosition);
		camera->SetRotate(cameraRotation);
	}
#else
	(void)camera;
	(void)inputBlocked;
#endif
}

void SceneEditor::DrawInspector(const InspectorOptions& options)
{
#ifdef USE_IMGUI
	// Transformを変更できるInspectorはEdit Viewだけで表示する。
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}

	if (!options.normalObjects || !options.animationObjects || !options.directionalLight || !options.pointLight || !options.spotLight) {
		return;
	}

	const unsigned int inspectorDockId = ImGuiManager::GetInstance()->GetInspectorDockId();
	if (inspectorDockId != 0) {
		ImGui::SetNextWindowDockID(inspectorDockId, options.forceDock ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
	}

	ImGui::Begin("Inspector");
	if (options.description && options.description[0] != '\0') {
		ImGui::TextWrapped("%s", options.description);
		ImGui::Separator();
	}
	if (options.drawHeader) {
		options.drawHeader();
	}

	const size_t addedNormalCount = options.normalObjects->size() > options.protectedNormalObjectCount
		? options.normalObjects->size() - options.protectedNormalObjectCount
		: 0;
	const size_t addedAnimationCount = options.animationObjects->size() > options.protectedAnimationObjectCount
		? options.animationObjects->size() - options.protectedAnimationObjectCount
		: 0;
	if (addedNormalCount > 0 || addedAnimationCount > 0 || options.addedSpriteCount > 0) {
		ImGui::TextDisabled("Added Models: %zu | 2D Textures: %zu", addedNormalCount + addedAnimationCount, options.addedSpriteCount);
	}

	if (ImGui::BeginTabBar("SceneEditorInspectorTabs")) {
		const ImGuiTabItemFlags spriteTabFlags = options.selectSpriteTab ? ImGuiTabItemFlags_SetSelected : 0;
		if (options.sprites && ImGui::BeginTabItem("Sprite", nullptr, spriteTabFlags)) {
			const int selectedSpriteIndex = ImGuiManager::GetInstance()->SpriteWindow(*options.sprites, true, options.forcedSpriteIndex);
			const bool canRemoveSprite = options.removeSprite && selectedSpriteIndex >= 0 &&
				static_cast<size_t>(selectedSpriteIndex) >= options.protectedSpriteCount;
			ImGui::Separator();
			ImGui::BeginDisabled(!canRemoveSprite);
			const bool removeSprite = ImGui::Button("Remove Added 2D Texture") ||
				(canRemoveSprite && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
					!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false));
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(canRemoveSprite
					? "Remove this 2D Texture. Delete key also works while Inspector is focused."
					: "Initial scene sprites are protected. Only textures added from Model Shelf can be removed here.");
			}
			if (removeSprite && canRemoveSprite) {
				options.removeSprite(static_cast<size_t>(selectedSpriteIndex));
			}
			ImGui::EndTabItem();
		}
		const ImGuiTabItemFlags modelTabFlags = options.selectModelTab ? ImGuiTabItemFlags_SetSelected : 0;
		if (ImGui::BeginTabItem("Model", nullptr, modelTabFlags)) {
			ImGuiManager::GetInstance()->ModelWindow(
				*options.normalObjects,
				*options.animationObjects,
				*options.directionalLight,
				*options.pointLight,
				*options.spotLight,
				true,
				options.protectedNormalObjectCount,
				options.protectedAnimationObjectCount,
				options.forcedNormalIndex,
				options.forcedAnimationIndex,
				options.onModelRemoved);
			ImGui::EndTabItem();
		}
		if (options.drawExtraTabs) {
			options.drawExtraTabs();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
#else
	(void)options;
#endif
}
