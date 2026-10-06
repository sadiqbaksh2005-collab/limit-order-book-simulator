#include "order_flow.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

int main() {
    using clock = std::chrono::steady_clock;
    const size_t N = 1'000'000;
    const std::vector<Op> ops = generate(N, 42);

    int64_t lo = ops[0].price, hi = ops[0].price;
    for (const Op& op : ops) {
        if (op.type == OpType::Limit) {
            lo = std::min(lo, op.price);
            hi = std::max(hi, op.price);
        }
    }
    std::cout << "Limit price range: " << lo << " to " << hi << "\n";

    {
        OrderBook book;
        uint64_t trades = 0;
        auto t0 = clock::now();
        for (const Op& op : ops) trades += apply(book, op).size();
        auto t1 = clock::now();

        double secs = std::chrono::duration<double>(t1 - t0).count();
        std::cout << "Orders processed: " << N << "\n";
        std::cout << "Trades generated: " << trades << "\n";
        std::cout << "Throughput: " << static_cast<uint64_t>(N / secs) << " ops/sec\n";
    }

    {
        OrderBook book;
        uint64_t trades = 0;
        std::vector<int64_t> lat;
        lat.reserve(N);

        for (const Op& op : ops) {
            auto a = clock::now();
            trades += apply(book, op).size();
            auto b = clock::now();
            lat.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count());
        }

        std::sort(lat.begin(), lat.end());
        std::cout << "Latency p50: " << lat[N / 2] << " ns\n";
        std::cout << "Latency p99: " << lat[N * 99 / 100] << " ns\n";
        std::cout << "Latency max: " << lat.back() << " ns\n";
    }
    return 0;
}