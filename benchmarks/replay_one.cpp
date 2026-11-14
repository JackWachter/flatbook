#include <string>
#include "events.hpp"
#include "orderbook.hpp"
#include "flatbook.hpp"

template <typename Book>
void run(const std::vector<Event>& events, int reps) {
    for (int r = 0; r < reps; r++) {
        Book book;
        for (const Event& e : events) {
            switch (e.action) {
                case Action::Add:     book.add(e.id, e.side, e.price, e.quantity); break;
                case Action::Cancel:  book.cancel(e.id); break;
                case Action::Execute: book.execute(e.id, e.quantity); break;
            }
        }
    }
}

int main(int argc, char** argv) {
    std::string which = (argc > 1) ? argv[1] : "flat";
    auto events = load_events("events.txt");
    if (which == "map") run<OrderBook>(events, 1000);
    else                run<FlatOrderBook>(events, 1000);
    return 0;
}