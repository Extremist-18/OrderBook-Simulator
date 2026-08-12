#include "core/OrderBook.hpp"
#include "simulation/Simulator.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"  
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <thread>
#include <chrono>
#include <deque>

std::deque<double> price_history;
int main() {
    std::cout<<"=====Started OrderBook=======\n";

    OrderBook book;
    Simulator sim(book);
    sim.start();

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window =glfwCreateWindow(1000,800,"Order Book Simulator", nullptr, nullptr);
    if(!window){
        std::cerr << "Failed to create GLFW window\n";
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext(); 

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double price = book.mid_price()/100.0;
        double bid = book.best_bid()/100.0;
        double ask = book.best_ask()/100.0;

        static double last_price = 0.0;
        if(price !=last_price){
            price_history.push_back(price);
            if(price_history.size()>1000) {
                price_history.pop_front();
            }
            last_price = price;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Live Market Data", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::SetWindowSize(ImVec2(1024, 768), ImGuiCond_Once);

        ImGui::Text("Current Price: %.2f", price);
        ImGui::SameLine(200);
        ImGui::Text("Best Bid: %.2f", bid);
        ImGui::SameLine(400);
        ImGui::Text("Best Ask: %.2f", ask);
        ImGui::SameLine(600);
        ImGui::Text("Spread: %.2f", ask - bid);

        ImGui::Separator();

        if(!price_history.empty()){
            std::vector<float> curr(price_history.begin(),price_history.end());

            double min_val = *std::min_element(curr.begin(), curr.end());
            double max_val = *std::max_element(curr.begin(), curr.end());
            double margin = (max_val-min_val)*0.05;
            if(margin==0.0) margin=1.0;

            ImPlot::SetNextAxesLimits(0,(int)(price_history.size()-1),min_val-margin, max_val+margin, ImGuiCond_Always);
            if(ImPlot::BeginPlot("Price Chart", ImVec2(-1, 250))) {
                ImPlot::PlotLine("Price", curr.data(), (int)curr.size());
                double curr_price = curr.back();
                ImPlot::Annotation(price_history.size() - 1, curr_price,ImVec4(1, 1, 0, 1), ImVec2(10, -5), true, "%.2f", curr_price);
                ImPlot::EndPlot();
            }
        }else{
            ImGui::Text("Waiting for data...");
        }

        ImGui::Separator();
        std::vector<std::pair<Price,Quantity>> buy_dep = book.buyOrders(10);
        std::vector<std::pair<Price,Quantity>> sell_dep = book.sellOrders(10);

        ImGui::Text("Order Book Depth ");

        ImGui::SetNextItemWidth(400.0f); 
        if(ImGui::BeginTable("Depth", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Bids", ImGuiTableColumnFlags_WidthFixed, 200);
            ImGui::TableSetupColumn("Asks", ImGuiTableColumnFlags_WidthFixed, 200);
        
            ImGui::TableHeadersRow();

            int rows = std::max(buy_dep.size(),sell_dep.size());
            for(int i=0;i<rows;i++){
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                if(i<buy_dep.size()){
                    auto& [price, qnty] = buy_dep[i];
                    ImGui::Text("%.2f (%d)", price/100.0,qnty);
                }else   ImGui::Text("-");

                ImGui::TableSetColumnIndex(1);
                if(i<sell_dep.size()){
                    auto &[price,qnty] = sell_dep[i];
                    ImGui::Text("%.2f (%d)", price/100.0, qnty);
                }else   ImGui::Text("-");
            }
            ImGui::EndTable();
        }

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    sim.stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Shutdown complete\n";
    return 0;
}