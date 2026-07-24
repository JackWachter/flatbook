#include "events.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

std::vector<Event> load_events(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("could not open " + filename);
    }

    std::vector<Event> events;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        char action_char;
        iss >> action_char;

        Event e{};
        if (action_char == 'A') {
            char side_char;
            iss >> e.id >> side_char >> e.price >> e.quantity;
            e.action = Action::Add;
            e.side = (side_char == 'B') ? Side::Buy : Side::Sell;
        } else if (action_char == 'C') {
            iss >> e.id;
            e.action = Action::Cancel;
        } else if (action_char == 'E') {
            iss >> e.id >> e.quantity;
            e.action = Action::Execute;
        } else {
            throw std::runtime_error("unknown action in " + filename + ": " + line);
        }
        events.push_back(e);
    }
    return events;
}