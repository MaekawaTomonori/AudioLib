# AudioLib Design Overview

## 概要

AudioLib は **独立した音声ライブラリ**として設計する。
主な目的は以下の2点である。

1. **役割を明確に分離すること**
2. **ユーザーフレンドリで安全に使えること**

ユーザーは内部実装を意識する必要なく、直感的な API だけで音声を扱えることを目指す。
また多少不正確な操作があっても **アプリケーションを停止させず安全に動作を継続する**設計を採用する。

---

# 設計思想

## 1. ユーザーフレンドリ

ユーザーは以下のようなシンプルなコードで音声を扱える。

```
Audio::Init();
auto se = Audio::Load("sample.wav");
se.Play();
```

ユーザーは次のような内部要素を意識する必要はない。

* AudioThread
* Mixer
* Repository
* CommandQueue
* PlaybackInstance

これらはすべてライブラリ内部に隠蔽される。

---

## 2. 安全性

AudioLib は **失敗してもアプリケーションを落とさない設計**を採用する。

### 方針

* 失敗しても安全に動作を継続する
* 無効な操作は何もせず終了する
* 将来的に Logger により通知可能にする
* ユーザーコードを壊さない

### 例

| 状況                    | 挙動                 |
| --------------------- | ------------------ |
| 音声ロード失敗               | 無効 SoundHandle を返す |
| 無効 SoundHandle で Play | 何も再生しない            |
| 無効 PlaybackHandle 操作  | 何もしない              |
| 不正パラメータ               | clamp 等で補正         |

この方針は **no-op (何もしない安全動作)** によって実現する。

---

## 3. 責務分離

AudioLib は以下の役割を明確に分離する。

| コンポーネント       | 役割                |
| ------------- | ----------------- |
| Loader        | 音声ファイルの読み込み       |
| Repository    | 音源実体の一意管理         |
| Mixer         | 再生インスタンス管理と最終ミックス |
| AudioThread   | 非同期処理とコマンド実行      |
| Handle        | ユーザー操作窓口          |
| Facade(Audio) | ライブラリ入口           |

---

# システム構成

## Audio (Facade)

ユーザーが直接使用する入口。

### 役割

* ライブラリ初期化
* 音声ロード
* システム管理

### 例

```
Audio::Init();
auto se = Audio::Load("sample.wav");
```

---

## Loader

音声ファイルを読み込むコンポーネント。

### 役割

* ファイルロード
* フォーマット処理
* AudioData生成

### 非責務

* 音源管理
* 再生管理

---

## Repository

音源実体を管理する。

### 役割

* AudioDataの一意管理
* 音源キャッシュ
* SoundHandleからの参照解決

### 所有

Repository が **音源データを所有する**

---

## Mixer

再生インスタンス管理と最終ミックスを担当する。

### 役割

* PlaybackInstance生成
* 再生状態管理
* 音量/ピッチ反映
* 最終ミックス

### 管理対象

PlaybackInstance

### 非責務

* 音声ロード
* 音源管理

---

## AudioThread

AudioLib内部の処理を行うスレッド。

### 役割

* CommandQueue処理
* Mixer更新
* 非同期処理

### 特徴

ユーザーは **Update() を呼ぶ必要がない**

---

# CommandQueue

ユーザー操作は直接処理されず **Queue に積まれる**。

### 処理方式

* 命令は **順序付き**
* 一定周期でまとめて処理
* 即時反映は不要

### コマンド種類

再生生成

```
Play(audioId, params)
```

再生操作

```
Stop(playbackId)
Pause(playbackId)
Resume(playbackId)
SetVolume(playbackId)
SetPitch(playbackId)
```

---

# Handle設計

ユーザーには **Handleベースの操作窓口**のみを公開する。

## SoundHandle

音源に対するアクセス窓口。

### 特徴

* 音源実体は持たない
* 所有権なし
* 軽量

### 操作

```
Play()
Play(params)
```

---

## PlaybackHandle

再生インスタンスの操作窓口。

### 操作

```
Stop()
Pause()
Resume()
SetVolume()
SetPitch()
```

### 使用例

```
auto se = Audio::Load("sample.wav");

se.Play();

auto p = se.Play();
p.Pause();
p.Resume();
p.Stop();
```

戻り値は無視しても問題ない。

---

# PlayParams

再生開始時パラメータ。

### 例

```
se.Play(PlayParams{}.Volume(0.5f).Loop(true));
```

### 目的

* 再生開始時設定
* APIの直感性向上

---

# Handle設計方針

Handleは **軽量アクセス窓口**とする。

### 特徴

* 実体を持たない
* 所有権なし
* ユーザーに生ポインタを公開しない

### 内部安全性

内部では過剰なラッパを作らない。

安全性は

* 責務分離
* 所有関係
* スレッド境界

によって担保する。

---

# 所有関係

| コンポーネント     | 所有               |
| ----------- | ---------------- |
| Repository  | AudioData        |
| Mixer       | PlaybackInstance |
| Loader      | 所有しない            |
| Handle      | 所有しない            |
| AudioThread | 実行のみ             |

---

# Logger

Loggerは将来実装予定。

現段階では以下のイベントを通知対象として想定。

* 初期化失敗
* ファイルロード失敗
* 無効操作
* パラメータ補正
* オーディオデバイスエラー

---

# 将来拡張

以下は将来追加可能。

* BGM / SE / Voice カテゴリ
* SubMixer / Bus
* フェード
* エフェクト
* 3D音響
* 高度なログ
* 診断ツール

---

# AudioLib設計まとめ

AudioLibは以下の思想で構築する。

### 公開API

* シンプル
* 直感的
* Handleベース
* 安全継続

### 内部設計

* Loader / Repository / Mixer 分離
* CommandQueueベース処理
* AudioThreadで更新
* Handleは軽量アクセス窓口

### 安全性

* アプリケーションを落とさない
* 無効操作はno-op
* 将来Loggerで通知

---

# 設計状態

現時点で **AudioLibの基本設計は確定**している。

今後は以下の順で実装を進める。

1. 基本クラス定義
2. CommandQueue
3. AudioThread
4. Mixer
5. Repository
6. Loader
7. 公開API整備

この方針により、ユーザーフレンドリで安全な音声ライブラリを構築する。
