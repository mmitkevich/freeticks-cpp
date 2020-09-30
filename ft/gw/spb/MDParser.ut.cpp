#include <boost/mp11/algorithm.hpp>
#include <boost/test/unit_test.hpp>
#include "dto/MarketData.hpp"
#include "toolbox/bm/Suite.hpp"
#include "toolbox/sys/Log.hpp"
#include <iostream>

#include "MDParser.hpp"

using namespace ft::gw;
namespace mp = boost::mp11;
namespace tbu = toolbox::util;
namespace tb = toolbox;

using namespace mp;

BOOST_AUTO_TEST_SUITE(SpbGw)

#define OFFSET_OF(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)

static_assert(OFFSET_OF(spb::dto::Frame, msgid) == 2);

BOOST_AUTO_TEST_CASE(Parser)
{
    using Schema = spb::dto::Schema::UDP;
    using SnapshotStart = spb::dto::SnapshotStart<Schema>;
    using SnapshotFinish = spb::dto::SnapshotFinish<Schema>;
    using TypeList = mp_list<SnapshotStart, SnapshotFinish>;
    using Parser = spb::MDParser<TypeList>;

    std::size_t n_snapshot_start = 0;
    auto on_snapshot_start = [&](const SnapshotStart& msg) {
        //TOOLBOX_INFO << "SnapshotStart("<<msg.frame.msgid<<", update_seq=" << msg.update_seq << ")";
        n_snapshot_start++;
    };

    std::size_t n_snapshot_finish = 0;    
    auto on_snapshot_finish = [&](const SnapshotFinish& msg) {
        //TOOLBOX_INFO << "SnapshotFinish("<<msg.frame.msgid<<", update_seq=" << msg.update_seq << ")";
        n_snapshot_finish++;
    };
    
    Parser p;

    p.subscribe(tbu::bind(&on_snapshot_start));
    p.subscribe(tbu::bind(&on_snapshot_finish));

    char msg[] = 
        "\x04\x00\x39\x30"
        "\x04\x00\x18\x30";
    tb::bm::BenchmarkSuite bm(std::cout);
    bm.run("Parser", [&](auto &ctx) {
         while(ctx) {
            for(auto _:ctx.range(10000)) {
                p.parse(std::string_view(msg, sizeof(msg)-1));
            }
         }
    });
    TOOLBOX_INFO << "snapshot_start "<<n_snapshot_start<<" snapshot_finish "<<n_snapshot_finish;
}

BOOST_AUTO_TEST_SUITE_END()