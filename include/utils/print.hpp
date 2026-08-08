#ifndef PRINT_HPP
#define PRINT_HPP

#include <iostream>
#include <type_traits>

#include "utils/traits.hpp"

template<typename T>
void print(const T& a)
{
    static_assert(has_ostream_operator<T>::value, "Type must overload operator<<");

    std::cout << a << '\n';
}

#endif // PRINT_HPP
