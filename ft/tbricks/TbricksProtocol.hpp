#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/tbricks/TbricksSchemaV1.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/io/Service.hpp"
#include "ft/io/Conn.hpp"

namespace ft::tbricks {


template<typename BinaryPacketT>
class TbricksProtocol : public io::BasicProtocol {
    using This = TbricksProtocol<BinaryPacketT>;
    using Base = io::BasicProtocol;
public:
    using Message = tbricks::v1::Message;
    using MessageType = tbricks::v1::MessageType;
    using Sequence = tbricks::v1::Sequence;
    using BinaryPacket = BinaryPacketT;

    using Base::Base;
    
    template<typename /*HeartBeated*/ ConnT>
    void async_write(ConnT& conn, const SubscriptionRequest& request, tb::DoneSlot slot) {
        // can't send request while some other request isn't written to media
        assert(conn.can_write());
        // remember request id
        switch(request.request()) {
            case core::Request::Subscribe: {
                Message msg(MessageType::SubscriptionRequest);
                msg.seq(++out_seq_);
                msg.symbol(request.symbol());
                conn.async_write(msg, slot);
            } break;
            default: assert(false);
        }
    }
    template<typename ConnT>
    void on_packet(ConnT& conn, const BinaryPacket& e, tb::DoneSlot done) { 
        auto& buf = e.buffer();
        const Message& msg = *reinterpret_cast<const Message*>(buf.data());
        switch(msg.msgtype) {
            case MessageType::SubscriptionRequest: {
                core::SubscriptionRequest req;
                req.symbol(msg.symbol());
                req.request(Request::Subscribe);
                subscribe_req().invoke(std::move(req));
            } break;
            case MessageType::SubscriptionCancelRequest: {
                core::SubscriptionRequest req;
                req.symbol(msg.symbol());
                req.request(core::Request::Unsubscribe);
                subscribe_req().invoke(std::move(req));
            } break;
            case MessageType::ClosingEvent: {
                core::SubscriptionRequest req;
                req.symbol(msg.symbol());
                req.request(core::Request::UnsubscribeAll);
                subscribe_req().invoke(std::move(req));
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

        }
    }
    auto& stats() { return stats_; }
    core::SubscriptionSignal & subscribe_req() { return subscribe_req_; }
    core::TickStream& ticks(core::StreamTopic topic) {
        return ticks_;
    }
    core::InstrumentStream& instruments(core::StreamTopic topic) {
        return instruments_;
    }    
    
    void on_parameters_updated(const core::Parameters& params) {

    }

protected:
    // from client 
    core::SubscriptionSignal subscribe_req_;

    // from server
    core::TickStream ticks_;
    core::InstrumentStream instruments_;
    
    // to server
    Sequence out_seq_ {};
    RequestId  out_req_id_;

    // from server
    Message response_;

    core::StreamStats stats_;
    
    tb::Timer heartbeats_timer_;
};

}