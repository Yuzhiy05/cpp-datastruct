#include<iostream>
#include <utility> 
#include<ranges>
#include<vector>
#include<algorithm>
constexpr auto first_eq = []<class T, class U>(const std::pair<T, U>& l, const std::pair<T, U>& r) { return l.first > r.first; };
constexpr auto second_less = []<class T, class U>(const std::pair<T, U>& l, const std::pair<T, U>& r) { return l.second < r.second; };
void print(auto const& seq) {
    for (auto const& [elem1,elem2] : seq) {
        std::cout<<elem1<<"|"<<elem2<<"|";
    }   
   // std::cout << '\n';
}


int main(){
   std::vector<std::pair<int, int>> r{{3,8},{7,6},{3,5}}; 
   

for(auto sr : r | std::views::chunk_by(first_eq)){ 
std::ranges::sort(sr, second_less);
}

print(r);




}

