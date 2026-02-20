#include <thread>
#include <iostream>

#include "time/scope_timer.hpp"
#include "containers/slist.hpp"

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

    SList<int> slist1{ 5 };
    SList<int> slist2{ 1, 2, 3 };
    SList<int> slist3 = { 4, 5, 6 };
    SList<int> slist4;
    SList<int> slist5{};
    SList<int> slist6{ slist2 };
    slist6 = slist3;

    cout << slist1.size() << endl;
    cout << slist2.size() << endl;
    cout << slist3.size() << endl;
    cout << slist4.size() << endl;
    cout << slist5.size() << endl;
    cout << slist6.size() << endl;
    cout << slist1.empty() << endl;
    cout << slist2.empty() << endl;
    cout << slist3.empty() << endl;
    cout << slist4.empty() << endl;
    cout << slist5.empty() << endl;
    cout << slist6.empty() << endl;

    return 0;
}
