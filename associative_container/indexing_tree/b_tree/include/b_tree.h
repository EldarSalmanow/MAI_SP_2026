#ifndef SYS_PROG_B_TREE_H
#define SYS_PROG_B_TREE_H

#include <algorithm>
#include <iterator>
#include <utility>
#include <boost/container/static_vector.hpp>
#include <stack>
#include <stdexcept>
#include <vector>
#include <pp_allocator.h>
#include <associative_container.h>
#include <not_implemented.h>
#include <initializer_list>

template <typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5>
class B_tree final : private compare // EBCO
{
public:

    using tree_data_type = std::pair<tkey, tvalue>;
    using tree_data_type_const = std::pair<const tkey, tvalue>;
    using value_type = tree_data_type_const;

private:

    static constexpr const size_t minimum_keys_in_node = t - 1;
    static constexpr const size_t maximum_keys_in_node = 2 * t - 1;

    // region comparators declaration

    inline bool compare_keys(const tkey& lhs, const tkey& rhs) const;
    inline bool compare_pairs(const tree_data_type& lhs, const tree_data_type& rhs) const;

    // endregion comparators declaration


    struct btree_node
    {
        boost::container::static_vector<tree_data_type, maximum_keys_in_node + 1> _keys;
        boost::container::static_vector<btree_node*, maximum_keys_in_node + 2> _pointers;
        btree_node() noexcept;
    };

    pp_allocator<value_type> _allocator;
    btree_node* _root;
    size_t _size;

    pp_allocator<value_type> get_allocator() const noexcept;

public:

    // region constructors declaration

    explicit B_tree(const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    explicit B_tree(pp_allocator<value_type> alloc, const compare& comp = compare());

    template<input_iterator_for_pair<tkey, tvalue> iterator>
    explicit B_tree(iterator begin, iterator end, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    B_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    // endregion constructors declaration

    // region five declaration

    B_tree(const B_tree& other);

    B_tree(B_tree&& other) noexcept;

    B_tree& operator=(const B_tree& other);

    B_tree& operator=(B_tree&& other) noexcept;

    ~B_tree() noexcept;

    // endregion five declaration

    // region iterators declaration

    class btree_iterator;
    class btree_reverse_iterator;
    class btree_const_iterator;
    class btree_const_reverse_iterator;

    class btree_iterator final
    {
        std::stack<std::pair<btree_node**, size_t>> _path;
        size_t _index;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_const_iterator;
        friend class btree_const_reverse_iterator;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_iterator(const std::stack<std::pair<btree_node**, size_t>>& path = std::stack<std::pair<btree_node**, size_t>>(), size_t index = 0);

    };

    class btree_const_iterator final
    {
        std::stack<std::pair<btree_node* const*, size_t>> _path;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_const_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_iterator;
        friend class btree_const_reverse_iterator;

        btree_const_iterator(const btree_iterator& it) noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_const_iterator(const std::stack<std::pair<btree_node* const*, size_t>>& path = std::stack<std::pair<btree_node* const*, size_t>>(), size_t index = 0);
    };

    class btree_reverse_iterator final
    {
        std::stack<std::pair<btree_node**, size_t>> _path;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_reverse_iterator;

        friend class B_tree;
        friend class btree_iterator;
        friend class btree_const_iterator;
        friend class btree_const_reverse_iterator;

        btree_reverse_iterator(const btree_iterator& it) noexcept;
        operator btree_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_reverse_iterator(const std::stack<std::pair<btree_node**, size_t>>& path = std::stack<std::pair<btree_node**, size_t>>(), size_t index = 0);
    };

    class btree_const_reverse_iterator final
    {
        std::stack<std::pair<btree_node* const*, size_t>> _path;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_const_reverse_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_const_iterator;
        friend class btree_iterator;

        btree_const_reverse_iterator(const btree_reverse_iterator& it) noexcept;
        operator btree_const_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_const_reverse_iterator(const std::stack<std::pair<btree_node* const*, size_t>>& path = std::stack<std::pair<btree_node* const*, size_t>>(), size_t index = 0);
    };

    friend class btree_iterator;
    friend class btree_const_iterator;
    friend class btree_reverse_iterator;
    friend class btree_const_reverse_iterator;

    // endregion iterators declaration

    // region element access declaration

    /*
     * Returns a reference to the mapped value of the element with specified key. If no such element exists, an exception of type std::out_of_range is thrown.
     */
    tvalue& at(const tkey&);
    const tvalue& at(const tkey&) const;

    /*
     * If key not exists, makes default initialization of value
     */
    tvalue& operator[](const tkey& key);
    tvalue& operator[](tkey&& key);

    // endregion element access declaration

    // region iterator begins declaration

    btree_iterator begin();
    btree_iterator end();

    btree_const_iterator begin() const;
    btree_const_iterator end() const;

    btree_const_iterator cbegin() const;
    btree_const_iterator cend() const;

    btree_reverse_iterator rbegin();
    btree_reverse_iterator rend();

    btree_const_reverse_iterator rbegin() const;
    btree_const_reverse_iterator rend() const;

    btree_const_reverse_iterator crbegin() const;
    btree_const_reverse_iterator crend() const;

    // endregion iterator begins declaration

    // region lookup declaration

    size_t size() const noexcept;
    bool empty() const noexcept;

    /*
     * Returns end() if not exist
     */

    btree_iterator find(const tkey& key);
    btree_const_iterator find(const tkey& key) const;

    btree_iterator lower_bound(const tkey& key);
    btree_const_iterator lower_bound(const tkey& key) const;

    btree_iterator upper_bound(const tkey& key);
    btree_const_iterator upper_bound(const tkey& key) const;

    bool contains(const tkey& key) const;

    // endregion lookup declaration

    // region modifiers declaration

    void clear() noexcept;

    /*
     * Does nothing if key exists, delegates to emplace.
     * Second return value is true, when inserted
     */
    std::pair<btree_iterator, bool> insert(const tree_data_type& data);
    std::pair<btree_iterator, bool> insert(tree_data_type&& data);

    template <typename ...Args>
    std::pair<btree_iterator, bool> emplace(Args&&... args);

    /*
     * Updates value if key exists, delegates to emplace.
     */
    btree_iterator insert_or_assign(const tree_data_type& data);
    btree_iterator insert_or_assign(tree_data_type&& data);

    template <typename ...Args>
    btree_iterator emplace_or_assign(Args&&... args);

    /*
     * Return iterator to node next ro removed or end() if key not exists
     */
    btree_iterator erase(btree_iterator pos);
    btree_iterator erase(btree_const_iterator pos);

    btree_iterator erase(btree_iterator beg, btree_iterator en);
    btree_iterator erase(btree_const_iterator beg, btree_const_iterator en);

    btree_iterator erase(const tkey& key);

    // endregion modifiers declaration
};

template<std::input_iterator iterator, comparator<typename std::iterator_traits<iterator>::value_type::first_type> compare = std::less<typename std::iterator_traits<iterator>::value_type::first_type>,
        std::size_t t = 5, typename U>
B_tree(iterator begin, iterator end, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> B_tree<typename std::iterator_traits<iterator>::value_type::first_type, typename std::iterator_traits<iterator>::value_type::second_type, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5, typename U>
B_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> B_tree<tkey, tvalue, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::compare_pairs(const B_tree::tree_data_type &lhs,
                                                     const B_tree::tree_data_type &rhs) const
{
    return compare_keys(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::compare_keys(const tkey &lhs, const tkey &rhs) const
{
    return compare::operator()(lhs, rhs);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_node::btree_node() noexcept
        : _keys(), 
          _pointers() {}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
pp_allocator<typename B_tree<tkey, tvalue, compare, t>::value_type> B_tree<tkey, tvalue, compare, t>::get_allocator() const noexcept
{
    return _allocator;
}

// region constructors implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        const compare& cmp,
        pp_allocator<value_type> alloc)
    : compare(cmp),
      _allocator(alloc),
      _root(nullptr),
      _size(0) {}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        pp_allocator<value_type> alloc,
        const compare& comp)
    : B_tree(comp, alloc) {}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<input_iterator_for_pair<tkey, tvalue> iterator>
B_tree<tkey, tvalue, compare, t>::B_tree(
        iterator begin,
        iterator end,
        const compare& cmp,
        pp_allocator<value_type> alloc)
        : B_tree(cmp, alloc)
{
    for (; begin != end; ++begin) {
        emplace(begin->first, begin->second);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        std::initializer_list<std::pair<tkey, tvalue>> data,
        const compare& cmp,
        pp_allocator<value_type> alloc)
    : B_tree(data.begin(), data.end(), cmp, alloc) {}

// endregion constructors implementation

// region five implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::~B_tree() noexcept
{
    clear();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(const B_tree& other)
                : compare(static_cast<const compare&>(other)),
                  _allocator(other._allocator),
                  _root(nullptr),
                  _size(0)
{
    for (const auto &[key, value] : other) {
        insert({key, value});
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>& B_tree<tkey, tvalue, compare, t>::operator=(const B_tree& other)
{
    if (this == &other) {
        return *this;
    }

    B_tree tmp(other);

    *this = std::move(tmp);

    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(B_tree&& other) noexcept
        : compare(static_cast<compare &&>(other)), 
          _allocator(), 
          _root(nullptr), 
          _size(0)
{
    std::swap(_allocator, other._allocator);
    std::swap(_root, other._root);
    std::swap(_size, other._size);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>& B_tree<tkey, tvalue, compare, t>::operator=(B_tree&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    clear();

    static_cast<compare &>(*this) = static_cast<compare &&>(other);
    std::swap(_allocator, other._allocator);
    std::swap(_root, other._root);
    std::swap(_size, other._size);
    
    return *this;
}

// endregion five implementation

// region iterators implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_iterator::btree_iterator(
        const std::stack<std::pair<btree_node**, size_t>>& path, size_t index)
    : _path(path),
      _index(index) {}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator*() const noexcept
{
    return reinterpret_cast<reference &>((*_path.top().first)->_keys[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator->() const noexcept
{
    return &(**this);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator&
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator++()
{
    if (_path.empty()) {
        return *this;
    }

    auto* node = *_path.top().first;

    if (!node->_pointers.empty()) {
        auto* child_ptr = &node->_pointers[_index + 1];
        
        _path.push({child_ptr, _index + 1});
        
        auto* current = *child_ptr;
        
        while (!current->_pointers.empty()) {
            child_ptr = &current->_pointers[0];
            
            _path.push({child_ptr, 0});
            
            current = *child_ptr;
        }
        
        _index = 0;
        
        return *this;
    }

    if (_index + 1 < node->_keys.size()) {
        ++_index;
        
        return *this;
    }

    while (_path.size() > 1) {
        const size_t child_index = _path.top().second;
        
        _path.pop();
        
        auto* parent = *_path.top().first;
        
        if (child_index < parent->_keys.size()) {
            _index = child_index;
            
            return *this;
        }
    }

    _path = std::stack<std::pair<btree_node**, size_t>>();
    _index = 0;

    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator++(int)
{
    self copy(*this);

    ++(*this);
    
    return copy;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator&
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator--()
{
    if (_path.empty()) {
        return *this;
    }

    auto* node = *_path.top().first;

    if (!node->_pointers.empty()) {
        auto* child_ptr = &node->_pointers[_index];
        
        _path.push({child_ptr, _index});
        
        auto* current = *child_ptr;
        
        while (!current->_pointers.empty()) {
            const size_t right = current->_keys.size();
            
            child_ptr = &current->_pointers[right];
            
            _path.push({child_ptr, right});
            
            current = *child_ptr;
        }
        _index = current->_keys.size() - 1;
        
        return *this;
    }

    if (_index > 0) {
        --_index;
        
        return *this;
    }

    while (_path.size() > 1) {
        const size_t child_index = _path.top().second;
        
        _path.pop();
        
        if (child_index > 0) {
            _index = child_index - 1;
            
            return *this;
        }
    }

    _path = std::stack<std::pair<btree_node**, size_t>>();
    _index = 0;
    
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator--(int)
{
    self copy(*this);
    
    --(*this);
    
    return copy;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::operator==(const self& other) const noexcept
{
    if (_index != other._index || _path.size() != other._path.size()) {
        return false;
    }

    if (_path.empty()) {
        return true;
    }

    return _path.top().first == other._path.top().first && _path.top().second == other._path.top().second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::current_node_keys_count() const noexcept
{
    return _path.empty() ? 0 : (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::is_terminate_node() const noexcept
{
    return _path.empty() || (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::btree_const_iterator(
        const std::stack<std::pair<btree_node* const*, size_t>>& path, size_t index)
    : _path(path),
      _index(index) {}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::btree_const_iterator(
        const btree_iterator& it) noexcept
        : _path(),
          _index(it._index)
{
    std::vector<std::pair<btree_node**, size_t>> path;
    
    for (auto temp = it._path; !temp.empty(); temp.pop()) {
        path.push_back(temp.top());
    }

    for (auto reverse_iterator = path.rbegin(); reverse_iterator != path.rend(); ++reverse_iterator) {
        _path.push({reverse_iterator->first, reverse_iterator->second});
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator*() const noexcept
{
    return reinterpret_cast<const reference&>((*_path.top().first)->_keys[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator->() const noexcept
{
    return &(**this);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator++()
{
    btree_iterator iterator;
    
    std::vector<std::pair<btree_node* const*, size_t>> path_as_vector;
    
    for (auto temp = _path; !temp.empty(); temp.pop()) {
        path_as_vector.push_back(temp.top());
    }
    
    for (auto reverse_iterator = path_as_vector.rbegin(); reverse_iterator != path_as_vector.rend(); ++reverse_iterator) {
        iterator._path.push({const_cast<btree_node**>(reverse_iterator->first), reverse_iterator->second});
    }
    
    iterator._index = _index;
    
    ++iterator;
    
    *this = btree_const_iterator(iterator);
    
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator++(int)
{
    self copy(*this);

    ++(*this);
    
    return copy;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator--()
{
    btree_iterator iterator;
    
    std::vector<std::pair<btree_node* const*, size_t>> path_as_vector;
    
    for (auto temp = _path; !temp.empty(); temp.pop()) {
        path_as_vector.push_back(temp.top());
    }

    for (auto reverse_iterator = path_as_vector.rbegin(); reverse_iterator != path_as_vector.rend(); ++reverse_iterator) {
        iterator._path.push({const_cast<btree_node**>(reverse_iterator->first), reverse_iterator->second});
    }
    
    iterator._index = _index;
    
    --iterator;

    *this = btree_const_iterator(iterator);
    
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator--(int)
{
    self copy(*this);

    --(*this);
    
    return copy;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator==(const self& other) const noexcept
{
    if (_index != other._index || _path.size() != other._path.size()) {
        return false;
    }

    if (_path.empty()) {
        return true;
    }

    return _path.top().first == other._path.top().first && _path.top().second == other._path.top().second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::current_node_keys_count() const noexcept
{
    return _path.empty() ? 0 : (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::is_terminate_node() const noexcept
{
    return _path.empty() || (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::btree_reverse_iterator(
        const std::stack<std::pair<btree_node**, size_t>>& path, size_t index)
    : _path(path),
      _index(index) {}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::btree_reverse_iterator(
        const btree_iterator& it) noexcept
    : _path(it._path),
      _index(it._index) {}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator B_tree<tkey, tvalue, compare, t>::btree_iterator() const noexcept
{
    return btree_iterator(_path, _index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator*() const noexcept
{
    return *static_cast<btree_iterator>(*this);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator->() const noexcept
{
    return &(**this);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator++()
{
    btree_iterator temp(_path, _index);
    
    --temp;

    _path = temp._path;
    _index = temp._index;
    
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator++(int)
{
    self copy(*this);
    
    ++(*this);
    
    return copy;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator--()
{
    btree_iterator temp(_path, _index);
    
    ++temp;

    _path = temp._path;
    _index = temp._index;

    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator--(int)
{
    self copy(*this);

    --(*this);
    
    return copy;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator==(const self& other) const noexcept
{
    if (_index != other._index || _path.size() != other._path.size()) {
        return false;
    }

    if (_path.empty()) {
        return true;
    }

    return _path.top().first == other._path.top().first && _path.top().second == other._path.top().second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::current_node_keys_count() const noexcept
{
    return _path.empty() ? 0 : (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::is_terminate_node() const noexcept
{
    return _path.empty() || (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::btree_const_reverse_iterator(
        const std::stack<std::pair<btree_node* const*, size_t>>& path, size_t index)
    : _path(path),
      _index(index) {}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::btree_const_reverse_iterator(
        const btree_reverse_iterator& it) noexcept
        : _path(),
          _index(it._index)
{
    std::vector<std::pair<btree_node**, size_t>> path;
    
    for (auto temp = it._path; !temp.empty(); temp.pop()) {
        path.push_back(temp.top());
    }

    for (auto reverse_iterator = path.rbegin(); reverse_iterator != path.rend(); ++reverse_iterator) {
        _path.push({reverse_iterator->first, reverse_iterator->second});
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator B_tree<tkey, tvalue, compare, t>::btree_const_iterator() const noexcept
{
    return btree_const_iterator(_path, _index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator*() const noexcept
{
    return *static_cast<btree_const_iterator>(*this);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator->() const noexcept
{
    return &(**this);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator++()
{
    btree_const_iterator temp(_path, _index);
    
    --temp;

    _path = temp._path;
    _index = temp._index;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator++(int)
{
    self copy(*this);

    ++(*this);
    
    return copy;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator--()
{
    btree_const_iterator temp(_path, _index);
    
    ++temp;

    _path = temp._path;
    _index = temp._index;
    
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator--(int)
{
    self copy(*this);
    
    --(*this);
    
    return copy;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator==(const self& other) const noexcept
{
    if (_index != other._index || _path.size() != other._path.size()) {
        return false;
    }

    if (_path.empty()) {
        return true;
    }

    return _path.top().first == other._path.top().first && _path.top().second == other._path.top().second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::current_node_keys_count() const noexcept
{
    return _path.empty() ? 0 : (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::is_terminate_node() const noexcept
{
    return _path.empty() || (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::index() const noexcept
{
    return _index;
}

// endregion iterators implementation

// region element access implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::at(const tkey& key)
{
    auto iterator = find(key);

    if (iterator == end()) {
        throw std::out_of_range("key not found in B_tree::at");
    }

    return iterator->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
const tvalue& B_tree<tkey, tvalue, compare, t>::at(const tkey& key) const
{
    auto iterator = find(key);

    if (iterator == end()) {
        throw std::out_of_range("key not found in B_tree::at const");
    }

    return iterator->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::operator[](const tkey& key)
{
    return emplace(key, tvalue{}).first->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::operator[](tkey&& key)
{
    return emplace(std::move(key), tvalue{}).first->second;
}

// endregion element access implementation

// region iterator begins implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::begin()
{
    if (_root == nullptr) {
        return end();
    }

    std::stack<std::pair<btree_node**, size_t>> path;
    
    btree_node** pointer = &_root;
    path.push({pointer, 0});
    
    while (!(*pointer)->_pointers.empty()) {
        pointer = &(*pointer)->_pointers[0];
        
        path.push({pointer, 0});
    }

    return btree_iterator(path, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::end()
{
    return btree_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::begin() const
{
    if (_root == nullptr) {
        return end();
    }

    std::stack<std::pair<btree_node* const*, size_t>> path;

    btree_node* const* pointer = &_root;
    path.push({pointer, 0});
    
    while (!(*pointer)->_pointers.empty()) {
        pointer = &(*pointer)->_pointers[0];
        
        path.push({pointer, 0});
    }

    return btree_const_iterator(path, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::end() const
{
    return btree_const_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::cbegin() const
{
    return begin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::cend() const
{
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator B_tree<tkey, tvalue, compare, t>::rbegin()
{
    if (_root == nullptr) {
        return rend();
    }

    std::stack<std::pair<btree_node**, size_t>> path;

    btree_node** pointer = &_root;
    path.push({pointer, 0});
    
    while (!(*pointer)->_pointers.empty()) {
        const size_t index = (*pointer)->_keys.size();
        
        pointer = &(*pointer)->_pointers[index];
        
        path.push({pointer, index});
    }

    return btree_reverse_iterator(path, (*pointer)->_keys.size() - 1);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator B_tree<tkey, tvalue, compare, t>::rend()
{
    return btree_reverse_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::rbegin() const
{
    if (_root == nullptr) {
        return rend();
    }

    std::stack<std::pair<btree_node* const*, size_t>> path;
    
    btree_node* const* pointer = &_root;
    path.push({pointer, 0});
    
    while (!(*pointer)->_pointers.empty()) {
        const size_t index = (*pointer)->_keys.size();

        pointer = &(*pointer)->_pointers[index];
        
        path.push({pointer, index});
    }

    return btree_const_reverse_iterator(path, (*pointer)->_keys.size() - 1);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::rend() const
{
    return btree_const_reverse_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::crbegin() const
{
    return rbegin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::crend() const
{
    return rend();
}

// endregion iterator begins implementation

// region lookup implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::size() const noexcept
{
    return _size;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::empty() const noexcept
{
    return size() == 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::find(const tkey& key)
{
    auto iterator = lower_bound(key);

    if (iterator == end()) {
        return iterator;
    }

    const compare& cmp = static_cast<const compare&>(*this);

    return (!cmp(key, iterator->first) && !cmp(iterator->first, key)) ? iterator : end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::find(const tkey& key) const
{
    return btree_const_iterator(const_cast<B_tree*>(this)->find(key));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }

    auto make_iterator = [](const std::vector<std::pair<btree_node**, size_t>>& path, size_t idx) {
        std::stack<std::pair<btree_node**, size_t>> stack_path;
        
        for (const auto& item : path) {
            stack_path.push(item);
        }

        return btree_iterator(stack_path, idx);
    };

    std::vector<std::pair<btree_node**, size_t>> path;
    std::vector<std::pair<btree_node**, size_t>> best_path;
    size_t best_index = 0;

    btree_node** ptr = &_root;
    path.push_back({ptr, 0});

    while (*ptr != nullptr) {
        btree_node* node = *ptr;
        const size_t index = static_cast<size_t>(std::lower_bound(node->_keys.begin(), node->_keys.end(), key,
            [this](const tree_data_type& item, const tkey& k) {
                return compare_keys(item.first, k);
            }) - node->_keys.begin());

        if (index < node->_keys.size()) {
            best_path = path;
            best_index = index;
        }

        if (node->_pointers.empty()) {
            if (index < node->_keys.size()) {
                return make_iterator(path, index);
            }

            return best_path.empty() ? end() : make_iterator(best_path, best_index);
        }

        ptr = &node->_pointers[index];
        path.push_back({ptr, index});
    }

    return best_path.empty() ? end() : make_iterator(best_path, best_index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key) const
{
    return btree_const_iterator(const_cast<B_tree*>(this)->lower_bound(key));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key)
{
    return lower_bound(key);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key) const
{
    return btree_const_iterator(const_cast<B_tree*>(this)->upper_bound(key));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::contains(const tkey& key) const
{
    return find(key) != end();
}

// endregion lookup implementation

// region modifiers implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::clear() noexcept
{
    if (_root == nullptr) {
        _size = 0;

        return;
    }

    std::vector<btree_node*> stack;
    stack.push_back(_root);
    
    while (!stack.empty()) {
        btree_node *node = stack.back();
        
        stack.pop_back();

        for (btree_node *child : node->_pointers) {
            if (child != nullptr) {
                stack.push_back(child);
            }
        }

        _allocator.delete_object(node);
    }

    _root = nullptr;
    _size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::insert(const tree_data_type& data)
{
    return emplace(data.first, data.second);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::insert(tree_data_type&& data)
{
    return emplace(std::move(data.first), std::move(data.second));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::emplace(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);
    tkey key = data.first;

    auto existed = find(data.first);

    if (existed != end()) {
        return {existed, false};
    }

    if (_root == nullptr) {
        _root = _allocator.new_object<btree_node>();
        _root->_keys.push_back(std::move(data));
        ++_size;
        
        std::stack<std::pair<btree_node**, size_t>> path;
        path.push({&_root, 0});
        
        return {btree_iterator(path, 0), true};
    }

    std::vector<std::pair<btree_node*, size_t>> path;
    btree_node* current = _root;

    while (!current->_pointers.empty()) {
        const size_t index = static_cast<size_t>(std::lower_bound(current->_keys.begin(), current->_keys.end(), data.first,
            [this](const tree_data_type& item, const tkey& key) {
                return compare_keys(item.first, key);
            }) - current->_keys.begin());

        path.push_back({current, index});
        
        current = current->_pointers[index];
    }

    const size_t insert_index = static_cast<size_t>(std::lower_bound(current->_keys.begin(), current->_keys.end(), data.first,
        [this](const tree_data_type& item, const tkey& key) {
            return compare_keys(item.first, key);
        }) - current->_keys.begin());
    
    current->_keys.insert(current->_keys.begin() + static_cast<ptrdiff_t>(insert_index), std::move(data));
    ++_size;

    while (current->_keys.size() > maximum_keys_in_node) {
        btree_node* right = _allocator.new_object<btree_node>();
        const tree_data_type median = current->_keys[t];

        for (size_t i = t + 1; i < current->_keys.size(); ++i) {
            right->_keys.push_back(std::move(current->_keys[i]));
        }

        if (!current->_pointers.empty()) {
            for (size_t i = t + 1; i < current->_pointers.size(); ++i) {
                right->_pointers.push_back(current->_pointers[i]);
            }

            current->_pointers.resize(t + 1);
        }

        current->_keys.resize(t);

        if (path.empty()) {
            btree_node* new_root = _allocator.new_object<btree_node>();
            new_root->_keys.push_back(median);
            new_root->_pointers.push_back(current);
            new_root->_pointers.push_back(right);

            _root = new_root;
            
            break;
        }

        auto [parent, parent_idx] = path.back();
        
        path.pop_back();
        
        parent->_keys.insert(parent->_keys.begin() + static_cast<ptrdiff_t>(parent_idx), median);
        parent->_pointers.insert(parent->_pointers.begin() + static_cast<ptrdiff_t>(parent_idx + 1), right);
        
        current = parent;
    }

    return {find(key), true};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::insert_or_assign(const tree_data_type& data)
{
    auto [iterator, inserted] = emplace(data.first, data.second);
    
    if (!inserted) {
        iterator->second = data.second;
    }
    
    return iterator;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::insert_or_assign(tree_data_type&& data)
{
    auto [iterator, inserted] = emplace(std::move(data.first), std::move(data.second));
    
    if (!inserted) {
        iterator->second = std::move(data.second);
    }
    
    return iterator;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::emplace_or_assign(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);

    return insert_or_assign(std::move(data));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_iterator pos)
{
    if (pos == end()) {
        return end();
    }

    return erase(pos->first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_const_iterator pos)
{
    if (pos == end()) {
        return end();
    }

    return erase(pos->first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_iterator beg, btree_iterator en)
{
    if (beg == en) {
        return en;
    }

    std::vector<tkey> keys;
    for (auto iterator = beg; iterator != en; ++iterator) {
        keys.push_back(iterator->first);
    }

    btree_iterator result = end();
    for (const auto &key : keys) {
        result = erase(key);
    }

    return result;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_const_iterator beg, btree_const_iterator en)
{
    if (beg == en) {
        return end();
    }

    std::vector<tkey> keys;
    for (auto iterator = beg; iterator != en; ++iterator) {
        keys.push_back(iterator->first);
    }

    btree_iterator result = end();
    for (const auto &key : keys) {
        result = erase(key);
    }

    return result;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }

    struct erase_helper {
        B_tree* tree;

        void borrow_from_prev(btree_node* parent, size_t child_idx) {
            btree_node* child = parent->_pointers[child_idx];
            btree_node* sibling = parent->_pointers[child_idx - 1];

            child->_keys.insert(child->_keys.begin(), std::move(parent->_keys[child_idx - 1]));
            parent->_keys[child_idx - 1] = std::move(sibling->_keys.back());
            sibling->_keys.pop_back();

            if (!sibling->_pointers.empty()) {
                child->_pointers.insert(child->_pointers.begin(), sibling->_pointers.back());
                sibling->_pointers.pop_back();
            }
        }

        void borrow_from_next(btree_node* parent, size_t child_idx) {
            btree_node* child = parent->_pointers[child_idx];
            btree_node* sibling = parent->_pointers[child_idx + 1];

            child->_keys.push_back(std::move(parent->_keys[child_idx]));
            parent->_keys[child_idx] = std::move(sibling->_keys.front());
            sibling->_keys.erase(sibling->_keys.begin());

            if (!sibling->_pointers.empty()) {
                child->_pointers.push_back(sibling->_pointers.front());
                sibling->_pointers.erase(sibling->_pointers.begin());
            }
        }

        void merge_with_next(btree_node* parent, size_t child_idx) {
            btree_node* child = parent->_pointers[child_idx];
            btree_node* sibling = parent->_pointers[child_idx + 1];

            child->_keys.push_back(std::move(parent->_keys[child_idx]));

            for (auto& key_item : sibling->_keys) {
                child->_keys.push_back(std::move(key_item));
            }

            for (auto* ptr : sibling->_pointers) {
                child->_pointers.push_back(ptr);
            }

            parent->_keys.erase(parent->_keys.begin() + static_cast<ptrdiff_t>(child_idx));
            parent->_pointers.erase(parent->_pointers.begin() + static_cast<ptrdiff_t>(child_idx + 1));

            tree->_allocator.delete_object(sibling);
        }

        void fill_child(btree_node* parent, size_t child_idx) {
            if (child_idx > 0 && parent->_pointers[child_idx - 1]->_keys.size() > minimum_keys_in_node) {
                borrow_from_prev(parent, child_idx);
            } else if (child_idx < parent->_pointers.size() - 1 && parent->_pointers[child_idx + 1]->_keys.size() > minimum_keys_in_node) {
                borrow_from_next(parent, child_idx);
            } else {
                if (child_idx < parent->_pointers.size() - 1) {
                    merge_with_next(parent, child_idx);
                } else {
                    merge_with_next(parent, child_idx - 1);
                }
            }
        }

        bool erase_internal(btree_node* node, const tkey& k) {
            auto it = std::lower_bound(node->_keys.begin(), node->_keys.end(), k,
                [this](const tree_data_type& item, const tkey& key_val) {
                    return tree->compare_keys(item.first, key_val);
                });

            size_t idx = static_cast<size_t>(it - node->_keys.begin());

            if (idx < node->_keys.size() && !tree->compare_keys(k, node->_keys[idx].first) && !tree->compare_keys(node->_keys[idx].first, k)) {
                if (node->_pointers.empty()) {
                    node->_keys.erase(node->_keys.begin() + static_cast<ptrdiff_t>(idx));
                    return true;
                }

                if (node->_pointers[idx]->_keys.size() > minimum_keys_in_node) {
                    btree_node* pred = node->_pointers[idx];
                    while (!pred->_pointers.empty()) {
                        pred = pred->_pointers.back();
                    }
                    node->_keys[idx] = std::move(pred->_keys.back());
                    return erase_internal(node->_pointers[idx], node->_keys[idx].first);
                } else if (node->_pointers[idx + 1]->_keys.size() > minimum_keys_in_node) {
                    btree_node* succ = node->_pointers[idx + 1];
                    while (!succ->_pointers.empty()) {
                        succ = succ->_pointers.front();
                    }
                    node->_keys[idx] = std::move(succ->_keys.front());
                    return erase_internal(node->_pointers[idx + 1], node->_keys[idx].first);
                } else {
                    merge_with_next(node, idx);
                    return erase_internal(node->_pointers[idx], k);
                }
            }

            if (node->_pointers.empty()) {
                return false;
            }

            bool is_in_subtree = (idx < node->_keys.size()) ? tree->compare_keys(k, node->_keys[idx].first) || !tree->compare_keys(node->_keys[idx].first, k) : true;

            if (!is_in_subtree) {
                return false;
            }

            if (node->_pointers[idx]->_keys.size() <= minimum_keys_in_node) {
                fill_child(node, idx);

                if (idx > 0 && idx >= node->_keys.size()) {
                    --idx;
                }

                if (idx < node->_keys.size() && !tree->compare_keys(k, node->_keys[idx].first) && !tree->compare_keys(node->_keys[idx].first, k)) {
                    return erase_internal(node, k);
                }
            }

            return erase_internal(node->_pointers[idx], k);
        }
    };

    erase_helper helper{this};
    bool erased = helper.erase_internal(_root, key);

    if (!erased) {
        return end();
    }

    --_size;

    if (_root->_keys.empty()) {
        btree_node* old_root = _root;
        _root = _root->_pointers.empty() ? nullptr : _root->_pointers[0];
        _allocator.delete_object(old_root);
    }

    return lower_bound(key);
}

// endregion modifiers implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool compare_pairs(const typename B_tree<tkey, tvalue, compare, t>::tree_data_type &lhs,
                   const typename B_tree<tkey, tvalue, compare, t>::tree_data_type &rhs)
{
    return compare{}(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool compare_keys(const tkey &lhs, const tkey &rhs)
{
    return compare{}(lhs, rhs);
}

#endif