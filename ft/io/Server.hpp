#pragma once
#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/io/Socket.hpp"
#include <unordered_map>

namespace ft::io {

/// multiple connected remote peers - single server socket
template<typename DerivedT, typename PeerT, typename ServerSocketT>
class BasicServer :  public core::BasicComponent<core::State>,
    public BasicParameterized<DerivedT>
{
public:
    using This = BasicServer<DerivedT, PeerT, ServerSocketT>;
    using Base = core::BasicComponent<core::State>;
    using ServerSocket = ServerSocketT;
    using ClientSocket = typename ServerSocket::ClientSocket;
    using Peer = PeerT;
    using Reactor = typename Peer::Reactor;
    using Endpoint = typename Peer::Endpoint;
    using Protocol = typename Peer::Protocol;
    using Peers = tb::unordered_map<Endpoint, Peer>;
public:
    BasicServer(Reactor* reactor)
    : reactor_{ reactor }
    , serv_{*reactor}
    {}
    void open() {
        serv_.open(*reactor_, Endpoint::protocol_type::v4());
        if constexpr(tb::SocketTraits::has_accept<ServerSocket>) {
            serv_.async_accept(remote_, tb::bind<&This::on_accepted>(this));
        }
    }
    void close() {
        peers_.clear();
        serv_.close();
    }
    Reactor& reactor() { assert(reactor_); return *reactor_; }
    Peers& peers() { return peers_; }

    // need to decouple socket and connection to make multiple connections in single socket possible
    void on_accepted(ClientSocket&& socket, std::error_code ec) {
        // remote_ filled with remote endpoint
        peers_.emplace_back(std::move(socket));
        remote_ = {};
    }
   void start() {
        state(State::Starting);
        open();
        state(State::Started);
    }

    void stop() {
        state(State::Stopping);
        close();
        state(State::Stopped);
    }
protected:
    Reactor* reactor_ {};
    Peers peers_; // connected peers
    ServerSocket serv_;
    Endpoint remote_;
};

} // ft::io