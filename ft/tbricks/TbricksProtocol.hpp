#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Requests.hpp"

#include "ft/tbricks/TbricksSchemaV1.hpp"

namespace ft::tbricks {


template<typename BinaryPacketT>
class TbricksProtocol {
    using Message = tbricks::v1::Message;
    using MsgType = tbricks::v1::MessageType;
public:
    using BinaryPacket = BinaryPacketT;
    void on_started() {}
    void on_packet(const BinaryPacket& e) { 
        auto& buf = e.buffer();
        Message& msg = *reinterpret_cast<Message*>(buf.data());
        switch(msg.msgtype) {
            case MsgType::SubscriptionRequest: {
                core::SubscriptionRequest req;
                req.symbol() = msg.subscription.symbol;
                //req.
            } break;
            case MsgType::SubscriptionCancelRequest: {

            } break;
            case MsgType::ClosingEvent: {

            } break;
            case MsgType::HeartBeat: {

            } break;
            case MsgType::MarketData: {

            } break;

        }
    }
    tb::Signal<ft::core::SubscriptionRequest>& subscribe_req() { return subscribe_req_; }
protected:
    tb::Signal<ft::core::SubscriptionRequest> subscribe_req_;
    //Multiplexor
};

}