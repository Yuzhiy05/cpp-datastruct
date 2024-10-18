#include <iostream>

#include <iterator>
#include <utility>
#include <memory>

template<typename Base, typename Extended>
struct alignas(std::max(alignof(Base), alignof(Extended)))  extend_align {
    Base v;
    inline static constexpr size_t pad = (sizeof(Extended) + sizeof(Base) - 1) / sizeof(Base);
};

struct flex_array_info {
    size_t len;
    std::string name;
};

template<typename T>
struct flex_array {
    using ex_align_t = extend_align<T, flex_array_info>;

    std::allocator<ex_align_t> alloc;
    ex_align_t* buffer;
    flex_array_info& info;
    T* data_;

    T* begin() { return data_; }
    const T* begin() const { return data_; }
    T* end() { return data_ + info.len; }
    const T* end() const { return data_ + info.len; }
    T* data() { return data_; }

#ifdef __cpp_concepts
    template<std::forward_iterator Iter>
#else
    template<typename Iter>
#endif
    flex_array(const std::string& name, Iter begin, Iter end) :
        buffer{ alloc.allocate(static_cast<size_t>(end - begin) + ex_align_t::pad) },
        info{ [&]()->flex_array_info& {
            size_t sz;
            size_t len = static_cast<size_t>(end - begin);
            auto wp = reinterpret_cast<void*>(buffer);
            std::align(alignof(flex_array_info), sizeof(flex_array_info), wp, sz);
            auto pinfo = reinterpret_cast<flex_array_info*>(wp);
            new (pinfo) flex_array_info{ len, name };
            return *pinfo;
        }() },
        data_{ [&] {
            size_t sz;
            auto wp = reinterpret_cast<void*>(buffer + ex_align_t::pad);
            std::align(alignof(T), sizeof(T), wp, sz);
            auto data = reinterpret_cast<T*>(wp);
            auto ret = data;
            for (auto i = begin; i != end; ++i) {
                new (data) T(*i);
                ++data;
            }
            return ret;
        }() } {}

        flex_array(const std::string& name, std::initializer_list<T> params) : flex_array(name, std::begin(params), std::end(params)) {}

        template<typename ...Args>
        flex_array(const std::string& name, Args&&...args) : flex_array(name, { args... }) {}

        flex_array(const flex_array& other) : flex_array(other.name, other.begin(), other.end()) {}

        flex_array(flex_array&& other) :
            buffer{ std::exchange(other.buffer, nullptr) },
            info{ other.info },
            data_{ other.data_ } {}

        ~flex_array() noexcept {
            if (buffer == nullptr) return;

            size_t len = info.len;
            info.~flex_array_info();
            for (auto& i : *this) { i.~T(); }
            auto alloc = std::allocator<ex_align_t>{};
            alloc.deallocate(buffer, len + ex_align_t::pad);
        }
};

int main() {
    auto result = flex_array<int>("test", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

    auto moved_result = std::move(result);

    for (auto i : moved_result) {
        std::cout << i << std::endl;
    }
    std::cout << "len:" << result.info.len << std::endl;
    std::cout << "name:" << result.info.name << std::endl;
}

//来自群@Da'Inihlus 