# AudioLib ミドルウェア化ロードマップ

middleware.md のスコープ 1〜6 を、自作ミキサー路線(ma_engine 撤去)で実装するための確定タスク計画。

**このファイルはセッションをまたぐ進行管理の起点。** 各セッションの開始時にここを読み、
「進行状況」と「セッションログ」から現在地を把握してから作業に入ること。

---

## 進行状況

- **現在のフェーズ**: Phase 1(未着手)
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
- future-plan.md(非同期ロード / ストリーミング / ステップシーケンサ)は本計画の後。
  ストリーミングは自作ミキサーの「ボイスソースの一種」として後から挿す
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
| 4 | 自前エフェクト(EQ / ディレイ) | 5 | TrackHandle にエフェクト API 追加 |
| 5 | RTPC 最小版 | 4 | SetParameter 系追加 |
| 6 | JSON イベントシステム | 1 | PlayEvent 系追加 |
| 7 | 2 つ目の Sink(任意) | - | なし |
