#pragma once
#include <cstdint>
#include <list>

using OrderId  = uint64_t;
using Price    = int32_t;
using Quantity = uint32_t;

enum class Side { Buy, Sell };

struct Quote { Price price; Quantity quantity; };
struct Order { OrderId order_id; Quantity quantity; };
struct Level { std::list<Order> orders; Quantity total_volume = 0; };
struct Location { Price price; std::list<Order>::iterator node; Side side; };