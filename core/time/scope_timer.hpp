#ifndef SCOPE_TIMER_HPP
#define SCOPE_TIMER_HPP

#include <chrono>
#include <string>
#include <iostream>
#include <utility>
#include <type_traits>
#include <cstdint>

template <typename Ratio>
struct ScopeTimerUnitSymbol
{
    static constexpr const char* value() { return " (custom)"; }
};

template <>
struct ScopeTimerUnitSymbol<std::milli>
{
    static constexpr const char* value() { return " ms"; }
};

template <>
struct ScopeTimerUnitSymbol<std::micro>
{
    static constexpr const char* value() { return " us"; }
};

template <>
struct ScopeTimerUnitSymbol<std::nano>
{
    static constexpr const char* value() { return " ns"; }
};

template<typename>
struct is_ratio_type : std::false_type {};

template<std::intmax_t N, std::intmax_t D>
struct is_ratio_type<std::ratio<N, D>> : std::true_type {};

template <typename Ratio>
class ScopeTimer
{
    static_assert(
        is_ratio_type<Ratio>::value,
        "ScopeTimer<Ratio>: Ratio must be a std::ratio type"
        );

public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<double, Ratio>;

    explicit ScopeTimer(std::string label) :
        m_label(std::move(label)),
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
            const auto duration = std::chrono::duration_cast<Duration>(end - m_start);
            std::cout << m_label << ": " << duration.count() << ScopeTimerUnitSymbol<Ratio>::value() << "\n";
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

#define SCOPE_TIMER_MILLI(label) \
  ScopeTimer<std::milli> SCOPE_TIMER_CONCAT(_scopeTimer_, SCOPE_TIMER_UNIQUE_ID)(label)

#define SCOPE_TIMER_MICRO(label) \
  ScopeTimer<std::micro> SCOPE_TIMER_CONCAT(_scopeTimer_, SCOPE_TIMER_UNIQUE_ID)(label)

#define SCOPE_TIMER_NANO(label) \
  ScopeTimer<std::nano> SCOPE_TIMER_CONCAT(_scopeTimer_, SCOPE_TIMER_UNIQUE_ID)(label)

#define SCOPE_TIMER(label) SCOPE_TIMER_MILLI(label)

#endif // SCOPE_TIMER_HPP
