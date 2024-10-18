#include <memory>
#include <iostream>

template<typename T>
struct stated_allocator: std::allocator<T> {
    size_t counter{};
    void deallocate( T* p, std::size_t n ){
        std::cout << "deallocate : " << counter << "\n";
        counter--;
        ((std::allocator<T>&)(*this)).deallocate(p,n);
    }
    T* allocate( std::size_t n){
        counter++;
        std::cout << "allocate : " << counter << "\n";
        return ((std::allocator<T>&)(*this)).allocate(n);
    }
};

template<typename type, typename memory_pool = std::allocator<type>>
struct marray : private memory_pool{
    struct array_delete{
        memory_pool &pool;
        constexpr array_delete(memory_pool &pool) noexcept : pool(pool)  {}
        void operator()(type *ptr) const{
            ptr->~type();
            pool.deallocate(ptr, 1);
        }
    };

    template<typename... Args>
    std::unique_ptr<type, array_delete > make_unique(Args&&... args){
        auto ptr = this->allocate(1);
        new (ptr) type(std::forward<Args>(args)...);
        return std::unique_ptr<type, array_delete >(ptr, array_delete{*this});
    }
};

struct foo{
    foo(){std::cout << "foo()\n";}
    foo(const foo&){std::cout << "foo(const foo&)\n";}
    foo(foo&&){std::cout << "foo(foo&&)\n";}
    ~foo(){std::cout << "~foo()\n";}
};

int main(){
    marray<foo, stated_allocator<foo>> pool;

    auto up1 = pool.make_unique();
    auto up2 = pool.make_unique();
    auto up3 = pool.make_unique();
}