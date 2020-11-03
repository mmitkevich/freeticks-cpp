#pragma once
#include "ft/core/StreamStats.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/spb/SpbFrame.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Packet.hpp"
#include "ft/spb/SpbSchema.hpp"
#include "ft/spb/SpbDecoder.hpp"
#include "ft/core/Instrument.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/sys/Time.hpp"
#include "ft/spb/SpbReplacingUpdates.hpp"

namespace ft::spb {
namespace tbn = toolbox::net;

#define FT_STREAM_DEBUG TOOLBOX_DEBUG << name() << ": "

using Timestamp = core::Timestamp;

using SpbBestPriceSnapshot = std::array<core::Tick,3>;

template<typename ProtocolT
,typename SnapshotUpdatesT = SpbReplacingUpdates<spb::SnapshotKey, spb::SpbBestPriceSnapshot>
> class SpbBestPriceStream : public BasicSpbStream<SpbBestPriceStream<ProtocolT>, ProtocolT, core::TickStream>
{
public:
    using This = SpbBestPriceStream<ProtocolT>;
    using Base = BasicSpbStream<This, ProtocolT, core::TickStream>;
    using Protocol = typename Base::Protocol;
    using Schema = typename Base::Schema;
    using SnapshotUpdates = SnapshotUpdatesT;  
    using Snapshot = SpbBestPriceSnapshot;

    // packet type meta function
    template<typename MessageT>
    using SpbPacket = typename Base::template SpbPacket<MessageT>;

    // supported messages
    using SnapshotStart = typename Schema::SnapshotStart;
    using SnapshotFinish = typename Schema::SnapshotFinish;
    using PriceSnapshot = typename Schema::PriceSnapshot;
    using PriceOnline = typename Schema::PriceOnline;
    // list of supported messages
    using TypeList = mp::mp_list<PriceOnline, PriceSnapshot, SnapshotStart, SnapshotFinish>;
public:
    using Base::Base;
    using Base::stats;
    using Base::protocol;
    using Base::invoke;

    static constexpr std::string_view name() { return "bestprice"; }
public:
    void on_parameters_updated(const core::Parameters &params) {
        Base::on_parameters_updated(params);
    }
    template<typename T>
    void on_packet(const SpbPacket<T>& pkt) {
        const auto &payload = pkt.value();
        on_message(payload.header(), payload.value(), pkt.header().recv_timestamp());
    }
    void on_message(const typename SnapshotStart::Header& h, const spb::Snapshot& e, Timestamp cts) { 
        impl_.start(h.sequence(), e.update_seq);
    }
    void on_message(const typename SnapshotFinish::Header& h, const spb::Snapshot& e, Timestamp cts) { 
        impl_.finish(h.sequence(), e.update_seq);
    }

    void on_message(const typename PriceSnapshot::Header& h, const spb::Price& e, Timestamp cts) {
        auto snap = to_snapshot(impl_.updates_snapshot_seq(), cts, h.server_time(), e);
        SnapshotKey key {e.instrument, h.header.sourceid};
        auto rv = impl_.snapshot(impl_.updates_snapshot_seq(), std::move(key), snap);
        if(rv.first)
            send(rv.second);
    }
    
    void on_message(const typename PriceOnline::Header& h, const spb::Price& e, Timestamp cts) {
        auto snap = to_snapshot(h.sequence(), cts, h.server_time(), e);
        SnapshotKey key {e.instrument, h.header.sourceid};
        auto rv = impl_.update(h.sequence(), std::move(key), snap);
        if(rv.first)
            send(rv.second);
    }

protected:
    void send(const Snapshot& snap)
    {
        //TOOLBOX_DEBUG << name()<<": "<<val.value;
        //stats().on_received(d); // FIXME
        for(const auto& e: snap) {
            if(!e.empty())
                invoke(e);
        }
    }

    core::Tick to_tick(Seq seq, const spb::MarketInstrumentId id, const tb::WallTime cts, const tb::WallTime sts, const spb::SubBest& best) {
        core::Tick ti {};
        ti.sequence(seq);
        core::TickType type = core::TickType::Update;
        if(best.type==SubBest::Type::Deal) {
            type = core::TickType::Fill;
        }
        ti.type(type);
        ti.venue_instrument_id(id.instrument_id);
        ti.timestamp(cts);
        ti.server_timestamp(sts);
        ti.price(protocol().price_conv().to_core(best.price));
        ti.side(get_side(best));
        ti.qty(core::Qty(best.amount));
        return ti;
    }
    
    Snapshot to_snapshot(Seq seq, const Timestamp cts, const Timestamp sts, const spb::Price& e) {
        Snapshot snap;
        assert(e.sub_best.size()<=snap.size());
        for(std::size_t i=0; i<std::min(e.sub_best.size(), snap.size()); i++) {
            auto &best = e.sub_best[i];
            snap[i] = to_tick(seq, e.instrument, cts, sts, best);
        }
        return snap;
    }
    
    static constexpr core::TickSide get_side(const SubBest& self) {  
        switch(self.type) {
            case SubBest::Type::Buy: return core::TickSide::Buy;
            case SubBest::Type::Sell: return core::TickSide::Sell;
            default: return core::TickSide::Invalid;
        }
    }
private:
    SnapshotUpdates impl_;
};

}