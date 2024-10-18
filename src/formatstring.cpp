#include <format>
#include <iostream>
#include <string>

template <typename CharT, size_t N>
struct const_string {
    CharT arr[N] = {};
    constexpr const_string() noexcept = default;
    constexpr const_string(const const_string &) noexcept = default;
    constexpr const_string(const_string &&) noexcept = default;

    constexpr const_string(const CharT (&src)[N]) noexcept { std::copy(src, src + N, arr); }

    template <size_t Nr>
    constexpr auto operator+(const const_string<CharT, Nr> &other) const noexcept {
        const_string<CharT, N + Nr - 1> result;
        std::copy(arr, arr + N - 1, result.arr);
        std::copy(other.arr, other.arr + Nr, result.arr + N - 1);
        return result;
    }
};

template <size_t N, auto delim>
constexpr auto repeat_filed() {
    constexpr const_string single_field{"{}"};
    if constexpr (N == 1) {
        return single_field;
    } else {
        return single_field + delim + repeat_filed<N - 1, delim>();
    }
}

template <size_t field_count, auto delim, auto tail>
constexpr auto build_format() {
    return repeat_filed<field_count, delim>() + tail;
}

constexpr auto fmt = build_format<3, const_string{", "}, const_string{"!\n"}>();

int main() {
    std::cout << fmt.arr << std::endl;
    std::cout << std::format(fmt.arr, 1, 2, 3) << std::endl;
    return 0;
}
