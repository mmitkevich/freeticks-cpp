#include "ft/spb/SpbFrame.hpp"
#include "ft/utils/Common.hpp"

#include <boost/test/unit_test.hpp>
#include "toolbox/bm/Suite.hpp"
#include <iostream>

#include "SpbDecoder.hpp"
#include "SpbSchema.hpp"
#include "SpbProtocol.hpp"
#include "toolbox/net/Packet.hpp"
#include "toolbox/net/Pcap.hpp"

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
    using SpbSchema = SpbSchema<SpbUdp>;
    using BinaryPacket = tbn::BinaryPacket<IpEndpoint>;
    using SpbProtocol = SpbProtocol<SpbSchema, BinaryPacket>;
    using SpbDecoder = typename  SpbProtocol::Decoder;
    std::size_t n_snapshot_start = 0;
    auto on_snapshot_start = [&](SpbDecoder::TypedPacket<SpbSchema::SnapshotStart> pkt) {
        auto& msg = *pkt.data();
        if constexpr (!BENCH) {
            TOOLBOX_INFO << "SnapshotStart("<<msg.base.frame.msgid<<", update_seq=" << msg.update_seq << ")";
        }
        n_snapshot_start++;
    };

    std::size_t n_snapshot_finish = 0;    
    auto on_snapshot_finish = [&](SpbDecoder::TypedPacket<SpbSchema::SnapshotStart> pkt) {
        auto& msg = *pkt.data();
        if constexpr (!BENCH) {
            TOOLBOX_INFO << "SnapshotFinish("<<msg.base.frame.msgid<<", update_seq=" << msg.update_seq << ")";
        }
        n_snapshot_finish++;
    };
    
   
    SpbDecoder decoder;
    decoder.signals().connect(tbu::bind(&on_snapshot_start));
    decoder.signals().connect(tbu::bind(&on_snapshot_finish));

    char msg[] = 
        "\x04\x00\x39\x30"
        "\x04\x00\x18\x30";

    maybe_bench("parser", 1000*BENCH, [&] {
        decoder.on_packet(BinaryPacket(msg, sizeof(msg)-1));
    });
    TOOLBOX_INFO << "snapshot_start "<<n_snapshot_start<<" snapshot_finish "<<n_snapshot_finish;
}

BOOST_AUTO_TEST_SUITE_END()