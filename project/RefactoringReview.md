# チーム向けエンジン整理の再点検

## 目的と完了条件

初見のメンバーが、機能の入口・所有者・更新順・調整場所を追えるようにする。
行数削減やファイル数増加そのものは目標にしない。既存の描画、素材、配置、操作は維持する。

- Scene／生成／描画準備／描画／Collider／Editorの分担を現在のコードで再点検する。
- 役割が混在している箇所だけを段階的に整理し、各変更を検証してから次へ進む。
- 利用ガイドの説明と実コードを一致させ、初見の人が読む入口を示す。
- Debug x64と普段のDevelopment x64をビルドし、影響する操作を実画面で確認する。
- 全体の見直しが終わるまで、部分的な成功をエンジン全体の完了としない。

## 今回確認した範囲

| 対象 | 現在の証拠・判断 | 状態 |
| --- | --- | --- |
| Title | Sceneにモデル・Spriteの生成、所有一覧、初期配置の保護境界が混在していた。TitleObjectManagerへ移管 | 分離済み。下記の操作を実画面確認 |
| 生成共通API | Object3dFactoryが読込・初期化を担当。Object3dCollectionは通常3Dの寿命管理だけを担当 | Titleでも既存Factoryを再利用 |
| 描画準備／描画 | Object3dRenderContextとSceneRenderPipelineが既存。Titleの呼出順を維持 | Title以外の詳細点検は未完 |
| Debug | DebugSceneContent、DebugSceneRenderer、Editor部品が既存 | 生成・所有とContext境界の詳細点検は未完 |
| Stage1 | 本編。固定鏡のLevelデータ変換をStageMirrorFactoryへ移管。InitializeStageGimmicksには別の具体的生成手順が残る | 固定鏡の分離と検証済み。Scene全体の点検は継続 |
| Player／Collider／Level／Audio | ArchitectureGuideに窓口の説明あり | 実装と利用例の照合は未完 |
| README／ArchitectureGuide | READMEの旧Scene名と通常Skinning経路の説明を修正し、初見向け入口を追加。Title所有者と調整場所を更新 | その他の説明は引き続き実装と照合する |

## Title分離で変えないもの

- 初期のplane.obj、uvChecker.png、SkyBox、座標、Light値。
- モデルの追加位置、Animation分類とLoop再生、画像の180px制限と4列配置。
- 通常モデル → Animation → SkyBox → Sprite → PostEffect／ImGuiの描画順。
- 初期配置を残す一括削除と、Titleを抜ける際のGPU完了待ち。
- Object3d、Shader、TextureManagerなど共有描画基盤の動作。

## 検証記録（2026-09-28）

- Debug x64：ビルド成功。
- Development x64：最初は検証用ゲームの実行ファイルロックでLNK1104。終了後の再ビルド成功。Visual StudioのF5でもビルド後に起動できた。
- .vcxprojのClCompile／ClInclude：ワイルドカードなし。
- TitleObjectManager.cpp／.h：.vcxprojと.filtersへ各1件の明示登録を確認。
- 分離前Development通常起動：Titleの初期画像、背景、平面を確認。比較画像は`resources/Captures/Screenshots/CG2_20260928_020707_862.png`。
- 分離後Development F5起動：Title初期画面を確認。比較画像は`resources/Captures/Screenshots/CG2_20260928_021142_929.png`。ウィンドウサイズが異なるため画素一致の比較ではない。
- Title Edit View：AnimatedCube_BaseColor.pngを追加。Inspectorで位置180,160、サイズ180x180、Anchor 0.5,0.5と実描画を確認。
- 同じTitleへmultiMesh.objとwalk.gltfを追加し、通常モデル・Animation・Spriteの同時描画を確認。walkの再生時刻と姿勢の変化も確認。画像は`resources/Captures/Screenshots/CG2_20260928_021338_127.png`。
- Restart Current SceneでTitleを再起動し、追加数0と初期背景・画像への復帰を確認。
- Scene一覧でSTAGE1へ切り替え、開始演出中と通常Cameraへの復帰後の白い携帯Mirrorを確認。JSONやモデルの保存・変更はしていない。
- 未確認：Clear Title Addedの実操作、Gamepad、追加画像の5枚目以降の折返し、欠損素材時の動作。
- Enterキーは自動操作で遷移を確認できなかった。Scene一覧からの切替成功を、Enter→Loading→Stage1の検証成功とは扱わない。入力・Scene遷移コードは今回変更していない。

この記録は作業状況であり、未確認の項目を成功の根拠として使わない。

## 固定鏡の生成・編集反映の分離（2026-09-28）

- 対象：`Stage1::CreateFixedMirrors`と`ApplyFixedMirrorEdits`を`game/mirror/StageMirrorFactory`へ移管。Sceneには再生成の判断・仮一覧からの採用・所有を残した。
- 既存クラスを使う判断：`Object3dFactory`は汎用モデル生成、`FixedMirror`は一枚の鏡の状態を担当するため、Levelデータの変換はゲーム側のFactoryへ置いた。
- 維持したもの：plane.objの既定値、幅・高さのScale×2、反射Textureの512指定、Y回転、BOXの半辺長変換、BOX指定なしの既存形状維持、呼出順。Shader・共有描画APIはこの段階では変更していない。
- Debug x64／Development x64：ビルド成功。最終Debugビルドも成功。出力にプロジェクト設定由来の警告・エラーなし。
- `.vcxproj`のClCompile／ClIncludeにワイルドカードなし。StageMirrorFactory.cpp／.hはプロジェクトと.filtersへそれぞれ1件ずつ登録。
- DevelopmentのF5起動でTitleの初期画像・背景・平面を確認。Scene一覧からStage1へ入り、開始演出中とEdit Viewで白い携帯鏡を確認した。
- 固定鏡は初期Playerから離れた座標にあるため、保存せず編集CameraをX=0、Z=-5へ移動して確認。元のStageMirror配置で鏡面と反射像を確認した。
- InspectorのStageMirrorのXを0.026から2へ一時変更し、`Edited in memory`表示、鏡面・反射像・Collider枠の移動を確認。保存画像：`resources/Captures/Screenshots/CG2_20260928_104144_389.png`。これは編集経路の確認であり、配置変更による不具合修正ではない。
- Restart Current Sceneで一時編集を破棄し、初期化とJSON再読込を確認。その後ゲームを終了した。JSONは保存せず、検証前後のstage1.jsonのSHA256は`EC63BC9BC4E7D4626B88F198540EED531D38A4138DA552F823A413AEBF7457F1`で一致。
- Debug／Development本編Smoke：両方で終了コード0、SUCCESS。Collider、Enemy生成、鏡の所持・解除・遮蔽・Laser反射、Door、Camera、落下の必須判定は通過した。ただし両方の`hazardReflection=0`は成功条件に含まれていないため、この機能は確認済みとしない。
- 未確認：EnterによるTitle→Loading→Stage1（自動入力で遷移を確認できず）、固定鏡の枚数変更、読込失敗時の実画面、BOX指定なしの実編集、幅・高さ・回転の全組合せ。配置の平行移動だけで全ての反射精度が正しいとは判断しない。

## 続けて照合する箇所

- `InitializeStageGimmicks`とCSV記号からの生成：Sceneに残る生成責任を整理できるか調べる。未実装のE0/G0/C0/L0を実装済みと説明しない。
- `EnemyManager.h/.cpp`：Object3dCommonをScene所有とするコメントは、GetInstanceのローカルstatic実体と照合して訂正済み。EnemyはManagerが所有し、共通3D設定は非所有参照。処理の変更はない。API全体の点検は引き続き行う。
- RenderContext／RenderPipeline：通常モデルとAnimationの混在時の状態設定、反射Cameraと通常Cameraの描画準備の順序を点検する。リファクタリングに不具合修正を混ぜない。
- Player／Collider／Level／Audio、DebugScene：公開APIの責任・寿命・失敗時の扱いとガイドを照合する。

## Release・携帯鏡・配置編集の修正（2026-09-28、検証待ち）

今回の依頼を優先し、機能変更・不具合修正として実施。上のリファクタリング時のビルド成功は、以下の変更後の成功を意味しません。

- LoadingScene：固定のWinApp初期サイズとSpriteの投影サイズが異なっていた。背景サイズを毎更新、DirectXCommonの実サイズに合わせた。通常Spriteは自動拡縮しない。
- 起動順はLoadingScene→TitleScene。Titleの描画はモデル→SkyBox→Spriteを一フレーム内で行う。Spriteの描画後にSkyBoxの読み込み完了を判定する仕組みではない。
- ImGuiManager::IsMouseOverGameView：USE_IMGUIなしでは常にfalseだったため、携帯鏡のMouse操作が無効だった。Releaseではクライアント領域内をtrueにする。Debug/DevelopmentのInspector上の入力除外は維持。
- CarryableMirror：縦持ち横ずれ-1.75→0、横持ち距離2.70→3.40。置いた直後に拾い直せるよう取得距離3→4。
- Object3d専用のCameraForGPUに裏面明度を追加。既定1で通常モデルを維持し、携帯鏡だけ0.60。共有Modelの材質を書き換えない。HLSLは元の法線と視線で裏面を判定する。plane.objの+Z法線とMirrorの反射面を照合済み。ただし画面確認は未実施。
- StageSceneRenderer：発射装置の壁越しシルエット呼出を削除。通常描画と壁の深度判定は残す。モデルを削除・移動して隠す修正ではない。
- TitleObjectManager：初期Spriteの位置0,0・サイズ512,512を明示。AddTextureの4引数版で位置・サイズを直接指定できる。Shelfの2引数版は同じ180,160へ仮配置し、長辺180以内で縦横比を保つ。追加順の4列自動配置は止めたため、複数画像はInspectorで移動する。
- Sprite Inspector：選択中Spriteの実際の色を読むよう修正。Copy Placement C++ボタンで位置・サイズ・Anchor・回転を直接数値のC++としてコピーする。保存機能ではない。
- StageEditor：Edit ViewのStage Player→Player Positionから、Player::SetPositionで本体とColliderを一緒に移動する。実行中の位置のみ。開始演出中はGame Viewに戻ると演出位置が優先される。開始地点の保存はPlayerStartのLevel編集で行う。
- 固定鏡の角度：Inspectorの値をStageLightPuzzleの基準角へ同期。JSONは基準角、充電による回転は実行時の加算に分離し、次フレームの上書きを防ぐ。
- プロジェクト照合で既存GameSceneRegistry.cpp/.hのfilters登録漏れを発見し、SceneManagementへ明示登録。
- Audioは直前の作業分（VoiceId検索の共通化とAPIコメント、独立Smokeテスト）が未検証のまま残る。今回の依頼に便乗した追加変更はしていない。

未確認：Debug/Development/Release x64の変更後ビルド、通常F5→Loading→Title→Stage1、最大化・通常サイズの白背景、Releaseの左右Mouse操作、鏡の縦横・表裏、拾い直し、壁の遮蔽、通常モデルとwalk.gltfの描画、各ImGuiの実操作。全ImGuiをチェック済みとは扱わない。

ビルド実行は承認チェックの利用制限で止まっている。迂回して実行せず、再開可能になってから上記を検証する。
