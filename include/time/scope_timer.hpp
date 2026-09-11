#ifndef SCOPE_TIMER_HPP
#define SCOPE_TIMER_HPP

#include <chrono>
#include <string>
#include <iostream>
#include <utility>
#include <type_traits>
#include <cstdint>
#include <sstream>
#include <ratio>

namespace ccs
{

namespace detail
{

template<typename>
struct is_ratio_type : std::false_type {};

template<std::intmax_t N, std::intmax_t D>
struct is_ratio_type<std::ratio<N, D>> : std::true_type {};

}

template <typename Ratio>
class ScopeTimer
{
    static_assert(
        detail::is_ratio_type<Ratio>::value,
        "ScopeTimer<Ratio>: Ratio must be a std::ratio type"
        );

public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<double, Ratio>;

    [[nodiscard("A ScopeTimer temporary measures nothing; name the variable.")]]
    explicit ScopeTimer(std::string label, std::ostream& os = std::cout) :
        m_label(std::move(label)),
        m_os(os),
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
            // std::ostream::operator<< can throw if exceptions are enabled on the stream
            // (via os.exceptions(...)). That's rare in practice, but a throwing destructor
            // would call std::terminate, so we swallow it unconditionally.
            const auto end = Clock::now();
            const Duration duration = end - m_start;
            std::ostringstream oss; // fresh stream: default flags, default locale
            oss.precision(15);
            oss << m_label << ": " << duration << "\n";
            m_os << oss.str(); // one insertion
        }
        catch (...)
        {
        }
    }

private:
    std::string m_label;
    std::ostream& m_os;
    Clock::time_point m_start;
};

} // namespace ccs

#if defined(__COUNTER__)
#define SCOPE_TIMER_UNIQUE_ID __COUNTER__
#else
#define SCOPE_TIMER_UNIQUE_ID __LINE__
#endif

#define SCOPE_TIMER_CONCAT_INNER(a, b) a##b
#define SCOPE_TIMER_CONCAT(a, b) SCOPE_TIMER_CONCAT_INNER(a, b)

#define SCOPE_TIMER_SEC(label) \
  ccs::ScopeTimer<std::ratio<1>> SCOPE_TIMER_CONCAT(scope_timer_, SCOPE_TIMER_UNIQUE_ID)(label)

#define SCOPE_TIMER_MILLI(label) \
  ccs::ScopeTimer<std::milli> SCOPE_TIMER_CONCAT(scope_timer_, SCOPE_TIMER_UNIQUE_ID)(label)

#define SCOPE_TIMER_MICRO(label) \
  ccs::ScopeTimer<std::micro> SCOPE_TIMER_CONCAT(scope_timer_, SCOPE_TIMER_UNIQUE_ID)(label)

#define SCOPE_TIMER_NANO(label) \
  ccs::ScopeTimer<std::nano> SCOPE_TIMER_CONCAT(scope_timer_, SCOPE_TIMER_UNIQUE_ID)(label)

#define SCOPE_TIMER(label) SCOPE_TIMER_MILLI(label)

#endif // SCOPE_TIMER_HPP
