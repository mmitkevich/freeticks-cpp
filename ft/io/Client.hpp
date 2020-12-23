#pragma once

#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/util/Slot.hpp"
#include <boost/mp11/algorithm.hpp>
#include <sstream>
#include <toolbox/io/Socket.hpp>
#include <ft/utils/StringUtils.hpp>
#include "ft/io/Router.hpp"

namespace ft::io {

template<typename ConnectionsT>
class RouteRoundRobin {
public:
    using Connection = typename ConnectionsT::mapped_type;

    template<typename RequestT>
    Connection* operator()(ConnectionsT& conns, RequestT& req)  {
        if(conns.empty())
            return nullptr;
        if(it_!=typename ConnectionsT::iterator() && it_!=conns.end())
            ++it_;
        else 
            it_ = conns.begin();
        return &it_->second;
    }
protected:
    typename ConnectionsT::iterator it_{};
};

template<typename DerivedT, 
    typename ConnT, // BasicConn<>
    typename ResponseSlotsTypeList = mp::mp_list<> // typelist with possible response types
> class BasicClient : public core::BasicComponent<core::State>
, public core::BasicParameterized<DerivedT>
, public io::BasicRouter<DerivedT>
{
    using This = BasicClient<DerivedT, ConnT>;
    using Base = core::BasicComponent<core::State>;
    using Router = io::BasicRouter<DerivedT>;
    using ResponseSlotTuple = mp::mp_rename<ResponseSlotsTypeList,std::tuple>;
public:
    using Connection = ConnT;
    using BinaryPacket = typename Connection::Packet;
    using Socket = typename Connection::Socket;
    using Endpoint = typename Connection::Endpoint;
    using typename Base::State;
    using Connections = tb::unordered_map<Endpoint, Connection>;
    using Reactor = typename ConnT::Reactor;    // reactor-aware connection
public:
    BasicClient(Reactor* reactor)
    : reactor_{ reactor }
    {}

    using Self = DerivedT;
    Self* self() { return static_cast<DerivedT*>(this); }


    using Base::state;

    Reactor& reactor() { assert(reactor_); return *reactor_; }

    Connections& connections() { return connections_; }

    bool is_open() const { 
        auto s = state();
        switch(s) {
            case State::Started: return true;
            default: return false;
        }
    }

    // open and close are non-blocking

    void open() {
        //TOOLBOX_DUMP_THIS;
        TOOLBOX_INFO << "Opening "<< connections_.size() << " connections";
        for(auto& [ep, c] : connections_) {
            c.open(reactor_);
        }  
    }
    
    void async_connect(tb::Slot<std::error_code> slot) {
        for(auto& [ep, c] : connections_) {
            c.async_connect(c.remote(), slot);
        }          
    }

    void on_connect() {

    }

    void close() {
        TOOLBOX_INFO << "Closing "<< connections_.size() << " connections";
        for(auto & [ep, c] : connections_) {
            c.close();
        }
    }

    void reopen(Connections& conns) {
        bool was_opened = is_open();
        if(was_opened)
            self()->close();
        std::swap(connections_, conns);
        if(was_opened)
            self()->open();
    }

    void start() {
        state(State::Starting);
        using namespace std::literals::chrono_literals;
        idle_timer_ = reactor().timer(tb::MonoClock::now()+1s, 10s, 
            tb::Priority::Low, tb::bind<&This::on_idle_timer>(this));
        self()->open();
        state(State::Started);
    }

    void stop() {
        state(State::Stopping);
        self()->close();
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
                    c.remote(url);
                    auto res = conns.emplace(c.remote(), std::move(c));
                    if(res.second) {
                        res.first->second.received().connect(tb::bind<&This::on_packet>(this));
                    }
                }
            }
        }
        return conns;
    }
    
    template<typename T> 
    using ResponseSlotTT = tb::Slot<const T&, std::error_code>;
    using SlotTypeList = mp::mp_transform<ResponseSlotTT, ResponseSlotsTypeList>;

    template<typename RequestT, typename ResponseT>
    void async_request(const RequestT& req, tb::Slot<const ResponseT&, std::error_code> slot, tb::DoneSlot done) 
    {
        io::RouteRoundRobin<Connections> route;
        auto* conn = route(connections(), req);
        if(conn)
            self()->async_request(*conn, req, slot, done);
    }
    
    /// by default just write message on wire
    /// need to override in derived class to use some other protocol
    template<typename RequestT, typename ResponseT>
    void async_request(Connection& conn, const RequestT& req, 
        tb::Slot<const ResponseT&, std::error_code> slot, tb::DoneSlot done) 
    {
        // save callback to launch after we got response
        std::get< mp::mp_find<ResponseSlotTT<ResponseT>,SlotTypeList>::value>(responses_) = slot;
        conn.async_write(tb::ConstBuffer{req, req.length()}, done);
    }

    /// process packet
    void on_packet(const BinaryPacket& packet, tb::Slot<std::error_code> slot) {
        slot({}); // immediately done
    }

    void on_idle_timer(tb::CyclTime now, tb::Timer& timer) {
       self()->on_idle();
    }

    /// idle tasks
    void on_idle() {}

protected:
    Reactor* reactor_{nullptr};;
    Connections connections_;
    tb::Timer idle_timer_; 
    ResponseSlotTuple responses_;
};

} // namespace ft::io