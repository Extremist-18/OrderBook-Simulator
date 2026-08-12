#pragma once
#include <cstdint> 

using OrderId = uint64_t;
using Price = int64_t;
using Quantity = uint32_t;

enum class Side: uint8_t {Buy=0, Sell=1};
enum class OrderType: uint8_t {Limit=0, Market=1};

struct Order{
    OrderId orderId;
    uint64_t seqNum;
    Price price;
    Quantity quantity;
    Side side;
    OrderType type;
};


static_assert(sizeof(Order) <= 32, "Order must fit in 32 bytes for maximum cache efficiency");
