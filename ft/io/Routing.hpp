#pragma once
#include <utility>
#include "ft/utils/Common.hpp"
#include <toolbox/util/Slot.hpp>
#include <toolbox/io/Socket.hpp>
#include "ft/core/Component.hpp"

namespace ft::io {


template<class PeerT, class KeyT=typename PeerT::Endpoint>
using PeersMap = tb::unordered_map<KeyT, std::unique_ptr<PeerT>>;

/// @brief: round-robin peer selected and function applied
/// @returns: number of peers applied
template<typename PeerT>
class RoundRobinRoutingStrategy {
public:
    using Peer = PeerT;
    using iterator = typename PeersMap<Peer>::iterator;

    template<typename FnT>
    int operator()(PeersMap<Peer>& peers, FnT&& fn)  {
        if(peers.empty())
            return 0;
        if(it_!=iterator() && it_!=peers.end())
            ++it_;
        else 
            it_ = peers.begin();
        fn(*it_->second);
        return 1;
    }
    void reset() { it_ = iterator(); }
protected:
    iterator it_{};
};

/// Route to peer according to RoutingStrategyT
template<class ParentT, class PeerT, class RoutingStrategyT>
class BasicRouting  {
public:
    using Peer = PeerT;

    BasicRouting(ParentT* parent, RoutingStrategyT&& strategy={})
    : parent_(parent)
    , strategy_(std::move(strategy))
    {}

    ParentT* parent() { return parent_; }

    template<typename MessageT>
    void async_write(const MessageT& m, tb::SizeSlot done) {
        assert(write_.empty());    
        write_.set_slot(done);     
        int pending = 0;   
        strategy_(parent()->peers(), [&m, &pending, this](auto& peer) {
            pending++;                
            parent()->async_write(peer, m, tb::bind([this](ssize_t size, std::error_code ec) {
                write_.invoke(size, ec);
            }));
        });
        write_.pending(pending);
    } 
protected:
    ParentT* parent_{};
    RoutingStrategyT strategy_;
    tb::PendingSlot<ssize_t, std::error_code> write_;
};

} // ft::io