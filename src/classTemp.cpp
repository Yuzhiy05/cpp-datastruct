#include <vector>
#include <iostream>
#include <iterator>
#include<numeric>
#include<algorithm>
struct No
{  
    No() { std::cout << "在 " << this << " 构造" << '\n'; }
    No(const No&) { std::cout << "复制构造\n"; }
    No(No&&) { std::cout << "移动构造\n"; }
    No operator=(const No&){std::cout << "复制运算符\n"; return *this;}
    ~No() { std::cout << "在 " << this << " 析构" << '\n'; }
    int a=10;
};

struct test{
    No no;
    No f()&&{
        return std::move(no);
    }
    No f2()&&{
        return no;
    }
    } ; 
 No&& fun(No&& a){
    return std::move(a);
 } 
 No fun(){
    return No{};
 }  
 
int main() 
{    
    //      No no;
    //      puts("------------");
    //  auto&& f=fun(std::move(no));
    //auto&& f=fun();
 auto&& f1=test{}.f2();
 //auto&& f2=test{}.no;
 puts("--------------");
};
