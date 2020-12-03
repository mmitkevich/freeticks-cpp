#pragma once
#include <ft/utils/Common.hpp>
#include "ft/core/Parameters.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/EndpointStats.hpp"
#include "ft/io/Connections.hpp"
#include "toolbox/net/Endpoint.hpp"

namespace ft::udp {

namespace tb = toolbox;


template<typename ProtocolT>
class UdpMdServer :  public core::BasicComponent<UdpMdServer<ProtocolT>> {
public:
    using This = UdpMdServer<ProtocolT>;
    using Base = core::BasicComponent<This>;
    using Protocol = ProtocolT;
    using IpProtocol = toolbox::UdpProtocol;
    using IpEndpoint = tb::BasicIpEndpoint<IpProtocol>;
    using Stats = core::EndpointStats<IpEndpoint>;
    static constexpr IpProtocol IpProto = IpProtocol::v4();
    using Connection = io::DgramConn;
    using BinaryPacket = typename Connection::Packet;
    using Socket = typename Connection::Socket;
    using Endpoint = typename Connection::Endpoint;
    using State = typename Base::State;

    using Connections = std::vector<Connection>;

  UdpMdServer(tb::Reactor* reactor)
  : conn_(*reactor)
  {}
  
  using Base::state;
  using Base::url;

  void start() {
      state(core::State::Starting);
      conn_.endpoint(tb::parse_ip_endpoint<IpProtocol>("0.0.0.0:7901"));
      conn_.open();
      // FIXME: on_connected ?
      state(State::Started);
      protocol_.on_started();
  }
  void stop() {}
private:
  Connection conn_;
  Protocol protocol_;
};

}
