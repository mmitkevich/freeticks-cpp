#include <toolbox/io/Reactor.hpp>
#include <ft/utils/Common.hpp>
#include <ft/core/Component.hpp>

namespace ft::io {
 
struct HeartbeatsTraits {
    template<typename T>
    using heartbeats_t = decltype(std::declval<T&>().on_heartbeats());
    template<typename T>
    constexpr static  bool has_heartbeats = boost::is_detected_v<heartbeats_t, T>;
};

template<class Self>
class BasicHeartbeats {
    FT_MIXIN(Self);
  public:
    
    tb::Duration heartbeats_interval() { 
        return heartbeats_interval_;
    }
    
    void heartbeats_interval(tb::Duration interval) {
        heartbeats_interval_ = interval;
    }

    void on_heartbeats() {
        restart_heartbeats_timer();
    }

    void restart_heartbeats_timer() {
        using namespace std::literals::chrono_literals;
        heartbeats_timer_ = self()->reactor()->timer(tb::MonoClock::now()+1ms, heartbeats_interval(),
            tb::Priority::High, tb::bind<&Self::on_heartbeats_timer>(this));
    }

    void on_heartbeats_timer(tb::CyclTime now, tb::Timer& timer) {
        self()->on_heartbeats_expired();
    }

    void on_heartbeats_expired() {
        self()->close();
    }
  protected:
    tb::Timer heartbeats_timer_;
    tb::Duration heartbeats_interval_{};
};

}