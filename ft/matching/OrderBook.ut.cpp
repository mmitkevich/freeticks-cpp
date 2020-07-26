#include "OrderBook.hpp"
#include <boost/test/unit_test.hpp>
#include <iostream>

using namespace ft::matching;

BOOST_AUTO_TEST_SUITE(OrderBookSuite)

BOOST_AUTO_TEST_CASE(OrderBook1)
{
    TOOLBOX_INFO << "OrderBook1";
    OrderBook<> book;
    book.place({100, 10});
    book.place({101, 11});
    book.place({200, -20});
    book.place({201, -21});

    std::cout << "\n\n:" << book << "\n";
}


BOOST_AUTO_TEST_SUITE_END()