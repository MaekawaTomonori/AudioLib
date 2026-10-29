#ifndef Reverberator_HPP_
#define Reverberator_HPP_

#include <memory>

#include "include/Reverb.hpp"

struct ma_engine;

namespace Audio::Internal {

    /** 全体で 1 つの響き（Audio::Reverb）。miniaudio の自作ノードで、Ear の 2 つ目の出口（送る分）を入口に集め、全体の出口に出す。
     *
     *  中身は FDN（フィードバック・ディレイ・ネットワーク）: 長さの違う 8 本のディレイライン（48 kHz で 30〜60 ms、たがいに素）の
     *  出力をアダマール行列（直交、エネルギーを増やさない）で混ぜ、1 回りごとに g = 10^(−3·長さ / (RT60·Fs)) を掛け、
     *  1 次のローパス（damping）を通して戻す。左の入力は偶数番、右は奇数番のラインに入れ、出力も同じ組で取り出す（左右で違う響き）。
     *  入口の前にプリディレイ（最大 0.1 秒）を置く。
     *
     *  スレッドの分担:
     *  - オーディオスレッド: ノードの処理だけ。確保・ロックはしない。値は 1 回の処理の間に近づける（プリディレイは 1 回に
     *    その回の長さの 5% までしか動かさない。速く動かすと響きの高さが揺れる）。音量が 0 のとき、また入力が無音で響きが
     *    −100 dB を下回ったら、バッファを 0 にして計算を止める（入力が来たらまた始める）。十分小さい値は 0 にする（デノーマル）。
     *  - API スレッド: 作成（バッファの確保もここ）、値の設定（atomic）、破棄（Ear を外した後）。
     */
    class Reverberator {
    public:
        // miniaudio のノードの中身（Reverberator.cpp だけが知っている。処理の関数から見えるよう public に宣言する）
        struct Node;

    private:
        std::unique_ptr<Node> node_;

        explicit Reverberator(std::unique_ptr<Node> _node);

    public:
        /** engine の出力がステレオでないときと、作れなかったときは nullptr。全体の出口にはつないである。 */
        static std::unique_ptr<Reverberator> Create(ma_engine* _engine);
        ~Reverberator();
        Reverberator(const Reverberator&)            = delete;
        Reverberator& operator=(const Reverberator&) = delete;

        /** Ear の送る分をつなぐ先（miniaudio の型を見せないため void*）。 */
        void* AsNode();

        /** 次の処理から、今の値から _reverb へ近づける。 */
        void Set(const Reverb& _reverb);
    };

} // namespace Audio::Internal

#endif // Reverberator_HPP_
