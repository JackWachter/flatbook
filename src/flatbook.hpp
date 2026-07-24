#pragma once
#include <vector>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <map>
#include <bit>
#include "types.hpp"

static constexpr size_t WINDOW_SIZE = 64;
static constexpr size_t CHUNK = 4;

class FlatOrderBook {
    std::vector<Level> bids;
    std::vector<Level> asks;
    std::unordered_map<OrderId, Location> id_index;
    std::map<Price, Level> spillover_bids;
    std::map<Price, Level> spillover_asks;

    Price base = 0;
    size_t head = 0;
    bool initialized = false;

    uint64_t bid_mask = 0;
    uint64_t ask_mask = 0;

    size_t slides_up = 0;
    size_t slides_down = 0;
    size_t orders_migrated = 0;

    bool in_window(Price price) const {
        return price >= base && price < base + static_cast<Price>(WINDOW_SIZE);
    }

    bool chunk_below(Price price) const {
        Price adjusted_price = price + static_cast<Price>(CHUNK);
        return in_window(adjusted_price);
    }

    bool chunk_above(Price price) const {
        Price adjusted_price = price - static_cast<Price>(CHUNK);
        return in_window(adjusted_price);
    }

    void set_occupied(Side side, size_t slot) {
        uint64_t& mask = (side == Side::Buy) ? bid_mask : ask_mask;
        mask |= (1ULL << slot);
    }

    void clear_occupied(Side side, size_t slot) {
        uint64_t& mask = (side == Side::Buy) ? bid_mask : ask_mask;
        mask &= ~(1ULL << slot);
    }

    void migrate_order(Level& from, std::list<Order>::iterator node, Level& to);

    public:
        FlatOrderBook() {
            bids.resize(WINDOW_SIZE);
            asks.resize(WINDOW_SIZE);
        }

        int add(OrderId order_id, Side side, Price price, Quantity quantity);
        int cancel(OrderId order_id);
        int execute(OrderId order_id, Quantity quantity);
        Quote best_bid();
        Quote best_ask();
        void slide_up();
        void slide_down();

        // Exposed for testing: lets tests observe window position and spillover occupancy
        Price current_base() const { return base; }
        size_t spillover_size() const { return spillover_bids.size() + spillover_asks.size(); }

        size_t slide_up_count() const { return slides_up; }
        size_t slide_down_count() const { return slides_down; }
        size_t migration_count() const { return orders_migrated; }
};