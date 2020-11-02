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

namespace ft::spb {
namespace tbn = toolbox::net;

#define FT_STREAM_DEBUG TOOLBOX_DEBUG << name() << ": "

template<typename K, typename V>
class SpbReplacingUpdates {
public:
    static constexpr std::uint64_t invalid_seq = 0;
public:
    void start(std::uint64_t snapshot_seq, std::uint64_t update_seq) {
        updates_snapshot_seq_ = update_seq;
        snapshot_start_seq_ = snapshot_last_seq_ = snapshot_seq;
        assert(snapshot_start_seq_!=0);
        log_state(update_seq, "SnapshotStart");
    }
    void finish(std::uint64_t snapshot_seq, std::uint64_t update_seq) {
        snapshot_last_seq_ = snapshot_seq;
        if(!snapshot_start_seq_) {
            log_state(update_seq, "SnapshotFinish: No SnapshotStart");
        } else if(update_seq!=updates_snapshot_seq_) { 
            log_state(update_seq, "SnapshotFinish: Incorrect SnapshotStart");
        }  else if(snapshot_last_seq_-snapshot_start_seq_-1!=snapshot_size_) {
            snapshots_stats_.on_gap((snapshot_last_seq_-snapshot_start_seq_-1) - snapshot_size_);
            log_state(update_seq, "SnapshotFinish: Snapshot has gaps");
        }else {
            log_state(update_seq, "SnapshotFinish");
        }
        snapshot_start_seq_ = snapshot_last_seq_ = invalid_seq;
        snapshot_size_ = 0;
    }
    bool snapshot(std::uint64_t seq, const K& key,  const V& value) {
        if(!snapshot_start_seq_) {
            //log_state(seq,"Snapshot: No SnapshotStart");
            return false;
        }
        snapshot_last_seq_ = seq;
        snapshot_size_++;
        
        snapshots_stats_.on_received();

        auto& prev = snapshot_[key];
        if(prev.seq == invalid_seq || updates_snapshot_seq_>prev.seq) {
            prev = value;
            return true;
        }else {
            //log_state(updates_snapshot_seq_, "Snapshot: Stale ignored");    
            snapshots_stats_.on_rejected();
            return false;
        }
    }
    bool update(std::uint64_t seq, const K& key,  const V& value) {
        if(updates_last_seq_!=invalid_seq && seq>updates_last_seq_ + 1) {
            updates_stats_.on_gap(seq-(updates_last_seq_ + 1));
        }
        updates_last_seq_ = std::max(updates_last_seq_, seq);
        
        updates_stats_.on_received();

        auto& prev = snapshot_[key];
        if(prev.seq == invalid_seq || seq>prev.seq) {
            prev = value;
            return true;
        }else {
            //log_state(seq, "Update: Stale ignored");    
            updates_stats_.on_rejected();
            return false;
        }
    }

    void log_state(std::uint64_t seq, const char* reason) {
        TOOLBOX_DEBUG << name_ << " " << reason << " seq:"<<seq
            <<", updates: { snapshot:"<<updates_snapshot_seq_<<", last:"<<updates_last_seq_<<", stats: {"<<updates_stats_<<"}}"
            <<", snapshot: { start:"<<snapshot_start_seq_<<", last:"<<snapshot_last_seq_<<", size:"<<snapshot_.size()<<", stats:{"<<snapshots_stats_<<"}}";
    }
private:
    std::string_view name_ {};
    
    utils::FlatMap<K, V> snapshot_;

    std::uint64_t snapshot_start_seq_{};    // when started
    std::uint64_t snapshot_last_seq_{};     // last snapshot seq
    std::uint64_t snapshot_size_{};     // last snapshot seq

    std::uint64_t updates_snapshot_seq_ {};

    std::uint64_t updates_last_seq_{};

    core::StreamStats updates_stats_ {};
    core::StreamStats snapshots_stats_ {};
    
};

template<typename ProtocolT>
class SpbBestPriceStream : public BasicSpbStream<SpbBestPriceStream<ProtocolT>, ProtocolT, core::TickStream>
{
public:
    using This = SpbBestPriceStream<ProtocolT>;
    using Base = BasicSpbStream<This, ProtocolT, core::TickStream>;
    using Protocol = typename Base::Protocol;
    using Schema = typename Base::Schema;
    
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
        on_message(payload.header(), payload.value());
    }
    void on_message(const typename SnapshotStart::Header& h, const spb::Snapshot& e) { 
        impl_.start(h.sequence(), e.update_seq);
    }
    void on_message(const typename SnapshotFinish::Header& h, const spb::Snapshot& e) { 
        impl_.finish(h.sequence(), e.update_seq);
    }
    void on_message(const typename PriceSnapshot::Header& h, const spb::Price& e) {
        SnapshotKey key {e.instrument, h.header.sourceid};
        spb::SeqValue<spb::Price> val { h.sequence(), h.server_time(),  e };

        if(impl_.snapshot(h.sequence(), std::move(key), val))
            send(val);
    }
    
    void on_message(const typename PriceOnline::Header& h, const spb::Price& e) {
        spb::SnapshotKey key { e.instrument, h.header.sourceid };
        spb::SeqValue<spb::Price> val { h.sequence(), h.server_time(),  e };

        if(impl_.update(h.sequence(), std::move(key), val))
            send(val);
    }

    void send(const spb::SeqValue<spb::Price>& e)
    {
        //TOOLBOX_DEBUG << name()<<": "<<e;
        //stats().on_received(d); // FIXME
        for(auto &best: e.value.sub_best) {
            core::Tick ti {};
            ti.type(core::TickType::Update);
            ti.venue_instrument_id(e.value.instrument.instrument_id);
            ti.timestamp(tb::WallClock::now());
            ti.server_timestamp(e.server_timestamp);
            ti.price(protocol().price_conv().to_core(best.price));
            ti.side(get_side(best));
            ti.qty(core::Qty(best.amount));
            if(ti.side()!=core::TickSide::Invalid)
                invoke(ti);
        }
    }
    static constexpr core::TickSide get_side(const SubBest& self) {  
        switch(self.type) {
            case SubBest::Type::Buy: return core::TickSide::Buy;
            case SubBest::Type::Sell: return core::TickSide::Sell;
            default: return core::TickSide::Invalid;
        }
    }
private:
    SpbReplacingUpdates<spb::SnapshotKey, spb::SeqValue<spb::Price> > impl_;
};

}