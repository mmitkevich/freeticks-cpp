#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "toolbox/net/Packet.hpp"
#include "ft/spb/SpbSchema.hpp"
#include "ft/spb/SpbDecoder.hpp"
#include "ft/core/Instrument.hpp"
#include "toolbox/sys/Time.hpp"
namespace ft::spb {
namespace tbn = toolbox::net;


template<typename ProtocolT>
class SpbBestPriceStream : public core::TickStream
{
public:
    using Base = core::TickStream;
    using This = SpbBestPriceStream<ProtocolT>;
    using Protocol = ProtocolT;
    using Decoder = typename Protocol::Decoder;
    using Schema = typename Decoder::Schema;
    // supported messages
    using SnapshotStart = typename Schema::SnapshotStart;
    using SnapshotFinish = typename Schema::SnapshotStart;
    using PriceSnapshot = typename Schema::PriceSnapshot;
    // list of supported messages
    using TypeList = mp::mp_list<SnapshotStart, SnapshotFinish, PriceSnapshot>;
    // packet type meta function
    template<typename MessageT>
    using TypedPacket = typename Decoder::template TypedPacket<MessageT>;
public:
    SpbBestPriceStream(Protocol& protocol)
    :   protocol_(protocol) {
        connect();
    }
    ~SpbBestPriceStream() {
        disconnect();
    }
    Decoder& decoder() { return protocol_.decoder(); } 
protected:
    void connect() {
        decoder().signals().connect(tbu::bind<&This::on_snapshot_start>(this));
        decoder().signals().connect(tbu::bind<&This::on_snapshot_finish>(this));
        decoder().signals().connect(tbu::bind<&This::on_price_snapshot>(this));
    }
    void disconnect() {
        decoder().signals().disconnect(tbu::bind<&This::on_snapshot_start>(this));
        decoder().signals().disconnect(tbu::bind<&This::on_snapshot_finish>(this));
        decoder().signals().disconnect(tbu::bind<&This::on_price_snapshot>(this));
    }
protected:
    void on_snapshot_start(TypedPacket<SnapshotStart> e) { }
    void on_snapshot_finish(TypedPacket<SnapshotFinish> e) { }
    void on_price_snapshot(TypedPacket<PriceSnapshot> e) {
        //TOOLBOX_INFO << e;
        stats().on_received(e);
        auto& snap = *e.data();
        for(auto &best: snap.sub_best) {
            core::Tick ti {};
            ti.type(core::TickType::Update);
            ti.venue_instrument_id = snap.instrument.instrument_id;
            ti.timestamp = core::WallClock::now();
            ti.server_timestamp = snap.base.header.system_time.wall_time();
            ti.price = protocol_.price_conv().to_core(best.price);
            ti.side = get_side(best);
            ti.qty = core::Qty(best.amount);
            if(ti.side!=core::TickSide::Invalid)
                invoke(ti);
        }
        //if(stats().total_received % 100'000 == 0) {
        //    TOOLBOX_INFO << "total_received:"<<stats().total_received<<", "<<e;
        //}
    }
    static constexpr core::TickSide get_side(const SubBest& self) {  
        switch(self.type) {
            case SubBest::Type::Buy: return core::TickSide::Buy;
            case SubBest::Type::Sell: return core::TickSide::Sell;
            default: return core::TickSide::Invalid;
        }
    }
private:
    Protocol& protocol_;
};

}