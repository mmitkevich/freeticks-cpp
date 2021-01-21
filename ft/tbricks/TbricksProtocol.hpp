#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/tbricks/TbricksSchemaV1.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/io/Buffer.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/io/Service.hpp"
#include "ft/io/Conn.hpp"
#include <system_error>
#include "ft/utils/StringUtils.hpp"

namespace ft::tbricks {


template<typename BinaryPacketT>
class TbricksProtocol : public io::BasicProtocol<TbricksProtocol<BinaryPacketT>> {
    using This = TbricksProtocol<BinaryPacketT>;
    using Base = io::BasicProtocol<This>;
public:
    using Message = tbricks::v1::Message<0>;
    using MessageOut = tbricks::v1::Message<4096>;
    using MessageType = tbricks::v1::MessageType;
    using Sequence = tbricks::v1::Sequence;
    using BinaryPacket = BinaryPacketT;

    using Base::Base;
    using Base::async_write;

    template<typename ConnT, typename DoneT>
    void async_write(ConnT& conn, const SubscriptionRequest& request, DoneT done) {
        // remember request id
        switch(request.request()) {
            case core::Request::Subscribe: {
                // FIXME: redo as stream << into buffer?
                MessageOut msg(MessageType::SubscriptionRequest);
                msg.symbol() = request.symbol();
                msg.seq() = ++out_seq_;
                auto buf = tb::to_const_buffer(msg);
                TOOLBOX_INFO << name()<<": out["<<msg.bytesize()<<"]: seq="<<msg.seq()<<"; sym="<<msg.symbol().str()<<"\n" << ft::to_hex_dump(std::string_view{(const char*)buf.data(), buf.size()});
                conn.async_write(buf, done);
            } break;
            default: {
                assert(false);
                done(0, {}); // should be always called
            }
        }
    }
    constexpr std::string_view name() { return "TB1"; }
    
    template<typename ConnT, typename DoneT>
    void async_handle(ConnT& conn, const BinaryPacket& e, DoneT done) { 
        std::error_code ec{};

        auto& buf = e.buffer();
        const Message& msg = *reinterpret_cast<const Message*>(buf.data());        
        TOOLBOX_INFO << name()<<": in["<<e.buffer().size()<<"]: t="<<tb::unbox(msg.msgtype())<<"\n"<<
            ft::to_hex_dump(std::string_view{(const char*)e.buffer().data(), e.buffer().size()});
        switch(msg.msgtype()) {
            case MessageType::SubscriptionRequest: {
                TOOLBOX_INFO << name()<<": in: t:"<<(int)tb::unbox(msg.msgtype())<<", sym:'"<<msg.symbol().str()<<"', seq:"<<msg.seq();
                core::SubscriptionRequest req {};
                req.symbol(msg.symbol().str());
                req.request(Request::Subscribe);
                TOOLBOX_INFO << req;
                subscribe().invoke(conn.id(), std::move(req));
            } break;
            case MessageType::SubscriptionCancelRequest: {
                core::SubscriptionRequest req;
                req.symbol(msg.symbol().str());
                req.request(core::Request::Unsubscribe);
                subscribe().invoke(conn.id(), std::move(req));
            } break;
            case MessageType::ClosingEvent: {
                core::SubscriptionRequest req;
                req.symbol(msg.symbol().str());
                req.request(core::Request::UnsubscribeAll);
                subscribe().invoke(conn.id(), std::move(req));
            } break;
            case MessageType::HeartBeat: {
                if constexpr(io::HeartbeatsTraits::has_heartbeats<ConnT>) {
                    conn.on_heartbeats();
                }
            } break;
            case MessageType::MarketData: {
                if constexpr(io::HeartbeatsTraits::has_heartbeats<ConnT>) {
                    conn.on_heartbeats();
                }
            } break;
            default:
                TOOLBOX_ERROR << "TbricksV1: unknown msgtype "<<tb::unbox(msg.msgtype());
                ec = std::make_error_code(std::errc::invalid_argument);
        }
        done(ec);
    }

    auto& stats() { return stats_; }

    /// requests
    core::SubscriptionSignal& subscribe() { return subscribe_; }

    /// streams
    core::TickStream& ticks(core::StreamTopic topic) { return ticks_; }
    core::InstrumentStream& instruments(core::StreamTopic topic) { return instruments_; }    
    
    void on_parameters_updated(const core::Parameters& params) { }

protected:
    core::StreamStats stats_;
    // from client to server
    core::SubscriptionSignal subscribe_;
    // from server to client
    core::TickStream ticks_;
    core::InstrumentStream instruments_;
    // from client to server 
    Sequence out_seq_ {};
    RequestId  out_req_id_;
    // from server
    Message response_;
};

}