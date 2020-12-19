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

#define FT_STREAM_DEBUG TOOLBOX_DEBUG << name() << ": "

using Timestamp = core::Timestamp;

using SpbTick = core::Ticks<3>;

template<typename ProtocolT
,typename SnapshotUpdatesT = SpbReplacingUpdates<spb::SnapshotKey, spb::SpbTick>
> class SpbBestPriceStream : public BasicSpbStream<SpbBestPriceStream<ProtocolT>, ProtocolT, core::TickStream>
{
public:
    using This = SpbBestPriceStream<ProtocolT>;
    using Base = BasicSpbStream<This, ProtocolT, core::TickStream>;
    using Protocol = typename Base::Protocol;
    using Schema = typename Base::Schema;
    using SnapshotUpdates = SnapshotUpdatesT;  

    // packet type meta function
    template<typename MessageT>
    using SpbPacket = typename Base::template SpbPacket<MessageT>;

    // supported messages
    using SnapshotStart = typename Schema::SnapshotStart;
    using SnapshotFinish = typename Schema::SnapshotFinish;
    using PriceSnapshot = typename Schema::PriceSnapshot;
    using PriceOnline = typename Schema::PriceOnline;
    using Heartbeat = typename Schema::Heartbeat;
    // list of supported messages
    using TypeList = mp::mp_list<PriceOnline, PriceSnapshot, SnapshotStart, SnapshotFinish, Heartbeat>;
public:
    SpbBestPriceStream(Protocol& protocol)
    : Base(protocol) 
    {
        TOOLBOX_DUMP_THIS;
    }
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
        auto ti = to_tick(impl_.updates_snapshot_seq(), cts, h.server_time(), e);
        SnapshotKey key {e.instrument, h.header.sourceid};
        auto [tick, is_replaced] = impl_.snapshot(impl_.updates_snapshot_seq(), std::move(key), ti);
        if(is_replaced) {
            invoke(tick->template as_size<1>());
        }
    }
    
    void on_message(const typename PriceOnline::Header& h, const spb::Price& e, Timestamp cts) {
        auto ti = to_tick(h.sequence(), cts, h.server_time(), e);
        SnapshotKey key {e.instrument, h.header.sourceid};
        auto [tick, is_replaced]  = impl_.update(h.sequence(), std::move(key), ti);
        if(is_replaced) {
            invoke(tick->template as_size<1>());
        }
    }
    void on_message(const typename Heartbeat::Header& h, const spb::Heartbeat& e, Timestamp cts) {
    }

protected:    
    SpbTick to_tick(Seq seq, const Timestamp cts, const Timestamp sts, const spb::Price& e) {
        SpbTick ti;
        assert(e.sub_best.size()<=ti.capacity());
        ti.sequence(seq);
        ti.resize(std::min(e.sub_best.size(), ti.capacity()));
        for(std::size_t i=0; i<ti.size(); i++) {
            auto &best = e.sub_best[i];
            core::TickType type = core::TickType::Update;
            if(best.type==SubBest::Type::Deal) {
                type = core::TickType::Fill;
            }
            ti.type(core::TickType::Tick);
            ti.venue_instrument_id(e.instrument.instrument_id);
            ti.recv_time(cts);
            ti.send_time(sts);
            auto& lvl = ti[i];
            lvl.type(core::TickType::Update);
            lvl.price(protocol().price_conv().to_core(best.price));
            lvl.side(get_side(best));
            lvl.qty(core::Qty(best.amount));
        }
        return ti;
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