#include<cstdio>
#include<mutex>
#include<thread>
#include <condition_variable>
std::condition_variable cv;
std::mutex locm;
bool ready = true;
static int print_x{};

void parllprint(int a){
    while(print_x<100){
    std::unique_lock<std::mutex> lock1(locm);
    cv.wait(lock1, []{return ready;});
    
    printf("thread id:%d,print_x:%d\n",a,print_x++); 
    ready=false;
    lock1.unlock();
    cv.notify_one();}
}
void parllprint2(int a){
    while(print_x<100){
    std::unique_lock<std::mutex> lock1(locm);
    cv.wait(lock1, []{return !ready;});
    
    printf("thread id:%d,print_x:%d\n",a,print_x++);
    ready=true;
    lock1.unlock();
    cv.notify_one();}
}

int main(){
   
    std::thread t1(parllprint,1);
    std::thread t2(parllprint2,2);
    t1.join();
    t2.join();
   
}