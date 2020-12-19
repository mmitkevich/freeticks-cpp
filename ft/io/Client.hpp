#pragma once

#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/io/Reactor.hpp"
#include <sstream>
#include <toolbox/io/Socket.hpp>
#include <ft/utils/StringUtils.hpp>

namespace ft::io {


/// client could have many connections, but they all conform to single protocol
template<typename DerivedT, typename ProtocolT, typename ConnT
> class BasicClient : public core::BasicComponent<core::State>,
    public core::BasicParameterized<DerivedT>
{
    using This = BasicClient<DerivedT, ProtocolT, ConnT>;
    using Base = core::BasicComponent<core::State>;
public:
    using Connection = ConnT;
    using BinaryPacket = typename Connection::Packet;
    using Socket = typename Connection::Socket;
    using Endpoint = typename Connection::Endpoint;
    using typename Base::State;
    using Connections = tb::unordered_map<Endpoint, Connection>;
    using Reactor = typename ConnT::Reactor;    // reactor-aware connection
    using Protocol = ProtocolT;
public:
    template<typename...ArgsT>
    BasicClient(Reactor* reactor, ArgsT...args)
    : reactor_{ reactor }
    , protocol_(std::forward<ArgsT>(args)...)
    {}

    using Base::state;

    Reactor& reactor() { assert(reactor_); return *reactor_; }

    Connections& connections() { return connections_; }

    auto& stats() { return protocol_.stats(); }
    
    Protocol& protocol() {   return protocol_; }

    bool is_open() const { 
        auto s = state();
        switch(s) {
            case State::Started: return true;
            default: return false;
        }
    }

    void open() {
        //TOOLBOX_DUMP_THIS;
        TOOLBOX_INFO << "Opening "<< connections_.size() << " connections";
        for(auto& [ep, c] : connections_) {
            c.open(reactor_, {});
        }
        protocol_.open();        
    }
    
    void close() {
        protocol_.close();
        TOOLBOX_INFO << "Closing "<< connections_.size() << " connections";
        for(auto & [ep, c] : connections_) {
            c.close();
        }
    }

    void reopen(Connections& conns) {
        bool was_opened = is_open();
        if(was_opened)
            close();
        std::swap(connections_, conns);
        if(was_opened)
            open();
    }

    void start() {
        state(State::Starting);
        using namespace std::literals::chrono_literals;
        idle_timer_ = reactor().timer(tb::MonoClock::now()+1s, 10s, 
            tb::Priority::Low, tb::bind<&This::on_idle_timer>(this));
        open();
        state(State::Started);
    }

    void stop() {
        state(State::Stopping);
        close();
        idle_timer_.cancel();
        state(State::Stopped);
    }

    Connections make_connections(const core::Parameters& params) {
        Connections conns;
        for(auto p: params) {
            std::string_view type = p["type"].get_string();
            auto proto = ft::str_suffix(type, '.');
            auto iface = p.value_or("interface", std::string{});
            if(Connection::supports(proto)) {
                for(auto e : p["urls"]) {
                    std::string url {e.get_string()};
                    if(!iface.empty())
                        url = url+"|interface="+iface;
                    Connection c;
                    c.url(url);
                    auto res = conns.emplace(c.remote(), std::move(c));
                    if(res.second) {
                        res.first->second.packet().connect(tb::bind<&This::on_packet>(this));
                    }
                }
            }
        }
        return conns;
    }

    void on_packet(const BinaryPacket& packet, tb::Slot<std::error_code> slot) {
        protocol_.on_packet(packet);    // will do all the stuuf
        slot({});   // done
    }
    void on_idle_timer(tb::CyclTime now, tb::Timer& timer) {
        std::stringstream ss;
        protocol_.stats().report(ss);
        TOOLBOX_INFO << ss.str();
    }
protected:
    Reactor* reactor_{nullptr};
    Protocol protocol_;
    Connections connections_;
    tb::Timer idle_timer_; 
};

} // namespace ft::io