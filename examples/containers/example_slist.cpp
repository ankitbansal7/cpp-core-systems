#include <iostream>
#include <sstream>
#include <vector>

#include "containers/slist.hpp"

struct Point
{
    double x{ 0.0 };
    double y{ 0.0 };
    Point() = default;
    Point(double xx, double yy) : x(xx), y(yy) {}
};
static std::ostream& operator<<(std::ostream& os, const Point& p)
{
    os << "(" << p.x << ", " << p.y << ")";
    return os;
}

struct Foo
{
    int v;
    Foo() = default;
    explicit Foo(int x) : v(x) {}
};

template<typename T>
static std::vector<T> collect(const SList<T>& list)
{
    std::vector<T> out;
    for (const auto& v : list)
        out.push_back(v);
    return out;
}

static void print_header(const char* title)
{
    std::cout << "\n=== " << title << " ===\n";
}

int main()
{
    print_header("Constructors");
    SList<int> empty;
    SList<int> single(42);
    SList<int> list = { 1, 2, 3 };
    std::cout << "empty.size() = " << empty.size() << '\n';
    std::cout << "single: " << single << " size=" << single.size() << '\n';
    std::cout << "list:   " << list << " size=" << list.size() << '\n';

    print_header("Push / Pop / Emplace");
    list.push_back(4);              // push_back(rvalue)
    list.push_front(0);             // push_front(lvalue/rvalue
    list.emplace_back(5);           // emplace_back forwarding
    list.emplace_front(-1);         // emplace_front forwarding
    std::cout << "after pushes/emplace: " << list << " size=" << list.size() << '\n';

    // pop_front/pop_back normal behavior
    list.pop_front();
    list.pop_back();
    std::cout << "after pop_front/pop_back: " << list << " size=" << list.size() << '\n';

    // pop_back on single-element list -> sets head/tail to nullptr
    SList<int> singlePop(100);
    std::cout << "singlePop before: " << singlePop << " size=" << singlePop.size() << '\n';
    singlePop.pop_back();
    std::cout << "singlePop after pop_back: size=" << singlePop.size() << " empty=" << singlePop.empty() << '\n';

    // pop on empty should be safe (no-op)
    SList<int> nothing;
    nothing.pop_back();
    nothing.pop_front();
    std::cout << "pop on empty is safe (no crash), size=" << nothing.size() << '\n';

    print_header("Contains / Accessors");
    std::cout << "list contains 2? " << (list.contains(2) ? "yes" : "no") << '\n';
    if (!list.empty())
    {
        std::cout << "front(): " << list.front() << "  back(): " << list.back() << '\n';
    }

    print_header("Iterator usage, operator->, and iterator comparisons");
    // Use Foo to show operator-> works
    SList<Foo> foos;
    foos.emplace_back(1); // forwards to Foo(int)
    foos.emplace_back(2);
    for (auto it = foos.begin(); it != foos.end(); ++it)
    {
        std::cout << "foo.v via -> : " << it->v << '\n';
        it->v += 10; // modify through iterator
    }
    std::cout << "foos after modification: ";
    for (const auto& f : foos) std::cout << f.v << ' ';
    std::cout << '\n';

    // iterator vs const_iterator comparison
    SList<int> cmp{ 7, 8, 9 };
    auto it = cmp.begin();
    auto cit = cmp.cbegin();
    std::cout << "it == cit? " << std::boolalpha << (it == cit) << '\n';
    ++it;
    std::cout << "after ++it, it != cit? " << (it != cit) << '\n';

    print_header("Range-for (const)");
    const SList<int> constList = cmp;
    std::cout << "constList iteration: ";
    for (const auto& v : constList) std::cout << v << ' ';
    std::cout << '\n';

    print_header("Copy / Move / Assignment");
    // copy ctor demonstrated earlier; show copy assignment
    SList<int> a = { 1, 2 };
    SList<int> b;
    b = a; // copy assignment
    std::cout << "b after copy assignment: " << b << '\n';

    // move assignment
    SList<int> c;
    c = std::move(b);
    std::cout << "c after move assignment: " << c << " b.size()=" << b.size() << '\n';

    // swap
    SList<int> A{ 1,2,3 };
    SList<int> B{ 7,8 };
    std::cout << "before swap A=" << A << " B=" << B << '\n';
    A.swap(B);
    std::cout << " after swap  A=" << A << " B=" << B << '\n';

    print_header("Clear");
    A.clear();
    std::cout << "A cleared, empty? " << std::boolalpha << A.empty() << '\n';

    print_header("Using SList with custom type (Point)");
    Point p1{ 2.5, 4.0 };
    SList<Point> pts;
    pts.push_back(p1);
    // emplace_back forwards to Point(double,double) because Point has matching ctor
    pts.emplace_back(1.0, 1.5);
    std::cout << "points: " << pts << " size=" << pts.size() << '\n';

    print_header("emplace_after / insert_after / erase_after / before_begin");
    SList<int> seq{ 1, 2, 3, 4, 5 };
    std::cout << "seq: " << seq << '\n';

    // insert at front using before_begin + emplace_after
    seq.emplace_after(seq.cbefore_begin(), 0);
    std::cout << "after emplace_after(cbefore_begin(), 0): " << seq << '\n';

    // insert after first element
    seq.insert_after(seq.begin(), 9);
    std::cout << "after insert_after(begin(), 9): " << seq << '\n';

    // erase the element after the first element
    seq.erase_after(seq.begin());
    std::cout << "after erase_after(begin()): " << seq << '\n';

    // erase a range: remove elements 20 and 30 from the list below
    SList<int> r{ 10, 20, 30, 40, 50 };
    std::cout << "r before range erase: " << r << '\n';
    auto first = r.begin();           // points to 10
    auto last = r.begin(); ++last; ++last; ++last; // advance last to point to 40
    // erase elements after 'first' until reaching 'last' (removes 20 and 30)
    r.erase_after(first, last);
    std::cout << "r after erase_after(first, last) removing range: " << r << '\n';

    // emplace_after with custom type
    pts.emplace_after(pts.cbefore_begin(), Point(3.3, 4.4));
    std::cout << "pts after emplace_after at front: " << pts << '\n';

    print_header("Collected values");
    auto values = collect(c);
    std::cout << "collected moved elements:";
    for (auto& v : values) std::cout << ' ' << v;
    std::cout << '\n';

    std::cout << "\nExample finished successfully.\n";
    return 0;
}
