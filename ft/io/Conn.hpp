#include "toolbox/io/Reactor.hpp"

namespace ft::io {
namespace tb = toolbox;
class BasicConn {
public:
    BasicConn(tb::Reactor& reactor)
    : reactor_(reactor)
    {}
    tb::Reactor& reactor() { return reactor_; }
protected:
    tb::Reactor& reactor_;
};

};