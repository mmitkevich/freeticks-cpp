#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"

#include "toolbox/net/EndpointFilter.hpp"

#include "toolbox/sys/Time.hpp"
#include "toolbox/util/String.hpp"
#include "ft/core/MdClient.hpp"

#include "ft/io/Service.hpp"
#include "ft/io/Connection.hpp"

#include <string_view>
#include <system_error>
#include <vector>

namespace ft::io {

/// Stateless MD client, uses multiple connections of same protocol
template<typename ProtocolT, typename ConnT, typename ReactorT=tb::Reactor>
class BasicMdClient : public io::BasicService<BasicMdClient<ProtocolT, ConnT, ReactorT>,
                             ProtocolT, ConnT, ReactorT>
{
public:
    using This = BasicMdClient<ProtocolT, ConnT, ReactorT>;
    using Base = io::BasicService<This, ProtocolT, ConnT, ReactorT>;
    using typename Base::Protocol;
    using typename Base::Connection;
    using typename Base::Connections;
    using typename Base::BinaryPacket;
    using typename Base::Socket;
    using typename Base::Endpoint;
    using typename Base::State;
public:
    template<typename...ProtocolArgsT>
    BasicMdClient(tb::Reactor* reactor, ProtocolArgsT...args)
    : Base(reactor, std::forward<ProtocolArgsT>(args)...)
    {
    }

    
    using Base::parameters, Base::protocol;
    using Base::report;
    using Base::state, Base::url, Base::reactor;
    using Base::is_open, Base::open, Base::close, Base::reopen;


    core::TickStream& ticks(core::StreamName stream) {
        return protocol().ticks(stream);
    }
    core::InstrumentStream& instruments(core::StreamName stream) {
        return protocol().instruments(stream);
    }
private:
    //std::string if_name_;
    //toolbox::EndpointsFilter filter_;
};

} // ft::mcasst