#include <iostream>
#include <assert.h>
using namespace std;
namespace Mylist
{
    template <class T>
    // 先定义结点
    struct __List_Node
    {
        __List_Node<T> *_prev;
        __List_Node<T> *_next;
        T _data;
        // 对这个结点进行初始化
        __List_Node(const T &x = T()) // 带参构造函数
            : _prev(nullptr), _next(nullptr), _data(x)
        {
        }
    };
    template <class T, class Ref, class Ptr>
    struct __List_iterator
    {
        typedef __List_Node<T> Node;
        typedef __List_iterator<T, Ref, Ptr> Self;
        Node *_node;
        __List_iterator(Node *node)
            : _node(node)
        {
        }
        Self &operator++() // 前置++  ++it
        {
            _node = _node->_next;
            return *this;
        }
        Self operator++(int) // 后置++ -> it++
        {
            Self tmep(*this);
            //_node = _node->_next;
            ++(*this);
            return tmep;
        }
        Ref &operator*()
        {
            assert(_node != nullptr);
            return _node->_data;
        }
        Ptr operator->()
        {
            // 返回的是一个指针
            return &_node->_data;
        }
        Self &operator--() // 前置--   --it
        {
            _node = _node->_prev;
            return *this;
        }
        bool operator!=(const Self &it)
        {
            return _node != it._node;
        }
    };
    template <class T>
    class List
    {
    public:
        typedef __List_Node<T> Node;
        typedef __List_iterator<T, T &, T *> iterator;
        typedef __List_iterator<T, const T &, const T *> const_iterator;
        List()
        {
            _phead = new Node;
            _phead->_prev = _phead;
            _phead->_next = _phead; // 初始化，前后指针都指向自己，证明此时链表为空！
        }
        void push_back(const T &x)
        {
            Insert(end(), x);
        }

        iterator begin()
        {
            return iterator(_phead->_next); // 哨兵结点也就是这个头结点的下一个就是开始的结点。
        }
        iterator end()
        {
            return iterator(_phead); // 最后一个就是自己。
        }
        const_iterator begin() const
        {
            return const_iterator(_phead->_next);
        }
        const_iterator end() const
        {
            return const_iterator(_phead);
        }
        void Print()
        {
            Node *cur = _phead->_next;
            while (cur != _phead)
            {
                cout << cur->_data << " ";
                cur = cur->_next;
            }
            cout << endl;
        }
        bool empty()
        {
            return _phead->_prev == _phead && _phead->_next == _phead;
        }
        ~List()
        {
            clear();
            delete _phead;
            _phead = nullptr;
        }
        T &front() // 返回头部数据
        {
            assert(!empty());
            return _phead->_next->_data;
        }

        const T &front() const
        {
            assert(!empty());
            return _phead->_next->_data;
        }
        void push_front(const T &x) // 头插
        {
            Insert(begin(), x);
        }
        void pop_front() // 头删
        {
            erase(iterator(_phead->_next));
        }
        void pop_back() // 尾删
        {
            erase(iterator(_phead->_prev));
            // erase(end()--
        }
        void Insert(iterator pos, const T &x) // 指定位置之前插入数据
        {
            Node *cur = pos._node;
            Node *prev = cur->_prev;
            Node *newNode = new Node(x);
            prev->_next = newNode;
            newNode->_next = cur;
            newNode->_prev = prev;
            cur->_prev = newNode;
        }
        void erase(iterator pos) // 删除指定位置的数据
        {
            assert(pos != end());
            Node *cur = pos._node;
            Node *prev = cur->_prev;
            prev->_next = cur->_next;
            cur->_next->_prev = prev;
            delete cur;
        }
        void clear() // 清除链表内除了头结点所有的元素
        {
            iterator it = begin();
            while (it != end())
            {
                erase(it++);
            }
        }
        int size() // 链表中有多少元素(除了头结点）
        {
            Node *cur = _phead->_next;
            int count = 0;
            while (cur != _phead)
            {
                count++;
                cur = cur->_next;
            }
            return count;
        }
        List<T> &operator=(const List<T> &LT)
        {
            if (this != &LT)
            {
                clear();
                for (const_iterator it = LT.begin(); it != LT.end(); ++it)
                {
                    push_back(*it);
                }
            }
            return *this;
        }

    private:
        Node *_phead;
    };
}
