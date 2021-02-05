#pragma once

#include "ft/utils/Common.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/util/TypeTraits.hpp"
#include "toolbox/net/Endpoint.hpp"


namespace ft { inline namespace core {


/// list of source endpoints multiplexed using same sequence id
template<typename PacketHandlerT, typename EndpointT = tb::IpEndpoint, typename SequenceT = std::uint64_t>
class BasicSequenceMultiplexer : public core::BasicSequenced<SequenceT> {
    using Base = core::BasicSequenced<SequenceT>;
    using Sequence = SequenceT;
    using Endpoint = EndpointT;
public:
    BasicSequenceMultiplexer(PacketHandlerT &handler, std::string_view name)
    : handler_(handler)
    , name_(name) {}
    
    using Base::sequence;

    constexpr std::string_view name() const { return name_; }

    const std::vector<Endpoint>& endpoints() const {
        return endpoints_;
    }
    template<typename TypedPacketT>
    bool match(const TypedPacketT& packet) const {
        
        if(endpoints_.size()==0)
            return true;
        for(auto &ep : endpoints_) {
            bool is_matched = (ep == packet.header().dst());
            TOOLBOX_DUMP<<"match filter name:"<<name_<<", ep: "<<ep<<
                ", packet dst " << packet.header().dst() <<
                " src " << packet.header().src() << " is_matched:"<<is_matched;
            if(is_matched) {
                return true;
            }
        }
        return false;
    }
    template<typename TypedPacketT>
    bool is_stale(const TypedPacketT& packet) const {
        return packet.sequence()<=sequence();
    }

    template<typename TypedPacketT>
    void on_packet(const TypedPacketT& packet) {
        if(is_stale(packet)) {
            handler_.on_stale(packet, *this);
        } else {
            sequence(packet.sequence());
            handler_.on_packet(packet);
        }
    }

    void on_parameters_updated(const core::Parameters &params) {
        endpoints_.clear();
        for(auto e: params["urls"]) {
            std::string url = e.get_string().data();
            Endpoint ep =  tb::TypeTraits<Endpoint>::from_string(url);
            endpoints_.push_back(ep);
            TOOLBOX_DEBUG<<name()<<": add url "<<ep;
        }
    }
private:
    std::vector<Endpoint> endpoints_;
    std::string_view name_;
    PacketHandlerT& handler_;
};


}} // ft::core