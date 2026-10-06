#pragma once
#include "order_book.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

class MarketMaker {
public:
    MarketMaker(int64_t half_spread, double gamma, int64_t quote_qty, uint64_t first_id)
        : half_spread_(half_spread), gamma_(gamma),
          quote_qty_(quote_qty), next_id_(first_id) {}

    // Cancel old quotes, then post a fresh bid and ask around the reservation price.
    void requote(OrderBook& book) {
        if (bid_id_ != 0) book.cancel(bid_id_);
        if (ask_id_ != 0) book.cancel(ask_id_);
        bid_id_ = 0;
        ask_id_ = 0;

        auto best_bid = book.best_bid();
        auto best_ask = book.best_ask();
        if (!best_bid || !best_ask) return;           // need both sides to read a mid

        double mid = (*best_bid + *best_ask) / 2.0;
        double r   = mid - gamma_ * static_cast<double>(inventory_);

        int64_t bid = std::llround(r - half_spread_);
        int64_t ask = std::llround(r + half_spread_);

        bid = std::min(bid, *best_ask - 1);           // stay passive, never cross
        ask = std::max(ask, *best_bid + 1);

        bid_id_ = next_id_++;
        ask_id_ = next_id_++;
        book.add_limit({bid_id_, Side::Buy,  bid, quote_qty_});   // cannot trade, quotes are passive
        book.add_limit({ask_id_, Side::Sell, ask, quote_qty_});
        quoted_qty_ += 2 * quote_qty_;
    }

    // Feed in the trades returned by other participants' orders.
    void on_trades(const std::vector<Trade>& trades) {
        for (const Trade& t : trades) {
            if (bid_id_ != 0 && t.buy_id == bid_id_) {        // we bought
                cash_      -= t.price * t.qty;
                inventory_ += t.qty;
                filled_qty_ += t.qty;
            }
            if (ask_id_ != 0 && t.sell_id == ask_id_) {       // we sold
                cash_      += t.price * t.qty;
                inventory_ -= t.qty;
                filled_qty_ += t.qty;
            }
        }
    }

    int64_t inventory()  const { return inventory_; }
    int64_t cash()       const { return cash_; }
    int64_t filled_qty() const { return filled_qty_; }
    int64_t quoted_qty() const { return quoted_qty_; }
    int64_t pnl(int64_t mid) const { return cash_ + inventory_ * mid; }

private:
    int64_t  half_spread_;
    double   gamma_;
    int64_t  quote_qty_;
    uint64_t next_id_;
    uint64_t bid_id_ = 0;
    uint64_t ask_id_ = 0;
    int64_t  inventory_  = 0;
    int64_t  cash_       = 0;
    int64_t  quoted_qty_ = 0;
    int64_t  filled_qty_ = 0;
};