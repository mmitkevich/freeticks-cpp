#pragma once

#include "util/Pool.hpp"

#include <cstdint>
#include <vector>
#include <cmath>
#include <ostream>

namespace freeticks {
inline namespace matching {

class OrderTraits {
public:
    using Price = std::int64_t;
    using Qty = std::int64_t;
public:
    bool is_less(const Price &lhs, const Price &rhs) {
        return lhs<rhs;
    }
    void print(std::ostream &os, Price &price) {
        os << (double) price/pow(10., exp10_);
    }
private:
    long exp10_;
};

template<
    typename OrderTraitsT=OrderTraits
>
class OrderBook {
public:
    using OrderTraits = OrderTraitsT;
    using Price = typename OrderTraits::Price;
    using Qty = typename OrderTraits::Qty;
    
    struct Order {
        Price price;
        Qty qty;

        Order(Price price, Qty qty) : price(price), qty(qty) {}
    };
public:
    OrderBook(OrderTraitsT traits=OrderTraitsT())
    : traits_(traits) {
    }
    Order *place(Price price, Qty qty) {
        Order *o = orders_.alloc(price, qty);
        return o;
    }
    bool cancel(Order *o) {
        orders_.dealloc(o);
        return true;
    }
private:

    OrderTraits traits_;
    util::Pool<Order> orders_;
};

} // namespace matching
} // namespace freeticks