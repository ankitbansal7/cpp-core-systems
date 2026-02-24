#ifndef TRAITS_HPP
#define TRAITS_HPP

#include <type_traits>
#include <utility>

template<typename T>
class has_ostream_operator
{
private:
    template<typename U>
    static auto test(int)
        -> decltype(std::declval<std::ostream&>() << std::declval<const U&>(),
            std::true_type());

    template<typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};


#endif // TRAITS_HPP
