#pragma once
#include <ctime>
#include <iomanip>
#include <chrono>

#include "toolbox/sys/Time.hpp"

namespace ft { inline namespace util {

using HundredNanos = std::chrono::duration<std::int64_t,
    std::ratio_multiply<std::ratio<100>, std::nano>>;

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

inline toolbox::WallTime to_wall_time(HundredNanos value) {
    // https://stackoverflow.com/questions/39459474/convert-c-sharp-datetime-to-c-stdchronosystem-clocktime-point
    std::int64_t val = (value.count() - 621355968000000000ull);
    return toolbox::WallTime(HundredNanos{val});
}

}} // ft::util

