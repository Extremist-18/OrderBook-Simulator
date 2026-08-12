#include "core/OrderBook.hpp"
#include "simulation/Simulator.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    OrderBook book;
    Simulator sim(book);
    sim.start();

    for(int i=0;i<20;i++){
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        double price = book.mid_price()/100.0;
        double bid = book.best_bid()/100.0;
        double ask = book.best_ask()/100.0;
        std::cout<<"Price: "<<price<<" | Bid: "<<bid<<" | Ask: "<<ask<<"\n";
    }

    sim.stop();
    return 0;
}