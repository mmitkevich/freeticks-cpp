#pragma once

#include "ft/utils/Common.hpp"

namespace ft::spb {

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
    std::uint64_t updates_snapshot_seq() const {
        return updates_last_seq_;
    }
    std::pair<const V*, bool> snapshot(std::uint64_t seq, const K& key,  const V& value) {
        if(!snapshot_start_seq_) {
            //log_state(seq,"Snapshot: No SnapshotStart");
            return {&value, false};
        }
        snapshot_last_seq_ = seq;
        snapshot_size_++;
        
        snapshots_stats_.on_received();

        SeqValue<V>& prev = snapshot_[key];
        if(prev.seq == invalid_seq || updates_snapshot_seq_>prev.seq) {
            prev = SeqValue<V>{updates_snapshot_seq_, value};
            return {&prev.value, true};
        }else {
            //log_state(updates_snapshot_seq_, "Snapshot: Stale ignored");    
            snapshots_stats_.on_rejected();
            return {&prev.value, false};
        }
    }
    std::pair<const V*,bool> update(std::uint64_t seq, const K& key,  const V& value) {
        if(updates_last_seq_!=invalid_seq && seq>updates_last_seq_ + 1) {
            updates_stats_.on_gap(seq-(updates_last_seq_ + 1));
        }
        updates_last_seq_ = std::max(updates_last_seq_, seq);
        
        updates_stats_.on_received();

        SeqValue<V>& prev = snapshot_[key];
        if(prev.seq == invalid_seq || seq>prev.seq) {
            prev = SeqValue<V>{seq, value};
            return {&prev.value,true};
        }else {
            //log_state(seq, "Update: Stale ignored");    
            updates_stats_.on_rejected();
            return {&prev.value,false};
        }
    }
    static constexpr bool log_state_enabled = true;
    void log_state(std::uint64_t seq, const char* reason) {
        if constexpr(log_state_enabled) {
            TOOLBOX_DEBUG << name_ << " " << reason << " seq:"<<seq
            <<", updates: { snapshot:"<<updates_snapshot_seq_<<", last:"<<updates_last_seq_<<", stats: {"<<updates_stats_<<"}}"
            <<", snapshot: { start:"<<snapshot_start_seq_<<", last:"<<snapshot_last_seq_<<", size:"<<snapshot_.size()<<", stats:{"<<snapshots_stats_<<"}}";
        }
    }
private:
    std::string_view name_ {};
    
    ft::unordered_map<K, SeqValue<V>> snapshot_;

    std::uint64_t snapshot_start_seq_{};    // when started
    std::uint64_t snapshot_last_seq_{};     // last snapshot seq
    std::uint64_t snapshot_size_{};     // last snapshot seq

    std::uint64_t updates_snapshot_seq_ {};

    std::uint64_t updates_last_seq_{};

    core::StreamStats updates_stats_ {};
    core::StreamStats snapshots_stats_ {};
    
};


} // ns 