#include "toolbox/io/Reactor.hpp"
#include "ft/core/Component.hpp"

namespace ft::io {
namespace tb = toolbox;


enum ConnState: std::uint16_t {
    Starting,
    Started,
    Stopping,
    Stopped,
    Failed
};

template<typename DerivedT, typename StateT=ConnState>
class BasicConn : core::BasicComponent<DerivedT, StateT>  {
public:
    using Base = core::BasicComponent<DerivedT, StateT>;
    using Base::Base;
};

};