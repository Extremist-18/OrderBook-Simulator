#pragma once
#include "core/Order.hpp"
#include "core/OrderBook.hpp"
#include "core/Messages.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <functional>

#if defined(__x86_64__) || defined(_M_X64)
  #include <immintrin.h>
#endif

inline void cpu_relax(){
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#endif
}

class IdleBackoff{
    int n_ = 0;
public:
    void reset(){ n_ = 0; }
    void wait(){
        if(n_ < 2000)      { ++n_; cpu_relax(); }
        else if(n_ < 2200) { ++n_; std::this_thread::yield(); }
        else               { std::this_thread::sleep_for(std::chrono::microseconds(100)); }
    }
};

class MatchingPipeline{
    public:
    using Sink = std::function<void(const std::vector<Event>&, const BookSnapshot*)>;
    MatchingPipeline(OrderBook &book, Sink sink) : book1(book), sink1(std::move(sink)){}
    
    ~MatchingPipeline(){
        stop();
    }
    void start(){
        if(runningMatcher.exchange(true))  return;
        runningPublisher = true;
        publisher1 =std::thread([this]{publisherLoop();});
        matcher1 = std::thread([this]{matcherLoop();});
    }   
    void stop(){
        if(!runningMatcher.exchange(false)) return;
        if(matcher1.joinable()) matcher1.join();
        runningPublisher = false;
        if(publisher1.joinable())   publisher1.join();
    }
    bool submit(const Input& c)            { return inQ->push(c); }     // false => queue full
    bool submitDepth(const DepthUpdate& d) { return depthQ->push(d); }  // false => dropped (ok)

    private:
        void matcherLoop(){
            std::vector<Trade> trades;   trades.reserve(256);
            IdleBackoff idle;
            Input c;
            DepthUpdate d;

            publishSnapshot();
            while(runningMatcher.load(std::memory_order_acquire)){
                bool worked = false;
                for(int i=0;i<256 && inQ->pop(c);i++){
                    handle(c,trades);
                    worked = true;
                }
                bool getDepth = false;
                while(depthQ->pop(d))    getDepth = true;
                if(getDepth){
                    applyDepth(d);
                    worked = true;
                }
                if(worked){
                    publishSnapshot();
                    idle.reset();
                }else   idle.wait();
            }
            while(inQ->pop(c))   handle(c,trades);
            publishSnapshot();
        }

        void pushEvent(const Event &e){
            while(!outQ->push(e))    std::this_thread::yield();
        }
        void applyDepth(const DepthUpdate&d){
            std::vector<std::pair<Price,Quantity>> bids, asks;
            for(int i=0;i<d.nBids;i++)  bids.push_back({d.bids[i].price,d.bids[i].qnty});
            for(int i=0;i<d.nAsks;i++)  asks.push_back({d.asks[i].price, d.asks[i].qnty});
            book1.setMarketDepth(bids,asks);
        }
        
        void publishSnapshot(){
            BookSnapshot s;
            s.mid = book1.mid_price();
            auto bids = book1.buyOrders(SNAP);
            auto asks = book1.sellOrders(SNAP);

            s.nBids = (uint8_t)bids.size();
            s.nAsks = (uint8_t)asks.size();
            for(int i=0;i<s.nBids;i++)  s.bids[i] = {bids[i].first, bids[i].second};
            for(int i=0;i<s.nAsks;i++)  s.asks[i] = {asks[i].first, asks[i].second};
            snapQ->push(s);
        }

        void handle(const Input &c, std::vector<Trade>&trades){
            if(c.cmd==CmdType::NewOrder){
                book1.addOrder(c.id,c.price, c.qnty, c.side, c.type);
                trades.clear();
                book1.matchInto(trades);
                for(const Trade &t:trades){
                    Event e{};
                    e.type = Type::Trade;
                    e.trade =t;
                    pushEvent(e);
                }
                Event done{};   done.type=Type::Done; done.ticket =c.ticket;
                done.orderId = c.id;
                done.ok =true;
                pushEvent(done);
            }else{
                bool ok = book1.cancelOrder(c.id);
                Event done{};   done.type=Type::Done; done.ticket =c.ticket;
                done.orderId = c.id;    done.ok =ok;
                done.isCancel = true;
                pushEvent(done);
            }
        }

        void publisherLoop(){
            std::vector<Event> batch; batch.reserve(1024);
            IdleBackoff idle;
            Event e;
            BookSnapshot s;
            
            while(runningPublisher.load(std::memory_order_acquire) || !outQ->empty() || !snapQ->empty()){
                batch.clear();
                while(batch.size()<1024 && outQ->pop(e))    batch.push_back(e);
                
                bool haveSnap = false;
                while(snapQ->pop(s))  haveSnap=true;
                if(!batch.empty() || haveSnap){
                    sink1(batch,haveSnap?&s:nullptr);
                    idle.reset();
                }else   idle.wait();
            }
        }
    
    OrderBook &book1;
    Sink sink1;

    std::unique_ptr<SPSCQueue<Input,(1<<14)>> inQ = std::make_unique<SPSCQueue<Input,(1<<14)>>();
    std::unique_ptr<SPSCQueue<DepthUpdate,(1<<4)>> depthQ = std::make_unique<SPSCQueue<DepthUpdate,(1<<4)>>();
    std::unique_ptr<SPSCQueue<Event,(1<<14)>> outQ = std::make_unique<SPSCQueue<Event,(1<<14)>>();
    std::unique_ptr<SPSCQueue<BookSnapshot,(1<<5)>> snapQ = std::make_unique<SPSCQueue<BookSnapshot,(1<<5)>>();

    std::atomic<bool> runningMatcher{false};
    std::atomic<bool> runningPublisher{false};
    std::thread matcher1, publisher1;
};