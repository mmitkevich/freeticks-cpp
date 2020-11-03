#pragma once

#include "ft/utils/Common.hpp"
#include "ft/core/Stream.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/util/TypeTraits.hpp"
#include "toolbox/net/Endpoint.hpp"


namespace ft::core {

/// list of source endpoints multiplexed using same sequence id
template<typename ImplT, typename EndpointT = tb::IpEndpoint, typename SequenceT = std::uint64_t>
class BasicSequencedMultiplexor : public core::Sequenced<SequenceT> {
    using Base = core::Sequenced<SequenceT>;
    using Sequence = SequenceT;
    using Endpoint = EndpointT;
public:
    BasicSequencedMultiplexor(ImplT &impl, std::string_view name)
    : impl_(impl)
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
            //TOOLBOX_DEBUG<<"match filter "<<ep<<" packet src "<<packet.header().dst()
            if(ep == packet.header().dst()) {
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
            impl_.on_stale(packet, *this);
        } else {
            sequence(packet.sequence());
            impl_.on_packet(packet);
        }
    }

    void on_parameters_updated(const core::Parameters &params) {
        endpoints_.clear();
        for(auto e: params["urls"]) {
            std::string url = e.get_string().data();
            Endpoint ep =  tbu::TypeTraits<Endpoint>::from_string(url);
            endpoints_.push_back(ep);
            TOOLBOX_DEBUG<<name()<<": add url "<<ep;
        }
    }
private:
    std::vector<Endpoint> endpoints_;
    std::string_view name_;
    ImplT& impl_;
};


}