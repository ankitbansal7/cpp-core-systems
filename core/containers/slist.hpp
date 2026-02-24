#ifndef SLIST_HPP
#define SLIST_HPP

#include <cstddef>
#include <utility>
#include <initializer_list>
#include <ostream>

#include "utils/traits.hpp"

template<typename T>
class SList
{
    struct Node
    {
    public:
        template<typename... Args>
        explicit Node(Args&&... args) : m_value(std::forward<Args>(args)...) {}

        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = delete;
        Node& operator=(Node&&) = delete;

        ~Node() = default;

    public:
        T m_value;
        Node* m_next{ nullptr };
    };

public:
    SList() = default;

    explicit SList(T val);
    SList(std::initializer_list<T> values);
    SList(const SList& other);

    SList& operator=(const SList& other);

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

            node = node->m_next;
        }

        return stream;
    }

    ~SList() noexcept;

    template<typename... Args>
    void emplace_back(Args&&... args);

    template<typename... Args>
    void emplace_front(Args&&... args);

    void push_back(const T& value);
    void push_back(T&& value);
    void push_front(const T& value);
    void push_front(T&& value);
    void pop_back() noexcept;
    void pop_front() noexcept;
    void clear() noexcept;
    void swap(SList& other) noexcept;

    inline std::size_t size() const noexcept { return m_size; }
    inline bool empty() const noexcept { return (m_size == 0); }
    inline T& front() noexcept { return m_head->m_value; }
    inline const T& front() const noexcept { return m_head->m_value; }
    inline T& back() noexcept { return m_tail->m_value; }
    inline const T& back() const noexcept { return m_tail->m_value; }

private:
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
}

template<typename T>
SList<T>::SList(std::initializer_list<T> values)
{
    for (const T& value : values)
    {
        push_back(value);
    }
}

template<typename T>
SList<T>::SList(const SList& other)
{
    const Node* temp = other.m_head;

    while (temp)
    {
        push_back(temp->m_value);
        temp = temp->m_next;
    }
}

template<typename T>
SList<T>& SList<T>::operator=(const SList& other)
{
    SList{ other }.swap(*this);
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
    }
    else
    {
        m_tail->m_next = node;
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
        node->m_next = m_head;
        m_head = node;
    }

    m_size++;
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
void SList<T>::pop_back() noexcept
{
    if (m_head)
    {
        if (!m_head->m_next)
        {
            delete m_head;
            m_size--;
            m_head = nullptr;
            m_tail = nullptr;
            return;
        }

        Node* prev = m_head;
        Node* curr = m_head->m_next;

        while (curr && curr->m_next)
        {
            curr = curr->m_next;
            prev = prev->m_next;
        }

        delete curr;
        prev->m_next = nullptr;
        m_tail = prev;
        m_size--;
    }
}

template<typename T>
void SList<T>::pop_front() noexcept
{
    Node* node = m_head;

    if (m_head)
    {
        m_head = m_head->m_next;
        m_size--;

        if (!m_head)
        {
            m_tail = nullptr;
        }
    }

    delete node;
}

template<typename T>
void SList<T>::clear() noexcept
{
    while (m_head)
    {
        Node* node = m_head;
        m_head = m_head->m_next;
        delete node;
    }

    m_head = nullptr;
    m_tail = nullptr;
    m_size = 0;
}

template<typename T>
void SList<T>::swap(SList& other) noexcept
{
    using std::swap;
    swap(m_head, other.m_head);
    swap(m_tail, other.m_tail);
    swap(m_size, other.m_size);
}

#endif // SLIST_HPP
