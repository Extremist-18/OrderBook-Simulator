#include "../simulation/Simulator.hpp"
#include <thread>
#include <cstdlib>

Simulator::Simulator(OrderBook &book): book_(book){
    random_number_generation.seed(std::chrono::steady_clock::now().time_since_epoch().count());
}
Simulator::~Simulator(){
    stop();
}
bool Simulator::is_running() const {
    return running.load();
}
void Simulator::start(){
    if(running.exchange(true)) return;
    worker = std::thread([this]() {this->run(); });
    std::cout<<"Market Simulator started\n";
}
void Simulator::stop(){
    if(!running.exchange(false))    return;
    if(worker.joinable())   worker.join();
    std::cout<<"Market Simulator Stopped\n";
}
void Simulator::run(){
    while (running.load()) {
        updateSentiment();
        generateOrders();

        book_.match();
        book_.recordPrice();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // generated 1000/50 = 20 orders/second
    }
}
void Simulator::updateSentiment(){
    double newsEffect = noise_dist(random_number_generation);
    sentiment += (0.5 - sentiment)*reversion + newsEffect;
    if(sentiment<0.0)   sentiment=0.0;
    if(sentiment>1.0)   sentiment=1.0;
}
void Simulator::generateOrders(){
    int n = rand()%10 + 1;
    for(int i=0;i<n;i++){
        double curr = sentiment_dist(random_number_generation);
        double buyProb = sentiment*0.7;
        double sellProb =(1.0 - sentiment)*0.7;
        if(curr< buyProb)
            generateBuyOrder();
        else if(curr> (buyProb + sellProb))
            generateSellOrder();
        else
            generateLimitOrder();
    }
}
void Simulator::generateBuyOrder(){
    Quantity qnty = qnty_dist(random_number_generation);
    Price price;

    if(rand()%10 <5){   // But at Market price: Assumption: 50% people in market buy at market price no matter what the price is, they just want the stock in thier portfolio
        price=basePrice;
        book_.addOrder(id++,price,qnty,Side::Buy, OrderType::Market);
    }else{
        price = book_.best_ask()+ (rand()%10 + 1)*5;
        book_.addOrder(id++,price,qnty,Side::Buy, OrderType::Limit);
    }
}
void Simulator::generateSellOrder(){
    Quantity qnty = qnty_dist(random_number_generation);
    Price price;

    if(rand()%10 <6){   // Sell at market price -> Assuming that 60% people in market just want the stock
        price=basePrice;
        book_.addOrder(id++,price,qnty,Side::Sell,OrderType::Market);
    }else{
        price = book_.best_bid() -(rand()%10 +1)*5;
        book_.addOrder(id++,price,qnty,Side::Sell,OrderType::Limit);
    }
}
void Simulator::generateLimitOrder(){
    Quantity qnty = qnty_dist(random_number_generation);
    Price mid = book_.mid_price();
    Side side = ((rand()%2 ==0)?Side::Buy:Side::Sell);
    Price price;

    if(side==Side::Buy){
        price = mid - (rand()%10 + 1)*5;
    }else{
        price = mid + (rand()%10 + 1)*5;
    }
    book_.addOrder(id++, price, qnty, side,OrderType::Limit);
}
