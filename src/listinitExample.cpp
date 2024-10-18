#include<iostream>
#include<vector>
struct X{int a=2;
X(int i):a(i){}
};
template<typename T>
struct A{
    std::vector<T> v;
    A(auto&&...args):v({args...}){std::cout<<"简写构造函数模板\n";}
    A(std::initializer_list<T> ll):v(ll){std::cout<<"列表初始化1\n";}
    //A(std::initializer_list<T> ll,double i=1,X x=X{1}):v(ll){std::cout<<"列表初始化2\n";}
    A(std::initializer_list<T> ll,X* xp ,X x):v(ll){std::cout<<"列表初始化3\n";}
    A(T i=3,T i2=4,T i3=5):v({i,i2,i3}){std::cout<<"参数匹配\n";}
};
int main(){
    X x{2};
    auto xp=&x;
    A<int> b{{1,2,4},xp,x};
    A<int>a={1,2,3,1};
    //A<int>a{};
    for(const auto&i:a.v)
    std::cout<<i<<"|";

}
   
