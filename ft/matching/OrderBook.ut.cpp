#include "OrderBook.hpp"
#include <boost/test/unit_test.hpp>
#include "toolbox/bm/Suite.hpp"
#include "toolbox/sys/Log.hpp"
#include <iostream>

using namespace ft::matching;

BOOST_AUTO_TEST_SUITE(OrderBookTests)
#if 1
BOOST_AUTO_TEST_CASE(SimplePlace)
{
    TOOLBOX_INFO << "OrderBookTests/SimplePlace";
    OrderBook<> book;
    book.place({100, 10});
    book.place({101, 11});
    book.place({101, 12});

    book.place({200, -20});
    book.place({201, -21});

    std::cout << "\n\n:" << book << "\n";
}
#endif
#if 0
BOOST_AUTO_TEST_CASE(SimpleBenchmark)
{
    TOOLBOX_INFO << "OrderBookTests/SimpleBenchmark";
    using namespace toolbox::bm;
    using namespace toolbox::hdr;

    OrderBook<> book;
    BenchmarkSuite bm(std::cout);
    bm.run("place_place_cancel_cancel", [&](auto& ctx) {
        book.place({100, 1});
        book.place({200, -2});
        while(ctx) {
            for(auto _:ctx.range(1000)) {
                auto o1 = book.place({100, 10});
                auto o2 = book.place({200, -20});
                book.cancel(o1);
                book.cancel(o2);
            }
        }
    });
    std::cout << "\n\n:" << book << "\n";
}
#endif

BOOST_AUTO_TEST_CASE(PlaceCancel1)
{
    TOOLBOX_INFO << "OrderBookTests/PlaceCancel1";
    OrderBook<> book;
    auto order1 = book.place({100, 10});
    auto order2 = book.place({200, -20});
    std::cout << "\n\n:" << book << "\n";
    book.cancel(order1);
    std::cout << "\n\n:" << book << "\n";
    book.cancel(order2);
    std::cout << "\n\n:" << book << "\n";
}

BOOST_AUTO_TEST_CASE(PlaceTwoOnLevelAndCancelOne)
{
    TOOLBOX_INFO << "OrderBookTests/PlaceTwoOnLevelAndCancelOne";
    OrderBook<> book;
    auto order1 = book.place({100, 10});
    auto order11 = book.place({100, 11});
    auto order2 = book.place({200, -20});
    std::cout << "\n\n:" << book << "\n";
    book.cancel(order1);
    std::cout << "\n\n:" << book << "\n";
    book.cancel(order2);
    std::cout << "\n\n:" << book << "\n";
}

BOOST_AUTO_TEST_CASE(PlaceTwoLevelsAndCancelBest)
{
    TOOLBOX_INFO << "OrderBookTests/PlaceTwoLevelsAndCancelBest";
    OrderBook<> book;
    auto buy1 = book.place({100, 10});
    auto buy2 = book.place({101, 11});
    std::cout << "\n\n:" << book << "\n";
    book.cancel(buy2);
    std::cout << "\n\n:" << book << "\n";
    book.cancel(buy1);
    std::cout << "\n\n:" << book << "\n";

    auto sell1 = book.place({200, -20});
    auto sell2 = book.place({201, -21});
    std::cout << "\n\n:" << book << "\n";
    book.cancel(sell1);
    std::cout << "\n\n:" << book << "\n";
    book.cancel(sell2);
    std::cout << "\n\n:" << book << "\n";
}

BOOST_AUTO_TEST_CASE(PlaceTwoLevelsAndCancelWorst)
{
    TOOLBOX_INFO << "OrderBookTests/PlaceTwoLevelsAndCancelWorst";
    OrderBook<> book;
    auto buy1 = book.place({100, 10});
    auto buy2 = book.place({101, 11});
    std::cout << "\n\n:" << book << "\n";
    book.cancel(buy1);
    std::cout << "\n\n:" << book << "\n";
    book.cancel(buy2);
    std::cout << "\n\n:" << book << "\n";

    auto sell1 = book.place({200, -20});
    auto sell2 = book.place({201, -21});
    std::cout << "\n\n:" << book << "\n";
    book.cancel(sell2);
    std::cout << "\n\n:" << book << "\n";
    book.cancel(sell1);
    std::cout << "\n\n:" << book << "\n";
}


BOOST_AUTO_TEST_CASE(PlaceCancelSingleOrder)
{
    TOOLBOX_INFO << "OrderBookTests/PlaceCancelSingleOrder";
    OrderBook<> book;
    auto order1 = book.place({100, 10});
    std::cout << "\n\n:" << book << "\n";
    book.cancel(order1);
    std::cout << "\n\n:" << book << "\n";
}

BOOST_AUTO_TEST_SUITE_END()