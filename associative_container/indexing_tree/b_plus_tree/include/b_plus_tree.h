#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>
#include <boost/container/static_vector.hpp>
#include <concepts>
#include <stack>
#include <pp_allocator.h>
#include <associative_container.h>
#include <not_implemented.h>
#include <initializer_list>

#ifndef SYS_PROG_B_PLUS_TREE_H
#define SYS_PROG_B_PLUS_TREE_H

template <typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5>
class BP_tree final : private compare //EBCO
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

    struct bptree_node_base
    {
        bool _is_terminate;

        bptree_node_base() noexcept;
        virtual ~bptree_node_base() =default;
    };

    struct bptree_node_term : public bptree_node_base
    {
        bptree_node_term* _next;

        boost::container::static_vector<tree_data_type, maximum_keys_in_node + 1> _data;
        bptree_node_term() noexcept;
    };

    struct bptree_node_middle : public bptree_node_base
    {
        boost::container::static_vector<tkey, maximum_keys_in_node + 1> _keys;
        boost::container::static_vector<bptree_node_base*, maximum_keys_in_node + 2> _pointers;
        bptree_node_middle() noexcept;
    };

    pp_allocator<value_type> _allocator;
    bptree_node_base* _root;
    size_t _size;

    pp_allocator<value_type> get_allocator() const noexcept;
    tkey min_key(const bptree_node_base* node) const;
    void refresh_keys(bptree_node_middle* node) const;

public:

    // region constructors declaration

    explicit BP_tree(const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    explicit BP_tree(pp_allocator<value_type> alloc, const compare& comp = compare());

    template<input_iterator_for_pair<tkey, tvalue> iterator>
    explicit BP_tree(iterator begin, iterator end, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    BP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    // endregion constructors declaration

    // region five declaration

    BP_tree(const BP_tree& other);

    BP_tree(BP_tree&& other) noexcept;

    BP_tree& operator=(const BP_tree& other);

    BP_tree& operator=(BP_tree&& other) noexcept;

    ~BP_tree() noexcept;

    // endregion five declaration

    // region iterators declaration

    class bptree_iterator;
    class bptree_const_iterator;

    class bptree_iterator final
    {
        bptree_node_term* _node;
        size_t _index;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bptree_iterator;

        friend class BP_tree;
        friend class bptree_const_iterator;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t current_node_keys_count() const noexcept;
        size_t index() const noexcept;

        explicit bptree_iterator(bptree_node_term* node = nullptr, size_t index = 0);

    };

    class bptree_const_iterator final
    {
        const bptree_node_term* _node;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bptree_const_iterator;

        friend class BP_tree;
        friend class bptree_iterator;

        bptree_const_iterator(const bptree_iterator& it) noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t current_node_keys_count() const noexcept;
        size_t index() const noexcept;

        explicit bptree_const_iterator(const bptree_node_term* node = nullptr, size_t index = 0);
    };

    friend class btree_iterator;
    friend class btree_const_iterator;

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

    bptree_iterator begin();
    bptree_iterator end();

    bptree_const_iterator begin() const;
    bptree_const_iterator end() const;

    bptree_const_iterator cbegin() const;
    bptree_const_iterator cend() const;

    // endregion iterator begins declaration

    // region lookup declaration

    size_t size() const noexcept;
    bool empty() const noexcept;

    /*
     * Returns end() if not exist
     */

    bptree_iterator find(const tkey& key);
    bptree_const_iterator find(const tkey& key) const;

    bptree_iterator lower_bound(const tkey& key);
    bptree_const_iterator lower_bound(const tkey& key) const;

    bptree_iterator upper_bound(const tkey& key);
    bptree_const_iterator upper_bound(const tkey& key) const;

    bool contains(const tkey& key) const;

    // endregion lookup declaration

    // region modifiers declaration

    void clear() noexcept;

    /*
     * Does nothing if key exists, delegates to emplace.
     * Second return value is true, when inserted
     */
    std::pair<bptree_iterator, bool> insert(const tree_data_type& data);
    std::pair<bptree_iterator, bool> insert(tree_data_type&& data);

    template <typename ...Args>
    std::pair<bptree_iterator, bool> emplace(Args&&... args);

    /*
     * Updates value if key exists, delegates to emplace.
     */
    bptree_iterator insert_or_assign(const tree_data_type& data);
    bptree_iterator insert_or_assign(tree_data_type&& data);

    template <typename ...Args>
    bptree_iterator emplace_or_assign(Args&&... args);

    /*
     * Return iterator to node next ro removed or end() if key not exists
     */
    bptree_iterator erase(bptree_iterator pos);
    bptree_iterator erase(bptree_const_iterator pos);

    bptree_iterator erase(bptree_iterator beg, bptree_iterator en);
    bptree_iterator erase(bptree_const_iterator beg, bptree_const_iterator en);


    bptree_iterator erase(const tkey& key);

    // endregion modifiers declaration
};

template<std::input_iterator iterator, comparator<typename std::iterator_traits<iterator>::value_type::first_type> compare = std::less<typename std::iterator_traits<iterator>::value_type::first_type>,
        std::size_t t = 5, typename U>
BP_tree(iterator begin, iterator end, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> BP_tree<typename std::iterator_traits<iterator>::value_type::first_type, typename std::iterator_traits<iterator>::value_type::second_type, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5, typename U>
BP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> BP_tree<tkey, tvalue, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::compare_pairs(const BP_tree::tree_data_type &lhs,
                                                     const BP_tree::tree_data_type &rhs) const
{
    return compare_keys(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_node_base::bptree_node_base() noexcept
{
    _is_terminate = false;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_node_term::bptree_node_term() noexcept
{
    this->_is_terminate = true;
    _next = nullptr;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_node_middle::bptree_node_middle() noexcept
{
    this->_is_terminate = false;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
pp_allocator<typename BP_tree<tkey, tvalue, compare, t>::value_type> BP_tree<tkey, tvalue, compare, t>::
get_allocator() const noexcept
{
    return _allocator;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tkey BP_tree<tkey, tvalue, compare, t>::min_key(const bptree_node_base* node) const
{
    const bptree_node_base* current = node;
    while (!current->_is_terminate) {
        auto* middle = static_cast<const bptree_node_middle*>(current);
        current = middle->_pointers.front();
    }

    auto* leaf = static_cast<const bptree_node_term*>(current);
    return leaf->_data.front().first;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::refresh_keys(bptree_node_middle* node) const
{
    node->_keys.clear();

    for (size_t i = 1; i < node->_pointers.size(); ++i) {
        node->_keys.push_back(min_key(node->_pointers[i]));
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::reference BP_tree<tkey, tvalue, compare, t>::
bptree_iterator::operator*() const noexcept
{
    return reinterpret_cast<reference&>(_node->_data[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::pointer BP_tree<tkey, tvalue, compare, t>::bptree_iterator
::operator->() const noexcept
{
    return &(**this);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::self & BP_tree<tkey, tvalue, compare, t>::bptree_iterator::
operator++()
{
    if (_node == nullptr) {
        return *this;
    }

    if (_index + 1 < _node->_data.size()) {
        ++_index;
        return *this;
    }

    _node = _node->_next;
    _index = 0;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::self BP_tree<tkey, tvalue, compare, t>::bptree_iterator::
operator++(int)
{
    self copy(*this);
    ++(*this);
    return copy;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_iterator::operator==(const self &other) const noexcept
{
    if (_node == nullptr && other._node == nullptr) {
        return true;
    }
    return _node == other._node && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_iterator::operator!=(const self &other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_iterator::current_node_keys_count() const noexcept
{
    return _node == nullptr ? 0 : _node->_data.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_iterator::bptree_iterator(bptree_node_term *node, size_t index)
{
    _node = node;
    _index = index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::bptree_const_iterator(const bptree_iterator &it) noexcept
{
    _node = it._node;
    _index = it._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::reference BP_tree<tkey, tvalue, compare, t>::
bptree_const_iterator::operator*() const noexcept
{
    return reinterpret_cast<const reference&>(_node->_data[_index]);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::pointer BP_tree<tkey, tvalue, compare, t>::
bptree_const_iterator::operator->() const noexcept
{
    return &(**this);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::self & BP_tree<tkey, tvalue, compare, t>::
bptree_const_iterator::operator++()
{
    if (_node == nullptr) {
        return *this;
    }

    if (_index + 1 < _node->_data.size()) {
        ++_index;
        return *this;
    }

    _node = _node->_next;
    _index = 0;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::self BP_tree<tkey, tvalue, compare, t>::
bptree_const_iterator::operator++(int)
{
    self copy(*this);
    ++(*this);
    return copy;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::operator==(const self &other) const noexcept
{
    if (_node == nullptr && other._node == nullptr) {
        return true;
    }
    return _node == other._node && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::operator!=(const self &other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::current_node_keys_count() const noexcept
{
    return _node == nullptr ? 0 : _node->_data.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::bptree_const_iterator(const bptree_node_term *node, size_t index)
{
    _node = node;
    _index = index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue & BP_tree<tkey, tvalue, compare, t>::at(const tkey &key)
{
    auto iterator = find(key);

    if (iterator == end()) {
        throw std::out_of_range("key not found in BP_tree::at");
    }

    return iterator->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
const tvalue & BP_tree<tkey, tvalue, compare, t>::at(const tkey &key) const
{
    auto iterator = find(key);

    if (iterator == end()) {
        throw std::out_of_range("key not found in BP_tree::at const");
    }

    return iterator->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue & BP_tree<tkey, tvalue, compare, t>::operator[](const tkey &key)
{
    return emplace(key, tvalue{}).first->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue & BP_tree<tkey, tvalue, compare, t>::operator[](tkey &&key)
{
    return emplace(std::move(key), tvalue{}).first->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator, bool> BP_tree<tkey, tvalue, compare, t>::insert(
    const tree_data_type &data)
{
    return emplace(data.first, data.second);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::compare_keys(const tkey &lhs, const tkey &rhs) const
{
    return compare::operator()(lhs, rhs);
}


template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(const compare& cmp, pp_allocator<value_type> alloc)
{
    static_cast<compare&>(*this) = cmp;
    _allocator = alloc;
    _root = nullptr;
    _size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(pp_allocator<value_type> alloc, const compare& cmp)
{
    *this = BP_tree(cmp, alloc);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<input_iterator_for_pair<tkey, tvalue> iterator>
BP_tree<tkey, tvalue, compare, t>::BP_tree(iterator begin, iterator end, const compare& cmp, pp_allocator<value_type> alloc)
{
    *this = BP_tree(cmp, alloc);
    for (; begin != end; ++begin) {
        emplace(begin->first, begin->second);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp, pp_allocator<value_type> alloc)
{
    *this = BP_tree(data.begin(), data.end(), cmp, alloc);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(const BP_tree& other)
{
    static_cast<compare&>(*this) = static_cast<const compare&>(other);
    _allocator = other._allocator;
    _root = nullptr;
    _size = 0;

    for (const auto& item : other) {
        insert({item.first, item.second});
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(BP_tree&& other) noexcept
{
    static_cast<compare&>(*this) = static_cast<compare&&>(other);
    _allocator = pp_allocator<value_type>();
    _root = nullptr;
    _size = 0;
    std::swap(_allocator, other._allocator);
    std::swap(_root, other._root);
    std::swap(_size, other._size);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>& BP_tree<tkey, tvalue, compare, t>::operator=(const BP_tree& other)
{
    if (this == &other) {
        return *this;
    }

    BP_tree tmp(other);
    *this = std::move(tmp);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>& BP_tree<tkey, tvalue, compare, t>::operator=(BP_tree&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    clear();
    static_cast<compare&>(*this) = static_cast<compare&&>(other);
    std::swap(_allocator, other._allocator);
    std::swap(_root, other._root);
    std::swap(_size, other._size);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::~BP_tree() noexcept
{
    clear();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::begin()
{
    if (_root == nullptr) {
        return end();
    }

    bptree_node_base* current = _root;
    while (!current->_is_terminate) {
        auto* middle = static_cast<bptree_node_middle*>(current);
        current = middle->_pointers[0];
    }

    auto* leaf = static_cast<bptree_node_term*>(current);
    if (leaf->_data.empty()) {
        return end();
    }

    return bptree_iterator(leaf, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::end()
{
    return bptree_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::begin() const
{
    if (_root == nullptr) {
        return end();
    }

    bptree_node_base* current = _root;
    while (!current->_is_terminate) {
        auto* middle = static_cast<bptree_node_middle*>(current);
        current = middle->_pointers[0];
    }

    auto* leaf = static_cast<bptree_node_term*>(current);
    if (leaf->_data.empty()) {
        return end();
    }

    return bptree_const_iterator(leaf, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::end() const
{
    return bptree_const_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::cbegin() const
{
    return begin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::cend() const
{
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::size() const noexcept
{
    return _size;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::empty() const noexcept
{
    return _size == 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::find(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }

    bptree_node_base* current = _root;
    while (!current->_is_terminate) {
        auto* middle = static_cast<bptree_node_middle*>(current);
        const size_t idx = static_cast<size_t>(std::upper_bound(middle->_keys.begin(), middle->_keys.end(), key,
            [this](const tkey& k, const tkey& sep) {
                return compare_keys(k, sep);
            }) - middle->_keys.begin());
        current = middle->_pointers[idx];
    }

    auto* leaf = static_cast<bptree_node_term*>(current);
    auto it = std::lower_bound(leaf->_data.begin(), leaf->_data.end(), key,
        [this](const tree_data_type& item, const tkey& k) {
            return compare_keys(item.first, k);
        });

    if (it == leaf->_data.end()) {
        return end();
    }

    if (compare_keys(key, it->first) || compare_keys(it->first, key)) {
        return end();
    }

    return bptree_iterator(leaf, static_cast<size_t>(it - leaf->_data.begin()));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::find(const tkey& key) const
{
    if (_root == nullptr) {
        return end();
    }

    bptree_node_base* current = _root;
    while (!current->_is_terminate) {
        auto* middle = static_cast<bptree_node_middle*>(current);
        const size_t idx = static_cast<size_t>(std::upper_bound(middle->_keys.begin(), middle->_keys.end(), key,
            [this](const tkey& k, const tkey& sep) {
                return compare_keys(k, sep);
            }) - middle->_keys.begin());
        current = middle->_pointers[idx];
    }

    auto* leaf = static_cast<bptree_node_term*>(current);
    auto it = std::lower_bound(leaf->_data.begin(), leaf->_data.end(), key,
        [this](const tree_data_type& item, const tkey& k) {
            return compare_keys(item.first, k);
        });

    if (it == leaf->_data.end()) {
        return end();
    }

    if (compare_keys(key, it->first) || compare_keys(it->first, key)) {
        return end();
    }

    return bptree_const_iterator(leaf, static_cast<size_t>(it - leaf->_data.begin()));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }

    bptree_node_base* current = _root;
    while (!current->_is_terminate) {
        auto* middle = static_cast<bptree_node_middle*>(current);
        const size_t idx = static_cast<size_t>(std::upper_bound(middle->_keys.begin(), middle->_keys.end(), key,
            [this](const tkey& k, const tkey& sep) {
                return compare_keys(k, sep);
            }) - middle->_keys.begin());
        current = middle->_pointers[idx];
    }

    auto* leaf = static_cast<bptree_node_term*>(current);
    auto it = std::lower_bound(leaf->_data.begin(), leaf->_data.end(), key,
        [this](const tree_data_type& item, const tkey& k) {
            return compare_keys(item.first, k);
        });

    if (it != leaf->_data.end()) {
        return bptree_iterator(leaf, static_cast<size_t>(it - leaf->_data.begin()));
    }

    if (leaf->_next != nullptr) {
        return bptree_iterator(leaf->_next, 0);
    }

    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key) const
{
    if (_root == nullptr) {
        return end();
    }

    bptree_node_base* current = _root;
    while (!current->_is_terminate) {
        auto* middle = static_cast<bptree_node_middle*>(current);
        const size_t idx = static_cast<size_t>(std::upper_bound(middle->_keys.begin(), middle->_keys.end(), key,
            [this](const tkey& k, const tkey& sep) {
                return compare_keys(k, sep);
            }) - middle->_keys.begin());
        current = middle->_pointers[idx];
    }

    auto* leaf = static_cast<bptree_node_term*>(current);
    auto it = std::lower_bound(leaf->_data.begin(), leaf->_data.end(), key,
        [this](const tree_data_type& item, const tkey& k) {
            return compare_keys(item.first, k);
        });

    if (it != leaf->_data.end()) {
        return bptree_const_iterator(leaf, static_cast<size_t>(it - leaf->_data.begin()));
    }

    if (leaf->_next != nullptr) {
        return bptree_const_iterator(leaf->_next, 0);
    }

    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }

    bptree_node_base* current = _root;
    while (!current->_is_terminate) {
        auto* middle = static_cast<bptree_node_middle*>(current);
        const size_t idx = static_cast<size_t>(std::upper_bound(middle->_keys.begin(), middle->_keys.end(), key,
            [this](const tkey& k, const tkey& sep) {
                return compare_keys(k, sep);
            }) - middle->_keys.begin());
        current = middle->_pointers[idx];
    }

    auto* leaf = static_cast<bptree_node_term*>(current);
    auto it = std::upper_bound(leaf->_data.begin(), leaf->_data.end(), key,
        [this](const tkey& k, const tree_data_type& item) {
            return compare_keys(k, item.first);
        });

    if (it != leaf->_data.end()) {
        return bptree_iterator(leaf, static_cast<size_t>(it - leaf->_data.begin()));
    }

    if (leaf->_next != nullptr) {
        return bptree_iterator(leaf->_next, 0);
    }

    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key) const
{
    if (_root == nullptr) {
        return end();
    }

    bptree_node_base* current = _root;
    while (!current->_is_terminate) {
        auto* middle = static_cast<bptree_node_middle*>(current);
        const size_t idx = static_cast<size_t>(std::upper_bound(middle->_keys.begin(), middle->_keys.end(), key,
            [this](const tkey& k, const tkey& sep) {
                return compare_keys(k, sep);
            }) - middle->_keys.begin());
        current = middle->_pointers[idx];
    }

    auto* leaf = static_cast<bptree_node_term*>(current);
    auto it = std::upper_bound(leaf->_data.begin(), leaf->_data.end(), key,
        [this](const tkey& k, const tree_data_type& item) {
            return compare_keys(k, item.first);
        });

    if (it != leaf->_data.end()) {
        return bptree_const_iterator(leaf, static_cast<size_t>(it - leaf->_data.begin()));
    }

    if (leaf->_next != nullptr) {
        return bptree_const_iterator(leaf->_next, 0);
    }

    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::contains(const tkey& key) const
{
    return find(key) != end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::clear() noexcept
{
    if (_root == nullptr) {
        _size = 0;
        return;
    }

    std::vector<bptree_node_base*> stack;
    stack.push_back(_root);

    while (!stack.empty()) {
        bptree_node_base* node = stack.back();
        stack.pop_back();

        if (!node->_is_terminate) {
            auto* middle = static_cast<bptree_node_middle*>(node);
            for (auto* child : middle->_pointers) {
                if (child != nullptr) {
                    stack.push_back(child);
                }
            }
            _allocator.delete_object(middle);
        } else {
            auto* leaf = static_cast<bptree_node_term*>(node);
            _allocator.delete_object(leaf);
        }
    }

    _root = nullptr;
    _size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator, bool> BP_tree<tkey, tvalue, compare, t>::insert(tree_data_type&& data)
{
    return emplace(std::move(data.first), std::move(data.second));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template <typename ...Args>
std::pair<typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator, bool> BP_tree<tkey, tvalue, compare, t>::emplace(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);
    tkey key = data.first;

    auto existed = find(key);
    if (existed != end()) {
        return {existed, false};
    }

    if (_root == nullptr) {
        auto* leaf = _allocator.new_object<bptree_node_term>();
        leaf->_data.push_back(std::move(data));
        _root = leaf;
        ++_size;
        return {bptree_iterator(leaf, 0), true};
    }

    std::vector<std::pair<bptree_node_middle*, size_t>> path;
    bptree_node_base* current = _root;
    while (!current->_is_terminate) {
        auto* middle = static_cast<bptree_node_middle*>(current);
        const size_t idx = static_cast<size_t>(std::upper_bound(middle->_keys.begin(), middle->_keys.end(), key,
            [this](const tkey& k, const tkey& sep) {
                return compare_keys(k, sep);
            }) - middle->_keys.begin());
        path.push_back({middle, idx});
        current = middle->_pointers[idx];
    }

    auto* leaf = static_cast<bptree_node_term*>(current);
    const size_t insert_index = static_cast<size_t>(std::lower_bound(leaf->_data.begin(), leaf->_data.end(), key,
        [this](const tree_data_type& item, const tkey& k) {
            return compare_keys(item.first, k);
        }) - leaf->_data.begin());
    leaf->_data.insert(leaf->_data.begin() + static_cast<ptrdiff_t>(insert_index), std::move(data));
    ++_size;

    if (leaf->_data.size() <= maximum_keys_in_node) {
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            refresh_keys(it->first);
        }
        return {bptree_iterator(leaf, insert_index), true};
    }

    bptree_node_base* left = leaf;
    bptree_node_base* right = nullptr;

    auto split_leaf = [this](bptree_node_term* node) {
        auto* new_leaf = _allocator.new_object<bptree_node_term>();
        const size_t split_index = t;

        for (size_t i = split_index; i < node->_data.size(); ++i) {
            new_leaf->_data.push_back(std::move(node->_data[i]));
        }

        node->_data.resize(split_index);
        new_leaf->_next = node->_next;
        node->_next = new_leaf;
        return new_leaf;
    };

    auto* new_leaf = split_leaf(leaf);
    right = new_leaf;

    while (true) {
        if (path.empty()) {
            auto* new_root = _allocator.new_object<bptree_node_middle>();
            new_root->_pointers.push_back(left);
            new_root->_pointers.push_back(right);
            refresh_keys(new_root);
            _root = new_root;
            break;
        }

        auto [parent, parent_idx] = path.back();
        path.pop_back();

        parent->_pointers.insert(parent->_pointers.begin() + static_cast<ptrdiff_t>(parent_idx + 1), right);
        refresh_keys(parent);

        if (parent->_keys.size() <= maximum_keys_in_node) {
            break;
        }

        auto* new_right = _allocator.new_object<bptree_node_middle>();
        const size_t total_children = parent->_pointers.size();
        const size_t left_children = t + 1;

        for (size_t i = left_children; i < total_children; ++i) {
            new_right->_pointers.push_back(parent->_pointers[i]);
        }

        parent->_pointers.resize(left_children);
        refresh_keys(parent);
        refresh_keys(new_right);

        left = parent;
        right = new_right;
    }

    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        refresh_keys(it->first);
    }

    return {find(key), true};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::insert_or_assign(const tree_data_type& data)
{
    auto [iterator, inserted] = emplace(data.first, data.second);
    if (!inserted) {
        iterator->second = data.second;
    }
    return iterator;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::insert_or_assign(tree_data_type&& data)
{
    auto [iterator, inserted] = emplace(std::move(data.first), std::move(data.second));
    if (!inserted) {
        iterator->second = std::move(data.second);
    }
    return iterator;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template <typename ...Args>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::emplace_or_assign(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);
    return insert_or_assign(std::move(data));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::erase(bptree_iterator pos)
{
    if (pos == end()) {
        return end();
    }

    return erase(pos->first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::erase(bptree_const_iterator pos)
{
    if (pos == end()) {
        return end();
    }

    return erase(pos->first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::erase(bptree_iterator beg, bptree_iterator en)
{
    if (beg == en) {
        return en;
    }

    std::vector<tkey> keys;
    for (auto iterator = beg; iterator != en; ++iterator) {
        keys.push_back(iterator->first);
    }

    bptree_iterator result = end();
    for (const auto& k : keys) {
        result = erase(k);
    }

    return result;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::erase(bptree_const_iterator beg, bptree_const_iterator en)
{
    if (beg == en) {
        return end();
    }

    std::vector<tkey> keys;
    for (auto iterator = beg; iterator != en; ++iterator) {
        keys.push_back(iterator->first);
    }

    bptree_iterator result = end();
    for (const auto& k : keys) {
        result = erase(k);
    }

    return result;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::erase(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }

    std::vector<std::pair<bptree_node_middle*, size_t>> path;
    bptree_node_base* current = _root;
    while (!current->_is_terminate) {
        auto* middle = static_cast<bptree_node_middle*>(current);
        const size_t idx = static_cast<size_t>(std::upper_bound(middle->_keys.begin(), middle->_keys.end(), key,
            [this](const tkey& k, const tkey& sep) {
                return compare_keys(k, sep);
            }) - middle->_keys.begin());
        path.push_back({middle, idx});
        current = middle->_pointers[idx];
    }

    auto* leaf = static_cast<bptree_node_term*>(current);
    auto it = std::lower_bound(leaf->_data.begin(), leaf->_data.end(), key,
        [this](const tree_data_type& item, const tkey& k) {
            return compare_keys(item.first, k);
        });

    if (it == leaf->_data.end() || compare_keys(key, it->first) || compare_keys(it->first, key)) {
        return end();
    }

    const size_t erased_index = static_cast<size_t>(it - leaf->_data.begin());
    leaf->_data.erase(leaf->_data.begin() + static_cast<ptrdiff_t>(erased_index));
    --_size;

    if (leaf == _root) {
        if (leaf->_data.empty()) {
            _allocator.delete_object(leaf);
            _root = nullptr;
        }
        return lower_bound(key);
    }

    if (erased_index == 0 && !leaf->_data.empty() && !path.empty() && path.back().second > 0) {
        auto [parent, child_idx] = path.back();
        parent->_keys[child_idx - 1] = leaf->_data.front().first;
    }

    if (leaf->_data.size() >= minimum_keys_in_node) {
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            refresh_keys(it->first);
        }
        return lower_bound(key);
    }

    if (path.empty()) {
        return lower_bound(key);
    }

    auto [parent, child_idx] = path.back();
    auto* left = child_idx > 0 ? static_cast<bptree_node_term*>(parent->_pointers[child_idx - 1]) : nullptr;
    auto* right = (child_idx + 1 < parent->_pointers.size())
        ? static_cast<bptree_node_term*>(parent->_pointers[child_idx + 1]) : nullptr;

    if (left != nullptr && left->_data.size() > minimum_keys_in_node) {
        leaf->_data.insert(leaf->_data.begin(), std::move(left->_data.back()));
        left->_data.pop_back();
        parent->_keys[child_idx - 1] = leaf->_data.front().first;
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            refresh_keys(it->first);
        }
        return lower_bound(key);
    }

    if (right != nullptr && right->_data.size() > minimum_keys_in_node) {
        leaf->_data.push_back(std::move(right->_data.front()));
        right->_data.erase(right->_data.begin());
        parent->_keys[child_idx] = right->_data.front().first;
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            refresh_keys(it->first);
        }
        return lower_bound(key);
    }

    if (right != nullptr) {
        for (auto& item : right->_data) {
            leaf->_data.push_back(std::move(item));
        }
        leaf->_next = right->_next;
        parent->_pointers.erase(parent->_pointers.begin() + static_cast<ptrdiff_t>(child_idx + 1));
        parent->_keys.erase(parent->_keys.begin() + static_cast<ptrdiff_t>(child_idx));
        _allocator.delete_object(right);
    } else if (left != nullptr) {
        for (auto& item : leaf->_data) {
            left->_data.push_back(std::move(item));
        }
        left->_next = leaf->_next;
        parent->_pointers.erase(parent->_pointers.begin() + static_cast<ptrdiff_t>(child_idx));
        parent->_keys.erase(parent->_keys.begin() + static_cast<ptrdiff_t>(child_idx - 1));
        _allocator.delete_object(leaf);
        leaf = left;
    }

    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        refresh_keys(it->first);
    }

    if (parent == _root && parent->_keys.empty()) {
        _root = parent->_pointers.empty() ? nullptr : parent->_pointers[0];
        _allocator.delete_object(parent);
    }

    return lower_bound(key);
}

#endif