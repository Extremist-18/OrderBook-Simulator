#include "server/Server.hpp"
#include "core/OrderBook.hpp"
#include "simulation/Simulator.hpp"
#include <csignal>
#include <iostream>
#include <thread>

static OrderBookServer *g_server = nullptr;
void signalHandler(int s){
    std::cout<<"\nShutting Down.."<<std::endl;
    if(g_server){
        g_server->stop();
    }
    exit(s);
}

int main(){
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM,signalHandler);

    std::cout<<"Starting Server.."<<std::endl;
    OrderBook book;
    Simulator sim(book);

    // std::thread simThread([&sim](){
    //     sim.start();
    // });
    OrderBookServer server(book,&sim,8080);
    g_server = &server;
    server.run();

    return 0;
}