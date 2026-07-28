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
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#endif

namespace {
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

void SceneEditor::HandleShelfDropOnGameView(ShelfState& state, const ShelfCallbacks& callbacks)
{
#ifdef USE_IMGUI
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(x, y, width, height)) {
		return;
	}

	const ImRect gameViewRect(ImVec2(x, y), ImVec2(x + width, y + height));
	const std::string dropTargetId = callbacks.sceneLabel + "GameViewShelfDropTarget";
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

void SceneEditor::DrawInspector(const InspectorOptions& options)
{
#ifdef USE_IMGUI
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
