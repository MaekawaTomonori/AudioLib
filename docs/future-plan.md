# Audio Library Future Plan

## 0. 前提

現行実装は `miniaudio` の `ma_engine` を使ったシンプルなインメモリ再生ライブラリで、主な性質は以下です。

- `Audio::Handle(path)` はコンストラクタで同期ロードする
- `Loader` はファイル全体を `float32 / stereo / 44100Hz` にデコードして `Sound` に保持する
- `Repository` は `path -> id -> Sound` を保持するがスレッドセーフではない
- `Mixer` は `ma_audio_buffer_ref` から再生しており、全再生は「全データがメモリにある」前提
- Track は `ma_sound_group` ベースで、バス/センド/エフェクト/メータはまだない
- Demo にはすでに DAW 風 UI の方向性があるが、内部モデルはまだ単発再生ライブラリのまま

このため、今後の機能追加は単発で足すより、`Asset`, `Playback`, `Transport`, `Editor` の責務を分けて進める必要があります。

## 1. 目標

優先度の高い目標は次の 3 本です。

1. Load の並列化
2. 大きい音声はストリーミング再生し、ロード方式を自動選択
3. DAW ライクなドラムロール / ピアノロールの追加

そのうえで、将来的に破綻しないために以下も同時に仕込むべきです。

- 非同期ロード状態を扱う API
- アセットメタデータとロードポリシー
- テンポ / 小節 / 再生位置を持つ Transport
- ノート列とオーディオクリップ列を扱える Timeline モデル
- 将来の bus / send / effect / automation を足せる拡張点

## 2. まず Grill すべき論点

この計画で最初に詰めるべき論点です。ここが曖昧なまま実装すると手戻りが大きいです。

### 2.1 非同期化の責務

- `Handle(path)` のコンストラクタ同期ロードは今後の中心 API にしない方がよい
- 「同期 API を残すか」「非同期 API を主役にするか」を決める必要がある
- UI / ゲーム本体がロード中状態を扱える設計に変える必要がある

推奨:

- 既存 `Handle(path)` は互換用に残す
- 新規に `LoadAsync` と `AssetHandle` 系の状態 API を追加する
- `Play()` は `Ready` でないと no-op ではなく、明示的に失敗理由を返せる方がよい

### 2.2 自動ロード方式の判定基準

「サイズが大きいものは streaming」だけだと不十分です。実際には以下が絡みます。

- ファイルサイズ
- デコード後メモリ量
- ループ有無
- seek 頻度
- 同時再生数
- フォーマット特性

推奨:

- 初期実装は `estimatedDecodedBytes` ベース
- 補助ヒントとして `AudioCategory` と `LoadHint` を許可
- 自動判定はできても、明示 override を必ず残す

### 2.3 ストリーミング対象

何を streaming にするかを先に制限する必要があります。

- BGM だけか
- Voice も含めるか
- ループ BGM は対応するか
- pitch/pan/seek を streaming でも同等サポートするか

推奨:

- Phase 1 の streaming 対象は BGM / 長尺 Voice
- pitch は `0.5x-2.0x` の範囲で対応、精度要件は緩める
- sample-accurate seek は初期スコープから外す

### 2.4 Piano Roll の定義

「ピアノロール」と言っても実装対象は幅があります。

- MIDI エディタか
- サンプラーシーケンサか
- 内蔵音源を持つのか
- オーディオクリップ編集まで入れるのか

推奨:

- 最初は `note event editor` と定義する
- 再生先は内蔵簡易 sampler / one-shot instrument に限定する
- 波形編集やタイムストレッチは後回し

## 3. 提案する全体アーキテクチャ

### 3.1 レイヤ分離

- `AssetSystem`: ファイル解析、ロード、デコード、キャッシュ、ストリーミング判定
- `PlaybackSystem`: one-shot 再生、stream 再生、voice 管理、track routing
- `TransportSystem`: tempo, bar/beat, playhead, loop range
- `SequencerSystem`: clip, note, pattern, arrangement
- `EditorModel`: piano roll, drum roll, selection, snap, quantize

### 3.2 アセット型の分離

`Sound` 1 種類では足りません。少なくとも以下に分けるべきです。

- `StaticAudioAsset`: 全 PCM を保持する
- `StreamingAudioAsset`: ファイルパス、デコーダ生成情報、総フレーム数、プリフェッチ設定を持つ
- `HybridAudioAsset`: 冒頭プリロード + 残り streaming の将来拡張

共通の基底メタデータ:

- `sampleRate`
- `channels`
- `durationFrames`
- `format`
- `category`
- `loadMode`
- `estimatedDecodedBytes`

### 3.3 再生インスタンス型の分離

- `StaticPlaybackVoice`
- `StreamingPlaybackVoice`
- `SequencedPlaybackVoice`

`Mixer` に直接全部寄せず、共通インターフェースで吸収した方がよいです。

例:

```cpp
struct IPlaybackVoice {
    virtual ~IPlaybackVoice() = default;
    virtual bool IsPlaying() const = 0;
    virtual void Stop() = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;
    virtual void SetVolume(float volume) = 0;
    virtual void SetPan(float pan) = 0;
    virtual void SetPitch(float pitch) = 0;
    virtual void SetLoop(bool loop) = 0;
};
```

## 4. 機能別仕様

## 4.1 Load の並列化

### 目的

- 大量アセット読込時の待ち時間短縮
- UI フリーズ回避
- BGM と SE の初期化を分離

### 要件

- 複数ファイルを同時にロードできる
- 同一パスの重複ロード要求は coalesce する
- 読込状態を観測できる
- 失敗理由を取得できる
- Cancel は後回しでよいが、状態遷移は cancel 可能な形にしておく

### 状態モデル

```cpp
enum class AssetState {
    Unloaded,
    Loading,
    Ready,
    Failed
};
```

必要 API 案:

```cpp
namespace Audio {
    struct LoadOptions {
        bool async = true;
        bool forceStreaming = false;
        bool forceStatic = false;
    };

    AssetHandle Load(std::string_view path, const LoadOptions& options = {});
    AssetHandle LoadAsync(std::string_view path, const LoadOptions& options = {});
    AssetState  GetAssetState(AssetHandle asset);
    bool        Wait(AssetHandle asset, uint32_t timeoutMs = UINT32_MAX);
}
```

### 実装方針

- `Repository` を `LoadedAssetRepository + PendingLoadRegistry` に分離
- `path -> shared_future<LoadResult>` を持ち、同一要求を統合
- まずは固定サイズ thread pool を内包する
- pool サイズは `min(4, hardware_concurrency)` を初期値

### 注意点

- `Repository` はスレッドセーフ化が必要
- `Handle` の move/copy semantics を再設計する必要がある
- `Shutdown` 時に load worker の drain が必要

### 受け入れ条件

- 100 個の SE をロードしても main thread がブロックしない
- 同一ファイルを 10 回 `LoadAsync` しても実デコードは 1 回
- 失敗パスでクラッシュしない

## 4.2 ロード方式の自動選択

### 新規 enum

```cpp
enum class LoadMode {
    Auto,
    Static,
    Streaming
};

enum class AudioCategory {
    Auto,
    Sfx,
    Bgm,
    Voice,
    Preview
};
```

### 自動判定ルール案

初期版は単純でよいです。

- `forceStatic == true` なら `Static`
- `forceStreaming == true` なら `Streaming`
- `category == Sfx` かつ `estimatedDecodedBytes <= 8 MB` なら `Static`
- `category == Bgm` かつ `duration >= 10 sec` なら `Streaming`
- それ以外は `estimatedDecodedBytes > 16 MB` で `Streaming`

将来版で追加する条件:

- ループ BGM 用に先頭プリフェッチ
- Voice は同時再生数に応じて static / streaming を切り替え
- platform memory budget を参照

### 必要メタデータ

ロード前に判定するため、軽量な probe が必要です。

- channels
- sampleRate
- totalFrames
- durationSec
- estimatedDecodedBytes

### 実装案

- `Loader::Probe(path)` を追加
- `Probe` はフル decode せずヘッダと duration を得る
- `Load` は `Probe -> Policy Decide -> StaticLoad or StreamingLoad`

### 受け入れ条件

- 短い SE は現行同様ほぼ即再生
- 長尺 BGM は全 PCM をメモリに載せず再生開始できる
- 判定理由を debug UI で確認できる

## 4.3 ストリーミング再生

### 初期スコープ

- 1 ファイル 1 再生デコーダ
- 読み込みはバックグラウンド
- 再生側はリングバッファ参照
- loop 対応あり
- pause / resume / stop 対応

### 非スコープ

- sample accurate seek
- reverse 再生
- time stretch
- granular / scrub

### 実装モデル

- `StreamingAudioAsset` は元ファイルパスとフォーマット情報を保持
- `StreamingPlaybackVoice` は専用 decoder と PCM ring buffer を持つ
- refill worker が閾値以下で非同期補充する

必要パラメータ:

- `prefetchFrames`
- `ringBufferFrames`
- `refillLowWatermark`
- `loopStartFrame`
- `loopEndFrame`

推奨初期値:

- prefetch: 0.5 sec
- ring buffer: 2.0 sec
- refill: 25%

### バックエンド選択

現行の `ma_engine + ma_sound` ベースを全面撤去する必要はまだありません。

段階案:

1. static 再生は既存 `ma_engine` 経路を維持
2. streaming だけ `ma_data_source` 実装を差し込む
3. 必要になったら全 voice を独自 mixer に寄せる

この方針なら導入コストが低いです。

### リスク

- `miniaudio` の data source 実装責務が増える
- thread safety と stop/shutdown race が出やすい
- looping と decoder 再初期化の境界がバグになりやすい

### 受け入れ条件

- 5 分以上の BGM を再生してもメモリ使用量が線形増加しない
- loop 有効時に無音区間や破裂音が出ない
- 停止と破棄を高速に繰り返してもクラッシュしない

## 4.4 DAW ライクなドラムロール

### 定義

ドラムロールは「固定の打楽器レーンに対して時刻付きイベントを置くエディタ」とする。

### データモデル

```cpp
struct DrumLane {
    std::string id;
    std::string displayName;
    AssetHandle sampleAsset;
    uint8_t     midiNote = 36;
};

struct DrumEvent {
    uint32_t tick;
    uint32_t lengthTick;
    uint8_t  velocity;
    uint32_t laneIndex;
};

struct DrumPattern {
    uint32_t ppq = 480;
    uint32_t lengthTick = 1920;
    std::vector<DrumLane>  lanes;
    std::vector<DrumEvent> events;
};
```

### 機能スコープ

- lane の追加削除
- grid snap
- note 追加/削除/移動/複製
- velocity 編集
- loop 再生
- pattern 長変更

### 先送り

- flam / ratchet
- probability
- humanize
- per-step automation

### 再生仕様

- `Transport` の tick に同期
- 先読み窓内のイベントをスケジュール
- SE 再生としてトリガーする
- 同レーンの voice 制限は設定可能にする

## 4.5 DAW ライクなピアノロール

### 定義

ピアノロールは「音高と長さを持つノートイベントエディタ」とする。

### 最小データモデル

```cpp
struct NoteEvent {
    uint32_t tick;
    uint32_t lengthTick;
    uint8_t  pitch;
    uint8_t  velocity;
};

struct NoteClip {
    uint32_t ppq = 480;
    uint32_t lengthTick = 1920;
    std::vector<NoteEvent> notes;
};
```

### 音源方針

最初に決めるべきです。ここを曖昧にするとロールだけ作っても鳴らせません。

推奨フェーズ:

1. 内蔵 sampler instrument
2. 単純な ADSR + pitch shift 再生
3. 将来 SoundFont / plugin host を検討

### 初期仕様

- 1 トラック 1 instrument
- 単音 / 和音対応
- snap, quantize, transpose
- velocity 編集
- audition 再生
- loop 範囲再生

### 明示的に先送りするもの

- automation lane
- CC 編集
- pitch bend
- time stretch
- external MIDI I/O

## 4.6 Transport / Timeline

ドラムロールとピアノロールを入れるなら必須です。

### 必要状態

- bpm
- time signature
- ppq
- playheadTick
- loopStartTick
- loopEndTick
- playing / paused / stopped

### API 案

```cpp
namespace Audio {
    struct TransportState {
        double bpm = 120.0;
        uint32_t numerator = 4;
        uint32_t denominator = 4;
        uint32_t ppq = 480;
        uint64_t playheadTick = 0;
        uint64_t loopStartTick = 0;
        uint64_t loopEndTick = 0;
        bool playing = false;
    };

    void SetTransport(const TransportState& state);
    TransportState GetTransport();
    void PlayTransport();
    void PauseTransport();
    void StopTransport();
}
```

### スケジューリング

- audio callback 内で今ブロックにかかる tick 範囲を計算
- 先読み窓でイベントを発行
- UI thread は transport 状態を観測するだけに寄せる

## 5. 追加提案

上記 3 本に加えて、後で確実に欲しくなるものを先に整理しておきます。

### 5.1 Debug / Profiler

- アセットごとの load mode 表示
- load queue 深さ
- streaming buffer fill 率
- voice 数
- track ごとの peak meter

これは開発効率に直結するので優先度は高いです。

### 5.2 Bus / Send / Effect Slot

DAW 的な拡張を考えると Track の次は Bus です。

最小仕様:

- `CreateBus`
- Track -> Bus routing
- Bus volume
- Effect slot 2 個

初期エフェクト候補:

- Gain
- LowPass
- Delay

### 5.3 Voice Limit / Priority

SE が増えると必要になります。

- category ごとの最大同時発音数
- priority
- oldest / quietest steal policy

### 5.4 Asset Manifest / Cook

将来を考えるとランタイムで毎回 probe/decode するより、ビルド時メタデータ生成が欲しいです。

- duration
- estimated size
- category
- loop point
- recommended load mode

これは streaming 自動判定の精度向上にも効きます。

## 6. 推奨ロードマップ

## Phase 1: 基盤整理

- `Repository` のスレッドセーフ化
- `AssetHandle` と `AssetState` 追加
- `Loader::Probe` 追加
- debug 情報の可視化

完了条件:

- 既存同期 API を壊さずに非同期 API を追加できる

## Phase 2: 並列 Load

- thread pool 実装
- pending registry 実装
- `LoadAsync` 実装
- wait / state / error 実装

完了条件:

- 重複ロード統合
- main thread 非ブロッキング

## Phase 3: 自動 Static / Streaming

- `LoadMode`, `AudioCategory`, `LoadOptions`
- policy 判定
- debug UI で判定理由表示

完了条件:

- 長尺ファイルが自動で streaming になる

## Phase 4: Streaming Playback

- `StreamingAudioAsset`
- `StreamingPlaybackVoice`
- ring buffer refill
- loop / pause / resume / shutdown race 対応

完了条件:

- BGM 実運用可能

## Phase 5: Transport / Sequencer

- transport state
- tick scheduler
- drum pattern model
- piano roll note model

完了条件:

- 再生ヘッドに同期してイベントが鳴る

## Phase 6: Editor UI

- drum roll UI
- piano roll UI
- snap / quantize / selection
- loop playback UI

完了条件:

- Demo 上で最低限の編集と再生ができる

## 7. 実装前に決めるべき仕様（解決済み）

以下はすべて合意済み。詳細は Section 10、Section 11 を参照。

1. ✅ 公開 API の中心は `Handle` のままにする（コンストラクタ方式を維持）
2. ✅ `Play()` 失敗時は no-op のまま。初回のみログを出す
3. ✅ streaming の初期実装は miniaudio ネイティブ（`MA_SOUND_FLAG_STREAM`）で始める
4. ✅ piano roll は今スコープから外し、Step Sequencer を先行する
5. ✅ timeline は pattern ベースから始める（arrangement は後回し）
6. ✅ editor 用 UI 状態と playback 用データは分離可能な形に設計する

## 8. 推奨する次の 1 スプリント（確定）

Phase 1 の実装順:

1. `Sound` の所有権を `unique_ptr` → `shared_ptr` に変更する（Unload 前提）
2. `Loader::Probe` と `AssetMeta` を追加する
3. `AssetState` と `AssetEntry` を追加する
4. `ThreadPool` を実装する
5. `Repository` をスレッドセーフ化し、pending registry を統合する
6. `Handle(path)` を非同期化する
7. `Handle::Unload()` を追加する
8. `LoadPolicy::Decide` を実装する（Phase 2）
9. streaming 再生を追加する（Phase 3）

理由:

- Sound の所有権修正は Unload 実装の前提であり、後から直すとコストが高い
- Probe とメタデータは並列 Load と streaming 自動判定の両方に必要
- ここを飛ばして streaming 実装に行くと API と内部構造を二度作ることになる

## 9. 結論

進め方としては「Load 並列化」と「自動 static/streaming 選択」を先に同じ基盤として作り、その上に `Transport + Sequencer + Roll Editor` を載せるのが最短です。

逆に、先に piano roll UI だけ作る進め方は避けた方がよいです。再生スケジューラと音源モデルが先にないと、見た目だけの機能になって作り直しが発生します。

## 10. 追加合意事項

対話で固まった事項を追記します。

### 10.1 ロードと Handle の方針

- 公開 API はシンプルさ優先で、ユーザーに `Loading/Ready/Failed` を意識させない
- `Handle(path)` は呼んだ時点で返るが、内部では非同期並列ロードへ自動で振り分ける
- 同一パスのロード要求は 1 回に統合し、2 回目以降にも同じ内部実体を参照する `Handle` を返す
- `Play()` がロード中に呼ばれても安全に無視し、初回のみログを出す
- ロード失敗時も `Handle` 自体は返し、ユーザーはログで確認する
- 状態 API は当面公開しない

### 10.2 自動 Static / Streaming

- 判定は完全自動寄りでよい
- 主目的は短期的には高速化、長期的にはメモリ効率
- `BGM` という名前ではなく、再生時間やメモリ効率などから streaming に適したものを自動選択する
- `SE` であっても大きいものは streaming に逃がしてよい
- override API は後回し

### 10.3 Unload と寿命

- `Handle::Unload()` を将来的に追加したい
- `Unload()` は他の `Handle` が同じ音源を指していても強制的に実体を落としてよい
- ただし再生中インスタンスは止めず、再生終了まで鳴らし切る
- `Unload()` 後の `Handle` に対する `Play()` は再ロードを試み、未完了なら無音で無視する
- キャッシュ解放ポリシーは後回し

### 10.4 Step Sequencer の初期仕様

- 初期対象は `FL Studio` の `channel rack + step sequencer` に近い体験
- piano roll は後回し
- 全体で 1 つの共通 16 step グリッドを持ち、音源チャンネルが縦に並ぶ
- 複数 pattern は持てるが、初期段階は 1 pattern loop 再生でよい
- 各チャンネルは 1 つの `Handle` を持つ
- 各 step は当面 `On/Off` のみ
- 編集は次の step から再生へ反映する
- 通過済み step の変更は次 loop から反映する
- メトロノームは sequencer 再生中のみ、音は固定でよい

### 10.5 Step Sequencer のチャンネル設定

- 初期設定は `Volume / Pan / Mute / CutSelf / Attack / Release`
- `Attack / Release` は当面 step sequencer 専用
- `CutSelf=true` のとき、新規 step 発火で直前の同チャンネル発音を切る
- `Release > 0` の場合は即停止ではなく release フェーズへ移る
- 同一チャンネルの連打は基本重ね鳴らし
- 再生中にチャンネル設定を変えた場合は次に鳴る音から反映する

### 10.6 詳細仕様の分離

実装仕様は次の 2 本に分離する。

- `docs/loading-streaming-spec.md`
- `docs/step-sequencer-spec.md`

### 10.7 Handle コンストラクタの方針（確定）

- `Handle(path)` はコンストラクタのままにする
- フロントエンド（ユーザー API）とバックエンド（ロード処理）は別物として扱う
- コンストラクタ内部で非同期ロードをキックオフするだけで、ブロックしない
- factory 関数への変更は行わない

### 10.8 Streaming 実装の段階方針（確定）

- Phase 3 では miniaudio ネイティブ streaming（`MA_SOUND_FLAG_STREAM`）を使う
- ring buffer + refill worker の独自実装は後のフェーズで乗せ換える
- 段階的に移行できるよう、streaming voice の実装は `StaticPlaybackInstance` と独立して持つ

### 10.9 Sound 所有権の修正（確定、Phase 1 で実施）

- 現行の `PlaybackInstance` は `Sound` の PCM を非所有参照で持つため、Unload 時に dangling になる
- `Repository::data_` を `shared_ptr<Sound>` に変更する
- `PlaybackInstance` が `shared_ptr<Sound>` を保持することで、Repository から消えても再生が続く
- この修正を Phase 1 の最初のステップとして実施する

### 10.10 Step Sequencer のタイミング方式（確定）

- `StepSequencerPlayer::Update(double deltaTimeSec)` をポーリング方式で実装する
- 呼び出しは Demo の毎フレームループから行う
- sample-accurate scheduler は後回し
- 専用スレッドは使わない（初期段階）

### 10.11 Step Sequencer のモジュール境界（確定）

- とりあえず `Demo/WithImGui/` 側に置く
- 依存は `Audio::Handle` と `Audio::PlaybackHandle` のみにする
- 将来のゲームランタイム利用を考慮し、playback データ構造と UI 状態は分離可能な形にしておく
- 独立ライブラリへの昇格は将来検討

### 10.12 Attack / Release の実装方針（確定）

- 初期は miniaudio の fade API（`ma_sound_set_fade_in_pcm_frames`）を使う
- そのために `PlaybackHandle::FadeIn(float targetVol, float durationSec)` と `FadeOut(float durationSec)` を追加する（Phase 4）
- 将来は独自 envelope 状態を `PlaybackInstance` に持たせる方式に移行する

### 10.13 メトロノーム音源（確定）

- 外部ファイル（`assets/metronome.wav` 等）を使う
- `Audio::Handle` として保持し、beat 発火時に `Play()` する

---

## 11. 確定実装プラン

### Phase 1: 基盤整理

**目的**: 非同期ロード・Unload・Probe の前提となる内部構造を整える。公開 API は変わらない。

#### 1-1. Sound 所有権の修正（最初に実施）

- `Repository::data_` を `unordered_map<uint64_t, shared_ptr<Sound>>` に変更
- `Mixer::Play()` のシグネチャを `const shared_ptr<Sound>&` 受け取りに変更
- `Mixer::PlaybackInstance` に `shared_ptr<Sound> asset` を追加
- これにより Repository から消えても再生インスタンスが生きる

#### 1-2. `Loader::Probe` と `AssetMeta` の追加

新規ファイル `src/AssetMeta.hpp`:

```cpp
struct AssetMeta {
    uint32_t sampleRate            = 0;
    uint32_t channels              = 0;
    uint64_t totalFrames           = 0;
    double   durationSec           = 0.0;
    uint64_t estimatedDecodedBytes = 0;
    bool     probeSucceeded        = false;
};
```

- `Loader::Probe(path) -> AssetMeta` を追加
- `ma_decoder` でヘッダのみ読んで `get_length_in_pcm_frames` → クローズ
- フルデコードは行わない

#### 1-3. `AssetState` と `AssetEntry` の追加

新規ファイル `src/AssetEntry.hpp`:

```cpp
enum class AssetState { Unloaded, Loading, Ready, Failed };

struct AssetEntry {
    std::string                    path;
    std::atomic<AssetState>        state{ AssetState::Unloaded };
    std::shared_ptr<Sound>         sound;   // Ready 時にセット
    AssetMeta                      meta;
    std::mutex                     mutex;   // sound の書き込み保護
};
```

#### 1-4. `ThreadPool` の追加

新規ファイル `src/ThreadPool.hpp`:

- スレッド数: `min(4, hardware_concurrency)`
- `std::queue<std::function<void()>>` + `std::mutex` + `std::condition_variable`
- `Shutdown()` で全ワーカーを drain してから終了

#### 1-5. `Repository` のスレッドセーフ化 + pending 統合

- `std::shared_mutex` を追加（読み取りは shared lock、書き込みは exclusive lock）
- `entryMap_: unordered_map<string, shared_ptr<AssetEntry>>` を追加
- `GetOrCreateEntry(path) -> shared_ptr<AssetEntry>` を追加
- `Handle` が持つ `uint64_t handle_` は AssetEntry の ID を指すよう統一

#### 1-6. `Handle(path)` の非同期化

```
Handle(path) の内部フロー:
1. entry = Repository::GetOrCreateEntry(path)
2. state == Ready   → handle_ = entry ID, return
3. state == Loading → handle_ = entry ID, return（同じ entry を共有）
4. else             → state = Loading, ThreadPool に投入:
     - Probe(path) → meta
     - LoadPolicy::Decide(meta) → StaticDecoded or StreamingPrepared
     - StaticLoad → Sound 生成 → entry->sound = sound, state = Ready
     - 失敗なら state = Failed, ログ出力（初回のみ）
```

- `Handle::Play()` で `entry->state != Ready` なら no-op + 初回のみ `std::cout` ログ

#### 1-7. `Handle::Unload()` の追加

公開 API に追加（`include/Handle.hpp`）:

```cpp
void Unload();
```

- Repository から entry の `sound` を null にし、state を `Unloaded` に変更
- 再生中 `PlaybackInstance` は `shared_ptr<Sound>` を保持しているので影響なし
- `Unload()` 後の `Play()` は再ロードをキューに積む（再度 Loading フローへ）

---

### Phase 2: 自動 Static/Streaming 判定

**目的**: Phase 1 の Probe 上に乗せる。ユーザー API は変わらない。**

#### 2-1. `LoadPolicy` の追加

新規ファイル `src/LoadPolicy.hpp`:

```cpp
struct LoadThresholds {
    double   streamingMinDurationSec  = 10.0;
    uint64_t streamingMinDecodedBytes = 16ull * 1024 * 1024; // 16 MB
};

AssetStorageKind Decide(const AssetMeta& meta, const LoadThresholds& thresholds = {});
```

判定ルール:

- `durationSec >= streamingMinDurationSec` または `estimatedDecodedBytes >= streamingMinDecodedBytes` → `StreamingPrepared`
- それ以外 → `StaticDecoded`

#### 2-2. ロードフローの分岐

- `StaticDecoded` → 既存 `ma_decode_file` フロー → `Sound`（全 PCM）
- `StreamingPrepared` → path + meta を保持するだけ（再生時に streaming 起動）

---

### Phase 3: Streaming 再生

**目的**: 長尺ファイルをメモリに全展開せず再生する。miniaudio ネイティブで始める。**

#### 3-1. `StreamingPlaybackInstance` の追加

- `ma_sound_init_from_file(engine, path, MA_SOUND_FLAG_STREAM, group, &sound)` で初期化
- `Pause / Resume / Stop / Loop / Volume / Pan / Mute` は既存 `ma_sound_*` API で対応
- `Pitch` は後回し

#### 3-2. Mixer の分岐

- `Mixer::Play()` が `AssetStorageKind` に応じて static path / streaming path を選択
- static path: 既存の `ma_audio_buffer_ref` フロー
- streaming path: `StreamingPlaybackInstance` を作成

---

### Phase 4: Step Sequencer

**目的**: FL Studio の channel rack + step sequencer に近い体験を Demo 上に実装する。**

#### 4-1. データ構造（`Demo/WithImGui/StepSequencer.hpp` 新規）

```cpp
struct StepChannel {
    std::string   name;
    Audio::Handle sound;
    float volume     = 1.0f;
    float pan        = 0.0f;
    bool  mute       = false;
    bool  cutSelf    = false;
    float attackSec  = 0.0f;
    float releaseSec = 0.0f;
};

struct StepPattern {
    std::string name;
    bool steps[64][16] = {};  // [channelIdx][stepIdx]
};

struct StepSequencerProject {
    std::vector<StepChannel> channels;
    std::vector<StepPattern> patterns;
    double bpm              = 120.0;
    bool   metronomeEnabled = false;
};
```

#### 4-2. `StepSequencerPlayer`

```cpp
class StepSequencerPlayer {
public:
    void SetProject(StepSequencerProject* project);
    void Play();
    void Stop();
    void Update(double deltaTimeSec);  // Demo の毎フレームから呼ぶ
};
```

タイミング計算:

- `stepIntervalSec = 60.0 / (bpm * 4)` （16th note）
- `accumTime_ += deltaTimeSec`、閾値を超えたら currentStep を進める
- `On` になっているチャンネルを発火

CutSelf + Attack / Release（初期: miniaudio fade API）:

- `cutSelf=true` → 直前の `PlaybackHandle::FadeOut(releaseSec)` → 後で Stop
- 新規発火 → `Handle::Play()` → `PlaybackHandle::FadeIn(volume, attackSec)`

#### 4-3. `PlaybackHandle` に FadeIn / FadeOut を追加（公開 API 変更）

```cpp
// include/PlaybackHandle.hpp
void FadeIn(float targetVolume, float durationSec) const;
void FadeOut(float durationSec) const;
```

内部では `ma_sound_set_fade_in_pcm_frames` を使用。

#### 4-4. メトロノーム

- `Audio::Handle metronomeSound_` として保持（外部 WAV ファイル）
- beat 発火時に `metronomeSound_.Play()`

#### 4-5. UI（既存 Demo に統合）

- Channel Rack: チャンネル名・volume / pan / mute / cutSelf / attack / release
- Step Grid: 16 step on/off トグル（色付き）
- Pattern 切り替えボタン
- BPM スライダー
- Play / Stop ボタン + メトロノーム on/off

---

### フェーズ別サマリー

| Phase | 内容 | 公開 API 変更 |
|-------|------|--------------|
| 1 | 基盤（所有権・Probe・非同期 Load・Unload・ThreadPool） | `Handle::Unload()` 追加 |
| 2 | 自動 Static / Streaming 判定 | なし（内部のみ） |
| 3 | Streaming 再生（miniaudio native） | なし |
| 4 | Step Sequencer（Demo 側） | `PlaybackHandle::FadeIn/FadeOut` 追加 |
---

## 12. Timeline / Seek Oriented Editor Direction

This library should remain capable of growing into a timeline-oriented editor, not only a playback helper.
The intended target includes:

- waveform preview on a timeline
- scrubbing / seek from arbitrary positions
- playback starting from the current playhead
- synchronized evaluation of UI, model state, and animation against the same timeline position
- future clip-based editing similar to lightweight video or motion editors

### 12.1 Core Rule

For this use case, audio playback time must not become the only source of truth.
`Transport` should be the canonical time authority, and audio / UI / model / animation should all evaluate from the same playhead.

Required implications:

- seek updates `Transport` first
- audio playback starts or repositions from the transport playhead
- waveform, clip view, keyframes, and preview widgets read the same transport state
- model pose / animation preview should be evaluated from timeline time, not from ad-hoc local timers

### 12.2 Required Capability Additions

The following items are required before the library can serve as the base of a video-editing-like tool:

1. stable `Transport` API with play, pause, stop, loop range, and playhead set/get
2. playback seek API for both static and streaming paths
3. waveform extraction / cache model for editor rendering
4. timeline clip model for audio clips and non-audio events
5. deterministic update path so editor preview and audio remain aligned after seek

### 12.3 Recommended Build Order

Recommended order for this direction:

1. finish async load + static/streaming policy
2. finish streaming playback
3. add seekable playback + transport playhead control
4. add waveform cache and clip/timeline data model
5. add editor-side synchronization for UI / model / animation preview

### 12.4 Scope Note

This means the project can become a foundation for a lightweight DAW-like or video-editing-like application,
but only if timeline synchronization is treated as a first-class system after the streaming/playback base is complete.
Building editor UI before transport-driven playback would likely cause rework.
