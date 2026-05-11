# Step Sequencer Spec

## 0. 目的

初期段階では `FL Studio` の `channel rack + step sequencer` に近い体験を目標にする。

- 音源をチャンネルとして登録
- 共通 16 step グリッドへ配置
- 1 pattern を loop 再生
- 再生中編集を次 step から反映

piano roll は後回しとし、本仕様の対象外とする。

## 1. スコープ

### 1.1 入れるもの

- channel rack
- step sequencer
- 複数 pattern
- 1 pattern loop 再生
- メトロノーム
- チャンネルごとの基本パラメータ

### 1.2 入れないもの

- piano roll
- playlist / arrangement
- step ごとの velocity
- swing
- probability
- ratchet
- micro timing
- channel ごとの polyphony 制限

## 2. 全体モデル

sequencer project は次を持つ。

- 複数 channel
- 複数 pattern
- sequencer 全体設定

### 2.1 基本構造

```cpp
struct StepChannel {
    std::string   name;
    Audio::Handle sound;

    float volume = 1.0f;
    float pan = 0.0f;
    bool  mute = false;

    bool  cutSelf = false;
    float attackSec = 0.0f;
    float releaseSec = 0.0f;
};

struct StepPattern {
    std::string name;
    bool steps[64][16] = {};
};

struct StepSequencerProject {
    std::vector<StepChannel> channels;
    std::vector<StepPattern> patterns;

    double bpm = 120.0;
    bool metronomeEnabled = false;
};
```

`steps[channelIndex][stepIndex]` は当面 `On/Off` のみ。

## 3. UI モデル

## 3.1 レイアウト

- 縦に channel
- 横に 16 step
- 全 channel は共通グリッドを共有

## 3.2 pattern

- `Pattern 1`, `Pattern 2`, ... を持てる
- 初期段階では 1 つ選んで編集する
- 再生も当面は選択中 pattern の loop のみ

playlist / arrangement は将来追加する。

## 4. チャンネル

## 4.1 音源

- 各 channel は 1 つの `Audio::Handle` を持つ
- 最初は 1 サンプル固定

## 4.2 初期パラメータ

各 channel は次を持つ。

- `Volume`
- `Pan`
- `Mute`
- `CutSelf`
- `Attack`
- `Release`

## 4.3 パラメータ反映タイミング

- 再生中に変更しても、次に鳴る音から反映
- 現在鳴っている音へ即時には反映しない

## 5. Step 発火モデル

## 5.1 基本

- step が `On` ならその channel の音を鳴らす
- 初期実装では `Handle.Play()` 相当の one-shot 発火でよい

## 5.2 重ね鳴らし

- 同一 channel の連続 step は基本重ね鳴らし

## 5.3 CutSelf

- `cutSelf=false` なら何もしない
- `cutSelf=true` なら新規発火時に同 channel の直前発音を切る

### Release との関係

- `releaseSec == 0` なら即停止寄り
- `releaseSec > 0` なら直前発音を release フェーズへ移す

## 5.4 Attack / Release

初期段階では step sequencer 専用設定として扱う。

- `attackSec=0` なら即音量立ち上がり
- `releaseSec=0` なら即停止寄り

将来:

- より一般的な envelope / curve 制御へ拡張
- 1 発音ごとの制御点編集に繋げる

## 6. 再生仕様

## 6.1 初期再生

- 常に `step 1` から開始
- pattern を loop 再生する
- playback 中の UI 編集は次の step から反映
- 通過済み step の変更は次 loop から反映

## 6.2 タイミング

初期段階は厳密さより安定性優先。

- 気持ちよくずれずに鳴ることを優先
- sample-accurate scheduler は後回し

## 6.3 メトロノーム

- sequencer 再生中のみ鳴ればよい
- 音は固定
- 1 拍目とそれ以外の音分けは後回し

## 7. pattern 編集

## 7.1 最小操作

- step の on/off 切り替え
- pattern 切り替え
- channel の追加削除
- channel 名変更
- 音源割り当て

## 7.2 pattern 長

- 当面は 16 step 固定

## 8. ランタイム利用

将来は Demo/Editor だけでなく、ゲームランタイムでも同じデータを再生できるようにする。

そのため、editor 用 UI 状態と playback 用データは分離可能な形にしておく。

## 9. 保存形式

初期段階は JSON でよい。

### 保存対象

- channel 名
- 音源パス
- channel 設定
- pattern 群
- BPM
- metronome on/off

### 保存例

```json
{
  "bpm": 120.0,
  "metronomeEnabled": true,
  "channels": [
    {
      "name": "Kick",
      "path": "kick.wav",
      "volume": 1.0,
      "pan": 0.0,
      "mute": false,
      "cutSelf": false,
      "attackSec": 0.0,
      "releaseSec": 0.0
    }
  ],
  "patterns": [
    {
      "name": "Pattern 1",
      "steps": [
        [true, false, false, false, true, false, false, false, true, false, false, false, true, false, false, false]
      ]
    }
  ]
}
```

## 10. 将来拡張

- playlist / arrangement
- piano roll
- step ごとの velocity
- swing
- probability
- ratchet
- micro timing
- polyphony limit
- channel solo
- envelope curve editor
- sampler / simple synth integration

## 11. 実装順

1. playback 用データ構造
2. 16 step loop 再生
3. channel rack UI
4. step on/off 編集
5. pattern 切り替え
6. metronome
7. `Attack/Release` と `CutSelf`
