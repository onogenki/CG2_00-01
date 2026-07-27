[![.github/workflows/DebugBuild.yml](https://github.com/onogenki/CG2_00-01/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/onogenki/CG2_00-01/actions/workflows/DebugBuild.yml)

# CG2_00-01

## 実装した機能

### Skinning / Animation

- Skinningモデルの表示
- Compute Shaderによるスキニング
- アニメーション補間
- Boneのデバッグ表示
- MultiMesh対応
- MultiMaterial対応
  - モデル読み込み時にMeshごとのMaterialとIndex範囲を保持する。
  - 描画時はMeshごとに対応するテクスチャを設定して描画する。

### 手に持つ武器

- `human.gltf` の手Joint `ボーン.016` に追従する武器を実装した。
- `sphere.obj` に `monsterBall.png` を設定し、モンスターボールとして表示する。

### GPU Particle

- Compute ShaderでParticleの初期化、射出、更新を行う。
- FreeListを用いて寿命切れParticleを再利用する。
- Particleの生存期間、移動、alphaの減衰をGPU上で管理する。
- アクティブなParticle数をGPU上で数え、ExecuteIndirectで描画する。
- `walk.gltf` の左右の足JointからParticleを発生させる。
- 2個のGPU Emitterを使用する。
- Sphere / Box / Cone / Meshを混ぜてParticleを発生させる。
  - Mesh EmitterはShader内で定義した四面体の三角形面を使用する。
- Emit Compute Shaderは64 threadでParticle生成を並列処理する。
- 加速度とdragを持つGPU FieldをUpdate Compute Shaderへ追加した。
- Trail型Particleを追加した。
- Directional Lightの色、方向、強度をGPU Particleの描画へ反映する。

## 操作

- GamePlaySceneでWASDを押すと、`walk.gltf` が移動する。
- 移動中は歩行アニメーションを再生する。
- 歩行中、左右の足元からGPU Particleが発生する。

## 補足

- 手に武器を持たせる機能は実装済み。
- GPU Particleは歩行モデルの足元から発生する設定であり、手からParticleを発生させる設定にはしていない。
