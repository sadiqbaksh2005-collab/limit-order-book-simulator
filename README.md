# Limit Order Book and Market Making Simulator

A C++20 limit order book with price time priority matching, plus a market making simulator built on top of it. The project has two goals: build a correct and fast matching engine, and use it to study how quote width and inventory skewing affect a market maker's profit and risk.

All results below come from synthetic order flow. See Limitations.

## Features

- Limit orders, market orders, and cancels
- Price time priority matching, with trades printed at the resting order's price
- Integer tick prices, so there are no floating point comparison bugs
- 17 unit tests (GoogleTest) covering matching, partial fills, multi level sweeps, FIFO within a level, cancels, and the strategy
- Seeded order flow generator, so every run is reproducible
- Benchmark for throughput and per order latency
- Market maker with inventory skewed quotes, plus a parameter sweep over spread, skew, and requote interval

## Design

| Piece | Structure | Why |
|---|---|---|
| Bids | `std::map<price, std::list<Order>, std::greater<>>` | Best bid is the first element |
| Asks | `std::map<price, std::list<Order>>` | Best ask is the first element |
| Each price level | `std::list<Order>` used as a FIFO queue | Front of the list is the oldest order, which gives time priority |
| Order lookup | `std::unordered_map<id, {side, price, list iterator}>` | A cancel finds the order in O(1) and erases it without scanning the level |

Costs: finding or creating a price level is O(log L), where L is the number of price levels. Cancelling an order is O(log L) for the level lookup plus O(1) for the erase. Matching walks the best levels and fills from the front of each queue.

The map is a deliberate baseline. A flat array indexed by price is a natural next optimization, to be measured against these numbers.

## Matching rules

1. A buy matches the lowest asks first, and a sell matches the highest bids first.
2. Within a price level, the oldest order fills first.
3. A trade executes at the resting order's price.
4. A limit order's unfilled remainder rests in the book. A market order's remainder is dropped.

## Market maker

Each requote, the strategy cancels its old quotes, reads the mid price from the book, and posts a bid and an ask around a reservation price:

```
mid   = (best_bid + best_ask) / 2
r     = mid - gamma * inventory
bid   = r - half_spread
ask   = r + half_spread
```

Quotes are clamped so they never cross the book, so the strategy only provides liquidity. A positive inventory lowers both quotes, which makes the ask more likely to fill and pulls inventory back toward zero. Profit is marked to market as `cash + inventory * mid`.

This is a simplified version of the reservation price idea from Avellaneda and Stoikov.

## Build and run

Requires CMake 3.14 or newer and a C++20 compiler.

```
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
.\build\Release\bench.exe
.\build\Release\simulate.exe
```

On Linux or macOS the executables are in `build/` instead of `build/Release/`. Always benchmark in Release mode, since Debug numbers are not meaningful.

## Results

### Matching engine benchmark

1,000,000 operations on synthetic flow: 60% limit orders, 10% market orders, 30% cancels. Single thread, Release build.

Environment: [AMD Ryzen 9 5900x 12-Core Processor], [MSBuild version 17.14.60+43b635718], [Windows 10]

| Metric | Result |
|---|---|
| Throughput | about 10.4 to 10.6 million operations per second |
| Latency p50 | about 100 ns (timer resolution on Windows is roughly 100 ns) |
| Latency p99 | about 400 ns |
| Trades generated | 679,848 |

The maximum latency (about 160 µs) is dominated by operating system pauses and is not a property of the matching logic. An earlier run with a fixed mid price gave about 8.2 million operations per second, so the figure varies with the flow and between runs.

### Market making simulation

Setup: 200,000 operations per run, 5 random seeds, quote size 10. The true price follows a random walk that moves by one tick with probability 0.1 each way per step. Other participants place limit orders within 10 ticks of that price. The strategy requotes every 10 steps or every step. Standard errors are the standard deviation over seeds divided by the square root of 5.

Mean PnL in ticks times quantity, with skew gamma = 0.2:

| Half spread | Requote every 10 steps | Requote every step |
|---|---|---|
| 1 | -69k ± 23k | -233k ± 20k |
| 2 | 24k ± 21k | -26k ± 19k |
| 5 | 155k ± 13k | 193k ± 9k |
| 10 | 214k ± 17k | 262k ± 23k |
| 15 | 259k ± 36k | 316k ± 45k |
| 20 | 291k ± 50k | 354k ± 61k |

Peak absolute inventory (average over seeds):

| Skew gamma | Peak inventory |
|---|---|
| 0.05 | about 1,200 to 1,500 |
| 0.20 | about 340 to 410 |

An earlier run with no skew (gamma = 0, requote every 10 steps) reached peak inventory of roughly 14,000 to 22,000 and extremely noisy PnL.

### Findings

1. **Inventory skew works.** Raising gamma cuts peak inventory sharply, and the product of gamma and peak inventory stays near 75 ticks across settings, so peak inventory scales roughly as 1/gamma.
2. **Wider spreads earn more in this setup.** PnL was still rising at a half spread of 20, so no optimum was found. Fill rate at that point is about 1% to 6%, so profit comes from rare, deep fills.
3. **Skew reduces inventory risk but not always PnL variance.** At half spreads of 15 and 20, gamma = 0.2 had a larger PnL standard deviation than gamma = 0.05.
4. **A hypothesis that failed.** Tight spreads lose money. I expected this was caused by stale quotes, since the strategy only requotes every 10 steps, and predicted that requoting every step would fix it. It did not: tight spread results got worse with every step requoting. The cause of the tight spread losses is still unexplained. A planned next step is to log edge per fill (fill price against the mid at fill time and 10 steps later).

## Limitations

- Order flow is synthetic and uninformed. Real markets have informed traders, so real adverse selection would be harsher than anything shown here, and the PnL figures should not be read as evidence of a profitable strategy.
- Prices are a one tick random walk. Real prices have jumps and volatility clustering.
- No fees, no latency between decision and execution, and no queue position modeling beyond price time priority.
- Single instrument. Five seeds is a small sample, so small differences between adjacent rows are not reliable.
- The benchmark is a single threaded throughput figure on one machine, not an end to end exchange latency.

## Possible extensions

- Replace the map with a flat price indexed array and measure the change against the numbers above
- Replay a real mid price path (for example from a public crypto feed) under the same synthetic order arrivals
- Log edge per fill to explain the tight spread losses
- Static charts of PnL against spread and peak inventory against gamma
