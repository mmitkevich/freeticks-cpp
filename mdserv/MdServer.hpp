#pragma once
#include "toolbox/io/Poller.hpp"
#include "ft/core/Component.hpp"
#include "toolbox/net/Endpoint.hpp"

namespace ft {

namespace tb = toolbox;

template<typename ConnT>
class MdServer {
public:
  using This = MdServer<ConnT>;
  using Connection = ConnT;

  MdServer(tb::Reactor& reactor)
  : conn_(reactor, {}, {})
  {}

  void url(std::string_view url) { 
    url_ = url; 
  }
  void start() {}
  void stop() {}
private:
  Connection conn_;
  std::string url_;
};

} // ns ft