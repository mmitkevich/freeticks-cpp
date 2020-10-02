#pragma once
#include <ctime>
#include <iomanip>
#include <chrono>

namespace ft::utils {

using BasicTicks = std::chrono::duration<std::int64_t,
    std::ratio_multiply<std::ratio<100>, std::nano>>;

using BasicTimePoint = std::chrono::system_clock::time_point;

inline std::string format_time_point(std::chrono::system_clock::time_point ts) {
    std::time_t t = std::chrono::system_clock::to_time_t(ts);
    std::string s(30, '\0');
    std::strftime(&s[0], s.size(), "%Y-%m-%d %H:%M:%S", std::gmtime(&t));
    std::stringstream ss;
    ss << s;
    ss << ".";
    auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(ts.time_since_epoch());
    ss << std::setfill('0') << std::setw(9) << (((unsigned long)(long)dur.count()) % 1000000000ul);
    return ss.str();
}

struct TimeStamp : BasicTimePoint {
    using BasicTimePoint::BasicTimePoint;
    std::string to_string() {
        return format_time_point(*this);
    }
};

struct Ticks : public BasicTicks
{
    using BasicTicks::BasicTicks;

    TimeStamp to_time_point() {
        // https://stackoverflow.com/questions/39459474/convert-c-sharp-datetime-to-c-stdchronosystem-clocktime-point
        std::int64_t val = (count() - 621355968000000000ull);
        return TimeStamp(Ticks{val});
    }
    std::string to_string() {
        return format_time_point(to_time_point());
    }
};

inline std::ostream & operator<<(std::ostream& os, TimeStamp timestamp) {
    os << timestamp.to_string();
    return os;
}


inline std::ostream & operator<<(std::ostream& os, Ticks ticks) {
    os << ticks.to_time_point();
    return os;
}

} // ft::utils

