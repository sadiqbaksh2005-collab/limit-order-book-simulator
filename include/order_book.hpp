#pragma once
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

enum class Side { Buy, Sell };

struct Order {
    uint64_t id;
    Side     side;
    int64_t  price;
    int64_t  qty;
};

struct Trade {
    uint64_t buy_id, sell_id;
    int64_t  price, qty;
};

class OrderBook {
public:
    std::vector<Trade> add_limit(Order o);
    std::vector<Trade> add_market(uint64_t id, Side side, int64_t qty);
    bool cancel(uint64_t id);
    std::optional<int64_t> best_bid() const;
    std::optional<int64_t> best_ask() const;

private:
    using Level = std::list<Order>;
    std::map<int64_t, Level, std::greater<>> bids_;
    std::map<int64_t, Level> asks_;

    struct Loc { Side side; int64_t price; Level::iterator it; };
    std::unordered_map<uint64_t, Loc> index_;

    template <typename BookSide>
    void match_against(Order& o, BookSide& book, std::vector<Trade>& trades);
};