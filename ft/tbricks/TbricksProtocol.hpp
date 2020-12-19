#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/tbricks/TbricksSchemaV1.hpp"
#include "ft/core/Parameters.hpp"

namespace ft::tbricks {


template<typename BinaryPacketT>
class TbricksProtocol : public core::ProtocolBase {
    using Message = tbricks::v1::Message;
    using MsgType = tbricks::v1::MessageType;
public:
    using BinaryPacket = BinaryPacketT;
    void start() {}
    void on_packet(const BinaryPacket& e) { 
        auto& buf = e.buffer();
        const Message& msg = *reinterpret_cast<const Message*>(buf.data());
        switch(msg.msgtype) {
            case MsgType::SubscriptionRequest: {
                core::SubscriptionRequest req;
                req.symbol() = msg.subscription.symbol;
                req.request_type(core::RequestType::Subscribe);
                subscribe_req().invoke(std::move(req));
            } break;
            case MsgType::SubscriptionCancelRequest: {
                core::SubscriptionRequest req;
                req.symbol() = msg.subscription.symbol;
                req.request_type(core::RequestType::Unsubscribe);
                subscribe_req().invoke(std::move(req));
            } break;
            case MsgType::ClosingEvent: {
                core::SubscriptionRequest req;
                req.symbol() = msg.subscription.symbol;
                req.request_type(core::RequestType::UnsubscribeAll);
                subscribe_req().invoke(std::move(req));
            } break;
            case MsgType::HeartBeat: {

            } break;
            case MsgType::MarketData: {
                
            } break;

        }
    }
    auto& stats() { return stats_; }
    core::SubscriptionSignal & subscribe_req() { return subscribe_req_; }
    core::TickStream& ticks(core::StreamName stream) {
        return ticks_;
    }
    core::InstrumentStream& instruments(core::StreamName stream) {
        return instruments_;
    }    
    
    void on_parameters_updated(const core::Parameters& params) {

    }

    void send(const core::Tick& tick) {
        
    }
protected:
    core::SubscriptionSignal subscribe_req_;
    core::TickStream ticks_;
    core::InstrumentStream instruments_;

    core::StreamStats stats_;
    //Multiplexor
};

}