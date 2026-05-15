#ifndef Sound_HPP_
#define Sound_HPP_

#include <cstdint>
#include <vector>

namespace Audio::Internal {

    /** 1ファイル分のデコード済み音声データ。
     *  WAV / MP3 問わず float32 PCM として保持する。
     *  Repository が所有し、PlaybackInstance が非所有参照で使用する。
     */
    class Sound {
        std::vector<float>  pcm_;
        uint32_t            sampleRate_ = 0;
        uint32_t            channels_   = 0;
        uint64_t            frameCount_ = 0;

    public:
        Sound() = default;
        Sound(std::vector<float> _pcm, uint32_t _sampleRate,
              uint32_t _channels, uint64_t _frameCount)
            : pcm_(std::move(_pcm))
            , sampleRate_(_sampleRate)
            , channels_(_channels)
            , frameCount_(_frameCount)
        {}

        bool IsValid() const { return !pcm_.empty(); }

        const std::vector<float>& GetPcm()        const { return pcm_; }
        uint32_t                  GetSampleRate()  const { return sampleRate_; }
        uint32_t                  GetChannels()    const { return channels_; }
        uint64_t                  GetFrameCount()  const { return frameCount_; }
    } typedef Data;

} // namespace Audio::Internal

#endif // Sound_HPP_
