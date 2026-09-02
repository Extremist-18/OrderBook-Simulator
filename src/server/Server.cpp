#include "server/Server.hpp"
#include "simulation/Simulator.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <App.h>
#include <ctime>
#include <fstream>
#include <sstream>

using json = nlohmann::json;
struct EmptyUserData {};

OrderBookServer::OrderBookServer(OrderBook &book, Simulator *sim, int port): book1(book),simulator1(sim),port1(port), running1(false){}
OrderBookServer::~OrderBookServer(){
    stop();
}

static std::string readFileContent(const std::string& path){
    std::ifstream file(path);
    if(!file.is_open()) return "";
    std::stringstream buffer;
    buffer<<file.rdbuf();
    return buffer.str();
}

std::string OrderBookServer::todayDate(){
    time_t t = time(nullptr);
    tm* now = gmtime(&t);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", now);
    return std::string(buf);
}
void OrderBookServer::run(){
    if(running1)    return;
    running1=true;

    // uWS::App app;
    // app1 = &app;

    // serverThread1 = std::thread([this]{
        auto app = std::make_unique<uWS::App>();
        app1 = app.get();
        app->post("/order",[this](auto *res, auto *req){ handleAddOrder(res,req);});
        app->del("/order",[this](auto *res, auto *req){ handleCancelOrder(res,req); });
        app->get("/bids",[this](auto *res, auto *req){ handleGetBids(res,req); });
        app->get("/asks",[this](auto *res, auto *req){ handleGetAsks(res,req); });
        app->get("/midprice",[this](auto *res, auto *req){ handleMidPrice(res,req); });
        // app->post("/account/login",[this](auto *res, auto *req){});

        app->post("/admin/price",[this](auto *res, auto *req){
            res->onAborted([](){}); 
            std::string body;
            res->onData([res,this,body=std::move(body)](std::string_view data,bool fin)mutable{
                body.append(data.data(),data.size());
                if(fin){
                    try{
                        auto data = json::parse(body);
                        Price price = data["price"];
                        simulator1->setBasePrice(price);
                        res->writeHeader("Content-Type","application/json");
                        res->end(R"({"status":"price updated"})");
                    }catch(const std::exception &e){
                        res->writeStatus("400 Bad Request");
                        res->end(json{{"error",e.what()}}.dump());
                    }
                }
            });
        });

        app->post("/admin/depth",[this](auto *res, auto *req){
            res->onAborted([](){});
            std::string body;
            res->onData([res,this,body=std::move(body)](std::string_view data,bool fin) mutable{
                body.append(data.data(),data.size());
                if(fin){
                    try{
                        auto j = json::parse(body);
                        std::vector<std::pair<Price,Quantity>> bidLevels, askLevels;
                        for(auto& b : j["bids"]) bidLevels.push_back({b["price"], b["qnty"]});
                        for(auto& a : j["asks"]) askLevels.push_back({a["price"], a["qnty"]});
                        book1.setMarketDepth(bidLevels, askLevels);
                        broadcastBookUpdate();
                        // std::cout<<"[DEPTH] Updated: "<<bidLevels.size()<<" bids, "<< askLevels.size()<<" asks\n"; 
                        // std::cout.flush(); 
                        res->writeHeader("Content-Type","application/json");
                        res->end(R"({"status":"depth updated"})");
                    }catch(const std::exception &e){
                        // std::cout<<"[DEPTH] ERROR: "<<e.what()<<"\n"; 
                        // std::cout.flush();
                        res->writeStatus("400 Bad Request");
                        res->end(json{{"error",e.what()}}.dump());
                    }
                }
            });
        });

        app->post("/account/login",[this](auto *res, auto *req){
            res->onAborted([](){});
            std::string body;
            res->onData([res,this,body=std::move(body)](std::string_view data,bool fin) mutable{
                body.append(data.data(),data.size());
                if(fin){
                    try{
                        auto data = json::parse(body);
                        std::string userId = data["userId"];
                        std::lock_guard<std::mutex> lock(accountsMtx);
                        if(accounts.find(userId)==accounts.end()){
                            Account ac;
                            ac.userId = userId;
                            ac.lastTopupDate = todayDate();
                            accounts[userId]=ac;
                        }
                        auto& ac = accounts[userId];
                        res->writeHeader("Content-Type","application/json");
                        res->end(json{{"userId",ac.userId},{"balance_Cents",ac.balance},{"position_Qnty",ac.qnty},{"avgEntry_Cents",ac.avgEntry}}.dump());
                    }catch(std::exception &e){
                        res->writeStatus("400 Bad Request");
                        res->end(json{{"error",e.what()}}.dump());
                    }
                }
            });
        });

        app->post("/account/topup",[this](auto *res, auto *req){
            res->onAborted([](){});
            std::string body;
            res->onData([res,this, body=std::move(body)](std::string_view data,bool fin)mutable{
                body.append(data.data(),data.size());
                if(fin){
                    try{
                        auto data = json::parse(body);
                        std::string userId = data["userId"];
                        std::lock_guard<std::mutex> lock(accountsMtx);
                        auto it = accounts.find(userId);
                        if(it == accounts.end()){
                            res->writeStatus("404 Not Found");
                            res->end(json{{"error","account not found, login first"}}.dump());
                            return;
                        }
                        std::string today = todayDate();
                        if(it->second.lastTopupDate == today){
                            res->writeStatus("400 Bad Request");
                            res->end(json{{"error","already claimed today"}}.dump());
                            return;
                        }
                        it->second.balance += 100000;   // adding 1000 dollars
                        it->second.lastTopupDate = today;
                        res->writeHeader("Content-Type","application/json");
                        res->end(json{{"status","topped up"},{"balance_Cents",it->second.balance}}.dump());
                    }catch(std::exception &e){
                        res->writeStatus("400 Bad Request");
                        res->end(json{{"error",e.what()}}.dump());
                    }
                }
            });
        });

        app->get("/account",[this](auto *res, auto *req){
            std::string query(req->getQuery());
            size_t pos = query.find("userId=");
            std::string userId = ((pos != std::string::npos)?query.substr(pos+7):"");
            std::lock_guard<std::mutex> lock(accountsMtx);
            auto it = accounts.find(userId);
            if(it == accounts.end()){
                res->writeStatus("404 Not Found");
                res->end(json{{"error","not found"}}.dump());
                return;
            }

            Price mid = book1.mid_price();
            int64_t unrealized = ((int64_t)(mid - it->second.avgEntry) * (int64_t)it->second.qnty) / 100000;
            int64_t equity = it->second.balance + ((int64_t)mid * (int64_t)it->second.qnty) / 100000;
            int64_t totalPnL = it->second.realizedPnL + unrealized;

            res->writeHeader("Content-Type","application/json");
            res->end(json{{"userId",it->second.userId},{"balance_Cents",it->second.balance},{"position_Qnty",it->second.qnty},{"avgEntry_Cents",it->second.avgEntry},
            {"unrealized_PnL_Cents",unrealized},{"realized_PnL_Cents",it->second.realizedPnL},{"total_PnL_Cents",totalPnL},{"equity_Cents",equity}}.dump());
        });

        app->ws<EmptyUserData>("/ws",{
            .open = [this](auto *ws){
                std::cout<<"[WS] client Connected!\n";
                std::cout.flush();
                ws->subscribe("book"); 
                broadcastBookState();
            },
            .message = [](auto *ws, std::string_view msg, uWS::OpCode opCode){
                std::cout<<"[WS] Msg Received: "<<msg<<std::endl;
            },
            .close = [](auto *ws,int code, std::string_view msg){
                std::cout<<"[WS] client disconnected!\n";
                std::cout.flush();
            }
        });

        // Serve the trading HTML at the root
        app->get("/", [&](auto *res, auto *req){
            std::string html = readFileContent("../src/trading.html");
            if(html.empty()) html = readFileContent("trading.html"); // fallback
            if(html.empty()){
                res->writeStatus("404 Not Found");
                res->end("trading.html not found");
                return;
            }
            res->writeHeader("Content-Type", "text/html");
            res->end(html);
        });

        app->get("/trading.html", [&](auto *res, auto *req){
            std::string html = readFileContent("../src/trading.html");
            if(html.empty()) html = readFileContent("trading.html");
            if(html.empty()){
                res->writeStatus("404 Not Found");
                res->end("File not found");
                return;
            }
            res->writeHeader("Content-Type", "text/html");
            res->end(html);
        });

        app->listen(port1, [this](auto *listen_socket) {
            if (listen_socket) {
                std::cout << "OrderBook is live on http://localhost:" << port1 << std::endl;
            } else {
                std::cerr << "ERROR: Failed to bind to PORT " << port1 << std::endl;
                running1 = false;
            }
        });

        std::cout<<"OrderBook is live on https://localhost:"<<port1<<std::endl;
    // });
    app->run();
    running1=false;
}

void OrderBookServer::stop() {
    if (!running1) return;
    running1 = false;
    if (serverThread1.joinable())
        serverThread1.join();
}

void OrderBookServer::settleTrades(const std::vector<Trade>& trades){
    std::lock_guard<std::mutex> lock(accountsMtx);
    for(auto &x:trades){
        Price transaction = ((int64_t)x.price * (int64_t)x.quantity)/100000;
        auto buyer = orderOwner.find(x.buyer_id);
        auto seller = orderOwner.find(x.seller_id);
        // Buyer Part
        if(buyer!=orderOwner.end()){
            auto &acc = accounts[buyer->second];
            int64_t oldQnty = acc.qnty;                          
            int64_t newQnty = oldQnty + (int64_t)x.quantity;     
            acc.avgEntry = ((newQnty!=0)?(((int64_t)acc.avgEntry* (int64_t)oldQnty + (int64_t)x.price* (int64_t)x.quantity)/newQnty):0);
            acc.qnty = newQnty;
            acc.balance -= transaction;
        }

        // Seller Side
        if(seller!=orderOwner.end()){
            auto &acc =accounts[seller->second];
            int64_t closedQty = std::min((int64_t)x.quantity, acc.qnty);
            acc.realizedPnL += ((int64_t)(x.price - acc.avgEntry) * closedQty) / 100000;
            acc.qnty -= x.quantity;
            if(acc.qnty == 0) acc.avgEntry = 0;
            acc.balance += transaction;
        }
        
    }
}

// https handles !!!
void OrderBookServer::handleAddOrder(uWS::HttpResponse<false> *res, uWS::HttpRequest *req){
    res->onAborted([]() {});

    std::string body;
    res->onData([res, this, body = std::move(body)](std::string_view data, bool fin) mutable{
        body.append(data.data(), data.size());   
        if(fin){
            try{
                auto data = json::parse(body);
                Price price =data["price"];
                Quantity qnty = data["quantity"];
                Side side = ((data["side"]=="buy")?Side::Buy : Side::Sell);
                OrderType type = ((data["type"]=="market")?OrderType::Market : OrderType::Limit);
                std::string userId = data.value("userId", "guest");

                OrderId id;
                {
                    std::lock_guard<std::mutex> lock(accountsMtx);
                    if(side == Side::Buy){
                        auto& acc = accounts[userId];
                        int64_t estCost = ((int64_t)price * (int64_t)qnty) / 100000;
                        if(estCost > acc.balance){
                            res->writeStatus("400 Bad Request");
                            res->end(json{{"error","insufficient balance"}}.dump());
                            return;
                        }
                    }else{
                        auto& acc = accounts[userId];
                        if((int64_t)qnty > acc.qnty){
                            res->writeStatus("400 Bad Request");
                            res->end(json{{"error","insufficient position — you can't sell more BTC than you hold"}}.dump());
                            return;
                        }
                    }
                    id = book1.addOrder(price, qnty, side, type);
                    orderOwner[id] = userId;
                }
                
                auto trades = book1.match();
                settleTrades(trades);
                broadcastBookUpdate();

                res->writeHeader("Content-Type","application/json");
                res->end(json{{"status","ok"},{"orderId",id}}.dump());
            }catch(const std::exception &e){
                res->writeStatus("400 Bad Request");
                res->end(json{{"error",e.what()}}.dump());
            }
        }
    });
}

void OrderBookServer::handleCancelOrder(uWS::HttpResponse<false> *res, uWS::HttpRequest *req){
    res->onAborted([]() {});
    std::string body;
    res->onData([res, this, body = std::move(body)](std::string_view data, bool fin) mutable{
        body.append(data.data(), data.size());
        if(fin){
            try{
                auto data = json::parse(body);
                OrderId id = data["orderId"];
                std::string userId = data.value("userId", "guest");
                {
                    std::lock_guard<std::mutex> lock(accountsMtx);
                    auto ownerIt = orderOwner.find(id);
                    if(ownerIt == orderOwner.end()){
                        res->writeStatus("404 Not Found");
                        res->end(json{{"error","order not found"}}.dump());
                        return;
                    }
                    if(ownerIt->second != userId){
                        res->writeStatus("403 Forbidden");
                        res->end(json{{"error","not your order"}}.dump());
                        return;
                    }
                }

                // bool ok = book1.cancelOrder(id);
                // if(!ok){
                //     res->writeStatus("404 Not Found");
                //     res->end(json{{"error","order not found"}}.dump());
                //     return;
                // }
                { std::lock_guard<std::mutex> lock(accountsMtx); orderOwner.erase(id); }
                broadcastBookUpdate();
                res->writeHeader("Content-Type","application/json");
                res->end(R"({"status":"ok"})");
            }catch(const std::exception &e){
                res->writeStatus("400 Bad Request");
                res->end(json{{"error",e.what()}}.dump());
            }
        }
    });
}

void OrderBookServer::handleGetBids(uWS::HttpResponse<false> *res, uWS::HttpRequest *req){
    int cnt = 10;
    std::string query(req->getQuery());
    size_t pos = query.find("count=");
    if (pos != std::string::npos) {
        size_t start = pos + 6;
        size_t end = query.find('&', start);
        std::string count_str = query.substr(start, end - start);
        cnt = std::stoi(count_str);
    }
    auto bids = book1.buyOrders(cnt);
    json response = json::array();
    for(auto &[price,qnty]:bids){
        response.push_back({{"price",price},{"quantity",qnty}});
    }
    res->writeHeader("Content-Type","application/json");
    res->end(response.dump());  
}

void OrderBookServer::handleGetAsks(uWS::HttpResponse<false> *res, uWS::HttpRequest *req){
    int cnt = 10;
    std::string query(req->getQuery());
    size_t pos = query.find("count=");
    if (pos != std::string::npos) {
        size_t start = pos + 6;
        size_t end = query.find('&', start);
        std::string count_str = query.substr(start, end - start);
        cnt = std::stoi(count_str);
    }
    auto asks = book1.sellOrders(cnt);
    json response = json::array();
    for(auto &[price,qnty]:asks){
        response.push_back({{"price",price},{"quantity",qnty}});
    }
    res->writeHeader("Content-Type","application/json");
    res->end(response.dump());   
}

void OrderBookServer::handleMidPrice(uWS::HttpResponse<false> *res, uWS::HttpRequest *req){
    json response = {{"midPrice",book1.mid_price()}};
    res->writeHeader("Content-Type","application/json");
    res->end(response.dump());
}

void OrderBookServer::broadcastBookState(){
    if(!app1) return;
    auto bids = book1.buyOrders(15);
    auto asks = book1.sellOrders(15);
    json payload;
    payload["bids"] = json::array();
    payload["asks"] = json::array();
    for(auto &[p,q] : bids) payload["bids"].push_back({{"price",p},{"quantity",q}});
    for(auto &[p,q] : asks) payload["asks"].push_back({{"price",p},{"quantity",q}});
    payload["midPrice"] = book1.mid_price();

    app1->publish("book", payload.dump(), uWS::OpCode::TEXT);
}

void OrderBookServer::broadcastBookUpdate(){
    if(!app1) return;
    json response = {{"midPrice", book1.mid_price()}, {"bids", json::array()}, {"asks", json::array()}};
    for(auto &[price,qnty] : book1.buyOrders(10))
        response["bids"].push_back({{"price",price},{"quantity",qnty}});
    for(auto &[price,qnty] : book1.sellOrders(10))
        response["asks"].push_back({{"price",price},{"quantity",qnty}});
    app1->publish("book", response.dump(), uWS::OpCode::TEXT);
}

void OrderBookServer::handleWebSocket(){

}