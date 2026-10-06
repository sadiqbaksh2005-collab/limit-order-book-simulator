#pragma once
#include "order_book.hpp"
#include <cstdint>
#include <random>
#include <vector>

enum class OpType { Limit, Market, Cancel };

struct Op {
    OpType   type;
    uint64_t id;
    Side     side;
    int64_t  price;
    int64_t  qty;
};

inline std::vector<Op> generate(size_t n, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Op> ops;
    ops.reserve(n);

    std::vector<uint64_t> limit_ids;
    uint64_t next_id = 1;
    int64_t mid = 10000;

    std::uniform_real_distribution<double> pick(0.0, 1.0);
    std::uniform_real_distribution<double> walk(0.0, 1.0);
    std::uniform_int_distribution<int> coin(0, 1);
    std::uniform_int_distribution<int64_t> offset(-10, 10);
    std::uniform_int_distribution<int64_t> limit_qty(1, 100);
    std::uniform_int_distribution<int64_t> market_qty(1, 50);

    for (size_t i = 0; i < n; ++i) {
        double w = walk(rng);
        if (w < 0.1)      mid -= 1;
        else if (w > 0.9) mid += 1;

        double r = pick(rng);
        Side side = coin(rng) ? Side::Buy : Side::Sell;

        if (r < 0.60 || limit_ids.empty()) {
            uint64_t id = next_id++;
            limit_ids.push_back(id);
            ops.push_back({OpType::Limit, id, side, mid + offset(rng), limit_qty(rng)});
        } else if (r < 0.70) {
            ops.push_back({OpType::Market, next_id++, side, 0, market_qty(rng)});
        } else {
            std::uniform_int_distribution<size_t> idx(0, limit_ids.size() - 1);
            ops.push_back({OpType::Cancel, limit_ids[idx(rng)], side, 0, 0});
        }
    }
    return ops;
}

inline std::vector<Trade> apply(OrderBook& book, const Op& op) {
    switch (op.type) {
        case OpType::Limit:  return book.add_limit({op.id, op.side, op.price, op.qty});
        case OpType::Market: return book.add_market(op.id, op.side, op.qty);
        case OpType::Cancel: book.cancel(op.id); return {};
    }
    return {};
}