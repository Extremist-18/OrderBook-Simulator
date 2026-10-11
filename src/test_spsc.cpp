#include <bits/stdc++.h>
#include "core/Order.hpp"
#include <thread>
using namespace std;
 
#define max(a, b) 	(a < b ? b : a)
#define min(a, b) 	((a > b) ? b : a)
#define MOD    			1000000007 
#define f(a,b,c) 		for (int a = b;  a <  c;  a++)
#define ia(a,b,c) 		for(int i = b; i<c ;i++){ cin>>a[i]; }
#define ll 			long long
#define vi 			vector<int>
#define vll 		vector<long long> 
#define vvi 		vector<vector<int>>
#define vvll 		vector<vector<ll>>
#define pi 			pair<int, int> 
#define pll 		pair<long long, long long> 
#define ff 		first 
#define ss 		second 
#define pb 			push_back 
#define sort(x) 		sort(x.begin(),x.end())
#define rev(x) 		reverse(x.begin(),x.end())
 

int main(){
    std::ios_base::sync_with_stdio(false);	cin.tie(NULL);
        SPSCQueue<int,1024> q;
        std::thread producer([&]{
            for(int i=0;i<10000000;i++){
                while(!q.push(i));
            }
        });

        std::thread consumer([&](){
            int ex =0,curr;
            while(ex<10000000){
                if(q.pop(curr)){
                    assert(curr==ex);
                    ex++;
                }
            }
        });

        producer.join();
        consumer.join();

        cout<<"Working as Expected!"<<'\n';
    return 0;
    }