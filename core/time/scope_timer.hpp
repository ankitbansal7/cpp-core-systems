#ifndef SCOPE_TIMER_HPP
#define SCOPE_TIMER_HPP

#include <chrono>
#include <string>
#include <iostream>

class ScopeTimer
{
public:
    using Clock = std::chrono::steady_clock;

    explicit ScopeTimer(std::string label)
        : m_label(std::move(label)),
        m_start(Clock::now())
    {
    }

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;
    ScopeTimer(ScopeTimer&&) = delete;
    ScopeTimer& operator=(ScopeTimer&&) = delete;

    ~ScopeTimer() noexcept
    {
        try
        {
            const auto end = Clock::now();
            const auto duration = std::chrono::duration<double, std::milli>(end - m_start);
            std::cout << m_label << ": " << duration.count() << " ms\n";
        }
        catch (...)
        {
        }
    }

private:
    std::string m_label;
    Clock::time_point m_start;
};

#if defined(__COUNTER__)
#define SCOPE_TIMER_UNIQUE_ID __COUNTER__
#else
#define SCOPE_TIMER_UNIQUE_ID __LINE__
#endif

#define SCOPE_TIMER_CONCAT_INNER(a, b) a##b
#define SCOPE_TIMER_CONCAT(a, b) SCOPE_TIMER_CONCAT_INNER(a, b)
#define SCOPE_TIMER(label) ScopeTimer SCOPE_TIMER_CONCAT(_scopeTimer_, SCOPE_TIMER_UNIQUE_ID)(label)

#endif // SCOPE_TIMER_HPP
