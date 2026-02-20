#ifndef SLIST_HPP
#define SLIST_HPP

#include <cstddef>
#include <utility>
#include <initializer_list>

template<typename T>
class SList
{
    struct Node
    {
    public:
        explicit Node(T val) : m_value(std::move(val)) {}

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

    explicit SList(T val) :
        m_head(new Node(std::move(val))),
        m_tail(m_head),
        m_size(1)
    {
    }

    SList(std::initializer_list<T> values)
    {
        for (const T& value : values)
        {
            push_back(value);
        }
    }

    SList(const SList& other)
    {
        const Node* temp = other.m_head;

        while (temp)
        {
            push_back(temp->m_value);
            temp = temp->m_next;
        }
    }

    SList& operator=(const SList& other)
    {
        SList{other}.swap(*this);
        return *this;
    }

    ~SList() noexcept
    {
        clear();
    }

    void push_back(const T& value)
    {
        if (!m_head)
        {
            m_head = new Node(value);
            m_tail = m_head;
        }
        else
        {
            Node* node = new Node(value);
            m_tail->m_next = node;
            m_tail = node;
        }

        m_size++;
    }

    void clear() noexcept
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

    void swap(SList& other) noexcept
    {
        using std::swap;
        swap(m_head, other.m_head);
        swap(m_tail, other.m_tail);
        swap(m_size, other.m_size);
    }

    std::size_t size() const noexcept { return m_size; }
    bool empty() const noexcept { return (m_size == 0); }

private:
    Node* m_head{ nullptr };
    Node* m_tail{ nullptr };
    std::size_t m_size{ 0 };
};

#endif // SLIST_HPP
