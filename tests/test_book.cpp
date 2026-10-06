#include <gtest/gtest.h>
#include "order_book.hpp"
#include "market_maker.hpp"

TEST(Book, EmptyHasNoBestPrices) {
    OrderBook book;
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(Book, RestingOrdersSetBestPrices) {
    OrderBook book;
    book.add_limit({1, Side::Buy, 100, 10});
    book.add_limit({2, Side::Sell, 105, 10});
    ASSERT_TRUE(book.best_bid().has_value());
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_bid(), 100);
    EXPECT_EQ(*book.best_ask(), 105);
}

TEST(Matching, ExactFill) {
    OrderBook book;
    book.add_limit({1, Side::Sell, 100, 10});
    auto trades = book.add_limit({2, Side::Buy, 100, 10});
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].buy_id, 2u);
    EXPECT_EQ(trades[0].sell_id, 1u);
    EXPECT_EQ(trades[0].price, 100);
    EXPECT_EQ(trades[0].qty, 10);
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(Matching, PartialFillRestsRemainder) {
    OrderBook book;
    book.add_limit({1, Side::Sell, 100, 5});
    auto trades = book.add_limit({2, Side::Buy, 100, 8});
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].qty, 5);
    EXPECT_FALSE(book.best_ask().has_value());
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 100);
}

TEST(Matching, SweepsMultipleLevels) {
    OrderBook book;
    book.add_limit({1, Side::Sell, 100, 5});
    book.add_limit({2, Side::Sell, 101, 5});
    auto trades = book.add_limit({3, Side::Buy, 101, 8});
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].price, 100);
    EXPECT_EQ(trades[0].qty, 5);
    EXPECT_EQ(trades[1].price, 101);
    EXPECT_EQ(trades[1].qty, 3);
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 101);
}

TEST(Matching, FifoWithinLevel) {
    OrderBook book;
    book.add_limit({1, Side::Sell, 100, 5});
    book.add_limit({2, Side::Sell, 100, 5});
    auto trades = book.add_limit({3, Side::Buy, 100, 5});
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].sell_id, 1u);
}

TEST(Matching, NoCrossNoTrade) {
    OrderBook book;
    book.add_limit({1, Side::Sell, 100, 5});
    auto trades = book.add_limit({2, Side::Buy, 99, 5});
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(*book.best_bid(), 99);
    EXPECT_EQ(*book.best_ask(), 100);
}

TEST(Cancel, RemovesRestingOrder) {
    OrderBook book;
    book.add_limit({1, Side::Buy, 100, 10});
    EXPECT_TRUE(book.cancel(1));
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(Cancel, UnknownIdReturnsFalse) {
    OrderBook book;
    EXPECT_FALSE(book.cancel(999));
}

TEST(Cancel, FilledOrderCannotBeCancelled) {
    OrderBook book;
    book.add_limit({1, Side::Sell, 100, 5});
    book.add_limit({2, Side::Buy, 100, 5});
    EXPECT_FALSE(book.cancel(1));
}

TEST(Cancel, KeepsOtherOrdersAtSameLevel) {
    OrderBook book;
    book.add_limit({1, Side::Buy, 100, 5});
    book.add_limit({2, Side::Buy, 100, 5});
    EXPECT_TRUE(book.cancel(1));
    ASSERT_TRUE(book.best_bid().has_value());
    auto trades = book.add_limit({3, Side::Sell, 100, 5});
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].buy_id, 2u);
}

TEST(Market, SweepsLevels) {
    OrderBook book;
    book.add_limit({1, Side::Sell, 100, 5});
    book.add_limit({2, Side::Sell, 102, 5});
    auto trades = book.add_market(3, Side::Buy, 8);
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].price, 100);
    EXPECT_EQ(trades[1].price, 102);
    EXPECT_EQ(trades[1].qty, 3);
}

TEST(Market, RemainderDoesNotRest) {
    OrderBook book;
    book.add_limit({1, Side::Sell, 100, 5});
    auto trades = book.add_market(2, Side::Buy, 10);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].qty, 5);
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(MarketMaker, NoQuotesWithoutTwoSidedBook) {
    OrderBook book;
    MarketMaker mm(2, 0.0, 10, 1000);
    mm.requote(book);
    EXPECT_FALSE(book.best_bid().has_value());
    book.add_limit({1, Side::Buy, 100, 5});
    mm.requote(book);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(MarketMaker, QuotesAroundMid) {
    OrderBook book;
    book.add_limit({1, Side::Buy, 100, 5});
    book.add_limit({2, Side::Sell, 110, 5});
    MarketMaker mm(2, 0.0, 10, 1000);
    mm.requote(book);
    EXPECT_EQ(*book.best_bid(), 103);   // mid 105 minus 2
    EXPECT_EQ(*book.best_ask(), 107);   // mid 105 plus 2
}

TEST(MarketMaker, BuyFillUpdatesInventoryAndCash) {
    OrderBook book;
    book.add_limit({1, Side::Buy, 100, 5});
    book.add_limit({2, Side::Sell, 110, 5});
    MarketMaker mm(2, 0.0, 10, 1000);
    mm.requote(book);
    auto trades = book.add_market(50, Side::Sell, 4);   // hits our bid at 103
    mm.on_trades(trades);
    EXPECT_EQ(mm.inventory(), 4);
    EXPECT_EQ(mm.cash(), -412);         // 4 units at 103
    EXPECT_EQ(mm.pnl(105), 8);          // -412 + 4 * 105
}

TEST(MarketMaker, LongInventoryLowersQuotes) {
    OrderBook book;
    book.add_limit({1, Side::Buy, 100, 5});
    book.add_limit({2, Side::Sell, 110, 5});
    MarketMaker mm(2, 1.0, 10, 1000);
    mm.requote(book);
    mm.on_trades(book.add_market(50, Side::Sell, 4));   // now long 4
    mm.requote(book);
    EXPECT_EQ(*book.best_ask(), 103);   // r = 105 - 1.0 * 4 = 101, ask = 101 + 2 = 103 
}