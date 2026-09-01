#pragma once
#include "Order.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <cstddef>
#include <mutex>
#include <atomic> 

struct PriceLevel{
    std::vector<Order> orders;
    size_t head=0;
    std::unordered_map<OrderId, size_t> idx_map;
    
    void addOrder(const Order &o){
        size_t idx = orders.size();
        orders.push_back(o);
        idx_map[o.orderId] = idx;
    }

    void cancelOrder(OrderId id){
        auto it = idx_map.find(id);
        if(it != idx_map.end()){
            orders[it->second].quantity=0;  // lazy deletion
            idx_map.erase(it);
        }
    }
};

struct Trade{
    Price price;
    Quantity quantity;
    uint64_t buyer_id;
    uint64_t seller_id;
    Side aggressiveSide;
};

class OrderBook{
    mutable std::mutex mtx;
    std::map<Price, PriceLevel, std::greater<Price>> bids; // bids -> zyada paisa dega to phele milega
    std::map<Price, PriceLevel, std::less<Price>> asks; // asks -> kam price wala phele bik jayega 
    struct OrderMeta { Side side; Price price; };
    std::unordered_map<OrderId, OrderMeta> orderMetadata;
    std::vector<Price> priceHistory;

    Price marketPrice = 12000;
    std::vector<Order> pendingMarketOrders;
    int64_t Volume =0;
    std::vector<Price> lastMarketBidPrices;
    std::vector<Price> lastMarketAskPrices;
    std::atomic<OrderId> nextOrderId{1};
    
    public:
    std::vector<std::pair<Price, Quantity>> buyOrders(int count) const;
    std::vector<std::pair<Price, Quantity>> sellOrders(int count) const;
    void setMarketDepth(std::vector<std::pair<Price,Quantity>>& bidLevels,std::vector<std::pair<Price,Quantity>>& askLevels);

    OrderId addOrder(Price price, Quantity qnty, Side side, OrderType type);
    void cancelOrder(OrderId id);
    
    std::vector<Trade> match();
    Price best_bid() const;
    Price best_ask() const;
    Price mid_price() const;
    const std::vector<Price> get_PriceHistory() const{
        return priceHistory;
    }
    void recordPrice(){
        priceHistory.push_back(mid_price());
    }
};
