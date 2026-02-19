#include <thread>

#include "time/scope_timer.hpp"

using namespace std;

int main()
{
    SCOPE_TIMER("HelloWorld");

    {
        SCOPE_TIMER("Sleep_test");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    {
        SCOPE_TIMER("String_test");
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    {
        SCOPE_TIMER("Default");
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    //ScopeTimer<int> timer("bad");

    return 0;
}