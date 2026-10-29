# AudioLib 設計ドキュメント

## 概要

AudioLib は C++20 製のスタンドアロン音声ライブラリ。
ゲームエンジンへの組み込みを主目的としつつ、任意のアプリケーションから利用可能な独立ライブラリとして設計されている。

---

## 設計原則

| 原則 | 適用 |
|---|---|
| KISS | 公開 API は 1 ヘッダ・最小クラス数に絞る |
| YAGNI | 現時点で必要な機能のみ実装、拡張点だけ設計に織り込む |
| SRP | Loader/Repository/Mixer それぞれが単一責務を持つ |
| SoC | デコード・リソース管理・再生制御・デバイス操作を分離 |
| 例外不使用 | すべてのエラーは戻り値・IsValid() で表現 |
| RAII | リソースの確保と解放はコンストラクタ/デストラクタで完結 |

---

## 公開 API

`include/Audio.hpp` の 1 ファイルのみを include すれば使用できる。

### グローバル関数

```cpp
namespace Audio {
    bool Initialize();  // AudioThread (miniaudio device) 起動
    void Shutdown();    // 全リソース解放・デバイス停止
}
```

### Audio クラス

音声リソース 1 つにつき 1 インスタンスを生成する。
コピー不可・ムーブ可。複数箇所から使用する場合はポインタで共有する。

```cpp
namespace Audio {

class Audio {
public:
    explicit Audio(std::string_view path); // ロード失敗時も例外なし
    ~Audio();                              // StopAll + Unregister

    Audio(Audio&&) noexcept;
    Audio& operator=(Audio&&) noexcept;
    Audio(const Audio&)            = delete;
    Audio& operator=(const Audio&) = delete;

    PlayHandle Play(float volume = 1.0f, float pitch = 1.0f, bool loop = false);
    void StopAll();       // このアセットの全再生インスタンスを停止
    bool IsValid() const; // ロード成功か否か
};

}
```

### PlayHandle クラス

`Play()` の戻り値。再生中インスタンスの操作ハンドル。
戻り値を無視しても問題ない（fire-and-forget）。
停止済み・無効なハンドルへの操作は安全に無視される。

```cpp
namespace Audio {

class PlayHandle {
public:
    void Stop();
    void Pause();
    void Resume();
    void SetVolume(float volume); // 0.0 〜 1.0
    void SetPitch(float pitch);   // 1.0 が基準
    bool IsPlaying() const;
    bool IsValid() const;         // id_ == 0 のとき false
};

}
```

### 使用例

```cpp
// 初期化（アプリ起動時）
Audio::Initialize();

// リソースのロードと所有（1 箇所）
Audio::Audio bgm("bgm.wav");
Audio::Audio jumpSe("se_jump.wav");

// 別システムへはポインタで渡す
void PlayerController::SetJumpSE(Audio::Audio* se) { jumpSe_ = se; }

// 再生（戻り値は使っても無視してもよい）
auto inst = jumpSe_->Play(0.8f);
bgm.Play(1.0f, 1.0f, /*loop=*/true);

// 個別制御
inst.SetVolume(0.5f);
inst.Stop();          // 停止済みでも安全

// アプリ終了時
Audio::Shutdown();
```

---

## 内部アーキテクチャ

### クラス一覧

| クラス / 構造体 | ファイル | 責務 |
|---|---|---|
| `SoundAsset` | `src/SoundAsset.hpp` | float32 インターリーブ PCM データと再生メタ情報を保持 |
| `Repository` | `src/Repository.hpp/.cpp` | `handle → shared_ptr<SoundAsset>` のマップ管理 |
| `Loader` | `src/Loader.hpp/.cpp` | ファイルをデコードして Repository に登録 |
| `PlaybackInstance` | `src/Mixer.hpp` | 再生状態（カーソル・音量・ピッチ・ループ等）を保持 |
| `Mixer` | `src/Mixer.hpp/.cpp` | PlaybackInstance の生成・操作・PCM ミックスダウン |

### データ構造

```cpp
// src/SoundAsset.hpp
struct SoundAsset {
    std::vector<float> pcm;   // float32 インターリーブ PCM
    uint32_t sampleRate = 0;
    uint32_t channels   = 0;
    uint64_t frameCount = 0;
};

// src/Mixer.hpp (内部)
struct PlaybackInstance {
    uint64_t                    id;
    uint64_t                    assetHandle;
    double                      cursor;   // フラクショナル（ピッチ対応）
    float                       volume;
    float                       pitch;
    bool                        looping;
    bool                        paused;
    bool                        active;
    std::shared_ptr<SoundAsset> asset;    // PCM データへの参照カウント
};
```

---

## データフロー

### ロード

```
Audio("se.wav")
  └─ Loader::Load(path)
       └─ ma_decoder_init_file()  ← miniaudio が WAV/MP3 を自動判別
       └─ ma_decoder_read_pcm_frames()  ← f32, stereo, 44100Hz に変換
       └─ SoundAsset 生成
       └─ Repository::Register(asset)  → uint64_t handle
  └─ Audio::handle_ = handle
```

### 再生

```
audio.Play(volume, pitch, loop)
  └─ Repository::Get(handle_)  → shared_ptr<SoundAsset>
  └─ Mixer::CreateInstance(assetHandle, asset, volume, pitch, loop)
       └─ 空き PlaybackInstance を確保（固定配列、動的確保なし）
       └─ inst.asset = shared_ptr<SoundAsset>（PCM への参照を保持）
       └─ instanceId 返却
  └─ PlayHandle(instanceId) 返却
```

### 音声コールバック（AudioThread）

```
miniaudio callback (AudioThread)
  └─ Mixer::Mix(output, frameCount, channels)
       └─ instances_ を走査
       └─ 各 PlaybackInstance の inst.asset->pcm から PCM サンプル取得
            └─ cursor += pitch  （フラクショナル進行でピッチ制御）
            └─ ループ判定・終了判定
       └─ 加算合成 + 音量適用
       └─ [-1.0, 1.0] にクリッピング
       └─ output バッファへ書き込み
```

### 破棄

```
~Audio()
  └─ Mixer::StopByAsset(handle_)   ← 関連インスタンスを全停止・shared_ptr 解放
  └─ Repository::Unregister(handle_)  ← マップから削除
  ※ active な PlaybackInstance がなければ SoundAsset のメモリも解放
```

### Unload

```
Handle::Unload()
  └─ 読み込み中なら完了を待つ（完了後に古いデータが登録されるのを防ぐ）
  └─ Repository::Unregister(id)  ← 名前と id の登録を外す
  └─ Handle を未ロード状態に戻す（Play は no-op、IsValid は false、再 Load 可）
  ※ 同じ音を共有する他の Handle の Play も no-op になる
  ※ 再生中のインスタンスは shared_ptr で実体を持ち続け、最後まで鳴る
```

旧計画（future-plan.md 10.3）では Unload 後の Play で再ロードする案だったが、
メモリから読んだ音は元のバイト列を持たず再ロードできないため、再ロードはしない。

### Streaming（長い音）

ロードマップ Phase 3.5 の設計（1 再生 1 デコーダ + SPSC リングバッファ + refill ワーカー）を、
自作ミキサーより先に、今の `ma_engine` の上に custom `ma_data_source` として載せたもの。
公開 API は変えない。どちらで持つかは Loader が決め、ユーザーは意識しない。

```
Handle::Load(path) / Load(name, bytes)
  └─ Loader::Probe()  ← ma_decoder でヘッダと長さだけ読む（フルデコードしない）
  └─ LoadPolicy::Decide(meta)
       ├─ 10 秒以上 か デコード後 16MB 以上 → StreamingAsset（パスかエンコード済みバイト列 + AssetMeta だけを持つ）
       └─ それ以外 / 長さ不明             → 従来どおりフルデコードして Sound
  └─ Repository::Register(...)  ← 1 つの id に Sound か StreamingAsset のどちらか

Handle::Play()
  └─ Repository::FindStreaming(id) があれば Mixer::PlayStreaming()
       └─ StreamingVoice を作る（専用デコーダ + PcmRingBuffer 2.0 秒）
       └─ ma_sound_init_from_data_source(voice のデータソース) して開始
       └─ StreamRefiller::Add(voice)  ← 準備（デコーダを開いて 0.5 秒分）はワーカーがする

AudioThread（miniaudio が ma_sound 経由でデータソースを読む）
  └─ 準備前: 無音を返す（何も消費しないので頭は欠けない。鳴り始めがその分だけ遅れる）
  └─ 準備後: リングバッファから読むだけ。足りなければ残りを無音にして underrun を数える
  └─ 最後まで読み終わり（ループしない）、バッファも空 → MA_AT_END

StreamRefiller（ワーカー 1 本、10ms ごとに巡回）
  └─ 残りが 25% を切った再生に、0.5 秒ずつデコードして満たす
  └─ ループ中は終端でデコーダを先頭へ seek して続ける（バッファ内でつながるので継ぎ目なし）
```

| スレッド | すること | しないこと |
|---|---|---|
| AudioThread | `PcmRingBuffer::Read` と無音の書き込み | デコード、file IO、確保、ロック |
| refill ワーカー | デコーダの生成・seek・読み出し、`PcmRingBuffer::Write`、最後の参照を手放したときのデコーダ破棄 | `Mixer` に触る |
| API スレッド | `StreamingVoice` の作成、`ma_sound` の初期化と破棄 | デコード |

- `PcmRingBuffer` は書き手 1・読み手 1 の固定長バッファ。位置を増え続けるフレーム数で持ち、
  release で進めて acquire で読む。Phase 1 のコマンドキューにも同じ考え方を使える。
- ループと範囲はデータソース自身が扱う（`MA_DATA_SOURCE_SELF_MANAGED_RANGE_AND_LOOP_POINT`）。
  `SetLoop` は `ma_sound_set_looping` → `onSetLooping` で、ワーカーが見るフラグを切り替える。
- Seek は未対応（`MA_NOT_IMPLEMENTED`）。止めた Streaming 再生は再利用しない（静的な再生だけが先頭へ戻して再利用される）。
- 寿命: `PlaybackInstance` は `ma_sound` を外してから `StreamingVoice` を手放す。ワーカーは `weak_ptr` で持ち、
  巡回中だけ `shared_ptr` にする。Unload しても再生中のものは最後まで鳴る。
- Shutdown: 再生（`ma_sound`）を外す → ワーカーを止める（残ったデコーダはここで破棄）→ グループ → エンジン → Repository。
- 自作ミキサー（Phase 1）に移るときは、データソースの読み出し（`StreamingVoice` の Read）を
  Voice の「N フレーム読み出し」境界に差し替える。`PcmRingBuffer`、`StreamRefiller`、`LoadPolicy` はそのまま使える。

---

## スレッド安全性

| 操作 | 呼び出しスレッド | 保護手段 |
|---|---|---|
| `Audio::Play()` / `StopAll()` | メインスレッド | `Mixer::mutex_` |
| `PlayHandle::Stop()` 等 | メインスレッド | `Mixer::mutex_` |
| `Mixer::Mix()` | AudioThread (miniaudio) | `Mixer::mutex_` |
| `Repository::Get()` / `Unregister()` | メインスレッドのみ | ロック不要 |
| `inst.asset->pcm` アクセス | AudioThread | `shared_ptr` で lifetime 保証、PCM は不変 |

**重要:** `Mix()` は Repository に一切アクセスしない。
`PlaybackInstance::asset`（shared_ptr）が PCM データの lifetime を保証するため、
`~Audio()` が Repository から削除した後も、再生中のインスタンスは安全に動作する。

---

## 外部依存

| ライブラリ | 用途 | 配置 | 公開 API への露出 |
|---|---|---|---|
| `miniaudio.h` 0.11.25 | オーディオデバイス管理・WAV/MP3/FLAC デコード | `vendor/miniaudio.h`（ライセンスは `vendor/LICENSE.txt`） | なし（internal only） |
| `stb_vorbis.c` v1.22 | OGG Vorbis デコード（miniaudio から使う） | `vendor/stb_vorbis.c`（nothings/stb `1ee679c`、ライセンスはファイル末尾） | なし（internal only） |

`MINIAUDIO_IMPLEMENTATION` は `src/Backend.cpp` のみに定義。
miniaudio は実装より前に stb_vorbis の宣言があるときだけ OGG Vorbis を有効にする（`STB_VORBIS_INCLUDE_STB_VORBIS_H` を見る）。
0.11.25 の説明文にはこの組み込み方が書かれていないため、miniaudio を更新するときはこの判定が残っているか確かめる。
他の翻訳単位では宣言のみを参照する。

---

## 実装フェーズ

| Phase | 内容 | 状態 |
|---|---|---|
| **1** | miniaudio による WAV/MP3 オンメモリ再生・公開 API 確定 | ✅ 完了 |
| **2** | 独自バイナリフォーマット `.aab`（生 PCM、ループポイント内包）・WAV/MP3 → .aab 変換ツール | 未着手 |
| **3** | ストリーミング再生・オンメモリ/ストリーミング自動切り替え（閾値ベース） | 未着手 |
| **4** | プールアロケータ（PlaybackInstance ホットパス）・安定化・スレッド安全性全面レビュー | 未着手 |
| **5** | 3D オーディオ・エフェクトプラグイン（VST/CLAP）・追加バックエンド | 任意 |

---

## 拡張設計メモ

### 独自フォーマット `.aab`（Phase 2）

```
[Header 64 bytes]
  magic[4]      "AAB\0"
  version       uint16
  flags         uint16   bit0: ストリーミング推奨, bit1: ループポイントあり
  sampleRate    uint32
  channelCount  uint8
  bitDepth      uint8    (16 固定)
  sampleCount   uint64
  loopStart     uint64
  loopEnd       uint64
  reserved[16]

[PCMData]
  int16_t samples[channelCount * sampleCount]  インターリーブ
```

- **圧縮なし（生 PCM）**: デコードコストゼロ
- **ループポイント内包**: BGM のシームレスループをフォーマットレベルでサポート
- **ストリーミング推奨フラグ**: 変換時にファイルサイズで自動判定、ランタイムはフラグ参照のみ

### 3D オーディオ（Phase 5）

コアへの影響は最小。後付け可能。

- `PlayParams::worldPosition` （`std::optional<...>`）を追加
- `Audio::SetListenerPosition()` を追加
- `Mixer::Mix()` に距離減衰計算を追加
- 公開 API の破壊的変更なし

### エフェクト（Phase 5）

`Mixer` 出力段に `IEffect` チェーンを追加する形で後付け可能。

```cpp
// 将来の拡張イメージ
class IEffect {
public:
    virtual void Process(float* buffer, uint32_t frameCount, uint32_t channels) = 0;
    virtual ~IEffect() = default;
};
```
