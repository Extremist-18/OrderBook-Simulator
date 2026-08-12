#pragma once
#include "../core/OrderBook.hpp"
#include <random>
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>

class Simulator{
    OrderBook& book_;
    std::atomic<bool> running{false};
    std::thread worker;

    std::uniform_int_distribution<Quantity> qnty_dist{1, 1000};
    std::uniform_real_distribution<double> sentiment_dist{0.0, 1.0};
    std::uniform_real_distribution<double> noise_dist{-0.1, 0.1};
    std::mt19937_64 random_number_generation;

    double sentiment=0.5;
    double reversion = 0.02;
    Price basePrice = 12000;
    double volatility = 0.4;
    // OrderId id = 10000;
    std::atomic<OrderId> id{10000};
    
    public:
    explicit Simulator(OrderBook &book);
    ~Simulator();
    bool is_running() const;
    void start();
    void stop();
    void run();

    private:
    void updateSentiment();
    void generateOrders();
    void generateBuyOrder();
    void generateSellOrder();
    void generateLimitOrder();
};