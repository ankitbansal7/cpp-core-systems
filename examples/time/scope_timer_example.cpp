#include <iostream>
#include <thread>

#include "time/scope_timer.hpp"

using namespace std;

int main()
{
    ScopeTimer st{ "Hello World!" };

    {
        ScopeTimer t{ "Sleep test" };
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}