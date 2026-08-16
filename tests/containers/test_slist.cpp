#include <gtest/gtest.h>

#include <any>
#include <concepts>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "containers/slist.hpp"

namespace
{
    // Collect a list's elements into a vector so assertions read clearly.
    template<typename T>
    std::vector<T> to_vector(const SList<T>& list)
    {
        std::vector<T> out;
        for (const auto& value : list)
        {
            out.push_back(value);
        }
        return out;
    }

    // A move-aware type used to verify that emplace/push forward correctly and
    // that the list owns/destroys exactly one instance per node.
    struct Tracker
    {
        static int alive;

        int value{ 0 };

        Tracker() { ++alive; }
        explicit Tracker(int v) : value(v) { ++alive; }
        Tracker(const Tracker& other) : value(other.value) { ++alive; }
        Tracker(Tracker&& other) noexcept : value(other.value) { ++alive; }
        Tracker& operator=(const Tracker&) = default;
        Tracker& operator=(Tracker&&) = default;
        ~Tracker() { --alive; }
    };

    int Tracker::alive = 0;

    // Throws from its copy constructor once a configurable number of copies has
    // succeeded, so that a half-built list can be checked for leaked elements.
    struct ThrowOnCopy
    {
        static int alive;
        static int copies_before_throw;          // negative disables throwing

        int value{ 0 };

        explicit ThrowOnCopy(int v) : value(v) { ++alive; }

        ThrowOnCopy(const ThrowOnCopy& other) : value(other.value)
        {
            if (copies_before_throw == 0)
            {
                throw std::runtime_error("copy failed");
            }

            if (copies_before_throw > 0)
            {
                --copies_before_throw;
            }

            ++alive;                             // only once the copy has survived
        }

        ThrowOnCopy& operator=(const ThrowOnCopy&) = default;
        ~ThrowOnCopy() { --alive; }
    };

    int ThrowOnCopy::alive = 0;
    int ThrowOnCopy::copies_before_throw = -1;

    // A user-defined type that is both comparable and streamable, used to check
    // that SList's operators forward to the element type's own operators.
    struct Point
    {
        int x{ 0 };
        int y{ 0 };

        bool operator==(const Point&) const = default;
    };

    std::ostream& operator<<(std::ostream& stream, const Point& point)
    {
        return stream << '(' << point.x << ',' << point.y << ')';
    }

    // Element types that support neither operator, used to check that SList's
    // constrained operators drop out instead of hard-erroring in the body.
    struct NotComparable { int v{ 0 }; };
    struct NotStreamable { int v{ 0 }; };
}

// ----------------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------------

TEST(SListConstruction, DefaultIsEmpty)
{
    SList<int> list;
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);
    EXPECT_EQ(list.begin(), list.end());
}

TEST(SListConstruction, SingleValue)
{
    SList<int> list(42);
    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.size(), 1u);
    EXPECT_EQ(list.front(), 42);
    EXPECT_EQ(list.back(), 42);
}

TEST(SListConstruction, InitializerList)
{
    SList<int> list{ 1, 2, 3 };
    EXPECT_EQ(list.size(), 3u);
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 1, 2, 3 }));
    EXPECT_EQ(list.front(), 1);
    EXPECT_EQ(list.back(), 3);
}

// ----------------------------------------------------------------------------
// Push / emplace / front / back
// ----------------------------------------------------------------------------

TEST(SListModifiers, PushBackAppends)
{
    SList<int> list;
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 1, 2, 3 }));
    EXPECT_EQ(list.back(), 3);
}

TEST(SListModifiers, PushFrontPrepends)
{
    SList<int> list;
    list.push_front(1);
    list.push_front(2);
    list.push_front(3);
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 3, 2, 1 }));
    EXPECT_EQ(list.front(), 3);
}

TEST(SListModifiers, EmplaceForwardsConstructorArgs)
{
    SList<Tracker> list;
    list.emplace_back(1);
    list.emplace_front(2);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list.front().value, 2);
    EXPECT_EQ(list.back().value, 1);
}

TEST(SListModifiers, FrontAndBackAreMutable)
{
    SList<int> list{ 1, 2, 3 };
    list.front() = 10;
    list.back() = 30;
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 10, 2, 30 }));
}

// ----------------------------------------------------------------------------
// Pop
// ----------------------------------------------------------------------------

TEST(SListModifiers, PopFrontRemovesHead)
{
    SList<int> list{ 1, 2, 3 };
    list.pop_front();
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 2, 3 }));
    EXPECT_EQ(list.front(), 2);
}

TEST(SListModifiers, PopBackRemovesTail)
{
    SList<int> list{ 1, 2, 3 };
    list.pop_back();
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 1, 2 }));
    EXPECT_EQ(list.back(), 2);
}

TEST(SListModifiers, PopUntilEmptyResetsState)
{
    SList<int> list{ 1, 2 };
    list.pop_back();
    list.pop_front();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);
    EXPECT_EQ(list.begin(), list.end());
}

TEST(SListModifiers, PopOnEmptyIsSafe)
{
    SList<int> list;
    list.pop_front();   // no-op, must not crash
    list.pop_back();    // no-op, must not crash
    EXPECT_TRUE(list.empty());
}

// ----------------------------------------------------------------------------
// insert_after / emplace_after / erase_after
// ----------------------------------------------------------------------------

TEST(SListPositional, EmplaceAfterBeforeBeginInsertsAtFront)
{
    SList<int> list{ 1, 2, 3 };
    list.emplace_after(list.cbefore_begin(), 0);
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 0, 1, 2, 3 }));
    EXPECT_EQ(list.front(), 0);
}

TEST(SListPositional, InsertAfterMiddleReturnsIteratorToNew)
{
    SList<int> list{ 1, 2, 3 };
    auto it = list.insert_after(list.begin(), 9);
    EXPECT_EQ(*it, 9);
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 1, 9, 2, 3 }));
}

TEST(SListPositional, InsertAfterTailUpdatesBack)
{
    SList<int> list{ 1, 2 };
    auto last = list.begin();
    ++last;                       // points at 2 (the tail)
    list.insert_after(last, 3);
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 1, 2, 3 }));
    EXPECT_EQ(list.back(), 3);
}

TEST(SListPositional, EraseAfterRemovesSingleElement)
{
    SList<int> list{ 1, 2, 3 };
    auto it = list.erase_after(list.begin());   // removes 2
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 1, 3 }));
}

TEST(SListPositional, EraseAfterTailUpdatesBack)
{
    SList<int> list{ 1, 2 };
    list.erase_after(list.begin());             // removes 2 (the tail)
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 1 }));
    EXPECT_EQ(list.back(), 1);
}

TEST(SListPositional, EraseAfterRangeRemovesInterior)
{
    SList<int> list{ 10, 20, 30, 40, 50 };
    auto first = list.begin();                  // points at 10
    auto last = list.begin();
    ++last; ++last; ++last;                      // points at 40
    list.erase_after(first, last);              // removes 20, 30
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 10, 40, 50 }));
}

// ----------------------------------------------------------------------------
// Copy / move / swap / clear
// ----------------------------------------------------------------------------

TEST(SListCopyMove, CopyConstructorIsDeep)
{
    SList<int> original{ 1, 2, 3 };
    SList<int> copy(original);

    original.front() = 99;                       // must not affect the copy
    EXPECT_EQ(to_vector(copy), (std::vector<int>{ 1, 2, 3 }));
    EXPECT_EQ(copy.size(), 3u);
}

TEST(SListCopyMove, CopyAssignmentIsDeep)
{
    SList<int> original{ 1, 2, 3 };
    SList<int> copy{ 7, 8 };
    copy = original;

    original.push_back(4);                       // must not affect the copy
    EXPECT_EQ(to_vector(copy), (std::vector<int>{ 1, 2, 3 }));
}

TEST(SListCopyMove, MoveConstructorTransfersAndLeavesSourceEmpty)
{
    SList<int> source{ 1, 2, 3 };
    SList<int> moved(std::move(source));

    EXPECT_EQ(to_vector(moved), (std::vector<int>{ 1, 2, 3 }));
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.size(), 0u);
}

TEST(SListCopyMove, MoveAssignmentTransfers)
{
    SList<int> source{ 1, 2, 3 };
    SList<int> target{ 9 };
    target = std::move(source);

    EXPECT_EQ(to_vector(target), (std::vector<int>{ 1, 2, 3 }));
    EXPECT_TRUE(source.empty());
}

TEST(SListCopyMove, MovedFromListIsReusable)
{
    SList<int> source{ 1, 2, 3 };
    SList<int> moved(std::move(source));

    source.push_back(7);                         // moved-from must be a valid state
    source.push_front(6);
    EXPECT_EQ(to_vector(source), (std::vector<int>{ 6, 7 }));
}

// Assignment must not list-initialize its temporary: for an element type that is
// constructible from the list itself (std::any and friends), braces would select
// the initializer_list constructor and collapse the source into one element.
TEST(SListCopyMove, AssignmentDoesNotWrapGreedyElementTypes)
{
    SList<std::any> source;
    source.push_back(std::any(1));
    source.push_back(std::any(2));

    SList<std::any> copy;
    copy = source;
    EXPECT_EQ(copy.size(), 2u);

    SList<std::any> moved;
    moved = std::move(source);
    EXPECT_EQ(moved.size(), 2u);
}

TEST(SListModifiers, SwapExchangesContents)
{
    SList<int> a{ 1, 2, 3 };
    SList<int> b{ 7, 8 };
    a.swap(b);

    EXPECT_EQ(to_vector(a), (std::vector<int>{ 7, 8 }));
    EXPECT_EQ(to_vector(b), (std::vector<int>{ 1, 2, 3 }));
}

TEST(SListModifiers, ClearEmptiesList)
{
    SList<int> list{ 1, 2, 3 };
    list.clear();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);
    EXPECT_EQ(list.begin(), list.end());

    list.push_back(5);                           // usable after clear
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 5 }));
}

// ----------------------------------------------------------------------------
// Lookup
// ----------------------------------------------------------------------------

TEST(SListLookup, Contains)
{
    SList<int> list{ 1, 2, 3 };
    EXPECT_TRUE(list.contains(2));
    EXPECT_FALSE(list.contains(99));

    SList<int> empty;
    EXPECT_FALSE(empty.contains(0));
}

// ----------------------------------------------------------------------------
// Comparison
// ----------------------------------------------------------------------------

TEST(SListComparison, EmptyListsAreEqual)
{
    SList<int> a;
    SList<int> b;

    EXPECT_TRUE(a == b);                         // size guard passes, loop never runs
    EXPECT_FALSE(a != b);
}

TEST(SListComparison, EqualContentsCompareEqual)
{
    SList<int> a{ 1, 2, 3 };
    SList<int> b{ 1, 2, 3 };

    EXPECT_EQ(a, b);
    EXPECT_FALSE(a != b);
}

TEST(SListComparison, ListIsEqualToItself)
{
    SList<int> a{ 1, 2, 3 };
    EXPECT_EQ(a, a);

    SList<int> empty;
    EXPECT_EQ(empty, empty);
}

TEST(SListComparison, DifferenceAtHeadIsDetected)
{
    SList<int> a{ 1, 2, 3 };
    SList<int> b{ 9, 2, 3 };

    EXPECT_NE(a, b);
}

TEST(SListComparison, DifferenceAtTailIsDetected)
{
    SList<int> a{ 1, 2, 3 };
    SList<int> b{ 1, 2, 9 };

    EXPECT_NE(a, b);
}

TEST(SListComparison, DifferentSizesCompareUnequalBothWays)
{
    SList<int> shorter{ 1, 2, 3 };
    SList<int> longer{ 1, 2, 3, 4 };

    // Both directions matter: the size guard is what keeps the walk from
    // running off the end of the shorter list.
    EXPECT_NE(shorter, longer);
    EXPECT_NE(longer, shorter);
}

TEST(SListComparison, EmptyIsUnequalToNonEmptyBothWays)
{
    SList<int> empty;
    SList<int> filled{ 1 };

    EXPECT_NE(empty, filled);
    EXPECT_NE(filled, empty);
}

TEST(SListComparison, ListsEmptiedByDifferentRoutesAreEqual)
{
    SList<int> fresh;

    SList<int> cleared{ 1, 2, 3 };
    cleared.clear();

    SList<int> popped{ 5 };
    popped.pop_front();

    SList<int> drained{ 5 };
    drained.pop_back();

    SList<int> source{ 1, 2 };
    SList<int> sink(std::move(source));          // source is now empty

    EXPECT_EQ(cleared, fresh);
    EXPECT_EQ(popped, fresh);
    EXPECT_EQ(drained, fresh);
    EXPECT_EQ(source, fresh);
}

TEST(SListComparison, CopyEqualsOriginalAndIsIndependent)
{
    SList<int> original{ 1, 2, 3 };
    SList<int> copy(original);
    EXPECT_EQ(copy, original);

    copy.push_back(4);
    EXPECT_NE(copy, original);
}

TEST(SListComparison, ComparesElementsNotAddresses)
{
    // Distinct nodes holding equal values must compare equal; a memberwise
    // (pointer) comparison would report these as different.
    SList<int> a{ 1, 2, 3 };
    SList<int> b;
    b.push_back(1);
    b.push_back(2);
    b.push_back(3);

    EXPECT_EQ(a, b);
}

TEST(SListComparison, UsesElementEqualityOperator)
{
    SList<Point> a{ Point{ 1, 2 }, Point{ 3, 4 } };
    SList<Point> b{ Point{ 1, 2 }, Point{ 3, 4 } };
    SList<Point> c{ Point{ 1, 2 }, Point{ 3, 5 } };

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(SListComparison, WorksForNonTrivialElements)
{
    SList<std::string> a{ "alpha", "beta" };
    SList<std::string> b{ "alpha", "beta" };
    SList<std::string> c{ "alpha", "gamma" };

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(SListComparison, EqualityIsConstrainedOnTheElementType)
{
    // The requires clause must make the concept honest: without it, SList<T>
    // claims to be equality comparable for any T and hard-errors on use.
    static_assert(std::equality_comparable<SList<int>>);
    static_assert(std::equality_comparable<SList<Point>>);
    static_assert(std::equality_comparable<SList<std::string>>);
    static_assert(!std::equality_comparable<NotComparable>);
    static_assert(!std::equality_comparable<SList<NotComparable>>);

    // Nested lists work because SList<int> is itself equality comparable.
    static_assert(std::equality_comparable<SList<SList<int>>>);

    // A non-comparable element type must still give a usable container.
    SList<NotComparable> list;
    list.push_back(NotComparable{ 1 });
    list.push_back(NotComparable{ 2 });
    SList<NotComparable> copy(list);
    copy.pop_front();

    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(copy.size(), 1u);
}

// ----------------------------------------------------------------------------
// Iterators
// ----------------------------------------------------------------------------

TEST(SListIterator, RangeForVisitsInOrder)
{
    SList<int> list{ 1, 2, 3 };
    std::vector<int> seen;
    for (int v : list)
    {
        seen.push_back(v);
    }
    EXPECT_EQ(seen, (std::vector<int>{ 1, 2, 3 }));
}

TEST(SListIterator, PostIncrementReturnsPreviousPosition)
{
    SList<int> list{ 1, 2 };
    auto it = list.begin();
    auto prev = it++;
    EXPECT_EQ(*prev, 1);
    EXPECT_EQ(*it, 2);
}

TEST(SListIterator, MutableIteratorWritesThrough)
{
    SList<int> list{ 1, 2, 3 };
    for (auto it = list.begin(); it != list.end(); ++it)
    {
        *it += 10;
    }
    EXPECT_EQ(to_vector(list), (std::vector<int>{ 11, 12, 13 }));
}

TEST(SListIterator, ConstIteratorComparesWithMutable)
{
    SList<int> list{ 1, 2, 3 };
    auto it = list.begin();
    auto cit = list.cbegin();
    EXPECT_EQ(it, cit);
    ++it;
    EXPECT_NE(it, cit);
}

TEST(SListIterator, ConstListIsIterable)
{
    const SList<int> list{ 4, 5, 6 };
    std::vector<int> seen;
    for (const auto& v : list)
    {
        seen.push_back(v);
    }
    EXPECT_EQ(seen, (std::vector<int>{ 4, 5, 6 }));
}

// ----------------------------------------------------------------------------
// Streaming
// ----------------------------------------------------------------------------

namespace
{
    template<typename T>
    std::string to_string(const SList<T>& list)
    {
        std::ostringstream oss;
        oss << list;
        return oss.str();
    }
}

TEST(SListStream, OstreamOperatorFormatsCommaSeparated)
{
    SList<int> list{ 1, 2, 3 };
    std::ostringstream oss;
    oss << list;
    EXPECT_EQ(oss.str(), "1, 2, 3");
}

TEST(SListStream, EmptyListStreamsNothing)
{
    SList<int> list;
    EXPECT_EQ(to_string(list), "");
}

TEST(SListStream, SingleElementHasNoSeparator)
{
    // The only case where m_head == m_tail, i.e. the boundary of the
    // "print a separator unless this is the tail" condition.
    SList<int> list{ 7 };
    EXPECT_EQ(to_string(list), "7");
}

TEST(SListStream, TwoElementsAreSeparatedOnce)
{
    SList<int> list{ 7, 8 };
    EXPECT_EQ(to_string(list), "7, 8");
}

TEST(SListStream, DuplicateValuesAreStillSeparated)
{
    // The separator test compares node addresses, not values. Comparing values
    // would suppress every separator here and print "555".
    SList<int> list{ 5, 5, 5 };
    EXPECT_EQ(to_string(list), "5, 5, 5");
}

TEST(SListStream, OutputTracksMutations)
{
    // Separator placement depends on m_tail, so streaming after each mutation
    // doubles as a check that the tail pointer stayed correct.
    SList<int> list{ 1, 2, 3 };

    list.pop_back();
    EXPECT_EQ(to_string(list), "1, 2");

    list.pop_front();
    EXPECT_EQ(to_string(list), "2");

    list.push_back(9);
    EXPECT_EQ(to_string(list), "2, 9");

    list.clear();
    EXPECT_EQ(to_string(list), "");

    list.push_front(4);
    EXPECT_EQ(to_string(list), "4");
}

TEST(SListStream, OutputIsCorrectAfterEraseAfter)
{
    SList<int> middle{ 1, 2, 3 };
    middle.erase_after(middle.cbegin());         // removes 2
    EXPECT_EQ(to_string(middle), "1, 3");

    SList<int> tail{ 1, 2, 3 };
    auto pos = tail.cbegin();
    ++pos;
    tail.erase_after(pos);                       // removes the tail, 3
    EXPECT_EQ(to_string(tail), "1, 2");
}

TEST(SListStream, SupportsChaining)
{
    SList<int> a{ 1, 2, 3 };
    SList<int> b{ 7 };

    std::ostringstream oss;
    oss << a << " | " << b;                      // must return the stream
    EXPECT_EQ(oss.str(), "1, 2, 3 | 7");
}

TEST(SListStream, UsesElementStreamOperator)
{
    SList<Point> list{ Point{ 1, 2 }, Point{ 3, 4 } };
    EXPECT_EQ(to_string(list), "(1,2), (3,4)");
}

TEST(SListStream, StreamingIsConstrainedOnTheElementType)
{
    static_assert(has_ostream_operator<SList<int>>::value);
    static_assert(has_ostream_operator<SList<Point>>::value);
    static_assert(!has_ostream_operator<NotStreamable>::value);
    static_assert(!has_ostream_operator<SList<NotStreamable>>::value);

    // A non-streamable element type must still give a usable container.
    SList<NotStreamable> list;
    list.push_back(NotStreamable{ 1 });
    EXPECT_EQ(list.size(), 1u);
}

// ----------------------------------------------------------------------------
// Resource management
// ----------------------------------------------------------------------------

TEST(SListResource, DestructorReleasesEveryNode)
{
    ASSERT_EQ(Tracker::alive, 0);
    {
        SList<Tracker> list;
        list.emplace_back(1);
        list.emplace_back(2);
        list.emplace_back(3);
        EXPECT_EQ(Tracker::alive, 3);
    }
    EXPECT_EQ(Tracker::alive, 0);   // no leaks, no double-frees
}

TEST(SListResource, CopyConstructorLeaksNothingWhenElementThrows)
{
    SList<ThrowOnCopy> source;
    source.push_back(ThrowOnCopy(1));
    source.push_back(ThrowOnCopy(2));
    source.push_back(ThrowOnCopy(3));

    const int before = ThrowOnCopy::alive;
    ThrowOnCopy::copies_before_throw = 2;        // fail on the third element

    EXPECT_THROW({ SList<ThrowOnCopy> copy(source); }, std::runtime_error);

    ThrowOnCopy::copies_before_throw = -1;
    EXPECT_EQ(ThrowOnCopy::alive, before);       // the two copies made are released
    EXPECT_EQ(source.size(), 3u);                // source is untouched
}

TEST(SListResource, InitializerListConstructorLeaksNothingWhenElementThrows)
{
    const int before = ThrowOnCopy::alive;
    ThrowOnCopy::copies_before_throw = 2;        // fail on the third element

    EXPECT_THROW(
        (SList<ThrowOnCopy>{ ThrowOnCopy(1), ThrowOnCopy(2), ThrowOnCopy(3) }),
        std::runtime_error);

    ThrowOnCopy::copies_before_throw = -1;
    EXPECT_EQ(ThrowOnCopy::alive, before);
}
