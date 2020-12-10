#pragma once

#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/io/Reactor.hpp"

namespace ft::io {

template<typename DerivedT, typename ProtocolT, typename ConnT, typename ReactorT = tb::Reactor>
class BasicService : public core::BasicComponent<DerivedT, core::State> {
    using This = BasicService<DerivedT, ProtocolT, ConnT>;
    using Base = core::BasicComponent<DerivedT, core::State>;
public:
    using Protocol = ProtocolT;
    using Connection = ConnT;
    using BinaryPacket = typename Connection::Packet;
    using Socket = typename Connection::Socket;
    using Endpoint = typename Connection::Endpoint;
    using State = typename Base::State;
    using Connections = std::vector<Connection>;
    using Reactor = ReactorT;
public:
    BasicService(Reactor* reactor)
    : Base{ reactor } {}

    using Base::state, Base::reactor;

    Protocol& protocol() {   return protocol_; }
    Connections& connections() { return connections_; }

    auto& stats() { return protocol_.stats(); }
        
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
        for(auto& c : connections_) {
            c.open();
        }
        protocol_.open();        
    }
    
    void close() {
        protocol_.close();
        TOOLBOX_INFO << "Closing "<< connections_.size() << " connections";
        for(auto &c : connections_) {
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
        run();
    }

    void run() { }

    void stop() {
        state(State::Stopping);
        close();
        idle_timer_.cancel();
        state(State::Stopped);
    }
   
    void report(std::ostream& os) {
        protocol_.stats().report(os);
    }
    
    void on_idle_timer(tb::CyclTime now, tb::Timer& timer) {
        on_idle();
    }
    void on_idle() {
        report(std::cerr);
    }
protected:
    Connections connections_;
    Protocol protocol_;
    tb::Timer idle_timer_; 
};

} // namespace ft::io