#include<iostream>
#include<algorithm>
#include<utility>
#include<array>
template <typename T>
struct type_identity {
  using type = T;
};

template <typename T, std::size_t L,std::size_t I, std::size_t... Is>
struct make_integer_N_sequence_impl
    : std::conditional<
          sizeof...(Is)==L , type_identity<std::integer_sequence<T,Is...>>,
          make_integer_N_sequence_impl<T, L,I,I,Is...>>::type {};

template <typename T,T N,T M>
using make_integer_N_sequence = typename make_integer_N_sequence_impl<T,N,M>::type;



template<std::size_t...index>
auto constexpr test(std::index_sequence<index...>){
     //constexpr auto t=std::array{index...};
     //return t;
     auto t={index...};
     return t;
      
}
 int main(){
    constexpr int x=10;
    auto t=test(make_integer_N_sequence<std::size_t,x,12>{});
        for(auto x:t)
        std::cout<<x<<'\n';
}