#include <algorithm>
#include <vector>
#include <thread>
#include <coroutine>
#include <compare>
#include <semaphore>
#include <atomic>
#include <mutex>
#include <exception>
#include <array>
#include <cassert>
#include <optional>
#include <type_traits>

// clang-format off
#if !defined(__cpp_size_t_suffix) || __cpp_size_t_suffix < 202011L
inline constexpr auto operator""uz(unsigned long long const value)
{
    return std::size_t{ value };
}

inline constexpr auto operator""z(unsigned long long const value)
{
    return std::ptrdiff_t(value);
}
#endif
// clang-format on

#define BIZWEN_THREAD_POOL_DEAD_LOCK_MITIGATE
#define BIZWEN_THREAD_POOL_ENABLE_SAFE_CHECK

namespace bizwen
{

    class thread_pool
    {
        class thread_
        {
            using vector = std::vector<std::coroutine_handle<>>;
            std::thread t_;
            std::vector<std::coroutine_handle<>> q_;

        public:
            thread_()
            {
                q_.reserve(10uz);
            }

            void thread(std::thread t) noexcept
            {
                t_ = std::move(t);
            }

            thread_(thread_&&) noexcept = default;
            thread_& operator=(thread_&&) noexcept = default;

            operator std::thread::id() const noexcept
            {
                return t_.get_id();
            }

            auto join() noexcept
            {
                return t_.join();
            }

            auto begin() noexcept
            {
                return q_.begin();
            }

            auto end() noexcept
            {
                return q_.end();
            }

            auto size() const noexcept
            {
                return q_.size();
            }

            auto clear() noexcept
            {
                return q_.clear();
            }

            auto resize(std::size_t s) noexcept
            {
                q_.resize(s);
            }

            auto push_back(std::coroutine_handle<> h)
            {
                q_.push_back(h);
            }
        };

        template <typename T> // breakline
        class priority_queue_: private std::vector<T>
        {
            using vector = std::vector<T>;

        public:
            using vector::begin;
            using vector::empty;
            using vector::end;
            using vector::size;
            using vector::vector;

            template <class... Args> // breakline
            void emplace_back(Args&&... args)
            {
                vector::emplace_back(std::forward<Args>(args)...);
                std::push_heap(vector::begin(), vector::end());
            }

            void pop() noexcept
            {
                std::pop_heap(vector::begin(), vector::end());
                vector::pop_back();
            }

            T top() noexcept
            {
                return vector::front();
            }
        };

        class task_base_
        {
        protected:
            std::coroutine_handle<> handle_;
            unsigned long long uid_;
            std::thread::id tid_;

        public:
            task_base_(std::coroutine_handle<> handle, unsigned long long id, std::thread::id tid) noexcept : handle_(handle), uid_(id), tid_(tid)
            {
            }

            void operator()() noexcept
            {
                handle_();
            }

            operator decltype(tid_)() const noexcept
            {
                return tid_;
            }

            operator decltype(handle_)() const noexcept
            {
                return handle_;
            }

            friend bool operator==(task_base_& t, unsigned long long id) noexcept
            {
                return t.uid_ == id;
            }
        };

        class normal_task_: public task_base_
        {
            std::size_t priority_;

        public:
            normal_task_(std::coroutine_handle<> handle, unsigned long long id, std::size_t priority, std::thread::id tid) noexcept : task_base_(handle, id, tid), priority_(priority)
            {
            }

            friend std::weak_ordering operator<=>(normal_task_ const& lhs, normal_task_ const& rhs) noexcept
            {
                if (lhs.priority_ > rhs.priority_)
                    return std::weak_ordering::greater;
                else if (lhs.priority_ < rhs.priority_)
                    return std::weak_ordering::less;

                if (lhs.uid_ < rhs.uid_)
                    return std::weak_ordering::greater;
                else if (lhs.uid_ > rhs.uid_)
                    return std::weak_ordering::less;

                return std::weak_ordering::equivalent;
            }
        };

        class lazy_task_: public task_base_
        {
            std::chrono::steady_clock::time_point time_;

        public:
            lazy_task_(std::coroutine_handle<> handle, unsigned long long id, std::chrono::steady_clock::time_point time, std::thread::id tid) noexcept : task_base_(handle, id, tid), time_(time)
            {
            }

            auto time() const noexcept
            {
                return time_;
            }

            friend std::weak_ordering operator<=>(lazy_task_ const& lhs, lazy_task_ const& rhs) noexcept
            {
                if (lhs.time_ < rhs.time_)
                    return std::weak_ordering::greater;
                else if (lhs.time_ > rhs.time_)
                    return std::weak_ordering::less;

                if (lhs.uid_ < rhs.uid_)
                    return std::weak_ordering::greater;
                else if (lhs.uid_ > rhs.uid_)
                    return std::weak_ordering::less;

                return std::weak_ordering::equivalent;
            }
        };

        class normal_callback_
        {
            thread_pool& pool_;
            std::size_t pos_;

        public:
            normal_callback_(thread_pool& pool, std::size_t pos) noexcept : pool_(pool), pos_(pos)
            {
            }

            void operator()() noexcept
            {
                pool_.normal_loop_(pos_);
            }
        };

        class lazy_callback_
        {
            thread_pool& pool_;

        public:
            lazy_callback_(thread_pool& pool) noexcept : pool_(pool)
            {
            }

            void operator()() noexcept
            {
                pool_.lazy_loop_();
            }
        };

        // to ensure exception safety, system_error are not accepted
        class mutex_
        {
            std::atomic<bool> s_{};

        public:
            void lock() noexcept
            {
                while (s_.exchange(true, std::memory_order::acquire))
                    s_.wait(true, std::memory_order::relaxed);
            }

            bool try_lock() noexcept
            {
                return !s_.exchange(true, std::memory_order::acquire);
            }

            void unlock() noexcept
            {
                s_.store(false, std::memory_order::release);
                s_.notify_one();
            }
        };

        // to ensure exception safety, system_error are not accepted
        class waiter_
        {
            std::counting_semaphore<> s_{ 0z };

        public:
            waiter_() noexcept = default;

            void notify_n(std::ptrdiff_t n) noexcept
            {
                s_.release(n);
            }

            void notify_one() noexcept
            {
                s_.release();
            }

            void wait() noexcept
            {
                s_.acquire();
            }

            bool try_wait() noexcept
            {
                return s_.try_acquire();
            }

            template <class Clock, class Duration> // breakline
            bool try_wait_until(std::chrono::time_point<Clock, Duration> abs_time) noexcept
            {
                return s_.try_acquire_until(abs_time);
            }

            template <class Rep, class Period> // breakline
            bool try_wait_for(std::chrono::duration<Rep, Period> rel_time) noexcept
            {
                return s_.try_acquire_for(rel_time);
            }
        };

        std::atomic<bool> exit_flag_{};
        mutex_ mitigate_mutex_{};
        mutex_ lazys_mutex_{};
        mutex_ normals_mutex_{};
#ifdef BIZWEN_THREAD_POOL_DEAD_LOCK_MITIGATE
        int thread_count_{};
        std::chrono::steady_clock::time_point time_{};
#endif
        std::atomic<unsigned long long> unique_id_{};
        priority_queue_<normal_task_> normals_queue_{};
        std::vector<thread_> normals_threads_{};
        waiter_ normals_waiter_{};
        priority_queue_<lazy_task_> lazys_queue_{};
        std::thread lazys_thread_{};
        waiter_ lazys_waiter_{};

#ifdef BIZWEN_THREAD_POOL_DEAD_LOCK_MITIGATE
        class count_lock_
        {
            thread_pool& pool_;

        public:
            count_lock_(thread_pool& pool) noexcept : pool_(pool)
            {
                ++pool_.thread_count_;
            }

            ~count_lock_()
            {
                --pool_.thread_count_;
                pool_.time_ = std::chrono::steady_clock::now();
            }
        };
#endif

        void increase_thread_()
        {
#ifdef BIZWEN_THREAD_POOL_DEAD_LOCK_MITIGATE

            if (!mitigate_mutex_.try_lock()) [[unlikely]]
                return;

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
#pragma warning(disable : 26110)
#endif
            std::unique_lock lock{ mitigate_mutex_, std::adopt_lock };

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

            if (auto size{ normals_threads_.size() }; int((size == thread_count_) & (time_ + std::chrono::milliseconds{ 16 } <= std::chrono::steady_clock::now()))) [[unlikely]]
            {
                normals_threads_.resize(size + 1uz);
                normals_threads_[size].thread(std::thread(normal_callback_{ *this, size }));
            }
#endif
        }

        bool is_exit_() const noexcept
        {
            return exit_flag_.load(std::memory_order::relaxed);
        }

        void run_in_(std::coroutine_handle<> handle, std::thread::id id) noexcept
        {
            assert(id != decltype(id){});

            std::lock_guard lock{ mitigate_mutex_ };

            for (auto& i : normals_threads_) [[likely]]
            {
                if (i == id) [[unlikely]]
                {
                    i.push_back(handle);

                    return;
                }
            }

            std::abort();
        }

        std::size_t consume_pre_thread_queue(std::size_t pos)
        {
            while (true)
            {
                // 锁定所有线程和队列防止修改
                std::unique_lock lock{ mitigate_mutex_ };
                // 上锁之后设置线程数量，为了之后通知用

                // 获得当前线程的队列，注意可能由于线程数组被扩容而导致失效，因此需要通过pos访问
                auto& src{ normals_threads_[pos] };

                // 取最多取10个任务存入tasks，优化size==0的情况
                auto size{ src.size() };

                if (!size) [[likely]]
                    return normals_threads_.size();

                // 每次最多取10个
                std::array<std::coroutine_handle<>, 10uz> tasks{};

                auto begin{ src.begin() };

                if (size < 10uz) [[likely]]
                {
                    std::copy(begin, begin + size, tasks.begin());
                    src.clear();
                }
                else
                {
                    std::copy(begin, begin + 10uz, tasks.begin());
                    std::copy(begin + 10uz, begin + size, begin);
                    src.resize(size - 10uz);
                    size = 10uz;
                }

                lock.unlock();

                for (auto begin{ tasks.begin() }, end{ begin + size }; begin != end; ++begin) [[unlikely]]
                    (*begin)();
            }
        }

        void normal_loop_(std::size_t pos)
        {
            auto tid{ std::this_thread::get_id() };

            while (true)
            {
                // 首先执行该线程的队列中的任务，并获得当前线程数
                std::size_t threads{ consume_pre_thread_queue(pos) };

                // 为了使得线程池有能够被析构的可能，不能使用无限阻塞的等待
                // 每秒至少取消等待一次，以提供检查退出标志的机会
                // 初始状态为阻塞，因为没有元素
                //
                // 通知器是自通知的，也就是说在执行任务之前所有线程通过通知器串行执行
                // 当首次出现任务时唤醒第一个通知器
                // 当存在下一个任务时唤醒第二个通知器
                // 但注意存在过通知的情况，因为要发出threads个信号使得任务可以在指定线程执行
                // 尝试等待有任务可用或者1s
                if (!normals_waiter_.try_wait_for(std::chrono::seconds{ 1 }))
                {
                    // 如果等待1s仍无任务
                    // 检查退出标志
                    if (is_exit_()) [[unlikely]]
                        break;
                    // 如果不退出，则进入下一轮
                    else
                        continue;
                }

                // 保护插入
                std::unique_lock lock{ normals_mutex_ };

                auto size{ normals_queue_.size() };

                // 由于可能会被指定任务时多唤醒，因此不保证容器内有元素
                if (!size) [[unlikely]]
                {
                    if (is_exit_()) [[unlikely]]
                        break;

                    std::this_thread::yield();
                    // 注意和lazys_loop不同，由于存在过通知，因此需要此处消耗掉多余的通知，而不是重新发出
                    continue;
                }

                auto task{ normals_queue_.top() };

                // 测试线程是否符合，如果不符合，释放锁，通知并则进入下一轮
                if (std::thread::id task_tid{ task }; int(int(task_tid != tid) & (task_tid != std::thread::id{}))) [[unlikely]]
                {
                    run_in_(task, task_tid);
                    normals_queue_.pop();
                    lock.unlock();

                    // 通知“所有”线程
                    normals_waiter_.notify_n(threads);

                    continue;
                }

                // 弹出并解锁
                normals_queue_.pop();
                lock.unlock();

                if (size >> 1)
                    normals_waiter_.notify_one();
                // 执行任务
                task();
            }
        }

        void lazy_loop_()
        {
            while (true)
            {
                // 尝试等待有任务可用或者1s
                if (!lazys_waiter_.try_wait_for(std::chrono::seconds{ 1 }))
                {
                    // 如果等待1s仍无任务
                    // 检查退出标志
                    if (is_exit_()) [[unlikely]]
                        break;

                    continue;
                }

                // 保护插入
                std::unique_lock lock{ lazys_mutex_ };
                // 注意严格保证容器内一定有元素
                auto task{ lazys_queue_.top() };

                // 如果未到时间，则等待到时间或被通知
                if (auto time{ task.time() }; time > decltype(time)::clock::now())
                {
                    // 先无条件释放锁，防止阻塞插入
                    lock.unlock();
                    // 并且此时count<=size-1，wait为0
                    // 尝试等待到时间，如果返回true，说明有多个任务，此时将count消耗为0，发生等待
                    lazys_waiter_.try_wait_until(time);
                    lazys_waiter_.notify_one();

                    continue;
                }

                assert(!lazys_queue_.empty());

                if (!lazys_queue_.empty())
                    lazys_waiter_.notify_one();

                lazys_queue_.pop();


                lock.unlock();
                // 如果成功等待，则推入normals并进入下一轮
                run_once(task, std::size_t(-1));
            }
        }

        auto gen_id_() noexcept
        {
            return unique_id_.fetch_add(1, std::memory_order::relaxed);
        }

        class context_base_
        {
        protected:
            std::thread::id tid_;

            context_base_() noexcept = default;

            context_base_(std::thread::id tid) noexcept : tid_(tid)
            {
            }
        };

    public:
        enum class id : unsigned long long
        {
        };

        class context: private context_base_
        {
            friend thread_pool;

            std::thread::id id() const noexcept
            {
                return tid_;
            }

            context(std::thread::id tid) noexcept : context_base_(tid)
            {
            }

        public:
            context() noexcept = default;

            context(context const& ctx) noexcept : context_base_(ctx.tid_)
            {
            }

            bool operator==(const context& c) const noexcept
            {
                return c.tid_ == tid_;
            }

            context& operator=(context const& ctx) noexcept
            {
                tid_ = ctx.tid_;

                return *this;
            }
        };

        // must be called from a thread owned by the thread pool
        static context capture_context() noexcept
        {
            return context{ std::this_thread::get_id() };
        }

    private:
        void check_context_legal_(context ctx) noexcept
        {
#ifdef BIZWEN_THREAD_POOL_ENABLE_SAFE_CHECK
            auto id{ ctx.id() };

            if (id == std::thread::id{}) [[likely]]
                return;

            std::lock_guard lock{ mitigate_mutex_ };

            for (auto const& i : normals_threads_) [[likely]]
                if (id == i) [[unlikely]]
                    return;

            std::abort();
#endif
        }

        static std::size_t cacl_thread_num(std::size_t num) noexcept
        {
            auto n{ std::thread::hardware_concurrency() };
            n = n ? n : 2;
            num = num > 1 ? num : n;

            return num;
        }

    public:
        id run_once(std::coroutine_handle<> callback, std::size_t priority = 0uz, context ctx = {})
        {
            auto n{ gen_id_() };
            check_context_legal_(ctx);
            std::unique_lock lock{ normals_mutex_ };

            normals_waiter_.notify_one();

            normals_queue_.emplace_back(callback, n, priority, ctx.id());
            lock.unlock();
            increase_thread_();

            return id{ n };
        }

        id run_after(std::coroutine_handle<> callback, std::chrono::milliseconds duration, context ctx = {})
        {
            auto n{ gen_id_() };
            auto time{ std::chrono::steady_clock::now() + duration };
            check_context_legal_(ctx);
            std::lock_guard lock{ lazys_mutex_ };

            lazys_waiter_.notify_one();

            lazys_queue_.emplace_back(std::move(callback), n, time, ctx.id());

            return id{ n };
        }

        void exit()
        {
            exit_flag_.store(true, std::memory_order::relaxed);
        }

        thread_pool(std::size_t num = 0uz) : normals_threads_(cacl_thread_num(num)), lazys_thread_(lazy_callback_{ *this })
        {
            for (std::size_t i{}, n{ normals_threads_.size() }; i != n; ++i) [[likely]]
                normals_threads_[i].thread(std::thread(normal_callback_{ *this, i }));
        }

        ~thread_pool()
        {
            exit();

            for (auto& i : normals_threads_) [[likely]]
                i.join();

            lazys_thread_.join();
        }
    };
}

bizwen::thread_pool pool{};
#include "fast_io.h"

namespace bizwen
{
    class canceled_coroutine
    {
    };

    class waiter_
    {
        std::binary_semaphore s_{ 0z };

    public:
        waiter_() noexcept = default;

        void notify_one() noexcept
        {
            s_.release();
        }

        void wait() noexcept
        {
            s_.acquire();
        }

        bool try_wait() noexcept
        {
            return s_.try_acquire();
        }

        template <class Clock, class Duration> // breakline
        bool try_wait_until(std::chrono::time_point<Clock, Duration> abs_time) noexcept
        {
            return s_.try_acquire_until(abs_time);
        }

        template <class Rep, class Period> // breakline
        bool try_wait_for(std::chrono::duration<Rep, Period> rel_time) noexcept
        {
            return s_.try_acquire_for(rel_time);
        }
    };

    class cancelable_promise_base: public waiter_
    {
        std::atomic<bool> canceled_{};

    protected:
        std::coroutine_handle<> next_{};
        std::exception_ptr e_{};

    public:
        void cancel() noexcept
        {
            canceled_.store(true, std::memory_order::relaxed);
        }

        bool canceled() noexcept
        {
            return canceled_.load(std::memory_order::relaxed);
        }

        auto next() noexcept
        {
            return next_;
        }

        void next(std::coroutine_handle<> handle) noexcept
        {
            next_ = handle;
        }
    };

    template <typename T> // breakline
    static std::coroutine_handle<cancelable_promise_base> to_cancelable(std::coroutine_handle<T> t)
        requires std::is_convertible_v<T&, cancelable_promise_base&>
    {
        return std::coroutine_handle<cancelable_promise_base>::from_address(t.address());
    }

    static std::coroutine_handle<cancelable_promise_base> to_cancelable(std::coroutine_handle<> t)
    {
        return std::coroutine_handle<cancelable_promise_base>::from_address(t.address());
    }

    class cancelable_awaiter_base: public std::suspend_always
    {
        std::coroutine_handle<cancelable_promise_base> h_;

    protected:
        void set_handle(std::coroutine_handle<> h) noexcept
        {
            h_ = to_cancelable(h);
        }

        void throw_if_canceled()
        {
            if (h_.promise().canceled())
                throw canceled_coroutine{};
        }
    };

    class progress_promise_base: public cancelable_promise_base
    {
    protected:
        std::size_t progress{};
    };

    auto resume_background()
    {
        struct background_awaiter: cancelable_awaiter_base
        {
            void await_suspend(std::coroutine_handle<> handle)
            {
                set_handle(handle);
                pool.run_once(handle);
            }

            void await_resume()
            {
                throw_if_canceled();
            }
        };

        return background_awaiter{};
    }

    auto operator co_await(thread_pool::context c)
    {
        struct apartment_awaiter: cancelable_awaiter_base
        {
            thread_pool::context c_;

            bool await_ready() const noexcept
            {
                return thread_pool::capture_context() == c_;
            }

            void await_suspend(std::coroutine_handle<> handle)
            {
                set_handle(handle);
                pool.run_once(handle, 0uz, c_);
            }

            void await_resume()
            {
                throw_if_canceled();
            }
        };

        return apartment_awaiter{ .c_ = c };
    }

    template <class Rep, class Period> // breakline
    auto operator co_await(std::chrono::duration<Rep, Period> d)
    {
        struct timer_awaiter: cancelable_awaiter_base
        {
            std::chrono::milliseconds d_;

            bool await_ready() const noexcept
            {
                return d_ <= decltype(d_)::zero();
            }

            void await_suspend(std::coroutine_handle<> handle)
            {
                set_handle(handle);
                pool.run_after(handle, d_);
            }

            void await_resume()
            {
                throw_if_canceled();
            }
        };

        return timer_awaiter{ .d_ = std::chrono::duration_cast<std::chrono::milliseconds>(d) };
    }

    // like C++/WinRT promise_base::final_suspend_awaiter

    class final_suspend_awaiter: public std::suspend_always
    {
        cancelable_promise_base& p_;

    public:
        final_suspend_awaiter(cancelable_promise_base& p) noexcept : p_(p)
        {
        }

        auto await_suspend(std::coroutine_handle<> handle) noexcept
        {
            fast_io::println("final");

            p_.notify_one();

            auto next{ p_.next() };

            if (next)
                return next;
            else
                return decltype(next)(std::noop_coroutine());
        }
    };

    struct task_awaiter_base
    {
    protected:
        std::coroutine_handle<cancelable_promise_base> current_;

    public:
        template <typename T>
            requires std::convertible_to<std::add_lvalue_reference_t<T>, cancelable_promise_base&>
        task_awaiter_base(std::coroutine_handle<T> current) noexcept : current_(decltype(current_)::from_address(current.address()))
        {
        }

        bool await_ready() const noexcept
        {
            assert(current_);
            return current_.done();
        }

        auto await_suspend(std::coroutine_handle<> h) noexcept
        {
            current_.promise().next(h);

            return current_;
        }
    };

    template <typename T = void> // breakline
    class task
    {
    public:
        class promise_type;

    private:
        std::coroutine_handle<promise_type> handle_{};
        friend promise_type;

        task(promise_type& p) noexcept : handle_(decltype(handle_)::from_promise(p))
        {
        }

    public:
        task() noexcept = default;

        task(task&& rhs) noexcept
        {
            std::swap(rhs.handle_, handle_);
        }

        task& operator=(task&& rhs) noexcept
        {
            std::swap(rhs.handle_, handle_);

            return *this;
        }

        task(task const&) = delete;

        task& operator=(const task& rhs) = delete;

        class promise_type: public cancelable_promise_base
        {
            friend task;
            T t_{};

        public:
            promise_type()
            {
            }

            task get_return_object()
            {
                return { *this };
            }

            // 协程是惰性启动的，要么被co_await，要么手动调用
            std::suspend_always initial_suspend()
            {
                return {};
            }

            final_suspend_awaiter final_suspend() noexcept
            {
                return { *this };
            }

            void return_value(T&& t)
            {
                t_ = std::move(t);
            }

            void unhandled_exception()
            {
                e_ = std::current_exception();
            }
        };

        void operator()() noexcept
        {
            handle_();
        }

        void cancel() noexcept
        {
            assert(handle_);
            handle_.promise().cancel();
        }

        /*
        T get()
        {
            p_.wait();

            std::exception_ptr e{ p_.e_ };
            T t{ std::move(p_.t_) };
            p_.notify_one();

            if (e)
                std::rethrow_exception(e);

            return t;
        }

        // like P2300 this_thread::sync_wait
        std::optional<T> try_get()
        {
            if (p_.try_wait())
            {
                std::exception_ptr e{ p_.e_ };
                T t{ std::move(p_.t_) };
                p_.notify_one();

                if (e)
                    std::rethrow_exception(e);
                else
                    return t;
            }

            return std::nullopt;
        }

        template <class Clock, class Duration> // breakline
        std::optional<T> try_get_until(std::chrono::time_point<Clock, Duration> abs_time) noexcept
        {
            if (p_.try_wait_until(abs_time))
            {
                std::exception_ptr e{ p_.e_ };
                T t{ std::move(p_.t_) };
                p_.notify_one();

                if (e)
                    std::rethrow_exception(e);
                else
                    return t;
            }

            return std::nullopt;
        }

        template <class Rep, class Period> // breakline
        std::optional<T> try_get_for(std::chrono::duration<Rep, Period> rel_time) noexcept
        {
            if (p_.try_wait_for(rel_time))
            {
                std::exception_ptr e{ p_.e_ };
                T t{ std::move(p_.t_) };
                p_.notify_one();

                if (e)
                    std::rethrow_exception(e);
                else
                    return t;
            }

            return std::nullopt;
        }
        */
        ~task()
        {
            if (handle_)
                handle_.destroy();
        }

        auto operator co_await()
        {
            struct awaiter: task_awaiter_base
            {
                T await_resume()
                {
                    auto& p{ static_cast<promise_type&>(current_.promise()) };

                    auto& e{ p.e_ };

                    if (e)
                        std::rethrow_exception(e);

                    return std::move(p.t_);
                }
            };

            assert(handle_);

            return awaiter{ handle_ };
        }
    };

    template <> // breakline
    class task<void>
    {
    public:
        class promise_type;

    private:
        // 为了实现移动task，必须使用handle储存
        std::coroutine_handle<promise_type> handle_{};
        friend promise_type;

        task(promise_type& p) noexcept : handle_(decltype(handle_)::from_promise(p))
        {
        }

    public:
        task(task&& rhs) noexcept
        {
            std::swap(rhs.handle_, handle_);
        }

        task& operator=(task&& rhs) noexcept
        {
            std::swap(rhs.handle_, handle_);

            return *this;
        }

        task(task const&) = delete;

        task& operator=(const task& rhs) = delete;

        class promise_type: public cancelable_promise_base
        {
            friend task;

        public:
            promise_type() noexcept
            {
            }

            task get_return_object()
            {
                return { *this };
            }

            std::suspend_always initial_suspend()
            {
                return {};
            }

            final_suspend_awaiter final_suspend() noexcept
            {
                return { *this };
            }

            void return_void()
            {
            }

            void unhandled_exception()
            {
                e_ = std::current_exception();
            }
        };

        void operator()() noexcept
        {
            handle_();
        }

        void cancel() noexcept
        {
            assert(handle_);
            handle_.promise().cancel();
        }

        void get()
        {
            auto& p_ = handle_.promise();

            p_.wait();

            std::exception_ptr e{ p_.e_ };
            p_.notify_one();

            if (e)
                std::rethrow_exception(e);
        }

        bool try_get()
        {
            auto& p_ = handle_.promise();

            if (p_.try_wait())
            {
                std::exception_ptr e{ p_.e_ };
                p_.notify_one();

                if (e)
                    std::rethrow_exception(e);

                return true;
            }

            return false;
        }

        template <class Clock, class Duration> // breakline
        bool try_get_until(std::chrono::time_point<Clock, Duration> abs_time) noexcept
        {
            auto& p_ = handle_.promise();

            if (p_.try_wait_until(abs_time))
            {
                std::exception_ptr e{ p_.e_ };
                p_.notify_one();

                if (e)
                    std::rethrow_exception(e);

                return true;
            }

            return false;
        }

        template <class Rep, class Period> // breakline
        bool try_get_for(std::chrono::duration<Rep, Period> rel_time) noexcept
        {
            auto& p_ = handle_.promise();

            if (p_.try_wait_for(rel_time))
            {
                std::exception_ptr e{ p_.e_ };
                p_.notify_one();

                if (e)
                    std::rethrow_exception(e);

                return true;
            }

            return false;
        }

        ~task()
        {
            if (handle_)
                handle_.destroy();
        }

        auto operator co_await()
        {
            struct awaiter: task_awaiter_base
            {
                void await_resume()
                {
                    auto& p{ static_cast<promise_type&>(current_.promise()) };

                    auto& e{ p.e_ };

                    if (e)
                        std::rethrow_exception(e);
                }
            };

            assert(handle_);

            return awaiter{ handle_ };
        }
    };

    struct cancellation_token: std::suspend_always
    {
    private:
        cancelable_promise_base* c_;

    public:
        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            c_ = &to_cancelable(h).promise();

            return false;
        }

        auto await_resume() noexcept
        {
            class token
            {
                friend cancellation_token;
                cancelable_promise_base& c_;

                token(cancelable_promise_base& c) noexcept : c_(c)
                {
                }

            public:
                bool canceled()
                {
                    return c_.canceled();
                }

                operator bool()
                {
                    return !canceled();
                }
            };

            return token{ *c_ };
        }
    };

    class fire_and_forget
    {
        class promise_type
        {
        public:
            promise_type() noexcept
            {
            }

            fire_and_forget get_return_object()
            {
                return {};
            }

            std::suspend_never initial_suspend()
            {
                return {};
            }

            std::suspend_never final_suspend() noexcept
            {
                return {};
            }

            void return_void()
            {
            }

            void unhandled_exception()
            {
            }
        };

        auto operator co_await()
        {
            return std::suspend_never{};
        }
    };

} // namespace bizwen

#include <fast_io.h>

bizwen::task<int> test_value()
{
    co_return 1;
}

bizwen::task<> test_void()
{
    co_return;
}

bizwen::task<> test_timer()
{
    using namespace std::chrono_literals;
    using namespace bizwen;
    co_await bizwen::resume_background();
    co_await 500ms;
    co_await 500ms;
    co_await test_void();

    int i = co_await test_value();
}

void test1()
{
    for (auto i{ 0uz }; i < 2000; ++i)
    {
        test_timer();
        fast_io::println(fast_io::err(), i);
    }
}

/* loop
先有个任务t
fire_and_forget t(...);
然后用lazy_task实现重复执行
fair_and_forget foo1(...)
{
while(true)
{
co_await t();
co_await duration;
}
}
*/

/* https://learn.microsoft.com/zh-cn/windows/uwp/cpp-and-winrt-apis/concurrency-2
IAsyncAction ExplicitCancelationAsync()
{
    auto cancelation_token{ co_await winrt::get_cancellation_token() };

    while (!cancelation_token())
    {
        std::cout << "ExplicitCancelationAsync: do some work for 1 second" << std::endl;
        co_await 1s;
    }
}

IAsyncAction MainCoroutineAsync()
{
    auto explicit_cancelation{ ExplicitCancelationAsync() };
    co_await 3s;
    explicit_cancelation.Cancel();
}
*/

bizwen::task<> ExplicitCancelationAsync()
{
    using namespace bizwen;
    using namespace std::chrono_literals;
    auto cancelation_token{ co_await bizwen::cancellation_token{} };

    while (cancelation_token)
    {
        fast_io::println(fast_io::err(), "ExplicitCancelationAsync: do some work for 1 second");
        co_await 1s;
        continue;
    }
}

bizwen::task<> MainCoroutineAsync()
{
    using namespace bizwen;
    using namespace std::chrono_literals;
    auto explicit_cancelation{ ExplicitCancelationAsync() };
    try
    {
        explicit_cancelation();
        co_await 10s;
        explicit_cancelation.cancel();
    }
    catch (...)
    {
        // canceled
    }
}

void test2()
{
    auto t1 = MainCoroutineAsync();
    t1();
    auto t2 = MainCoroutineAsync();
    t2();
    auto t3 = MainCoroutineAsync();
    t3();
    auto t4 = MainCoroutineAsync();
    t4();
    auto t5 = MainCoroutineAsync();
    t5();
    auto t6 = MainCoroutineAsync();
    t6();
    auto t7 = MainCoroutineAsync();
    t7();
    auto t8 = MainCoroutineAsync();
    t8();

    t1.get();
    t2.get();
    t3.get();
    t4.get();
    t5.get();
    t6.get();
    t7.get();
    t8.get();
}

int main()
{
    test2();
}