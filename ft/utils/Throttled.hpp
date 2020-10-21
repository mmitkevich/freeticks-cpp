#pragma once

#include <memory>
#include <toolbox/sys/Time.hpp>

namespace ft::utils {

template<typename FnT>
class Throttled {
public:
    Throttled(FnT fn)
    : fn_(std::forward<FnT>(fn))
    {}
    void set_interval(toolbox::sys::Duration val) {
        duration_ = val;
    }
    template<typename ...ArgsT>
    void operator()(ArgsT&&...args) {
        auto now = toolbox::sys::MonoClock::now();
        bool is_time = now - last_time_ > duration_;
        if(is_time) {
            fn_(args...);
            last_time_ = now;            
        }
    }
protected:
    FnT fn_;
    toolbox::sys::MonoTime last_time_ {};
    toolbox::sys::Duration duration_ {};
};

}