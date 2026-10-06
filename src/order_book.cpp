#include "order_book.hpp"
#include <algorithm>
#include <iterator>
#include <limits>

std::optional<int64_t> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;   // bids are sorted high to low
}

std::optional<int64_t> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;   // asks are sorted low to high
}

template <typename BookSide>
void OrderBook::match_against(Order& o, BookSide& book, std::vector<Trade>& trades) {
    while (o.qty > 0 && !book.empty()) {
        auto level_it = book.begin();            // best price on the opposite side
        int64_t price = level_it->first;

        bool crosses = (o.side == Side::Buy) ? (price <= o.price)
                                             : (price >= o.price);
        if (!crosses) break;                     // best price is too far away, stop

        Level& level = level_it->second;
        while (o.qty > 0 && !level.empty()) {
            Order& resting = level.front();      // oldest order at this price
            int64_t fill = std::min(o.qty, resting.qty);

            if (o.side == Side::Buy)
                trades.push_back({o.id, resting.id, price, fill});
            else
                trades.push_back({resting.id, o.id, price, fill});

            o.qty       -= fill;
            resting.qty -= fill;

            if (resting.qty == 0) {              // fully filled, remove it
                index_.erase(resting.id);
                level.pop_front();
            }
        }
        if (level.empty()) book.erase(level_it); // remove the empty price level
    }
}

std::vector<Trade> OrderBook::add_limit(Order o) {
    std::vector<Trade> trades;

    if (o.side == Side::Buy) match_against(o, asks_, trades);
    else                     match_against(o, bids_, trades);

    if (o.qty > 0) {                             // leftover quantity rests in the book
        if (o.side == Side::Buy) {
            Level& level = bids_[o.price];
            level.push_back(o);
            index_.emplace(o.id, Loc{o.side, o.price, std::prev(level.end())});
        } else {
            Level& level = asks_[o.price];
            level.push_back(o);
            index_.emplace(o.id, Loc{o.side, o.price, std::prev(level.end())});
        }
    }
    return trades;
}

bool OrderBook::cancel(uint64_t id) {
    auto found = index_.find(id);
    if (found == index_.end()) return false;      // unknown or already filled

    Loc loc = found->second;                      // copy before erasing
    if (loc.side == Side::Buy) {
        auto level_it = bids_.find(loc.price);
        level_it->second.erase(loc.it);           // O(1) thanks to the saved iterator
        if (level_it->second.empty()) bids_.erase(level_it);
    } else {
        auto level_it = asks_.find(loc.price);
        level_it->second.erase(loc.it);
        if (level_it->second.empty()) asks_.erase(level_it);
    }
    index_.erase(found);
    return true;
}

std::vector<Trade> OrderBook::add_market(uint64_t id, Side side, int64_t qty) {
    std::vector<Trade> trades;

    // A market order accepts any price, so use the most extreme limit price.
    int64_t limit = (side == Side::Buy) ? std::numeric_limits<int64_t>::max()
                                        : std::numeric_limits<int64_t>::min();
    Order o{id, side, limit, qty};

    if (side == Side::Buy) match_against(o, asks_, trades);
    else                   match_against(o, bids_, trades);

    return trades;                                // any unfilled remainder is dropped
}