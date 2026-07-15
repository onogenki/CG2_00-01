#pragma once
#include "DirectXCommon.h"
#include "Object3dCommon.h"
#include "SpriteCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Object3d.h"
#include "Sprite.h"
#include "ParticleEmitter.h"
#include "Audio.h"
#include"BaseScene.h"
#include "SceneEditor.h"
#include "SkyBox.h"
#include <array>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>
#include <memory>

//BaseSceneを継承する(publicをつけることで公認の親子関係)
class GamePlayScene : public BaseScene
{
public:
    // overrideをつけて、親の純粋仮想関数を実装する
    void Initialize()override;
	void Finalize()override;
	void Update()override;
	void Draw()override;

private:
    void UpdateGameViewCameraControl();
    void DrawInspectorImGui();
    void DrawGamePlayInspectorTabs();
    void DrawCaptureImGui();
    void DrawTopToolsImGui();
    void ScanResourceModels();
    void DrawModelShelfImGui();
    void HandleModelDropOnGameView();
    void HandleGameViewSpriteSelection();
    void HandleGameViewObjectSelection();
    void DrawGameViewModelToolsOverlay();
    bool AddTextureToScene(const std::string& textureFilePath);
    bool AddTextureToScene(const std::string& textureFilePath, const Vector2& position);
    bool TryGetGameViewSpritePosition(float screenX, float screenY, Vector2& outPosition) const;
    Sprite* GetSelectedSceneSprite();
    const Sprite* GetSelectedSceneSprite() const;
    void SelectSceneSprite(size_t index);
    void ClearSceneSpriteSelection();
    bool IsMouseOverSelectedSpriteGizmo(float mouseScreenX, float mouseScreenY) const;
    void DrawSelectedSpriteGizmo();
    bool IsMouseOverSelectedObjectGizmo(float mouseScreenX, float mouseScreenY) const;
    void DrawSelectedObjectGizmo();
    void DrawCollisionDebugOverlay();
    void UpdateRecordingCapture();
    bool EnterModelPreview(const std::string& fileName);
    bool EnterTexturePreview(const std::string& textureFilePath);
    void ExitModelPreview();
    void ResetModelPreviewCamera();
    void ResetTexturePreviewView();
    void ClearAddedSceneModels();
    bool AddModelToScene(const std::string& fileName);
    bool AddModelToScene(const std::string& fileName, const Vector3& spawnPosition);
    std::unique_ptr<Object3d> CreateObjectFromModel(const std::string& fileName, bool playAnimation);
    bool BuildWorldAabb(const Object3d& object, MyMath::AABB& outAabb) const;
    bool TryGetGameViewWorldPosition(float screenX, float screenY, Vector3& outPosition) const;
    Object3d* GetSelectedSceneObject();
    const Object3d* GetSelectedSceneObject() const;
    void SelectSceneObject(bool animationObject, size_t index);
    void ClearSceneObjectSelection();
    bool CaptureGameViewPixels(std::vector<unsigned char>& pixels, int& width, int& height);
    bool SavePixelsAsBmp(const std::filesystem::path& filePath, const std::vector<unsigned char>& pixels, int width, int height);
    bool BeginRecordingAvi(const std::filesystem::path& filePath, int width, int height);
    bool AppendRecordingFrame(const std::vector<unsigned char>& pixels, int width, int height);
    void EndRecordingAvi();
    std::filesystem::path GetUserMediaDirectory(const char* folderName, const char* projectFolderName) const;
    std::string MakeTimestampString() const;
    void DrawParticleEffectImGui(bool embedded = false);
    void UpdateParticleEffectEmission();
    Vector3 GetParticleEffectPosition() const;
    void InitializeUiSmokeFromEnvironment();
    void UpdateUiSmoke();
    void UpdateUiSmokeAfterDraw();
    void FinishUiSmoke(bool success, const std::string& message);

    struct ParticleEffectControl {
        bool enabled = false;
        int emitCount = 1;
        float scale = 1.0f;
        bool billboard = true;
    };

    using ResourceModelEntry = SceneEditor::ShelfEntry;

    struct CollisionDebugBox {
        MyMath::AABB aabb{};
        bool overlaps = false;
    };

    enum class GizmoAxis {
        None,
        X,
        Y,
        Z
    };

    //ゲームプレイシーン固有のデータ

    std::unique_ptr<Camera> upCamera;


    std::vector<std::unique_ptr<Object3d>> normalObjects;//通常モデル  
    std::vector<std::unique_ptr<Object3d>> animationObjects;//アニメーションモデル 

    std::vector<std::unique_ptr<Sprite>> sprites;
    std::unique_ptr<SkyBox> skyBox_;
    std::vector<ResourceModelEntry> modelLibrary_;
    std::unique_ptr<Object3d> previewObject_;
    std::unique_ptr<Sprite> previewSprite_;
    std::string selectedLibraryModel_;
    std::string previewModelFile_;
    std::string previewTextureFilePath_;
    Model::Animation previewAnimation_{};
    Vector3 previewReturnCameraTranslate_{};
    Vector3 previewReturnCameraRotate_{};
    Vector3 previewCameraTarget_{};
    float previewCameraDistance_ = 3.0f;
    float previewCameraDefaultDistance_ = 3.0f;
    float previewCameraYaw_ = 0.0f;
    float previewCameraPitch_ = 0.0f;
    bool isModelPreviewMode_ = false;
    bool isTexturePreviewMode_ = false;
    bool suppressPreviewExitUntilMouseRelease_ = false;
    bool showCollisionDebug_ = true;
    size_t baseNormalObjectCount_ = 0;
    size_t baseAnimationObjectCount_ = 0;
    size_t baseSpriteCount_ = 0;
    bool isRecordingGameView_ = false;
    float recordingTime_ = 0.0f;
    float recordingFrameTimer_ = 0.0f;
    int recordingFrameIndex_ = 0;
    std::filesystem::path recordingDirectory_;
    std::filesystem::path recordingVideoPath_;
    std::filesystem::path lastScreenshotPath_;
    std::filesystem::path lastVideoPath_;
    void* recordingAviFile_ = nullptr;
    void* recordingAviStream_ = nullptr;
    int recordingVideoWidth_ = 0;
    int recordingVideoHeight_ = 0;
    std::string lastCaptureMessage_;
    std::string lastModelShelfMessage_;
    bool selectedSceneObjectIsAnimation_ = false;
    size_t selectedSceneObjectIndex_ = 0;
    bool hasSelectedSceneObject_ = false;
    GizmoAxis activeGizmoAxis_ = GizmoAxis::None;
    bool isDraggingGizmo_ = false;
    float activeGizmoScreenDirectionX_ = 0.0f;
    float activeGizmoScreenDirectionY_ = 0.0f;
    Vector3 activeGizmoWorldDirection_{ 1.0f, 0.0f, 0.0f };
    size_t selectedSceneSpriteIndex_ = 0;
    bool hasSelectedSceneSprite_ = false;
    GizmoAxis activeSpriteGizmoAxis_ = GizmoAxis::None;
    bool isDraggingSpriteGizmo_ = false;
    int inspectorAutoSelectModelFrames_ = 0;
    int inspectorAutoSelectSpriteFrames_ = 0;

    // パーティクル関連
    std::unique_ptr<ParticleEmitter> emitterCircle;
    std::unique_ptr<ParticleEmitter> emitterPlane;

    // objects vectorの中にある特定のオブジェクトを指すためのポインタ
    Object3d* objectPlane = nullptr;
    Object3d* objectAxis = nullptr;
    // 今使っているエミッターを指すだけ
    ParticleEmitter* activeEmitter = nullptr;

    Object3d::DirectionalLight directionalLight;//平行光源
    Object3d::PointLight pointLight;//点光源
    Object3d::SpotLight spotLight;//スポットライト

    // パーティクルのトランスフォーム
    Transform emitterTransform{};

    int selectedUI = 0;
    bool isCylinderEffectVisible_ = false;
    ParticleEffectControl hitEffect_{ false, 8, 1.0f, true };
    ParticleEffectControl ringEffect_{ false, 3, 1.0f, true };
    ParticleEffectControl cylinderEffect_{ false, 1, 1.0f, false };
    ParticleEffectControl pillarSparkleEffect_{ false, 10, 1.0f, true };
    ParticleEffectControl lightCoreEffect_{ false, 1, 1.0f, true };
    ParticleEffectControl lightRainEffect_{ false, 8, 1.0f, true };
    ParticleEffectControl lightSpiralEffect_{ false, 24, 1.0f, true };
    float particleEffectEmitTimer_ = 0.0f;
    bool lastCylinderEffectEnabled_ = false;
    int lastCylinderEffectEmitCount_ = 1;
    float lastCylinderEffectScale_ = 1.0f;
    bool lastCylinderEffectBillboard_ = false;
    bool refreshCylinderEffect_ = false;
    bool isGameViewCameraDragging_ = false;
    bool uiSmokeEnabled_ = false;
    bool uiSmokeFinished_ = false;
    bool uiSmokePendingCapture_ = false;
    int uiSmokeFrame_ = 0;
    int uiSmokeStage_ = 0;
    std::string uiSmokeModelFile_;
    std::filesystem::path uiSmokeLogPath_;
    int inspectorForceDockFrames_ = 120;
  
    //アニメーション
    Model::Animation walkAnimation_;
    Model::Animation sneakWalkAnimation_;
    Model::Animation humanAnimation_;
    Model::Animation hissatu_;
    
    // アニメーションの再生時間を管理
    // (ダメージ発生タイミングやコンボ時間、イベントシーンなどをアニメーションの時間タイミングで合わせれる)
    float animationTime_ = 0.0f;
};

