#include "market_maker.hpp"
#include "order_flow.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>
#include <cstdlib>
#include "market_maker.hpp"
#include "order_flow.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

struct Result {
    double  pnl;
    double  fill_rate;
    int64_t max_abs_inventory;
};

Result run(const std::vector<Op>& ops, int64_t half_spread, double gamma, size_t requote_every) {
    OrderBook book;
    MarketMaker mm(half_spread, gamma, 10, 1'000'000'000'000ULL);
    int64_t max_abs_inv = 0;
    double last_mid = 0.0;

    for (size_t i = 0; i < ops.size(); ++i) {
        mm.on_trades(apply(book, ops[i]));
        max_abs_inv = std::max(max_abs_inv, std::llabs(mm.inventory()));

        if (i % requote_every == 0) mm.requote(book);

        auto b = book.best_bid();
        auto a = book.best_ask();
        if (b && a) last_mid = (*b + *a) / 2.0;
    }

    Result r;
    r.pnl = static_cast<double>(mm.cash()) + static_cast<double>(mm.inventory()) * last_mid;
    r.fill_rate = mm.quoted_qty() > 0
        ? static_cast<double>(mm.filled_qty()) / static_cast<double>(mm.quoted_qty())
        : 0.0;
    r.max_abs_inventory = max_abs_inv;
    return r;
}

int main() {
    const size_t N = 200'000;
    const int seeds = 5;

    std::vector<std::vector<Op>> flows;
    for (int s = 1; s <= seeds; ++s) flows.push_back(generate(N, s));

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "requote  half_spread  gamma   mean_pnl      sd_pnl   fill_rate  mean_max_abs_inv\n";

    for (size_t rq : {1, 10}) {
        for (int64_t hs : {1, 2, 5, 10, 15, 20}) {
            for (double g : {0.05, 0.2}) {
                std::vector<double> pnls;
                double fill_sum = 0.0, inv_sum = 0.0;

                for (const auto& ops : flows) {
                    Result r = run(ops, hs, g, rq);
                    pnls.push_back(r.pnl);
                    fill_sum += r.fill_rate;
                    inv_sum  += static_cast<double>(r.max_abs_inventory);
                }

                double mean = 0.0;
                for (double p : pnls) mean += p;
                mean /= seeds;

                double var = 0.0;
                for (double p : pnls) var += (p - mean) * (p - mean);
                double sd = std::sqrt(var / (seeds - 1));

                std::cout << std::setw(7) << rq << std::setw(13) << hs << std::setw(8) << g
                          << std::setw(11) << mean << std::setw(12) << sd
                          << std::setw(11) << fill_sum / seeds
                          << std::setw(18) << inv_sum / seeds << "\n";
            }
        }
    }
    return 0;
}