#ifndef SLIST_HPP
#define SLIST_HPP

#include <cstddef>
#include <utility>
#include <initializer_list>
#include <ostream>
#include <type_traits>
#include <iterator>
#include <memory>

#include "utils/traits.hpp"

template<typename T>
class SList
{
    struct NodeBase
    {
    public:
        NodeBase* m_next{ nullptr };
    };

    struct Node : public NodeBase
    {
    public:
        template<typename... Args>
        explicit Node(Args&&... args) : m_value(std::forward<Args>(args)...) {}

        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = delete;
        Node& operator=(Node&&) = delete;

        ~Node() = default;

        Node* next() noexcept { return static_cast<Node*>(this->m_next); }
        const Node* next() const noexcept { return static_cast<const Node*>(this->m_next); }
        void set_next(Node* node) noexcept { this->m_next = node; }

    public:
        T m_value;
    };

    template<bool IsConst>
    class SListIterator
    {
        template<bool>
        friend class SListIterator;

        friend class SList<T>;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;

    private:
        using NodePtr = std::conditional_t<IsConst, const NodeBase*, NodeBase*>;
        using ValueNodePtr = std::conditional_t<IsConst, const Node*, Node*>;

    public:
        SListIterator() = default;
        explicit SListIterator(NodePtr node) noexcept : m_current(node) {}
        SListIterator(const SListIterator<false>& other) noexcept
            requires IsConst
            : m_current(other.m_current) {}

        SListIterator(const SListIterator&) = default;
        SListIterator& operator=(const SListIterator&) = default;
        SListIterator(SListIterator&&) = default;
        SListIterator& operator=(SListIterator&&) = default;

        ~SListIterator() = default;

        reference operator*() const { return static_cast<ValueNodePtr>(m_current)->m_value; }
        pointer operator->() const { return std::addressof(static_cast<ValueNodePtr>(m_current)->m_value); }

        template<bool OtherIsConst>
        bool operator==(const SListIterator<OtherIsConst>& other) const noexcept { return m_current == other.m_current; }

        template<bool OtherIsConst>
        bool operator!=(const SListIterator<OtherIsConst>& other) const noexcept { return !(*this == other); }

        SListIterator& operator++() noexcept
        {
            m_current = m_current->m_next;
            return *this;
        }

        SListIterator operator++(int) noexcept
        {
            SListIterator temp = *this;
            ++(*this);
            return temp;
        }

    private:
        NodePtr m_current{ nullptr };
    };

public:
    using iterator = SListIterator<false>;
    using const_iterator = SListIterator<true>;

public:
    SList() = default;

    explicit SList(T val);
    SList(std::initializer_list<T> values);
    SList(const SList& other);
    SList(SList&& other) noexcept;

    SList& operator=(const SList& other);
    SList& operator=(SList&& other) noexcept;

    friend std::ostream& operator<<(std::ostream& stream, const SList& list)
    {
        static_assert(has_ostream_operator<T>::value, "SList Type must overload operator<<");

        const Node* node = list.m_head;

        while (node)
        {
            stream << node->m_value;

            if (node != list.m_tail)
            {
                stream << ", ";
            }

            node = node->next();
        }

        return stream;
    }

    ~SList() noexcept;

    template<typename... Args>
    void emplace_back(Args&&... args);

    template<typename... Args>
    void emplace_front(Args&&... args);

    template<typename... Args>
    iterator emplace_after(const_iterator pos, Args&&... args);

    void push_back(const T& value);
    void push_back(T&& value);
    void push_front(const T& value);
    void push_front(T&& value);
    iterator insert_after(const_iterator pos, const T& value);
    iterator insert_after(const_iterator pos, T&& value);
    void pop_back() noexcept;
    void pop_front() noexcept;
    void clear() noexcept;
    void swap(SList& other) noexcept;
    bool contains(const T& value) const;

    inline std::size_t size() const noexcept { return m_size; }
    inline bool empty() const noexcept { return (m_size == 0); }
    inline T& front() noexcept { return m_head->m_value; }
    inline const T& front() const noexcept { return m_head->m_value; }
    inline T& back() noexcept { return m_tail->m_value; }
    inline const T& back() const noexcept { return m_tail->m_value; }

    inline iterator begin() noexcept { return iterator(m_head); }
    inline const_iterator begin() const noexcept { return const_iterator(m_head); }
    inline const_iterator cbegin() const noexcept { return begin(); }
    inline iterator end() noexcept { return iterator(nullptr); }
    inline const_iterator end() const noexcept { return const_iterator(nullptr); }
    inline const_iterator cend() const noexcept { return end(); }
    inline iterator before_begin() noexcept { return iterator(&m_before_head); }
    inline const_iterator before_begin() const noexcept { return const_iterator(&m_before_head); }
    inline const_iterator cbefore_begin() const noexcept { return before_begin(); }

private:
    void sync_before_head() noexcept { m_before_head.m_next = m_head; }

private:
    NodeBase m_before_head;
    Node* m_head{ nullptr };
    Node* m_tail{ nullptr };
    std::size_t m_size{ 0 };
};

template<typename T>
SList<T>::SList(T val) :
    m_head(new Node(std::move(val))),
    m_tail(m_head),
    m_size(1)
{
    sync_before_head();
}

template<typename T>
SList<T>::SList(std::initializer_list<T> values)
{
    for (const T& value : values)
    {
        push_back(value);
    }

    sync_before_head();
}

template<typename T>
SList<T>::SList(const SList& other)
{
    const Node* temp = other.m_head;

    while (temp)
    {
        push_back(temp->m_value);
        temp = temp->next();
    }

    sync_before_head();
}

template<typename T>
SList<T>::SList(SList&& other) noexcept :
    m_head(std::exchange(other.m_head, nullptr)),
    m_tail(std::exchange(other.m_tail, nullptr)),
    m_size(std::exchange(other.m_size, 0))
{
    sync_before_head();
    other.sync_before_head();
}

template<typename T>
SList<T>& SList<T>::operator=(const SList& other)
{
    SList{ other }.swap(*this);
    return *this;
}

template<typename T>
SList<T>& SList<T>::operator=(SList&& other) noexcept
{
    SList{ std::move(other) }.swap(*this);
    return *this;
}

template<typename T>
SList<T>::~SList() noexcept
{
    clear();
}

template<typename T>
template<typename... Args>
void SList<T>::emplace_back(Args&&... args)
{
    Node* node = new Node(std::forward<Args>(args)...);

    if (!m_head)
    {
        m_head = node;
        m_tail = m_head;
        sync_before_head();
    }
    else
    {
        m_tail->set_next(node);
        m_tail = node;
    }

    m_size++;
}

template<typename T>
template<typename... Args>
void SList<T>::emplace_front(Args&&... args)
{
    Node* node = new Node(std::forward<Args>(args)...);

    if (!m_head)
    {
        m_head = node;
        m_tail = m_head;
    }
    else
    {
        node->set_next(m_head);
        m_head = node;
    }

    sync_before_head();
    m_size++;
}

template<typename T>
template<typename... Args>
SList<T>::iterator SList<T>::emplace_after(SList<T>::const_iterator pos, Args&&... args)
{
    NodeBase* current = const_cast<NodeBase*>(pos.m_current);
    Node* node = new Node(std::forward<Args>(args)...);

    node->set_next(static_cast<Node*>(current->m_next));
    current->m_next = node;

    if (current == &m_before_head)
    {
        m_head = node;
    }

    if ((m_tail == current) || (m_tail == nullptr))
    {
        m_tail = node;
    }

    m_size++;

    return iterator(node);
}

template<typename T>
void SList<T>::push_back(const T& value)
{
    emplace_back(value);
}

template<typename T>
void SList<T>::push_back(T&& value)
{
    emplace_back(std::move(value));
}

template<typename T>
void SList<T>::push_front(const T& value)
{
    emplace_front(value);
}

template<typename T>
void SList<T>::push_front(T&& value)
{
    emplace_front(std::move(value));
}

template<typename T>
SList<T>::iterator SList<T>::insert_after(SList<T>::const_iterator pos, const T& value)
{
    return emplace_after(pos, value);
}

template<typename T>
SList<T>::iterator SList<T>::insert_after(SList<T>::const_iterator pos, T&& value)
{
    return emplace_after(pos, std::move(value));
}

template<typename T>
void SList<T>::pop_back() noexcept
{
    if (!m_head)
    {
        return;
    }

    if (!m_head->next())
    {
        delete m_head;
        m_size--;
        m_head = nullptr;
        m_tail = nullptr;
        sync_before_head();
        return;
    }

    Node* prev = m_head;
    Node* curr = m_head->next();

    while (curr->next())
    {
        curr = curr->next();
        prev = prev->next();
    }

    delete curr;
    prev->set_next(nullptr);
    m_tail = prev;
    m_size--;
    sync_before_head();
}

template<typename T>
void SList<T>::pop_front() noexcept
{
    if (!m_head)
    {
        return;
    }

    Node* node = m_head;
    m_head = m_head->next();
    m_size--;

    if (!m_head)
    {
        m_tail = nullptr;
    }

    delete node;
    sync_before_head();
}

template<typename T>
void SList<T>::clear() noexcept
{
    while (m_head)
    {
        Node* node = m_head;
        m_head = m_head->next();
        delete node;
    }

    m_head = nullptr;
    m_tail = nullptr;
    m_size = 0;
    sync_before_head();
}

template<typename T>
void SList<T>::swap(SList& other) noexcept
{
    using std::swap;

    swap(m_head, other.m_head);
    swap(m_tail, other.m_tail);
    swap(m_size, other.m_size);

    sync_before_head();
    other.sync_before_head();
}

template<typename T>
bool SList<T>::contains(const T& value) const
{
    const Node* node = m_head;

    while (node)
    {
        if (node->m_value == value)
        {
            return true;
        }

        node = node->next();
    }

    return false;
}

#endif // SLIST_HPP
