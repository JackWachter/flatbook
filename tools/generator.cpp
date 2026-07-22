#include <random>
#include <fstream>
#include <iostream>
#include "../src/orderbook.hpp"

int main() {
    int num_events = 10000;
    uint64_t seed = 17;
    double center = 100.0;
    int price_min = 90, price_max = 110;
    int qty_min = 1, qty_max = 100;
    int add_threshold = 60;
    int cancel_threshold = 95;
    int adds = 0, cancels = 0, executes = 0;
    double drift_rate = 0.2;

    std::mt19937_64 rng(seed);

    std::uniform_int_distribution<int> price_dist(price_min, price_max);
    std::uniform_int_distribution<int> qty_dist(qty_min, qty_max);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> action_dist(0, 100);
    std::uniform_real_distribution<double> drift_dist(-drift_rate, drift_rate * 3);
    std::uniform_int_distribution<int> offset_dist(-5, 5);

    uint64_t id = 1;
    std::vector<Order> live;

    std::ofstream out("events.txt");

    for (int i = 0; i < num_events; i++) {
        int action = action_dist(rng);
        center += drift_dist(rng);
        if (live.empty() or action <= add_threshold) {
            int32_t price = static_cast<int32_t>(center) + offset_dist(rng);
            uint32_t quantity = qty_dist(rng);
            int side = side_dist(rng);
            char side_char = (side == 0) ? 'B' : 'S';
            out << "A " << id << " " << side_char << " " << price << " " << quantity << "\n";
            live.push_back(Order {id, quantity});
            id += 1;
            adds++;
        } else if (action <= cancel_threshold) {
            std::uniform_int_distribution<int> cancel_dist(0, live.size() - 1);
            uint64_t cancel_id = cancel_dist(rng);
            out << "C " << live[cancel_id].order_id << "\n";
            std::swap(live[cancel_id], live.back());
            live.pop_back();
            cancels++;
        } else {
            std::uniform_int_distribution<int> execute_dist(0, live.size() - 1);
            uint64_t execute_id = execute_dist(rng);
            uint32_t max_qty = live[execute_id].quantity;
            std::uniform_int_distribution<int> execute_qty_dist(1, max_qty);
            uint32_t execute_qty = execute_qty_dist(rng);
            OrderId return_id = live[execute_id].order_id;
            if (execute_qty == max_qty) {
                std::swap(live[execute_id], live.back());
                live.pop_back();
            } else {
                live[execute_id].quantity = max_qty - execute_qty;
            }
            out << "E " << return_id << " " << execute_qty << "\n";
            executes++;
        }
    }
    std::cerr << "adds: " << adds << " cancels: " << cancels << " executes: " << executes << "\n";
    return 0;
}
