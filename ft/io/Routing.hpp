#pragma once
#include <utility>
#include "ft/utils/Common.hpp"
#include <toolbox/util/Slot.hpp>
#include <toolbox/io/Socket.hpp>
#include "ft/core/Component.hpp"

namespace ft::io {


template<class PeerT, class PeerPtrT=std::unique_ptr<PeerT>>
using PeersMap = tb::unordered_map<PeerId, PeerPtrT>;

/// @brief: round-robin peer selected and function applied
/// @returns: number of peers applied
template<typename PeersMapT>
class RoundRobinRouting {
public:
    using Peer = std::decay_t<decltype(*std::declval<typename PeersMapT::mapped_type>())>;
    using iterator = typename PeersMapT::iterator;

    template<typename FnT>
    int operator()(PeersMapT& peers, FnT&& fn)  {
        if(it_ == iterator() || it_ == peers.end())
            it_ = peers.begin();
        else {
            if(++it_ == peers.end())
                it_ = peers.begin();
        }
        if(it_!=peers.end()) {
            Peer* peer = it_->second.get();
            assert(peer);
            fn(*peer);
            return 1;
        }
        return 0;
    }
    void reset() { it_ = iterator(); }
protected:
    iterator it_;
};

template<typename PeersMapT>
class BroadcastRouting {
public:
    using Peer = std::decay_t<decltype(*std::declval<typename PeersMapT::mapped_type>())>;
    using iterator = typename PeersMapT::iterator;

    template<typename FnT>
    int operator()(PeersMapT& peers, FnT&& fn)  {
        std::size_t npeers = peers.size();
        if(it_ == iterator() || it_ == peers.end())
            it_ = peers.begin();
        else {
            if(++it_ == peers.end())
                it_ = peers.begin();
        }
        if(it_!=peers.end()) {
            Peer* peer = it_->second.get();
            assert(peer);
            fn(*peer);
            return 1;
        }
        return 0;
    }
    void reset() { it_ = iterator(); }
protected:
    iterator it_{};
};

/// Route to peer according to RoutingStrategyT
template<class ParentT, class PeersMapT, template<class> class RoutingStrategyT>
class BasicRouter  {
public:
    using Peer = std::decay_t<decltype(*std::declval<typename PeersMapT::mapped_type>())>;
    using PeersMap = PeersMapT;

    BasicRouter(ParentT* parent, RoutingStrategyT<PeersMap>&& strategy={})
    : parent_(parent)
    , strategy_(std::move(strategy))
    {}

    ParentT* parent() { return parent_; }

    template<typename MessageT>
    void async_write(const MessageT& m, tb::SizeSlot done) {
        assert(write_.empty());    
        write_.set_slot(done);     
        int pending = 0;   
        auto peers = parent()->peers();
        strategy_(peers, [&m, &pending, this](auto& peer) {
            pending++;                
            parent()->async_write(peer, m, tb::bind([this](ssize_t size, std::error_code ec) {
                write_.invoke(size, ec);
            }));
        });
        write_.pending(pending);
    } 
protected:
    ParentT* parent_{};
    RoutingStrategyT<PeersMap> strategy_;
    tb::PendingSlot<ssize_t, std::error_code> write_;
};

} // ft::io