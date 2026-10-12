#pragma once
#include "core/Order.hpp"
#include "core/OrderBook.hpp"
#include <cstdint>
#include <type_traits>

constexpr int DEPTH = 20;
constexpr int SNAP = 15;

struct Level{
    Price price;
    Quantity qnty;
};

enum class CmdType : uint8_t {NewOrder,CancelOrder};
class Input{
    public:
    OrderId id;
    uint64_t ticket;
    Price price;
    Quantity qnty;
    Side side;
    OrderType type;
    CmdType cmd;
};

static_assert(sizeof(Input)<=32);
// static_assert(std::is_trivially_copyable_v<Input>);

class DepthUpdate{
    public:
    uint8_t nBids=0,nAsks=0;
    Level bids[DEPTH];
    Level asks[DEPTH];
};

enum class OrderStatus : uint8_t {Open, Partial, Filled, Unfilled};
enum class Type : uint8_t {Trade, Done};

class Event{
    public:
    Trade trade;
    OrderId orderId;
    uint64_t ticket;
    Type type;
    bool ok;
    bool isCancel;
    OrderStatus orderStatus;    
    Quantity filled;
};
static_assert(sizeof(Event)<=64);

class BookSnapshot{
    public:
    Price   mid = 0;
    uint8_t nBids = 0, nAsks = 0;
    Level   bids[SNAP];
    Level   asks[SNAP];
};