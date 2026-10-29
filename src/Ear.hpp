#ifndef Ear_HPP_
#define Ear_HPP_

#include <cstdint>
#include <memory>

#include "include/Ears.hpp"

struct ma_engine;

namespace Audio::Internal {

    /** 1 回の再生の、左右の耳ごとの音量と遅れ（Audio::Ears）。miniaudio の自作ノードで、ma_sound とその行き先の間にはさむ。
     *
     *  スレッドの分担:
     *  - オーディオスレッド: ノードの処理だけ。左右それぞれ、入ってきたサンプルをリングバッファ（256 サンプル）に書き、
     *    遅れの分だけ前の値を読む（1 サンプルより細かい遅れは、隣の 2 つを線形補間）。カットオフが上限より下なら、
     *    それをローパス（RBJ のバイクアッド、Q 0.707）に通し、音量を掛ける。値が変わったら、1 回の処理の間に前の値から
     *    少しずつ近づける（プツッと鳴らないように。カットオフは 2 を底とする対数で近づけ、動いている間は係数を 8 サンプル
     *    ごとに作り直す。鳴っている途中で削り始めるときは、その回の間に素の音から混ぜ替え、次の Restart まで削り続ける）。
     *    確保・ロックはしない。
     *  - API スレッド: 作成、値の設定（atomic で渡す）、破棄。破棄は ma_sound を外した後に行う（ma_node_uninit は
     *    オーディオスレッドがこのノードの処理を終えるまで待つ）。
     *  - 音量 1・遅れ 0・カットオフなし（Ears の既定）のときの出力は入力と同じ値になる。
     */
    class Ear {
    public:
        // miniaudio のノードの中身（Ear.cpp だけが知っている。処理の関数から見えるよう public に宣言する）
        struct Node;

    private:
        std::unique_ptr<Node> node_;

        explicit Ear(std::unique_ptr<Node> _node);

    public:
        /** engine の出力がステレオでないとき（はさめない）と、作れなかったときは nullptr。 */
        static std::unique_ptr<Ear> Create(ma_engine* _engine);
        ~Ear();
        Ear(const Ear&)            = delete;
        Ear& operator=(const Ear&) = delete;

        /** ma_node_attach_output_bus に渡すノード（miniaudio の型を見せないため void*）。 */
        void* AsNode();

        // --- Parameters ---

        /** 次の処理から、今の値から _ears へ近づける。 */
        void Set(const Ears& _ears);
        /** 再生のし直し: 次の処理の前にリングバッファを 0 にし、近づけずに _ears から始める。 */
        void Restart(const Ears& _ears);
    };

} // namespace Audio::Internal

#endif // Ear_HPP_
