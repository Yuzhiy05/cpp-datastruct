#include <coroutine>
#include <cstdlib>
#include <iostream>

// A simple generator type that does not use heap allocation
template <typename T> struct generator
{
    struct promise_type
    {
        void* operator new(std::size_t)
        {
            std::exit(1);
            __builtin_unreachable();
        }
        T value;

        std::suspend_always yield_value(T val)
        {
            this->value = val;
            return {};
        }

        std::suspend_always initial_suspend()
        {
            return {};
        }

        std::suspend_always final_suspend() noexcept
        {
            return {};
        }

        generator get_return_object()
        {
            return generator{ this };
        }

        void return_void()
        {
        }

        void unhandled_exception()
        {
        }
    };

    struct iterator
    {
        std::coroutine_handle<promise_type> coro;
        bool done;

        iterator(std::coroutine_handle<promise_type> coro, bool done) : coro(coro), done(done)
        {
        }

        iterator& operator++()
        {
            coro.resume();
            done = coro.done();
            return *this;
        }

        bool operator==(iterator const& other) const
        {
            return done == other.done;
        }

        bool operator!=(iterator const& other) const
        {
            return !(*this == other);
        }

        T const& operator*() const
        {
            return coro.promise().value;
        }

        T const* operator->() const
        {
            return &(coro.promise().value);
        }
    };

    iterator begin()
    {
        p.resume();
        return { p, p.done() };
    }

    iterator end()
    {
        return { p, true };
    }

    generator(generator const&) = delete;

    generator(generator&& rhs) : p(rhs.p)
    {
        rhs.p = nullptr;
    }

    ~generator()
    {
        if (p)
        {
            p.destroy();
        }
    }

private:
    explicit generator(promise_type* p) : p(std::coroutine_handle<promise_type>::from_promise(*p))
    {
    }

    std::coroutine_handle<promise_type> p;
};

// A coroutine function that returns a generator
generator<int> range(int first, int last)
{
    for (int i = first; i < last; ++i)
    {
        co_yield i; // suspend and yield a value
    }
}

// A coroutine caller that uses the generator
int main()
{
    for (int i : range(1, 10))
    { // resume the coroutine and print the values
        std::cout<<i;
    }
}
