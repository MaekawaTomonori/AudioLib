# AudioLib ミドルウェア化ロードマップ

middleware.md のスコープ 1〜6 を、自作ミキサー路線(ma_engine 撤去)で実装するための確定タスク計画。

**このファイルはセッションをまたぐ進行管理の起点。** 各セッションの開始時にここを読み、
「進行状況」と「セッションログ」から現在地を把握してから作業に入ること。

---

## 進行状況

- **現在のフェーズ**: Phase 1(未着手)。Phase 3.5 のストリーミングの中核だけは、今の ma_engine の上に先に入っている(2026-10-28、下のログ)
- **次にやるタスク**: 1-1. Sink インターフェース設計(実装前に自作ミキサー全体の設計説明から入る)
- 各フェーズ内のチェックボックス `[ ]` / `[x]` が個別タスクの完了状態を表す

## 運用ルール(セッションの回し方)

開発者はこの実装を「完全に理解しながら」進めることを最優先とする。
コードを書くだけでなく、設計意図と動作原理の理解が目的。

1. **セッション開始**: この md を読み、進行状況とセッションログで現在地を確認する
2. **実装前**: そのタスクの設計意図・データフロー・なぜその設計なのかを先に説明し、
   開発者が理解してから実装に入る(理解確認なしで書き始めない)
3. **実装中**: 小さい単位で進め、各ステップで「何をしたか・なぜか」を説明する
4. **ビルド/動作確認**: 開発者が行う(CLAUDE.md の方針どおり Claude はビルドしない)
5. **セッション終了時**: チェックボックスを更新し、セッションログに
   「日付 / やったこと / 決定事項 / 次にやること / 未解決の疑問」を追記する

## セッションログ

| 日付 | やったこと | 決定事項・メモ | 次にやること |
|------|-----------|--------------|-------------|
| 2026-07-02 | 現状分析・ギャップ分析・本ロードマップ策定 | 自作ミキサー路線を採用。XAudio2 は移行ではなく将来の 2 つ目 sink 候補。コマンドキューは Phase 1 に必須統合 | Phase 0: Sound の shared_ptr 化 |
| 2026-07-02 | **Phase 0 完了**(開発者が実装、Claude がレビュー2巡)。ビルド・Demo 動作確認済み | ReferencePtr(GESTD)は検出のみで延命不可のため不採用。shared_ptr は src/ 内部限定。学習ポイント: データ(Sound)と再生状態(PlaybackInstance)の分離、値渡し+move の所有権受け渡し、`const shared_ptr<T>` と `shared_ptr<const T>` の違い、非 const 左辺値参照は右辺値を束縛できない | Phase 1: 自作ミキサー(まず全体設計の説明から。unique_ptr+破棄遅延方式への置換もここで再検討) |
| 2026-07-06 | BGM ストリーミングを **Phase 3.5** として本ロードマップに統合。future-plan.md を旧計画と位置づけ、冒頭に廃止バナーを追加 | `MA_SOUND_FLAG_STREAM` 案は ma_sound 撤去により廃止。future-plan 4.3 の独自実装(1 再生 1 デコーダ + SPSC リングバッファ + refill ワーカー)を採用。Phase 1 の Voice 設計で「N フレーム読み出し」境界を 1 箇所に閉じ込めておく(1-2 にタスク追加済み) | Phase 1-1: Sink インターフェース設計(自作ミキサー全体の設計説明から入る) |
| 2026-10-28 | **Phase 3.5 の一部を先に実装**(NexelClient の要望。Claude が実装、開発者の承認済みの設計で)。Probe / LoadPolicy / StreamingAsset / Repository の 2 型対応(3.5-1, 3.5-2)、PcmRingBuffer(自作 SPSC)・StreamingVoice・StreamRefiller(3.5-3〜3.5-5 相当)を、今の ma_engine の上に custom ma_data_source として載せた。公開 API は不変 | MA_SOUND_FLAG_STREAM は使っていない(廃止の判断はそのまま)。準備はワーカー、準備前は無音を返して頭を欠かさない。pitch は当面 ma_sound のリサンプラ任せ(自作ミキサーで線形補間に置き換える)。Seek は未対応。確認(リポジトリ外の確認プログラム、35 項目): リングバッファの順序(2 スレッド 200 万フレーム)、判定と Probe、フルデコードと同じサンプル列、ループの継ぎ目、壊れたデータで止まらず終わる、346 秒の ogg を再生中のメモリ増 +4.7MB、再生・一時停止・停止 300 回、再生中の Unload、Shutdown。Demo(Console / WithImGui)のビルドと Console の再生 | 開発者による動作確認。Phase 1 で読み出し元を自作ミキサーの Voice 境界に差し替える。3.5-6(デバッグ表示)は Phase 3 の後 |
| 2026-10-29 | **Ears（左右の耳ごとの音量と遅れ）を追加**（NexelClient の BetterSound、Issue #99 の最初の部品。Claude が実装、開発者の承認済みの設計で）。公開: `include/Ears.hpp`、`Handle::Play(volume, pitch, pan, ears, loop, track)`（鳴らす前に Ear をはさむ）、`PlaybackHandle::SetEars`。今ある関数は不変。内部: `src/Ear.cpp` の自作ノード（ma_node_base を先頭に持つ）を ma_sound と行き先（Track か出口）の間にはさむ。左右 256 サンプルのリングバッファ、1 サンプル未満の遅れは線形補間、値は atomic、1 回の処理の間に線形に近づける、Restart でバッファを 0 にして近づけずに始める | Ears を使った再生にだけはさむ（使わない再生は今と同じ処理）。遅れは 1 ms まで、負の音量は 0、NaN は既定値。音が止まっている間はノードを処理しない（1 ms 以下の残りは次の Restart で消す）。鳴っている途中の最初の SetEars は付け替えで 10 ms ほど欠けることがある。なお既存の Play も、音量・pan は鳴り始めの後に設定している（変えていない）。確認（リポジトリ外の確認プログラム、19 項目、デバイスなしのエンジンを手で読む）: 既定の Ear で 1 サンプルも変わらない、遅れと音量（半端な遅れは 2 つに分かれる）、1 ms で止まる・壊れた値、音量の変化がなめらか、遅れを動かしてもクリックしない、Restart で残りが消える、実デバイスで Play・SetEars・再利用・200 回・再生中の Shutdown。Demo のビルドはしていない | 開発者による確認（Demo のビルド）。次: Ears に左右のローパスを足す（後ろの音、頭の陰、壁越し）。自作ミキサー（Phase 1）では Voice の後段に同じ処理を置く |
| 2026-10-29 | **Ears に左右のローパスを追加**（BetterSound、Issue #99 の 2 つ目の部品。Claude が実装、開発者の承認済みの設計で）。公開: `Ears::leftCutoff` / `rightCutoff`（Hz、既定 `Ears::noCutoff` = 20000 で削らない）。今ある項目・関数は不変。内部: 左右に RBJ Audio EQ Cookbook の 2 次ローパス（Q 0.707、転置直接形 II）を自作 | **係数は音のスレッドで作る**（Phase 4 の「API スレッドで計算して結果だけ渡す」とは違う。理由: 渡すのが 1 つの数で済み、5 つの係数が途中で混ざる心配がない。sin/cos はカットオフが動いている間だけ 8 サンプルごと）。カットオフは 2 を底とする対数で近づける。32 サンプルごとでは 10 ms で 5 オクターブ動かすと段差が出たので 8 に。鳴っている途中で削り始めるとき、20 kHz でもフィルタは素通しと位相が少し違って段差になったので、その回の間に素の音から混ぜ替え、次の Restart まで削り続ける。削り始めの状態は直前の値が続いていたものとして作る（Prime）。確認（28 項目）: 1000 Hz で 250 Hz −0.02 dB、1000 Hz −3.01 dB、4000 Hz −24.5 dB、削らない耳は 1 サンプルも変わらない、削り始めと 20 kHz→500 Hz の動きで入力より大きな段差なし、1 Hz・−50 Hz・NaN・1e9 Hz で壊れない。Demo のビルドはしていない | 開発者による確認（Demo のビルド）。次: 響き（Reverb）の説明と設計 |

## 決定事項(経緯)

- `ma_engine` / `ma_sound` / `ma_sound_group` を撤去し、`ma_device` コールバック内で自作ミキサーが全ボイスを合成する
- miniaudio は「部品」として使い続ける: デバイス I/O(`ma_device`)とデコード(`ma_decoder`、mp3 は dr_mp3 内蔵のため Media Foundation 不要)
- バックエンドは sink インターフェースで抽象化する。実装は当面 miniaudio のみ。
  XAudio2 / WASAPI は将来の 2 つ目として任意(機能メリットはなく、抽象境界の証明と経歴価値が目的)
- 公開 API(`Handle` / `PlaybackHandle` / `TrackHandle` / `Result`)は変更しない。内部差し替えのみ(OCP)
- `shared_ptr` は `src/` 内部に閉じ込め、公開ヘッダには一切出さない。
  ユーザーは `uint64_t` ベースのハンドルのみを扱い、寿命管理には参加させない
  (Handle のコピー = uint64_t のコピー。参照カウントはユーザー操作では動かない)。
  代替案として検討した `ReferencePtr`(検出のみ・延命不可・TOCTOU レースあり)は不採用。
  Phase 1 以降で「unique_ptr + 破棄遅延(ボイス走査 + ガベージキュー)」方式への置換を再検討する
- PCM は現行どおり f32 / stereo / 44.1kHz に正規化して保持する(ミキサー内のフォーマット変換を排除する簡略化)
- **future-plan.md は旧計画(ma_engine 前提)であり、本ロードマップが優先する。**
  特に future-plan 10.8 / 11章 Phase 3 の「miniaudio ネイティブ streaming(`MA_SOUND_FLAG_STREAM`)で始める」案は、
  `ma_sound` の撤去により**廃止**。ストリーミングは future-plan 4.3 の独自実装モデル
  (専用デコーダ + リングバッファ + refill ワーカー)を自作ミキサーの「ボイスソースの一種」として
  Phase 3.5 で実装する。非同期ロード / ステップシーケンサは引き続き本計画の後
- future-plan 10.12 の fade は `ma_sound_set_fade` 前提だったが、自作ミキサーでは自前エンベロープ(Phase 2 のスチールフェードと同じ仕組み)で実現する

## 依存関係の要点

- コマンドキュー(スコープ 3)は Phase 1 に含める。
  自作ミキサー化した瞬間、オーディオスレッドがボイス配列を所有するため、
  API スレッドからの直接変更は即データレースになる。「後で足す」は不可能
- ボイスプールは Phase 1 で固定長事前確保にする(コールバック内 alloc 禁止)。
  これがそのまま Phase 2 の同時発音上限の土台になる
- 現行 Mixer の「自然終了ボイスが回収されないバグ」は Phase 1 の書き直しで消滅する
  (自作ミキサーは再生位置を自分で進めるので終了検知が自明になる)

---

## Phase 0: 土台修正(書き直し後も生きる部分のみ)

**目的**: Sound の所有権問題を先に解消する。future-plan 1-1 と同一。

- [x] `Repository::data_` を `unordered_map<uint64_t, shared_ptr<Sound>>` に変更
- [x] `Mixer::Play()`(→ Phase 1 で新ミキサーの Play)へ `shared_ptr<Sound>` を渡す形に変更
- [x] 再生インスタンスが `shared_ptr<Sound>` を保持し、Repository から消えても鳴らし切れることを確認

**完了条件**: 既存 Demo が現状どおり動く。 → **✅ 2026-07-02 完了**(開発者によるビルド・Demo 動作確認済み)

**実装メモ(2026-07-02)**:
- `Repository::Find` は `shared_ptr<const Sound>` を値返し(所有権のコピー=生存保証の受け渡し)。
  `data_` 自体は所有者として非 const の `shared_ptr<Sound>` のまま
- `Mixer::Play` は `shared_ptr<const Sound>` の**値渡し**(呼び出し側が move すればカウント操作ゼロ)。
  参照渡し `&` は右辺値を束縛できずコンパイル不可、`const` 値渡しは move 不可、が理由
- `PlaybackInstance::asset` が延命の本体。再利用パスはコピー代入(後続の新規パスで使う可能性があるため)、
  新規パスは move 代入し、以降のフレーム数等の参照は `instance->asset->` から取る
- Boy Scout: `Sound.hpp` の未使用 `typedef Data` を削除、所有コメントを実態に更新

---

## Phase 1: 自作ミキサーコア(旧 Phase 0.5)+ コマンドキュー(スコープ 3)

**目的**: ma_engine を撤去し、`ma_device` コールバック内で自分のコードが全ミキシングを行う。
面接チェックリスト「再生指示が音になるまでのデータフロー」「コールバック内 lock/alloc 禁止の理由」がここで実体験になる。

### 1-1. Sink インターフェース

- [ ] `src/AudioSink.hpp`: デバイス抽象(Open(format, callback) / Close / 実行中コールバックでフレーム要求)
- [ ] `src/MiniaudioSink.cpp`: `ma_device` による実装(f32 / stereo / 44.1kHz 固定)

### 1-2. ボイスとミキシングループ

- [ ] `Voice` 構造体: `shared_ptr<Sound>`、再生位置(double、ピッチ用に小数)、
      volume / pan / pitch / loop / mute / state(Free / Playing / Paused / Stopping)
- [ ] ボイスプール: 固定長配列(初期 64)。コールバック内で new しない
- [ ] ミキシングループ: 全 Playing ボイスを合成 → Track バス → Master
  - ピッチ: 線形補間リサンプラ自作(位置を pitch 刻みで進める)
  - パン: stereo ソースなので balance 方式(定数パワー則はエフェクト以降の改善課題としてよい)
  - ループ: 終端で位置を巻き戻す。非ループは終端で state = Free
- [ ] 「再生位置から N フレーム読み出す」処理を Voice 内の 1 箇所に閉じ込める
      (Phase 3.5 でストリーミングソースに差し替えられる境界にする。抽象化の先取り実装は不要、境界を汚さないだけでよい)
- [ ] Track: 自作バス(中間ミックスバッファ + volume / pan / pitch)。Track → Master の 2 段構成

### 1-3. コマンドキュー

- [ ] SPSC リングバッファ(固定長、POD コマンド)。API スレッド = producer、オーディオスレッド = consumer
- [ ] コマンド: Play / Stop / StopAllOfSound / StopAllInTrack / Pause / Resume /
      SetVolume / SetPan / SetPitch / SetLoop / Mute / Track 系 / SetMasterVolume
- [ ] 再生 ID は API スレッド側で採番し、enqueue 前に `PlaybackHandle` として即返す
- [ ] `IsPlaying` 等の読み取りは、ボイススロットの atomic な状態フィールドを API スレッドから参照する
- [ ] コールバック先頭でキューを消費 → ミキシング、の順序を固定

### 1-4. 接続と退行確認

- [ ] `Handle` / `PlaybackHandle` / `TrackHandle` / `AudioLib.cpp` を新ミキサーに接続
- [ ] Shutdown 手順: デバイス停止 → ボイス解放 → Repository::Clear の順序を保証
- [ ] Demo(Console / WithImGui)で退行確認:
      play / stop / pause / resume / volume / pan / pitch / loop / mute / track 再生 / StopAll / master volume

**完了条件**:
- 既存 Demo の全操作が同等に動く
- オーディオコールバック内に new / delete / mutex lock / file IO が存在しない
- 旧 Mixer(ma_engine 系)のコードが削除されている

---

## Phase 2: ボイス管理(スコープ 2)

**目的**: 同時発音上限、優先度、ノイズを出さないボイススチール。

- [ ] `Play()` 系に priority(内部パラメータ。公開 API は既定値で不変)を通す経路を追加
- [ ] プール満杯時のスチールポリシー: 最低 priority → 同値なら最古、を犠牲に選ぶ
- [ ] スチール時は即切りせず短いフェードアウト(5〜20ms のランプ)後に解放(クリックノイズ防止)
- [ ] フェード用の簡易エンベロープをボイスに追加(後の FadeIn/FadeOut・Attack/Release にも流用)
- [ ] 同一 Sound の発音数上限(任意。カテゴリ別上限は将来)

**完了条件**: 上限を超えて連射しても音が破綻せず、スチール時にクリックノイズが出ない。

---

## Phase 3: メータリング + ImGui デバッグパネル(スコープ 6)

**目的**: Demo の偽 VU(`SimulateVU`)を実データに差し替え、以降の開発の可視化基盤を得る。

- [ ] ミキシングループ内で Track / Master ごとにピークと RMS を計算
- [ ] オーディオスレッド → UI スレッドへ atomic(または triple buffer)で公開。ロック不使用
- [ ] Core に統計 API を追加(ImGui 依存は入れない):
      `Audio::Debug::GetTrackMeter(track)` / `GetMasterMeter()` / `GetActiveVoices()`(スナップショット)
- [ ] Demo/WithImGui: `SimulateVU` を削除し実メーター表示、発音中ボイス一覧パネルを追加

**完了条件**: 再生中の音量変化がメーターに追従し、ボイス一覧でスチール挙動が目視確認できる。

---

## Phase 3.5: BGM ストリーミング再生

**目的**: 長尺 BGM をデコード後 PCM 全量をメモリに置かずに再生する。
future-plan 4.3 の独自実装モデルを自作ミキサーのボイスソースとして実装する
(future-plan 10.8 の `MA_SOUND_FLAG_STREAM` 案は ma_sound 撤去により廃止済み)。

**前提**: Phase 1〜3 完了。
- Phase 1: ストリーミングボイスはボイスプール・コマンドキューの上に載る
- Phase 2: スチール時の後始末(デコーダ破棄)をストリーミングにも適用するため
- Phase 3: バッファ充填率・underrun をデバッグパネルで目視できないと診断が困難なため

**設計原則(実装前に必ず説明すること)**:
- オーディオスレッドはリングバッファから**読むだけ**。デコード・file IO・refill は全てワーカースレッド
- デコーダ(`ma_decoder`)の生成・seek・破棄はオーディオスレッドで行わない
- 静的ボイスとの違いは「N フレームの読み出し元」だけ。ミキシングループ側は差を知らない

### 3.5-1. Probe と自動判定

- [x] `src/AssetMeta.hpp`: sampleRate / channels / totalFrames / durationSec / estimatedDecodedBytes
- [x] `Loader::Probe(path)`: `ma_decoder` でヘッダ + `ma_decoder_get_length_in_pcm_frames` のみ。フルデコードしない(メモリ版も)
- [x] `src/LoadPolicy.hpp`: `Decide(meta)` — durationSec >= 10.0 または estimatedDecodedBytes >= 16MB → Streaming、それ以外 → Static。閾値は定数で持ち、明示 override API は後回し(YAGNI)

### 3.5-2. アセット型の分離

- [x] ストリーミングアセット: path + AssetMeta のみ保持(PCM を持たない)。メモリから読んだ音はエンコード済みバイト列を持つ(`StreamingAsset`)。
      既存 `Sound`(静的、全 PCM 保持)と並ぶ 2 つ目のアセット型
- [x] Repository が両アセット型を扱えるようにする(公開 API の `Handle` は不変。ユーザーはロード方式を意識しない)

### 3.5-3. ボイスソースの差し替え

- [ ] Phase 1 で閉じ込めた「N フレーム読み出し」境界に、静的 PCM / ストリーミングの 2 実装を用意
- [x] ストリーミング側の読み出し元: SPSC リングバッファ(f32 / stereo / 44.1kHz 固定、容量 2.0 秒)
- [x] 枯渇(underrun)時は無音を返し、underrun カウンタを増やす(ノイズ・クラッシュ禁止)
- [ ] pitch は静的ボイスと同じ線形補間リサンプラを通す(リングバッファ読み出しは順方向のみなので成立する。
      pitch > 1.0 は消費が速くなるため低水位判定に効く点に注意)

### 3.5-4. refill ワーカー

- [x] ストリーミングボイスごとに専用 `ma_decoder` を保持(1 再生 1 デコーダ)
- [x] refill スレッド: 充填率が低水位(25%)を下回ったら prefetch 単位(0.5 秒)でデコードして書き込む
- [x] 再生開始時: prefetch 完了(0.5 秒分)を待ってからボイスを Playing にする(頭欠け防止。ma_engine 上では、準備前は何も消費せず無音を返す)
- [x] ループ: デコーダが終端に達したら `ma_decoder_seek_to_pcm_frame(0)` して継続充填
      (リングバッファ内でシームレスにつながるため、境界の無音・破裂音が出ない)
- [x] 非ループ: 終端到達をワーカーが記録し、リングバッファが空になった時点でボイス終了

### 3.5-5. 寿命とレース対応

- [x] Stop / 自然終了 / スチール時のデコーダ破棄はオーディオスレッドで行わず、
      ガベージキュー経由でワーカー(または API スレッド)が行う
- [x] Shutdown 順序: デバイス停止 → refill ワーカー停止 → デコーダ破棄 → Repository::Clear(ma_engine 上では、ma_sound を外す → ワーカー停止 → グループ → エンジン → Repository)
- [x] pause / resume / stop / 破棄の高速反復でクラッシュ・リークしないこと(退行確認項目)

### 3.5-6. デバッグ可視化

- [ ] Phase 3 のボイス一覧パネルに、ストリーミングボイスのバッファ充填率と underrun 累計を追加
- [ ] ロード方式(Static / Streaming)と判定理由(duration / size)をパネルで確認できるようにする

**完了条件**:
- 5 分以上の BGM を再生してもメモリ使用量が再生時間に対して線形増加しない
- ループ有効時に境界で無音・破裂音が出ない
- オーディオコールバック内に file IO / デコード / alloc / lock がない状態を維持している
- 停止と破棄を高速に繰り返してもクラッシュしない
- 短い SE は従来どおり Static 判定され、挙動が変わらない(退行なし)

---

## Phase 4: 自前エフェクト(スコープ 5)

**目的**: Track インサートに自作 DSP を 2 つ。

- [ ] Track に EffectChain(固定スロット 2)を追加。処理は Track バッファに in-place 適用
- [ ] バイクアッド EQ: RBJ Audio EQ Cookbook の係数計算を自作(LPF / HPF、cutoff / Q)
- [ ] ディレイ: フィードバック付きディレイライン自作(time / feedback / mix)。
      バッファはエフェクト装着時(API スレッド)に確保し、コールバック内では確保しない
- [ ] パラメータ変更はコマンドキュー経由。係数再計算は API スレッド側で行い、結果だけ渡す
- [ ] 公開 API: `TrackHandle` にエフェクト装着 / パラメータ設定を追加(既存関数は不変)

**完了条件**: Demo 上で LPF の cutoff を動かして聴感変化が確認でき、パラメータ変更時にノイズが出ない。

---

## Phase 5: RTPC 最小版(スコープ 4)

**目的**: float パラメータ 1 本で Track ボリュームがカーブ追従する。

- [ ] パラメータレジストリ(名前 → float)
- [ ] 線形区分カーブ(点列 → 補間)
- [ ] バインド: パラメータ → カーブ → Track volume(コマンドキュー経由で反映)
- [ ] 公開 API 案: `Audio::SetParameter("name", value)` + Track へのバインド登録

**完了条件**: Demo でスライダー 1 本を動かすと Track 音量がカーブに沿って変化する。

---

## Phase 6: イベントシステム(スコープ 1)

**目的**: JSON でイベント定義し `PlayEvent("名前")` で発火。データとコードの分離。

- [ ] `vendor/` に nlohmann/json(シングルヘッダ)を追加
- [ ] イベント定義: ファイル(複数ならランダム選択)/ Track 割当 / volume・pitch のランダム幅 / priority
- [ ] `Audio::LoadEventBank(jsonPath)` / `Audio::PlayEvent(name)` を追加
- [ ] 未定義イベント名は安全に no-op + 初回のみログ(Defensive Programming)
- [ ] Demo にイベント発火ボタンとバンク再読込を追加

**完了条件**: JSON 編集 → 再読込 → 発音変化、がコード変更なしで回る。

---

## Phase 7(任意): 2 つ目の Sink

**目的**: 抽象境界の実証。学習重視なら WASAPI 直、経歴キーワード重視なら XAudio2。

- [ ] `XAudio2Sink`(ミックス済みバッファを 1 本のソースボイスに submit)または `WasapiSink`
- [ ] コンパイル時 or 初期化時にバックエンド選択

**完了条件**: どちらの sink でも Demo が同一に動く(コアのコード変更ゼロ)。

---

## フェーズ別サマリー

| Phase | 内容 | スコープ対応 | 公開 API 変更 |
|-------|------|------------|--------------|
| 0 | Sound 所有権 shared_ptr 化 | (future-plan 1-1) | なし |
| 1 | 自作ミキサー + Sink 抽象 + コマンドキュー | 3 | なし |
| 2 | ボイス上限・優先度・スチール | 2 | なし |
| 3 | メーター + ImGui パネル | 6 | Debug 統計 API 追加 |
| 3.5 | BGM ストリーミング再生 | (future-plan 4.3) | なし |
| 4 | 自前エフェクト(EQ / ディレイ) | 5 | TrackHandle にエフェクト API 追加 |
| 5 | RTPC 最小版 | 4 | SetParameter 系追加 |
| 6 | JSON イベントシステム | 1 | PlayEvent 系追加 |
| 7 | 2 つ目の Sink(任意) | - | なし |
