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
#include "FileHotReload.h"
#include "../ecs/EcsWorld.h"
#include"BaseScene.h"
#include "SceneEditor.h"
#include "SkyBox.h"
#include <array>
#include <deque>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>
#include <memory>

//BaseSceneを継承する(publicをつけることで公認の親子関係)
class DebugScene : public BaseScene
{
public:
    // Debug画面へ入った一度だけ、確認用のCamera・モデル・UIを作成します。
    void Initialize()override;
	// Debug画面を抜ける時に、DebugSceneが所有するデータを解放します。
	void Finalize()override;
	// 毎フレーム、確認用モデル・入力・ImGuiを更新します。
	void Update()override;
	// 毎フレーム、確認用モデル・Sprite・ImGuiを描画します。
	void Draw()override;

private:
	// ---------- Game ViewのCamera操作 ----------

	// Debug画面でだけ使う、右マウスによるCamera移動を更新します。
    void UpdateGameViewCameraControl();

	// ---------- scene.jsonの読込 ----------

    // Blenderから出力したscene.jsonを読み直し、JSON由来のモデルだけを再配置します。
    bool ReloadLevelData();
    // scene.jsonの保存を検出したフレームだけ、レベルデータを再読込します。
    void UpdateLevelHotReload();

	// ---------- ImGuiの確認・編集UI ----------

	// モデル・Sprite・ECSを編集するInspector全体を表示します。
    void DrawInspectorImGui();
	// Inspector内のDebug専用タブを表示します。
    void DrawDebugInspectorTabs();
	// スクリーンショット・動画・リプレイの操作UIを表示します。
    void DrawCaptureImGui();
	// 描画設定などをまとめた上部ツールUIを表示します。
    void DrawTopToolsImGui();
	// resources内を調べ、モデル棚へ表示する項目を作ります。
    void ScanResourceModels();
	// モデル棚を表示し、選択・プレビュー・追加を受け付けます。
    void DrawModelShelfImGui();
	// モデル棚からEdit Viewへ落としたモデルをSceneへ追加します。
	void HandleModelDropOnEditView();
	// Game View上のSprite選択を処理します。
    void HandleGameViewSpriteSelection();
	// Game View上の3Dモデル選択を処理します。
    void HandleGameViewObjectSelection();
	// 選択中モデルを移動するギズモをEdit Viewへ重ねて表示します。
	void DrawEditViewModelToolsOverlay();
	// 3Dモデル用のEdit Viewを描画します。
	void DrawEditViewportImGui();
	// Sprite用のEdit Viewを描画します。
	void DrawSpriteEditViewportImGui();

	// ---------- ECS登録 ----------

	// 追加した3DモデルをECSのEntityとして登録します。
	void RegisterEcsModel(Object3d* object, const std::string& sourceFile, bool isAnimated);
	// 追加したSpriteをECSのEntityとして登録します。
	void RegisterEcsSprite(Sprite* sprite, const std::string& sourceFile);
	// Sceneの追加・削除に合わせ、ECSの一覧を更新します。
	void UpdateEcsWorld();
	// ECSへ登録されたEntityの内容をInspectorへ表示します。
	void DrawEcsInspectorImGui();

	// ---------- モデル・Spriteの追加と選択 ----------

	// TextureをSpriteとしてScene中央へ追加します。
    bool AddTextureToScene(const std::string& textureFilePath);
	// TextureをSpriteとして指定した2D座標へ追加します。
    bool AddTextureToScene(const std::string& textureFilePath, const Vector2& position);
	// Game Viewの画面座標を、Sprite用の2D座標へ変換します。
    bool TryGetGameViewSpritePosition(float screenX, float screenY, Vector2& outPosition) const;
	// 現在選択中のSpriteを返します。未選択ならnullptrです。
    Sprite* GetSelectedSceneSprite();
	// const版の選択中Sprite取得です。
    const Sprite* GetSelectedSceneSprite() const;
	// 指定番号のSpriteをInspector編集対象にします。
    void SelectSceneSprite(size_t index);
	// Spriteの選択状態を解除します。
    void ClearSceneSpriteSelection();
	// Mouseが選択Spriteのギズモ上にあるかを判定します。
    bool IsMouseOverSelectedSpriteGizmo(float mouseScreenX, float mouseScreenY) const;
	// 選択Spriteの移動用ギズモを描画します。
    void DrawSelectedSpriteGizmo();
	// Mouseが選択3Dモデルのギズモ上にあるかを判定します。
    bool IsMouseOverSelectedObjectGizmo(float mouseScreenX, float mouseScreenY) const;
	// 選択3Dモデルの移動用ギズモを描画します。
    void DrawSelectedObjectGizmo();
	// モデル同士のAABB当たり判定を色付きで重ねて描画します。
    void DrawCollisionDebugOverlay();

	// ---------- 画面保存・動画・リプレイ ----------

	// Game Viewの動画記録を毎フレーム更新します。
    void UpdateRecordingCapture();
	// 常時ためているリプレイ画像を毎フレーム更新します。
	void UpdateReplayCapture();
	// ためたリプレイ画像を動画として保存します。
	bool SaveReplayClip();
	// 指定モデルだけを見るプレビューモードへ入ります。
    bool EnterModelPreview(const std::string& fileName);
	// 指定Textureだけを見るプレビューモードへ入ります。
    bool EnterTexturePreview(const std::string& textureFilePath);
	// モデル・Textureのプレビューモードを終了します。
    void ExitModelPreview();
	// モデルプレビューCameraを初期位置へ戻します。
    void ResetModelPreviewCamera();
	// Textureプレビューの拡大率と位置を初期化します。
    void ResetTexturePreviewView();
	// Edit Viewから追加したモデル・Spriteだけを削除します。
    void ClearAddedSceneModels();
	// 指定したモデルをScene中央へ追加します。
    bool AddModelToScene(const std::string& fileName);
	// 指定したモデルを3D座標へ追加します。
    bool AddModelToScene(const std::string& fileName, const Vector3& spawnPosition);
	// ModelManagerからObject3dを作り、必要ならAnimation再生を有効にします。
    std::unique_ptr<Object3d> CreateObjectFromModel(const std::string& fileName, bool playAnimation);
	// モデルの見た目の大きさから、簡易当たり判定用AABBを作ります。
    bool BuildWorldAabb(const Object3d& object, MyMath::AABB& outAabb) const;
	// Game Viewの画面座標を、3D空間の床上座標へ変換します。
    bool TryGetGameViewWorldPosition(float screenX, float screenY, Vector3& outPosition) const;
	// 現在選択中の3Dモデルを返します。未選択ならnullptrです。
    Object3d* GetSelectedSceneObject();
	// const版の選択中3Dモデル取得です。
    const Object3d* GetSelectedSceneObject() const;
	// 通常モデルまたはAnimationモデルをInspector編集対象にします。
    void SelectSceneObject(bool animationObject, size_t index);
	// 3Dモデルの選択状態を解除します。
    void ClearSceneObjectSelection();
	// RenderTextureからGame Viewの画素を読み出します。
    bool CaptureGameViewPixels(std::vector<unsigned char>& pixels, int& width, int& height);
	// 読み出した画素をBMPファイルとして保存します。
    bool SavePixelsAsBmp(const std::filesystem::path& filePath, const std::vector<unsigned char>& pixels, int width, int height);
	// AVI動画ファイルへの記録を開始します。
    bool BeginRecordingAvi(const std::filesystem::path& filePath, int width, int height, int frameRate = 10);
	// 一枚分の画素をAVI動画へ追加します。
    bool AppendRecordingFrame(const std::vector<unsigned char>& pixels, int width, int height);
	// AVI動画ファイルへの記録を終了します。
    void EndRecordingAvi();
	// Captures配下の種別ごとの保存先を返します。
	std::filesystem::path GetCaptureDirectory(const char* folderName) const;
	// スクリーンショット・動画・リプレイ用フォルダを作ります。
	bool CreateCaptureDirectories() const;
	// 保存ファイル名に使う重複しにくい日時文字列を作ります。
    std::string MakeTimestampString() const;

	// ---------- Particle確認 ----------

	// Particle設定をImGuiで編集します。
    void DrawParticleEffectImGui(bool embedded = false);
	// 現在のParticle設定に従ってEmitterへ発生を依頼します。
    void UpdateParticleEffectEmission();
	// Particleを出す基準となる3D座標を返します。
    Vector3 GetParticleEffectPosition() const;

	// ---------- 自動確認用テスト ----------

	// UI操作の自動確認を環境変数から開始します。
    void InitializeUiSmokeFromEnvironment();
	// UI操作の自動確認をUpdate内で進めます。
    void UpdateUiSmoke();
	// 描画後の画像保存を必要とするUI自動確認を進めます。
    void UpdateUiSmokeAfterDraw();
	// UI自動確認を成功・失敗として終了します。
    void FinishUiSmoke(bool success, const std::string& message);
	// 時間・Animation・Particleの自動確認を環境変数から開始します。
    void InitializeTimePlaybackSmokeFromEnvironment();
	// 時間・Animation・Particleの自動確認をUpdate内で進めます。
    void UpdateTimePlaybackSmoke();
	// 時間・Animation・Particleの自動確認を成功・失敗として終了します。
    void FinishTimePlaybackSmoke(bool success, const std::string& message);

    struct ParticleEffectControl {
		// trueの間だけ、この種類のParticleを発生させます。
        bool enabled = false;
		// 一度に発生させるParticle数です。
        int emitCount = 1;
		// Particleの見た目の大きさです。
        float scale = 1.0f;
		// trueならCameraを向く板として描画します。
        bool billboard = true;
    };

	// モデル棚に表示する一件分の情報です。
    using ResourceModelEntry = SceneEditor::ShelfEntry;

    struct CollisionDebugBox {
		// モデルの見た目から作った簡易AABBです。
        MyMath::AABB aabb{};
		// trueなら、ほかのAABBと重なっています。
        bool overlaps = false;
    };

    struct ReplayFrame {
		// 一枚分のGame View画像です。
		std::vector<unsigned char> pixels;
		// pixelsの横幅です。
		int width = 0;
		// pixelsの高さです。
		int height = 0;
    };

	// 3D・2D編集ギズモで、現在操作している軸です。
    enum class GizmoAxis {
        None,
        X,
        Y,
        Z
    };

    // ---------- Debug用Camera ----------

    // 上から確認する時に使える補助Cameraです。
    std::unique_ptr<Camera> upCamera;

    // ---------- Scene内の3Dモデル・Sprite・ECS ----------

    // Animationを使わない3Dモデルです。
    std::vector<std::unique_ptr<Object3d>> normalObjects;
    // Animationを再生する3Dモデルです。
    std::vector<std::unique_ptr<Object3d>> animationObjects;
    // Animationモデルの手へ付ける確認用Weaponです。
    std::unique_ptr<Object3d> handWeapon_;
	// 歩行入力で移動させる確認用Animationモデルへの非所有ポインタです。
	Object3d* walkObject_ = nullptr;

	// Debug画面へ追加した2D画像です。
    std::vector<std::unique_ptr<Sprite>> sprites;
	// モデル・SpriteをEntityとして一覧管理するECS本体です。
	Ecs::World ecsWorld_;
	// ECS Inspectorで選択しているEntityです。
	Ecs::Entity selectedEcsEntity_ = Ecs::kInvalidEntity;
	// 背景として描画するSkyBoxです。
    std::unique_ptr<SkyBox> skyBox_;

    // ---------- モデル棚とプレビュー ----------

	// resourcesから見つけたモデルを並べる、モデル棚の項目です。
    std::vector<ResourceModelEntry> modelLibrary_;
	// モデル棚で一件だけ大きく表示する3Dモデルです。
    std::unique_ptr<Object3d> previewObject_;
	// モデル棚で一件だけ大きく表示するTextureです。
    std::unique_ptr<Sprite> previewSprite_;
	// モデル棚で選択しているモデル名です。
    std::string selectedLibraryModel_;
	// 現在プレビューしているモデル名です。
    std::string previewModelFile_;
	// 現在プレビューしているTextureのパスです。
    std::string previewTextureFilePath_;
	// プレビュー中に再生するAnimationです。
    Model::Animation previewAnimation_{};
	// プレビューへ入る前のCamera位置です。
    Vector3 previewReturnCameraTranslate_{};
	// プレビューへ入る前のCamera回転です。
    Vector3 previewReturnCameraRotate_{};
	// プレビューCameraが見るモデル中心です。
    Vector3 previewCameraTarget_{};
	// プレビューCameraとモデル中心との距離です。
    float previewCameraDistance_ = 3.0f;
	// プレビューCameraをリセットする時の距離です。
    float previewCameraDefaultDistance_ = 3.0f;
	// プレビューCameraの左右角度です。
    float previewCameraYaw_ = 0.0f;
	// プレビューCameraの上下角度です。
    float previewCameraPitch_ = 0.0f;
	// trueならモデルだけを見るプレビューモードです。
    bool isModelPreviewMode_ = false;
	// trueならTextureだけを見るプレビューモードです。
    bool isTexturePreviewMode_ = false;
	// Mouseを離すまで、誤ってプレビューを閉じないようにします。
    bool suppressPreviewExitUntilMouseRelease_ = false;
	// trueなら3Dモデルの簡易当たり判定を重ねて表示します。
    bool showCollisionDebug_ = true;

    // ---------- scene.jsonのHot Reload ----------

    // Debugが最初から作成する通常モデルの個数です。
    size_t levelNormalObjectBaseCount_ = 0;
    // scene.jsonから追加した通常モデルの個数です。
    size_t levelNormalObjectCount_ = 0;
    // scene.jsonの保存時刻を監視し、保存後の再読込に使用します。
    FileHotReload levelHotReload_;
    // Top Toolsへ表示する、scene.jsonの読込結果です。
    std::string levelReloadStatus_ = "Waiting for scene.json changes.";
    // ECS登録まで終わった後の再読込かを判定します。
    bool isLevelHotReloadReady_ = false;
	// 初期配置の通常モデル数です。
    size_t baseNormalObjectCount_ = 0;
	// 初期配置のAnimationモデル数です。
    size_t baseAnimationObjectCount_ = 0;
	// 初期配置のSprite数です。
    size_t baseSpriteCount_ = 0;

    // ---------- スクリーンショット・動画・リプレイ ----------

	// trueの間、Game ViewをAVI動画として記録します。
    bool isRecordingGameView_ = false;
	// 現在の動画記録時間です。
    float recordingTime_ = 0.0f;
	// 次の動画フレームを保存するまでの時間です。
    float recordingFrameTimer_ = 0.0f;
	// 動画へ追加したフレーム数です。
    int recordingFrameIndex_ = 0;
	// 今記録している動画の保存先フォルダです。
    std::filesystem::path recordingDirectory_;
	// 今記録している動画ファイルのパスです。
    std::filesystem::path recordingVideoPath_;
	// 最後に保存したスクリーンショットのパスです。
    std::filesystem::path lastScreenshotPath_;
	// 最後に保存した動画のパスです。
    std::filesystem::path lastVideoPath_;
	// リプレイ動画の保存先フォルダです。
	std::filesystem::path replayDirectory_;
	// 最後に保存したリプレイ動画のパスです。
	std::filesystem::path lastReplayPath_;
	// 直近数秒分のGame View画像をためるキューです。
	std::deque<ReplayFrame> replayFrames_;
	// falseならリプレイ画像をためません。
	bool replayBufferEnabled_ = true;
	// 次のリプレイ画像をためるまでの時間です。
	float replayFrameTimer_ = 0.0f;
	// リプレイとして保持する秒数です。
	static constexpr int kReplaySeconds_ = 30;
	// リプレイへ保存する一秒あたりの画像数です。
	static constexpr int kReplayFramesPerSecond_ = 5;
	// リプレイ画像の横幅です。
	static constexpr int kReplayWidth_ = 480;
	// リプレイ画像の高さです。
	static constexpr int kReplayHeight_ = 270;
	// WindowsのAVIファイルを開いている間だけ使う内部ハンドルです。
    void* recordingAviFile_ = nullptr;
	// WindowsのAVI映像ストリームを表す内部ハンドルです。
    void* recordingAviStream_ = nullptr;
	// 記録開始時の動画横幅です。
    int recordingVideoWidth_ = 0;
	// 記録開始時の動画高さです。
    int recordingVideoHeight_ = 0;
	// Capture UIへ表示する直近の結果メッセージです。
    std::string lastCaptureMessage_;
    // モデル棚UIへ表示する直近の結果メッセージです。
    std::string lastModelShelfMessage_;

    // ---------- 3D・2D選択ギズモ ----------

	// trueなら、選択中3DモデルはanimationObjects側にあります。
    bool selectedSceneObjectIsAnimation_ = false;
	// 選択中3Dモデルの配列番号です。
    size_t selectedSceneObjectIndex_ = 0;
	// trueなら、選択中3DモデルをInspector・ギズモで編集します。
    bool hasSelectedSceneObject_ = false;
	// 現在Mouseで操作している3Dギズモ軸です。
    GizmoAxis activeGizmoAxis_ = GizmoAxis::None;
	// trueなら3DギズモをDrag中です。
    bool isDraggingGizmo_ = false;
	// 3Dギズモを画面上で動かすX方向です。
    float activeGizmoScreenDirectionX_ = 0.0f;
	// 3Dギズモを画面上で動かすY方向です。
    float activeGizmoScreenDirectionY_ = 0.0f;
	// 3Dギズモが動かすワールド座標の方向です。
    Vector3 activeGizmoWorldDirection_{ 1.0f, 0.0f, 0.0f };
	// 選択中Spriteの配列番号です。
    size_t selectedSceneSpriteIndex_ = 0;
	// trueなら、選択中SpriteをInspector・ギズモで編集します。
    bool hasSelectedSceneSprite_ = false;
	// 現在Mouseで操作しているSpriteギズモ軸です。
    GizmoAxis activeSpriteGizmoAxis_ = GizmoAxis::None;
	// trueならSpriteギズモをDrag中です。
    bool isDraggingSpriteGizmo_ = false;
	// モデル追加直後にInspector選択を維持する残りフレーム数です。
    int inspectorAutoSelectModelFrames_ = 0;
	// Sprite追加直後にInspector選択を維持する残りフレーム数です。
    int inspectorAutoSelectSpriteFrames_ = 0;

    // ---------- ParticleのEmitter ----------

	// 円形Particleを発生させるEmitterです。
    std::unique_ptr<ParticleEmitter> emitterCircle;
	// 平面Particleを発生させるEmitterです。
    std::unique_ptr<ParticleEmitter> emitterPlane;

	// Emitter位置の基準にする平面モデルへの非所有ポインタです。
    Object3d* objectPlane = nullptr;
	// Animation時間やEmitter位置の基準にするモデルへの非所有ポインタです。
    Object3d* objectAxis = nullptr;
	// Inspectorで現在選択しているEmitterへの非所有ポインタです。
    ParticleEmitter* activeEmitter = nullptr;

    // ---------- Debug画面全体のライト ----------

	// 太陽光のように、全モデルへ同じ方向から当てる光です。
    Object3d::DirectionalLight directionalLight;
	// 指定した一点から周囲へ広がる光です。
    Object3d::PointLight pointLight;
	// 円すい状の範囲だけを照らす光です。
    Object3d::SpotLight spotLight;

    // ---------- Particleの見た目設定 ----------

	// Emitterを置く位置・回転・大きさです。
    Transform emitterTransform{};

	// Inspector内で選択しているParticle設定の番号です。
    int selectedUI = 0;
	// trueならCylinder Particleを表示します。
    bool isCylinderEffectVisible_ = false;
	// ぶつかった時のParticle設定です。
    ParticleEffectControl hitEffect_{ false, 8, 1.0f, true };
	// 輪状に広がるParticle設定です。
    ParticleEffectControl ringEffect_{ false, 3, 1.0f, true };
	// 柱状Particle設定です。
    ParticleEffectControl cylinderEffect_{ false, 1, 1.0f, false };
	// 柱の周りで光るParticle設定です。
    ParticleEffectControl pillarSparkleEffect_{ false, 10, 1.0f, true };
	// 光の中心を表すParticle設定です。
    ParticleEffectControl lightCoreEffect_{ false, 1, 1.0f, true };
	// 光が降るParticle設定です。
    ParticleEffectControl lightRainEffect_{ false, 8, 1.0f, true };
	// 螺旋状に回る光Particle設定です。
    ParticleEffectControl lightSpiralEffect_{ false, 24, 1.0f, true };
	// 次のParticle発生までの経過時間です。
    float particleEffectEmitTimer_ = 0.0f;
	// 前フレームにCylinderが有効だったかを記録します。
    bool lastCylinderEffectEnabled_ = false;
	// 前回発生したCylinder数です。
    int lastCylinderEffectEmitCount_ = 1;
	// 前回発生したCylinderの大きさです。
    float lastCylinderEffectScale_ = 1.0f;
	// 前回のCylinderがBillboardだったかを記録します。
    bool lastCylinderEffectBillboard_ = false;
    // trueならCylinder設定変更後に発生し直します。
    bool refreshCylinderEffect_ = false;
	// trueならGame ViewのCameraをMouseでDrag中です。
    bool isGameViewCameraDragging_ = false;

    // ---------- Edit Viewの状態 ----------

	// 3Dモデルを置くViewのCamera・選択状態です。
	SceneEditor::ViewportState viewportEditorState_{};
	// Spriteを置くViewの選択状態です。
	SceneEditor::SpriteViewportState spriteViewportEditorState_{};

    // ---------- UI操作の自動確認 ----------

	// trueならUI操作の自動確認を実行します。
    bool uiSmokeEnabled_ = false;
	// trueならUI操作の自動確認は終了済みです。
    bool uiSmokeFinished_ = false;
	// trueならDraw後の画面保存を待っています。
    bool uiSmokePendingCapture_ = false;
	// 自動確認で処理したフレーム数です。
    int uiSmokeFrame_ = 0;
	// 自動確認のどの段階かを表す番号です。
    int uiSmokeStage_ = 0;
	// 自動確認で追加するモデル名です。
    std::string uiSmokeModelFile_;
	// 自動確認結果を書き出すログのパスです。
    std::filesystem::path uiSmokeLogPath_;

    // ---------- 時間・Animation・Particleの自動確認 ----------

	// trueなら時間・Animation・Particleの自動確認を実行します。
    bool timePlaybackSmokeEnabled_ = false;
	// trueなら時間・Animation・Particleの自動確認は終了済みです。
    bool timePlaybackSmokeFinished_ = false;
	// 自動確認のどの段階かを表す番号です。
    int timePlaybackSmokeStage_ = 0;
	// 同じ状態が安定して続いたフレーム数です。
    int timePlaybackSmokeStableFrames_ = 0;
	// 削除操作を繰り返した回数です。
    int timePlaybackSmokeDeleteIterations_ = 0;
	// 現在段階を開始してからの時間です。
    float timePlaybackSmokeStageTime_ = 0.0f;
	// 前フレームのAnimation再生時間です。
    float timePlaybackSmokePreviousAnimationTime_ = 0.0f;
	// 自動確認中のParticle回転角です。
    float timePlaybackSmokeParticleRotation_ = 0.0f;
	// 自動確認開始時のTransformです。
    Transform timePlaybackSmokeOrigin_{};
	// 自動確認で移動先として使うTransformです。
    Transform timePlaybackSmokeTarget_{};
	// 自動確認で一時停止中に期待するTransformです。
    Transform timePlaybackSmokePaused_{};
	// 自動確認で使うモデル名です。
    std::string timePlaybackSmokeModelFile_;
	// 自動確認結果を書き出すログのパスです。
    std::filesystem::path timePlaybackSmokeLogPath_;
	// InspectorをDock位置へ固定する残りフレーム数です。
    int inspectorForceDockFrames_ = 120;

    // ---------- Animation確認 ----------

	// 歩くAnimationです。
    Model::Animation walkAnimation_;
	// 忍び歩きAnimationです。
    Model::Animation sneakWalkAnimation_;
	// 人型モデルのAnimationです。
    Model::Animation humanAnimation_;
	// 必殺技Animationです。
    Model::Animation hissatu_;

	// Animationの現在再生時間です。攻撃判定やイベント開始時刻の基準にできます。
    float animationTime_ = 0.0f;
};

