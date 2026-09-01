#include "OrderBook.hpp"
#include <algorithm>

static int64_t counter=0;
OrderId OrderBook::addOrder(Price price, Quantity qnty,Side side, OrderType type){
    std::lock_guard<std::mutex> lock(mtx);
    OrderId id = nextOrderId++;
    uint64_t seq = ++counter;
    Order o{id,seq,price, qnty,side,type};

    if(type == OrderType::Market){
        pendingMarketOrders.push_back(o);
        orderMetadata[id]={side,price};
        return id;
    }

    if(side== Side::Buy){
        auto it = bids.find(price);
        if(it==bids.end()){ 
            PriceLevel new_level;
            new_level.addOrder(o);
            bids[price]= std::move(new_level);
        }else{
            it->second.addOrder(o);
        }
    }else{
        auto it=asks.find(price);
        if(it==asks.end()){
            PriceLevel new_level;
            new_level.addOrder(o);
            asks[price]= std::move(new_level);
        }else{
            it->second.addOrder(o);
        }
    }
    orderMetadata[id] ={side, price};
    return id;
}

void OrderBook::cancelOrder(OrderId id){
    std::lock_guard<std::mutex> lock(mtx);
    auto it = orderMetadata.find(id);
    if(it == orderMetadata.end())  return;

    auto [side,price] = orderMetadata[id];
    if(side==Side::Buy){
        auto it = bids.find(price);
        if(it!=bids.end()){
            it->second.cancelOrder(id);
            if(it->second.head>= it->second.orders.size())
                bids.erase(it);
        }
    }else{
        auto it = asks.find(price);
        if(it!=asks.end()){
            it->second.cancelOrder(id);
            if(it->second.head >= it->second.orders.size())
                asks.erase(it);
        }
    }

    orderMetadata.erase(it);
}

void OrderBook::setMarketDepth(std::vector<std::pair<Price,Quantity>>& bidLevels,std::vector<std::pair<Price,Quantity>>& askLevels){
    std::lock_guard<std::mutex> lock(mtx);

    for(Price p:lastMarketBidPrices)    bids.erase(p);
    for(Price p:lastMarketAskPrices)    asks.erase(p);
    lastMarketAskPrices.clear();
    lastMarketBidPrices.clear();

    static OrderId marketId = 900000;
    for(auto &x:bidLevels){
        // x = [price, qnty]
        uint64_t seq = ++counter;
        Order o{marketId++,seq,x.first,x.second, Side::Buy, OrderType::Limit};
        PriceLevel lvl;
        lvl.addOrder(o);
        bids[x.first] = std::move(lvl);
        lastMarketBidPrices.push_back(x.first);
    }
    for(auto &x:askLevels){
        // x = [price,qnty]
        uint64_t seq = ++counter;
        Order o{marketId++,seq, x.first, x.second,Side::Sell, OrderType::Limit};
        PriceLevel lvl;
        lvl.addOrder(o);
        asks[x.first] = std::move(lvl);
        lastMarketAskPrices.push_back(x.first);
    }
}


std::vector<Trade> OrderBook::match(){
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Trade> trades;

    int64_t buyVolume=0, sellVolume=0;
    while(!pendingMarketOrders.empty()){
        Order market = pendingMarketOrders.back();
        pendingMarketOrders.pop_back();

        if(market.side==Side::Buy){
            while(market.quantity>0 && !asks.empty()){
                auto &ask_level = asks.begin()->second;
                while(ask_level.head < ask_level.orders.size() && ask_level.orders[ask_level.head].quantity==0){
                    ask_level.head++;
                } 

                if(ask_level.head>= ask_level.orders.size()){
                    asks.erase(asks.begin());
                    continue;
                }
                Order sellOrder = ask_level.orders[ask_level.head];
                Quantity tradingQnty = std::min(market.quantity, sellOrder.quantity);
                trades.push_back({marketPrice,tradingQnty, market.orderId,sellOrder.orderId, Side::Buy});

                buyVolume += tradingQnty;
                market.quantity -= tradingQnty;
                sellOrder.quantity -= tradingQnty;
                if(sellOrder.quantity==0)   ask_level.head++;
            }
        }else{
            while(market.quantity>0 && !bids.empty()){
                auto &bid_level = bids.begin()->second;
                while(bid_level.head < bid_level.orders.size() && bid_level.orders[bid_level.head].quantity==0){
                    bid_level.head++;
                } 
                if(bid_level.head>= bid_level.orders.size()){
                    bids.erase(bids.begin());
                    continue;
                }
                Order buyOrder = bid_level.orders[bid_level.head];
                Quantity tradingQnty = std::min(market.quantity,buyOrder.quantity);
                trades.push_back({marketPrice,tradingQnty,buyOrder.orderId, market.orderId,Side::Sell});

                sellVolume += tradingQnty;
                buyOrder.quantity -= tradingQnty;
                market.quantity -= tradingQnty;

                if(buyOrder.quantity==0)    bid_level.head++;
            }
        }
    }

    int64_t netVolume = buyVolume - sellVolume;
    Volume += netVolume;
    marketPrice += ((Volume/5000))*5;
    Volume %= 5000;

    while(!bids.empty() && !asks.empty() && best_bid()>=best_ask()){
        PriceLevel &bidding = bids.begin()->second;
        PriceLevel & asking = asks.begin()->second;

        while(bidding.head< bidding.orders.size() && bidding.orders[bidding.head].quantity==0)
            bidding.head++;
        
        while(asking.head<asking.orders.size() && asking.orders[asking.head].quantity==0)
            asking.head++;
        
        if(bidding.head>=bidding.orders.size()){
            bids.erase(bids.begin());
            continue;
        }
        if(asking.head >= asking.orders.size()){
            asks.erase(asks.begin());
            continue;
        }

        Order &buying = bidding.orders[bidding.head];
        Order &selling = asking.orders[asking.head];

        Quantity trade_qnty = std::min(buying.quantity, selling.quantity);
        trades.push_back({buying.price, trade_qnty, buying.orderId, selling.orderId, Side::Buy});
        // trades.push_back({buying.price, trade_qnty, buying.orderId, selling.orderId, Side::Sell});

        buying.quantity -= trade_qnty;
        selling.quantity -= trade_qnty;

        if(bidding.orders[bidding.head].quantity==0)    bidding.head++;
        if(asking.orders[asking.head].quantity==0)  asking.head++;

    }
    // if(!bids.empty() && !asks.empty() && best_bid()>0 && best_ask()>0){
    //     marketPrice = (best_ask()+ best_bid())/2;
    // }
    return trades;
}

Price OrderBook::best_bid() const{
    return (bids.empty()?0:bids.begin()->first);
}

Price OrderBook::best_ask() const{
    return (asks.empty()?0:asks.begin()->first);
}

Price OrderBook::mid_price() const{
    if (bids.empty() || asks.empty()) return marketPrice;
    return (best_bid() + best_ask()) / 2;
}

std::vector<std::pair<Price, Quantity>> OrderBook::buyOrders(int cnt) const{
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::pair<Price,Quantity>> res;
    int i=0;
    for(const auto &[Price,level]:bids){
        if(i++>=cnt)    break;
        Quantity qnty=0;
        for(int j=level.head;j<level.orders.size();j++){
            qnty += level.orders[j].quantity;
        }
        res.push_back({Price,qnty});
    }
    return res;
};

std::vector<std::pair<Price, Quantity>> OrderBook::sellOrders(int cnt) const{
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::pair<Price,Quantity>> res;
    int i=0;
    for(const auto &[Price,level]:asks){
        if(i++>=cnt)    break;
        Quantity qnty=0;
        for(int j=level.head;j<level.orders.size();j++){
            qnty += level.orders[j].quantity;
        }
        res.push_back({Price,qnty});
    }
    return res;
};