#include <thread>
#include <iostream>

#include "time/scope_timer.hpp"
#include "containers/slist.hpp"
#include "utils/print.hpp"

using namespace std;

struct Point
{
    double x{ 0.0 };
    double y{ 0.0 };
};

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

    int a = 25;
    Point p1 = { 2.6, 8.5 };
    SList<int> slist1{ 5 };
    SList<int> slist2{ 1, 2, 3 };
    SList<int> slist3 = { 4, 5, 6 };
    SList<int> slist4;
    SList<int> slist5{};
    SList<int> slist6{ slist2 };
    SList<Point> slist7{ p1 };
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
    print(slist1);
    print(slist2);
    print(slist3);
    print(slist4);
    print(slist5);
    print(slist6);
    //cout << slist7 << endl;
    slist4.push_back(5);
    cout << slist4 << endl;
    slist4.push_back(int{ 6 });
    cout << slist4 << endl;
    slist4.push_back(a);
    cout << slist4 << endl;
    slist4.push_front(a);
    cout << slist4 << endl;
    slist4.push_front(34);
    cout << slist4 << endl;
    slist4.push_front(int{ 78 });
    cout << slist4 << endl;
    slist4.pop_front();
    cout << slist4 << endl;
    slist4.pop_back();
    cout << slist4 << endl;
    slist4.pop_back();
    cout << slist4 << endl;
    cout << slist4.contains(25) << endl;
    cout << slist4.contains(35) << endl;

    return 0;
}
