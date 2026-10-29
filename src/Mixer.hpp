#ifndef Mixer_HPP_
#define Mixer_HPP_

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "include/Ears.hpp"
#include "include/Reverb.hpp"
#include "Reverberator.hpp"
#include "Sound.hpp"
#include "StreamRefiller.hpp"
#include "StreamingAsset.hpp"

struct ma_engine;

namespace Audio::Internal {

    class Ear;

    class Mixer {
        struct PlaybackInstance;
        struct TrackInstance;

        ma_engine* engine_ = nullptr;
        std::unordered_map<uint64_t, std::shared_ptr<PlaybackInstance>> sounds_;
        std::unordered_map<uint64_t, std::shared_ptr<TrackInstance>>    tracks_;
        uint64_t nextId_      = 1;
        uint64_t nextTrackId_ = 1;
        StreamRefiller refiller_;
        // 全体の響き。最初の SetReverb で作る（それまで、また作れなかったときは nullptr で、Ear の送る分はどこにもつながない）
        std::unique_ptr<Reverberator> reverb_;

    public:
        ~Mixer();
        bool Init();
        void Shutdown();

        // _ears があれば、鳴らし始める前に Ear をはさみ、その値から鳴らす
        uint64_t Play(uint64_t _soundId, std::shared_ptr<const Sound> _sound, uint64_t _trackId = 0, const Ears* _ears = nullptr);
        // 再生しながらデコードする（StreamingVoice を 1 つ作り、refill ワーカーに渡す）
        uint64_t PlayStreaming(uint64_t _soundId, std::shared_ptr<const StreamingAsset> _asset, uint64_t _trackId = 0,
                               const Ears* _ears = nullptr);
        bool IsPlaying(uint64_t _id) const;

        void Stop(uint64_t _id);
        void StopAll(uint64_t _soundId);
        void Pause(uint64_t _id);
        void Resume(uint64_t _id);
        void Mute(uint64_t _id, bool _mute);
        void SetVolume(uint64_t _id, float _volume);
        void SetPan(uint64_t _id, float _pan);
        void SetPitch(uint64_t _id, float _pitch);
        void SetLoop(uint64_t _id, bool _loop);
        // まだ Ear のない再生には、ここではさむ（その値から始める）。あれば今の値から近づける
        void SetEars(uint64_t _id, const Ears& _ears);

        // --- Track ---
        uint64_t CreateTrack();
        void     DestroyTrack(uint64_t _trackId);
        void     StopAllInTrack(uint64_t _trackId);
        void     SetTrackVolume(uint64_t _trackId, float _volume);
        void     SetTrackPan(uint64_t _trackId, float _pan);
        void     SetTrackPitch(uint64_t _trackId, float _pitch);

        // --- Master ---
        void SetMasterVolume(float _volume) const;
        // 全体の響きを設定する（なければ作り、今ある Ear の送る分をつなぐ）
        void SetReverb(const Reverb& _reverb);

    private:
        // --- Playback Control ---
        // 止めて Stopped にする。静的な再生は先頭へ戻して再利用に備える
        static void StopInstance(PlaybackInstance& _instance);
        // ma_sound を作って鳴らし始め、id を振って登録する。失敗なら 0
        uint64_t StartInstance(std::shared_ptr<PlaybackInstance> _instance, void* _dataSource, uint64_t _trackId, const Ears* _ears);

        // --- Ear ---
        // 鳴らし始める前に: _ears があれば Ear をはさんでその値から、なくて Ear が残っていれば素通しの値から始める
        void PrepareEar(PlaybackInstance& _instance, const Ears* _ears);
        // ma_sound と行き先（Track か全体の出口）の間に Ear をはさむ（響きがあれば送る分もつなぐ）。はさめなければ false で、音はそのまま鳴る
        bool AttachEar(PlaybackInstance& _instance);
        // Ear の 2 つ目の出口（送る分）を響きにつなぐ
        void SendToReverb(Ear& _ear) const;

        // --- State Management ---
        void Cleanup();
    };

} // namespace Audio::Internal

#endif // Mixer_HPP_
