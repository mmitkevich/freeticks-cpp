#include "ft/utils/Common.hpp"

#include <boost/test/unit_test.hpp>
#include "toolbox/bm/Suite.hpp"
#include <iostream>

#include "SpbDecoder.hpp"
#include "SpbSchema.hpp"
#include "SpbProtocol.hpp"

using namespace ft;
using namespace ft::spb;
using namespace tb;

BOOST_AUTO_TEST_SUITE(SpbGw)

#define OFFSET_OF(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)

static_assert(OFFSET_OF(Frame, msgid) == 2);

constexpr std::size_t BENCH = 0;

template<typename F, typename...Args>
void maybe_bench(const char*name, std::size_t N, F fn, Args...args) {
    if(N>0) {
        bm::BenchmarkSuite bm(std::cout);
        bm.run(name, [&](auto &ctx) {
            while(ctx) {
                for(auto _:ctx.range(N)) {
                    fn(std::forward<Args>(args)...);
                }
            }
        });
    } else {
        fn(std::forward<Args>(args)...);
    }
}

BOOST_AUTO_TEST_CASE(Parser)
{
    using Udp = SpbSchema<SpbUdp>;

    std::size_t n_snapshot_start = 0;
    auto on_snapshot_start = [&](const typename Udp::SnapshotStart& msg) {
        if constexpr (!BENCH) {
            TOOLBOX_INFO << "SnapshotStart("<<msg.frame.msgid<<", update_seq=" << msg.update_seq << ")";
        }
        n_snapshot_start++;
    };

    std::size_t n_snapshot_finish = 0;    
    auto on_snapshot_finish = [&](const typename Udp::SnapshotFinish& msg) {
        if constexpr (!BENCH) {
            TOOLBOX_INFO << "SnapshotFinish("<<msg.frame.msgid<<", update_seq=" << msg.update_seq << ")";
        }
        n_snapshot_finish++;
    };
    
    using TypeList = typename SpbSchema<SpbUdp>::TypeList;

    SpbDecoder<Frame, TypeList> decoder;

    decoder.connect(tbu::bind(&on_snapshot_start));
    decoder.connect(tbu::bind(&on_snapshot_finish));

    char msg[] = 
        "\x04\x00\x39\x30"
        "\x04\x00\x18\x30";

    maybe_bench("parser", 1000*BENCH, [&] {
        decoder.decode(std::string_view(msg, sizeof(msg)-1));
    });
    TOOLBOX_INFO << "snapshot_start "<<n_snapshot_start<<" snapshot_finish "<<n_snapshot_finish;
}

BOOST_AUTO_TEST_SUITE_END()