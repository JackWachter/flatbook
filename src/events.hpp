#pragma once
#include <string>
#include <vector>
#include "types.hpp"

enum class Action { Add, Cancel, Execute };

struct Event {
    Action   action;
    OrderId  id;
    Side     side;
    Price    price;
    Quantity quantity;
};

std::vector<Event> load_events(const std::string& filename);

template <typename Book>
int apply_events(Book& book, const std::vector<Event>& events) {
    int failures = 0;
    for (const Event& e : events) {
        switch (e.action) {
            case Action::Add:
                book.add(e.id, e.side, e.price, e.quantity);
                break;
            case Action::Cancel:
                if (book.cancel(e.id) != 0) failures++;
                break;
            case Action::Execute:
                if (book.execute(e.id, e.quantity) != 0) failures++;
                break;
        }
    }
    return failures;
}