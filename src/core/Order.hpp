#pragma once
#include <cstdint> 
#include <atomic>
#include <cstddef>

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

// SPSC implementation
template<typename T, size_t capacity> 
class SPSCQueue{
    static_assert((capacity & (capacity - 1)) == 0, "Capacity must be power of 2");
    alignas(64) std::atomic<size_t> tail{0};
    alignas(64) size_t tail_cache{0};
    alignas(64) std::atomic<size_t> head{0};
    alignas(64) size_t head_cache{0};
    alignas(64) T buffer[capacity];
    
    public:
        SPSCQueue() = default;
        ~SPSCQueue()= default;

        bool push(const T& x){
            size_t t = tail.load(std::memory_order_relaxed);
            size_t inc = (t+1)&(capacity-1);
            if(inc == head_cache){
                head_cache = head.load(std::memory_order_acquire);
                if(head_cache == inc)   return false;   // Queue already full
            }
            buffer[t] = x;
            tail.store(inc,std::memory_order_release);
            return true;
        }

        bool pop(T& x){
            size_t h = head.load(std::memory_order_relaxed);
            if(h==tail_cache){
                tail_cache = tail.load(std::memory_order_acquire);
                if(h==tail_cache)   return false;   // queue already empty
            }
            x = buffer[h];
            head.store((h+1)&(capacity-1), std::memory_order_release);
            return true;
        }
        
        bool empty(){
            return tail.load(std::memory_order_acquire)==head.load(std::memory_order_acquire);
        }
};
