#pragma once
#include "ft/core/Parameters.hpp"
#include "ft/utils/Common.hpp"
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
    using TypedPacket = typename Base::template TypedPacket<MessageT>;

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

    void on_packet(const TypedPacket<SnapshotStart>& e) { 
        auto &d = *e.data();
        
        next_updates_snapshot_seq_ = d.update_seq;
        if(!updates_snapshot_seq_)  // this is "base index" for our updates queue
            updates_snapshot_seq_ = next_updates_snapshot_seq_;
        snapshot_start_seq_ = snapshot_last_seq_ = d.frame.seq;
        assert(snapshot_start_seq_!=0);
        snapshot_.clear(); // we keep only new updates. If Start comes before prev Finish we effectively lost whole snapshot.
        TOOLBOX_DEBUG << name() << " SnapshotStart "
                <<" snapshot: { start:"<<snapshot_start_seq_<<", last:"<<snapshot_last_seq_<<" }"
                <<", updates: { snapshot:"<<updates_snapshot_seq_<<", next_snapshot:"<<next_updates_snapshot_seq_<<", last:"<<updates_last_seq_
                << ", front:"<<updates_front_seq()<<", back:"<<updates_back_seq()<<", count:"<<updates_.size()<<" }";

    }
    std::int64_t updates_front_seq() const {
        if(updates_.empty())
            return invalid_snapshot_seq;
        return updates_.front().frame.seq;
    }
    std::int64_t updates_back_seq() const {
        if(updates_.empty())
            return invalid_snapshot_seq;
        return updates_.back().frame.seq;
    }
    void on_bad_snapshot(const char* reason) {
        TOOLBOX_DEBUG << name() << " BadSnapshot( "<<reason<<" )"
            <<", snapshot: { start:"<<snapshot_start_seq_<<", last:"<<snapshot_last_seq_<<", count:"<<snapshot_.size()<<" }"
            <<", updates: { snapshot:"<<updates_snapshot_seq_<<", last:"<<updates_last_seq_ << ", front:"<<updates_front_seq() 
            <<", back:"<< updates_back_seq() << ", count:"<<updates_.size() <<" }";
    }
    void on_packet(const TypedPacket<SnapshotFinish>& e) { 
        auto &d = *e.data();
        snapshot_last_seq_ = d.frame.seq;
        if(!snapshot_start_seq_) {
            // Start was lost or we connected after Start before Finish
            on_bad_snapshot("finish without start");
        } else if(d.update_seq!=next_updates_snapshot_seq_) { 
            // Start was lost or next Start came before prev Finish (reordered)
            on_bad_snapshot("finish with wrong start");
        } else if(std::any_of(snapshot_.begin(), snapshot_.end(), [](auto &x) { return x.empty(); })) {
            // some slots were not filled with snapshot
            on_bad_snapshot("gaps in snapshot");
        } /*else if(updates_front_seq()!=updates_snapshot_seq_+1) {
            on_bad_snapshot("no valid update");
        } */else {
            // we fully received new snapshot here
            if(updates_snapshot_seq_!=next_updates_snapshot_seq_) {
                // drop updates before this snapshot
                updates_.erase(updates_.begin(), updates_.begin()+std::min(updates_.size(), next_updates_snapshot_seq_-updates_snapshot_seq_));
                updates_snapshot_seq_ = next_updates_snapshot_seq_;
            }
        
            // give them snapshot. updates coming after snapshot should be merged
            for(auto &s: snapshot_) {
                on_packet_online(s);
            }

            if(updates_last_seq_<updates_snapshot_seq_) {
                // GAPS: updates between updates_last and updates_snapshot are lost
                std::size_t ngaps = updates_snapshot_seq_ - updates_last_seq_;
                TOOLBOX_DEBUG<<name()<<" SnapshotFinish with gaps"
                    <<", snapshot: { start:"<<snapshot_start_seq_<<", last:"<<snapshot_last_seq_<<" }"
                    <<", updates: { snapshot:"<<updates_snapshot_seq_<<", last:"<<updates_last_seq_
                    <<", front:"<<updates_front_seq()<<", back:"<<updates_back_seq()<<", count:"<<updates_.size() <<", ngaps:" <<ngaps<<"}";
                updates_last_seq_ = updates_snapshot_seq_;
            }

            send_updates("SnapshotFinish");
        }
        snapshot_.clear();
        snapshot_start_seq_ = snapshot_last_seq_  = invalid_snapshot_seq;
        next_updates_snapshot_seq_ = invalid_snapshot_seq;
        // keep updates_snapshot_seq_+1 pointing to updates_ first element
    }
    void on_packet(const TypedPacket<PriceSnapshot>& e) {
        // cache price snapshots
        auto &d = *e.data();
        if(!snapshot_start_seq_)
        {
            //TOOLBOX_DEBUG<<"Snapshot without SnapshotStart, seq:"<<d.frame.seq;
        }else {
            snapshot_last_seq_ = d.frame.seq;
            // place in correct position
            std::size_t index = d.frame.seq - snapshot_start_seq_;
            snapshot_.resize(std::max(snapshot_.size(), index+1));
            snapshot_[index] = *e.data();
        }
        //on_price_packet(e);
    }
    void on_packet(const TypedPacket<PriceOnline>& e) {
        auto& d = *e.data();
        if(!updates_snapshot_seq_) { // no snapshot yet
            
        } else {
            if(d.frame.seq>=updates_snapshot_seq_+1) {
                std::size_t index = d.frame.seq - updates_snapshot_seq_ - 1;
                updates_.resize(std::max(updates_.size(), index+1));
                updates_[index] = d;
                if(updates_last_seq_)
                    send_updates("Online");
            }
        }
    }
    struct SendResult {
        std::size_t nsent{};
        std::size_t ngaps{};
    };

    SendResult send_updates(const char* reason) {
        // check if we can send it
        SendResult result;

        assert(updates_snapshot_seq_);
        assert(updates_last_seq_);
        assert(updates_last_seq_>=updates_snapshot_seq_);
        
        for(std::int64_t index=(updates_last_seq_+1)-(updates_snapshot_seq_+1); index<updates_.size(); index++) {
            auto& u = updates_[index];
            if(!u.frame.seq) {
                #ifdef FT_REPORT_GAPS
                TOOLBOX_DEBUG<<name()<<" "<<reason<<" updates_gap:"<<updates_last_seq_+1<<", nsent:"<<nsent<<", ngaps:"<<ngaps
                <<", snapshot: { start:"<<snapshot_start_seq_<<", last:"<<snapshot_last_seq_<<" }"
                <<", updates: { snapshot:"<<updates_snapshot_seq_<<", last:"<<updates_last_seq_<<", count:"<<updates_.size() 
                <<" }";
                #endif
                /*constexpr std::size_t ignore_gaps_queue_size = 100;
                if(updates_.size()-index < ignore_gaps_queue_size)    // wait for gaps until queue is too big
                    break; // some gap policy here? tcp replay? ignore?
                */
                updates_last_seq_++;   
                result.ngaps++;                 
            } else {
                assert(u.frame.seq == updates_last_seq_+1);
                updates_last_seq_++;
                on_packet_online(u);
                result.nsent++;                
            }
        }
        if(result.nsent>1)
        TOOLBOX_DEBUG<<name()<<" "<<reason
                <<", snapshot: { start:"<<snapshot_start_seq_<<", last:"<<snapshot_last_seq_<<" }"
                <<", updates: { snapshot:"<<updates_snapshot_seq_<<", last:"<<updates_last_seq_
                <<", front:"<<updates_front_seq()<<", back:"<<updates_back_seq()<<", count:"<<updates_.size()
                <<", nsent:"<<result.nsent<<", ngaps:"<<result.ngaps
                <<" }";

        return result;
    }
    template<typename PriceOnlineT>
    void on_packet_online(const PriceOnlineT& d)
    {
        //TOOLBOX_DEBUG << name()<<": "<<e;
        //stats().on_received(d); // FIXME
        for(auto &best: d.sub_best) {
            core::Tick ti {};
            ti.type(core::TickType::Update);
            ti.venue_instrument_id = d.instrument.instrument_id;
            ti.timestamp = tb::WallClock::now();
            ti.server_timestamp = d.header.system_time.wall_time();
            ti.price = protocol().price_conv().to_core(best.price);
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
    // note: this works while 0 is bad seq num
    static constexpr std::uint64_t invalid_snapshot_seq = 0;

    std::vector<PriceSnapshot> snapshot_;
    std::uint64_t snapshot_start_seq_{};    // when started
    std::uint64_t snapshot_last_seq_{};     // last snapshot seq
    std::uint64_t updates_snapshot_seq_ {};
    std::uint64_t next_updates_snapshot_seq_ {};


    // this are updates.
    std::deque<PriceOnline> updates_;
    std::uint64_t updates_last_seq_{};
};

}