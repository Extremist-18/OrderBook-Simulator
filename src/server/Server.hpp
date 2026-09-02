#pragma once
#include "core/OrderBook.hpp"
#include "simulation/Simulator.hpp"
#include <atomic>
#include <thread>
#include <memory>
#include <nlohmann/json.hpp>
#include <App.h>
#include <unordered_map>
#include <string>

// namespace uWS {
//     struct WebSocket;
//     struct HttpRequest;
//     struct HttpResponse;
//     class App;
// }

class Account{                  // all amount are dealth in cents for precision concerns
    public:     
    std::string userId;
    Price balance = 1000000;   // starts with $10,000 paper money
    int64_t qnty = 0;          
    Price avgEntry = 0;        
    Price realizedPnL = 0; 
    std::string lastTopupDate;         // "YYYY-MM-DD", for the daily grant
};

class OrderBookServer{
    public:
    OrderBookServer(OrderBook &book, Simulator *simulator1,int port=8080);
    ~OrderBookServer();

    void run();
    void stop();

    std::unordered_map<std::string, Account> accounts;
    std::unordered_map<OrderId, std::string> orderOwner;
    std::mutex accountsMtx;       

    private:
    void handleAddOrder(uWS::HttpResponse<false> *res, uWS::HttpRequest *req);
    void handleCancelOrder(uWS::HttpResponse<false> *res, uWS::HttpRequest *req);
    void handleGetBids(uWS::HttpResponse<false> *res, uWS::HttpRequest *req);
    void handleGetAsks(uWS::HttpResponse<false> *res, uWS::HttpRequest *req);
    void handleMidPrice(uWS::HttpResponse<false> *res, uWS::HttpRequest *req);
    void broadcastBookState(); 
    void broadcastBookUpdate();
    
    void settleTrades(const std::vector<Trade>& trades);
    std::string todayDate();
    
    void handleWebSocket();
    OrderBook &book1;
    Simulator *simulator1;
    int port1;
    uWS::App* app1 = nullptr;
    std::atomic<bool> running1{false};
    std::thread serverThread1;
};