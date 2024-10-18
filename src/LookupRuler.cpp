#include <iostream>
 namespace N{
    struct S{};
    void f(S,int){std::cout<<"this is  N::f\n";}
    void f22(S,unsigned){std::cout<<"this is N::f22\n";}
 }
 namespace Y{
    N::S s{};
    void f22(N::S,int){std::cout<<"this is Y::f22\n";}
    void g(){f22(s,1);}
 struct Base{
    void f(N::S,double){std::cout<<"this is Base::f\n";}
 };
 struct D:Base{
    void g(N::S x,int x2){
        f(x,x2);
    }
 };
 }

int main() {
    N::S s{};
	Y::D d{};
    d.g(s,1); //无ADL在其类及其子类中查找
    Y::g();//ADL 重载决议选择了Y::f22

}