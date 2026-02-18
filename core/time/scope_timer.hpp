#pragma once

#ifndef scope_timer_hpp
#define scope_timer_hpp

#include <chrono>
#include <string>

class ScopeTimer
{
public:
    using Clock = std::chrono::steady_clock;

    explicit ScopeTimer(const char* label)
        : m_label(label),
        m_start(Clock::now())
    {
    }

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
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start);
        std::cout << m_label << ": " << duration.count() << " ms\n";
    }

private:
    std::string m_label;
    Clock::time_point m_start;
};

#endif // !scope_timer_hpp
