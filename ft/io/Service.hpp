#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/io/Runner.hpp"
#include "ft/io/Router.hpp"
#include "toolbox/util/Slot.hpp"

namespace ft::io {

using Reactor = toolbox::io::Reactor;

Reactor* current_reactor();

template<typename ReactorT, typename StateT>
class BasicComponent : public core::BasicComponent<StateT> {
    using Base = core::BasicComponent<StateT>;
public:
    using Reactor = ReactorT;
    BasicComponent(Reactor* reactor=nullptr) {
        reactor_ = reactor;
    }
    using Base::state, Base::state_changed;

    Reactor* reactor() { return reactor_; }
protected:
    Reactor* reactor_{};
};

using Service = BasicComponent<io::Reactor, core::State>;

/// Service =  Parameterized + Reactive + Component
template<typename DerivedT, typename ReactorT, typename StateT>
class BasicService : public io::BasicComponent<ReactorT, StateT>
, public core::BasicParameterized<DerivedT>
{
    using Base = io::BasicComponent<ReactorT, StateT>;
    using Self = DerivedT;
    Self* self() { return static_cast<Self*>(this); }
public:
    using typename Base::Reactor;
    
    using Base::Base;
    using Base::state, Base::reactor, Base::state_changed;

    // Copy
    BasicService(const BasicService&) = delete;
    BasicService&operator=(const BasicService&) = delete;

    // Move
    BasicService(BasicService&&) = default;
    BasicService& operator=(BasicService&&) = default;

    void open() { self()->on_open(); }
    void on_open() {}
    void close() { self()->on_close(); }
    void on_close() {}

    bool is_open() const { 
        auto s = state();
        switch(s) {
            case State::Started: return true;
            default: return false;
        }
    }
    void start() {
        state(State::Starting);
        self()->open();
        state(State::Started);
    }

    void stop() {
        state(State::Stopping);
        self()->close();
        state(State::Stopped);
    }
};


template<typename DerivedT, typename ProtocolT, typename PeerT, typename StateT>
class BasicPeersService: public BasicService<DerivedT, typename PeerT::Reactor, StateT>
, public io::BasicRouter<DerivedT>
{
    using Base = BasicService<DerivedT, typename PeerT::Reactor, StateT>;
    using Self = DerivedT;
    Self* self() { return static_cast<Self*>(this); }
public:
    using Reactor = typename PeerT::Reactor;
    using Peer = PeerT;
    using Socket = typename Peer::Socket;
    using Endpoint = typename Peer::Endpoint;
    using Packet = typename Peer::Packet;
    using Transport = typename Socket::Protocol;
    using PeersMap = tb::unordered_map<Endpoint, Peer>;
    using Protocol = ProtocolT;
public:
    using Base::state, Base::open, Base::close;

    using Base::Base;
    explicit BasicPeersService(Reactor* reactor, Transport transport)
    : Base(reactor)
    , transport_(transport)
    {}

    /// transport level
    Transport& transport() { return transport_; }
    /// application level
    Protocol& protocol() { return protocol_; }
    
    /// read buffer size
    constexpr std::size_t buffer_size() { return 4096; };
    tb::Buffer& rbuf() { return rbuf_; }   
    
    /// connected peers
    const PeersMap& peers() const { return peers_; }
    Peer& emplace(Peer&& peer) {
        auto it = peers_.emplace(peer.remote(), std::move(peer));
        return it.first->second;
    }

    /// reopens if started
    void on_parameters_updated(const core::Parameters& params) {
        if(state()==State::Started) {
            self()->close();
            self()->open();
        }
    }
    
    void open() {
        for(auto&it: peers()) {
            auto& c = it.second;
            c.open(self(), transport());
        }
        protocol().open();
        self()->on_open();
    }

    void close() {  
        protocol().close();
        TOOLBOX_INFO << "Closing "<< peers().size() << " peers";
        for(auto & [ep, c] : peers()) {
            c.close();
        }
        peers_.clear();
        self()->on_close();
    }
    
    /// via protocol
    template<typename MessageT>
    void async_write(Peer& c, const MessageT& m, tb::SizeSlot done) {
        protocol_.async_write(c, m, done);
    }
    
    /// via protocol
    template<typename MessageT>
    void async_read(Peer& c, const MessageT& m, tb::SizeSlot done) {
        protocol_.async_read(c, m, done);
    }

    /// drain data from the peer and process them
    void async_drain(Peer& peer, tb::Slot<Peer&, std::error_code> done) {
        drain_.set_slot(done);
        async_read(peer, self()->rbuf().prepare(self()->buffer_size()), 
            tb::bind([&peer](ssize_t size, std::error_code ec) {
                auto* self = static_cast<Self*>(peer.service());
                if(!ec) {
                    self->async_process(peer, size, tb::bind([&peer](std::error_code ec) {
                        auto* self = static_cast<Self*>(peer.service());
                        auto rsize = peer.packet().buffer().size(); 
                        self->rbuf().consume(rsize); // size from packet
                        self->drain_.notify(peer, ec);
                    }));
                } else {
                    self->drain_.notify(peer, ec);    // error
                }
            })
        );
    }
    void async_process(Peer& peer, ssize_t size, tb::Slot<std::error_code> done) {
        auto& packet = peer.packet(size);
        packet.header().recv_timestamp(tb::CyclTime::now().wall_time());
        assert(packet.buffer().size() == size);
        async_process(peer, packet, done);
    }
    /// process using protocol
    void async_process(Peer& src, Packet& packet, tb::Slot<std::error_code> done) {
        protocol().async_process(src, packet, done);
    }
protected:
    PeersMap& peers() { return peers_; }
protected:
    PeersMap peers_;
    Transport transport_ = Transport::v4();
    tb::CompletionSlot<Peer&, std::error_code> drain_;
    tb::Buffer rbuf_;
    Protocol protocol_;
};

} // ft::io