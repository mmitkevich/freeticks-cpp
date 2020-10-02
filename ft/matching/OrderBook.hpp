#pragma once

#include "ft/utils/Pool.hpp"
#include <boost/intrusive/circular_list_algorithms.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <cstdint>
#include <functional>
#include <vector>
#include <deque>
#include <cmath>
#include <ostream>

#define FT_NO_INLINE __attribute__((noinline)) 
#define FT_LIKELY
#define FT_UNLIKELY

namespace ft { inline namespace matching {

enum class Side : int {
    Buy     =   1,
    Sell    =   -1
};

inline Side operator-(Side rhs) {
    return (Side)(-(int)(rhs));
}

using Price = std::int64_t;

using Qty = std::int64_t;

template<typename OrderT,
typename PriceT=Price,
typename QtyT=Qty>
class OrderTraits {
public:
    using Price=PriceT;
    using Qty=QtyT;

    int compare(const PriceT &lhs, const PriceT &rhs) {
        return lhs-rhs;
    }
    void print(std::ostream &os, Price &price) {
        os << (double) price/pow(10., exp10_);
    }
    PriceT price(const OrderT &order) {
        return order.price;
    }
    QtyT qty(const OrderT &order) {
        return order.qty;
    }
    ssize_t to_long(PriceT price) {
        return price / mpi_;
    }
    PriceT to_price(ssize_t price_index) {
        return price_index * mpi_;
    }
    std::size_t book_size() {
        return book_size_;
    }
    OrderTraits& book_size(std::size_t val) {
        book_size_ = val;
        return *this;
    }
    std::size_t index_size() {
        return index_size_;
    }
    OrderTraits& index_size(std::size_t val) {
        index_size_ = val;
        return *this;
    }
    Side side(const OrderT &order) {
        static Side sides[2] = {Side::Buy, Side::Sell};
        return sides[order.qty<0];
    }
private:
    long exp10_;
    std::size_t book_size_ = 4096;
    std::size_t index_size_ = 256;
    PriceT mpi_{1};
};

    
struct PriceQty {
    Price price {0};
    Qty qty {0};
};

template<typename OrderT>
struct DoNothingOnFill {
    void operator()(OrderT& order, OrderT& other) {}
};

namespace bi = boost::intrusive;
/*
    @brief OrderBook
        arena is slabs list;
        each Node is intrusive list
*/
template<
    typename OrderT = PriceQty,
    typename OnFillT = DoNothingOnFill<OrderT>,
    typename OrderTraitsT = OrderTraits<OrderT>
> class OrderBook {
public:
    using OrderTraits = OrderTraitsT;
    using Price = typename OrderTraits::Price;
    using Qty = typename OrderTraits::Qty;
    using Order = OrderT;
    using OnFill = OnFillT;

    struct Node : OrderT
                , bi::list_base_hook<>
    {
        Node() {}
        Node(OrderT &&order) : OrderT(order) {}
    };

    struct Level : PriceQty
                 , bi::list<Node>   // linked list of non-empty levels
                 , bi::list_base_hook<>
    {
       using Base = bi::list<Node>;
       using Base::Base;
       using Base::iterator;
    };
    
    using LevelsArray = std::vector<Level>;

public:
    OrderBook(OrderTraitsT traits = OrderTraitsT{})
    : traits(traits)
    {
        auto index_size = this->traits.index_size();
        levels.resize(index_size);
        low = &levels[0];
    }

    Qty try_fill(Order& order) {
        Side side = traits.side(order);
        Qty active_qty = traits.qty(order);
        if(active_qty>0 && !empty(-side)) {
            // do fills, mutate active qty
        }
        return active_qty;
    }

    FT_NO_INLINE
    Node *place(Order &&order, OnFill on_fill = OnFill{}) {
        Qty active_qty = try_fill(order);
        if(active_qty==0)
            return nullptr;

        Node* node = pool.alloc(order);
        Side side = traits.side(order);
        Level& lvl = get_level(side, order.price);
        lvl.push_back(*node);

        return node;
    }
    bool cancel(Node *order) {
        auto side = traits.side(*order);
        auto shift = traits.to_long(order->price) - traits.to_long(low->price);
        Level* level = get_shifted_level(low, shift);
        level->erase(Level::s_iterator_to(*order));
        Level*& best = get_best(side);
        if(level==best && level->empty()) {
            // scan new best
            const Level* end = get_outlier(-side);
            while(best!=end) {
                best = get_shifted_level(best, -(ssize_t)side);
                if(!best->empty())
                    break;
            }
            if(best==end)
                best = nullptr;
        }
        pool.dealloc(order);
        return true;
    }
    bool empty(Side side) {
        return get_best(side)!=nullptr;
    }

    struct LevelsView {
        struct LevelsViewIterator {
            LevelsViewIterator(const LevelsView& view, const Level* current)
            : view_(view)
            , current_(current)
            {}
            const Level& operator*() {
                return *current_;
            }
            LevelsViewIterator& operator++() {
                Side side = view_.side();
                auto const & book = view_.book();
                const Level* end = book.get_outlier(-side);
                while(current_!=end) {
                    current_ = book.get_shifted_level(current_, -(ssize_t)side);
                    if(!current_->empty())
                        return *this;
                }
                return *this;
            }
            bool operator!=(const LevelsViewIterator &rhs) {
                return current_ != rhs.current_;
            }
            bool operator==(const LevelsViewIterator &rhs) {
                return current_ == rhs.current_;
            }
            const LevelsView& view_;
            const Level* current_;
        };   

        LevelsView(const OrderBook &book, Side side)
        : book_(book)
        , side_(side)
        {}
        Side side() const {
            return side_;
        }
        const OrderBook& book() const {
            return book_;
        }
        LevelsViewIterator begin() {
            Level *best = book_.get_best(side_);
            if(best)
                return LevelsViewIterator(*this, best);
            else 
                return end();
        }
        LevelsViewIterator end() {
            return LevelsViewIterator(*this, book_.get_outlier(-side_));
        }
        const OrderBook &book_;
        Side side_;
    };
    
    LevelsView get_levels(Side side) const {
        return LevelsView(*this, side);
    }

    ssize_t level_to_index(const Level *lvl) const {
        return lvl <0 ? (-1): (lvl - levels.data());
    }

    Level* get_best(Side side) const {
       return best_level[side_to_index(side)];
    }

    /// get low/high level by side.
    /// low/high levels contain all orders with price less or equal/greater or equal to the level price.
    const Level* get_outlier(Side side) const {
        ssize_t index = low - levels.data();
        index = (index - side_to_index(side)) % levels.size();
        return &levels[index];
    }


    // OrdersContainer& get_orders() const
private:
    using cla = boost::intrusive::circular_list_algorithms<Node>;
    /// ensure dist(L, B) ~= dist(A, H)
    void rebalance() {
        ssize_t dist[2] = {
            (get_best(Side::Buy)-get_outlier(Side::Buy)) % levels.size(),
            (get_best(Side::Sell)-get_outlier(Side::Sell)) % levels.size(),
        };
        for(;;) {
            ssize_t imbalance = dist[0] - dist[1];
            if(imbalance < -1) {
                // bid space should be increased, shifting L down
                Level* high = get_outlier(Side::Sell);
                Level* new_high = get_shifted_level(high, -1);
                // transfer high's orders to new_high
                cla::transfer(new_high->end(), high->begin(), high->end());
                // shift low|high down
                low = high;
            } else if(imbalance > 1) {
                // ask space should be increased, shifting H up
                Level* new_low = get_shifted_level(low, 1);
                // transfer low's orders to new_low
                cla::transfer(new_low->end(), low->begin(), low->end());
                // shift low|high up
                low = new_low;
            } else 
                break;  // 0,+1,-1 is OK
        }
    }
    /// get level for side and price in the levels circular buffer
    Level& get_level(Side side, const Price &price) {
        Level*& best = get_best(side);
        Level*& opp_best = get_best(-side);

        if(/*FT_LIKELY*/(best)) {
            // our not empty
            auto prc = traits.to_long(price);
            auto base_prc = traits.to_long(best->price);
            Level* lvl = get_level(best, prc - base_prc, side);
            //lvl->price = price;
            if(((ssize_t)side)*traits.compare(prc, base_prc)>0) {
                best = lvl;
            }
            return *lvl;
        } else if(opp_best) {
            // opposite side not empty
            auto base_prc = traits.to_long(opp_best->price);
            auto prc = traits.to_long(price);
            best = get_level(opp_best, prc - base_prc, side);
            //best->price = price;
            return *best;
        } else { // empty book
            // empty book
            best = &levels[levels.size()/2];
            // initialize prices of levels
            for(Level& level: levels) {
                level.price = traits.to_price(traits.to_long(price)+(&level-best));
            }
            return *best;
        }
    }
    /// shift level of side by offset given in shift, returning existing or new level of that side.
    /// if no more space to shift, outlier level will be returned
    /// L . . . . B . . . A . . H
    /// . . H L . . . B . . . A .
    Level* get_level(Level* base, ssize_t shift, Side side) {
        ssize_t base_index = base - levels.data();
        ssize_t space_left = (get_outlier(side) - base) % levels.size();
        // clamp shift
        if(shift>0 && shift>space_left)
            shift = space_left;
        if(shift<0 && shift<space_left)
            shift = space_left;
        ssize_t dest_index = (base_index + shift) % levels.size();
        return &levels[dest_index];
    }
    Level* get_shifted_level(const Level* base, ssize_t shift) {
        ssize_t base_index = base - levels.data();
        base_index = (base_index + shift) % levels.size();
        return &levels[base_index];
    }

    const Level* get_shifted_level(const Level* base, ssize_t shift) const {
        ssize_t base_index = base - levels.data();
        base_index = (base_index + shift) % levels.size();
        return &levels[base_index];
    }
    /// Buy => 0, Sell => 1
    static ssize_t side_to_index(Side side) {
        return ((ssize_t)side)<0;
    }
    /// get best bid/best ask level by side. Could be nullptr if not defined
    Level*& get_best(Side side) {
       return best_level[side_to_index(side)];
    }
private:
    OrderTraits traits;
    ft::utils::Pool<Node> pool;
    std::vector<Level> levels;  // all levels as an indexable array 
    Level *low {};
    std::array<Level*, 2> best_level {};  // best bid, best ask
};

template<
    typename OrderT,
    typename OnFillT,
    typename OrderTraitsT
> 
std::ostream& operator<<(std::ostream& os, const OrderBook<OrderT, OnFillT, OrderTraitsT> &book) {
    auto print = [&](Side side) {
        #if 0
            os << "best "<<book.level_to_index(book.get_best(side))
            << " out " << book.level_to_index(book.get_outlier(-side))<<"\n";
        #endif
        for(auto& level : book.get_levels(side)) {
            os << book.level_to_index(&level)<<" [" << level.price << "]";
            for(auto& order : level) {
                os << order.qty << " ";
            }
            os << std::endl;
        } 
    };
    os << "SELL:\n";
    print(Side::Sell);
    os << "BUY:\n";
    print(Side::Buy);
    return os;
}

}} // ft::matching
