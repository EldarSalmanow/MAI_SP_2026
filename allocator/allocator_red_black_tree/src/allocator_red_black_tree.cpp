#include <cstring>
#include <new>
#include <typeinfo>
#include "../include/allocator_red_black_tree.h"

std::uint8_t *byte_ptr(void *ptr) noexcept {
    return reinterpret_cast<std::uint8_t *>(ptr);
}

const std::uint8_t *byte_ptr(const void *ptr) noexcept {
    return reinterpret_cast<const std::uint8_t *>(ptr);
}

enum class rb_color : std::uint8_t {
    red = 0,
    black = 1
};

class block_view {
    void *memory_;

    static constexpr size_t data_offset = 0;
    static constexpr size_t size_offset = data_offset + sizeof(std::uint8_t);
    static constexpr size_t left_offset = size_offset + sizeof(size_t);
    static constexpr size_t right_offset = left_offset + sizeof(void *);
    static constexpr size_t parent_offset = right_offset + sizeof(void *);
    static constexpr size_t next_offset = parent_offset + sizeof(void *);

public:
    static constexpr size_t occupied_meta_size = size_offset + sizeof(size_t) + 2 * sizeof(void *);
    static constexpr size_t free_meta_size = next_offset + sizeof(void *);

    explicit block_view(void *memory) noexcept : memory_(memory) {}

    void *raw() const noexcept {
        return memory_;
    }

    std::uint8_t &data() const noexcept {
        return *reinterpret_cast<std::uint8_t *>(byte_ptr(memory_) + data_offset);
    }

    bool occupied() const noexcept {
        return (data() & 0x0F) != 0;
    }
    
    void set_occupied(bool value) noexcept {
        data() = (data() & 0xF0) | (value ? 1 : 0);
    }

    rb_color color() const noexcept {
        return static_cast<rb_color>((data() >> 4) & 0x0F);
    }
    
    void set_color(rb_color value) noexcept {
        data() = (data() & 0x0F) | (static_cast<std::uint8_t>(value) << 4);
    }

    size_t &size() const noexcept {
        return *reinterpret_cast<size_t *>(byte_ptr(memory_) + size_offset);
    }

    void *&left() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + left_offset);
    }

    void *&right() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + right_offset);
    }

    void *&parent() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + parent_offset);
    }

    void *&next() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + next_offset);
    }

    void *payload() const noexcept {
        return byte_ptr(memory_) + occupied_meta_size;
    }

    static block_view from_payload(void *payload) noexcept {
        return block_view(byte_ptr(payload) - occupied_meta_size);
    }
};

class control_block_view {
    void *memory_;

    static constexpr size_t parent_allocator_offset = 0;
    static constexpr size_t fit_mode_offset = parent_allocator_offset + sizeof(std::pmr::memory_resource *);
    static constexpr size_t space_size_offset = fit_mode_offset + sizeof(allocator_with_fit_mode::fit_mode);
    static constexpr size_t mutex_offset = space_size_offset + sizeof(size_t);
    static constexpr size_t root_offset = mutex_offset + sizeof(std::mutex);

public:
    static constexpr size_t metadata_size = root_offset + sizeof(void *);

    explicit control_block_view(void *memory) noexcept : memory_(memory) {}

    std::pmr::memory_resource *&parent_allocator() const noexcept {
        return *reinterpret_cast<std::pmr::memory_resource **>(byte_ptr(memory_) + parent_allocator_offset);
    }

    allocator_with_fit_mode::fit_mode &fit_mode() const noexcept {
        return *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(byte_ptr(memory_) + fit_mode_offset);
    }

    size_t &space_size() const noexcept {
        return *reinterpret_cast<size_t *>(byte_ptr(memory_) + space_size_offset);
    }

    std::mutex &mutex() const noexcept {
        return *reinterpret_cast<std::mutex *>(byte_ptr(memory_) + mutex_offset);
    }

    void *&root() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + root_offset);
    }

    std::uint8_t *arena_begin() const noexcept {
        return byte_ptr(memory_) + metadata_size;
    }

    std::uint8_t *arena_end() const noexcept {
        return arena_begin() + space_size();
    }
};

inline bool is_red(void *node) noexcept {
    return node && block_view(node).color() == rb_color::red;
}

inline void set_color(void *node, rb_color color) noexcept {
    if (!node) {
        return;
    }

    block_view(node).set_color(color);
}

void rotate(control_block_view &ctrl, void *x, bool left) noexcept {
    block_view bx(x);
    void *y = left ? bx.right() : bx.left();
    block_view by(y);

    (left ? bx.right() : bx.left()) = (left ? by.left() : by.right());
    if (left ? by.left() : by.right()) {
        block_view(left ? by.left() : by.right()).parent() = x;
    }

    by.parent() = bx.parent();
    if (!bx.parent()) {
        ctrl.root() = y;
    } else if (x == block_view(bx.parent()).left()) {
        block_view(bx.parent()).left() = y;
    } else {
        block_view(bx.parent()).right() = y;
    }

    (left ? by.left() : by.right()) = x;
    bx.parent() = y;
}

void rotate_left(control_block_view &ctrl, void *x) noexcept {
    block_view bx(x);
    void *y = bx.right();
    block_view by(y);

    bx.right() = by.left();
    if (by.left()) block_view(by.left()).parent() = x;

    by.parent() = bx.parent();
    if (!bx.parent()) {
        ctrl.root() = y;
    } else if (x == block_view(bx.parent()).left()) {
        block_view(bx.parent()).left() = y;
    } else {
        block_view(bx.parent()).right() = y;
    }

    by.left() = x;
    bx.parent() = y;
}

void rotate_right(control_block_view &ctrl, void *x) noexcept {
    block_view bx(x);
    void *y = bx.left();
    block_view by(y);

    bx.left() = by.right();
    if (by.right()) block_view(by.right()).parent() = x;

    by.parent() = bx.parent();
    if (!bx.parent()) {
        ctrl.root() = y;
    } else if (x == block_view(bx.parent()).right()) {
        block_view(bx.parent()).right() = y;
    } else {
        block_view(bx.parent()).left() = y;
    }

    by.right() = x;
    bx.parent() = y;
}

void insert_fixup(control_block_view &ctrl, void *z) noexcept {
    while (is_red(block_view(z).parent())) {
        void *parent = block_view(z).parent();
        void *grandparent = block_view(parent).parent();
        bool parent_is_left = (parent == block_view(grandparent).left());

        void *uncle = parent_is_left ? block_view(grandparent).right() : block_view(grandparent).left();

        if (is_red(uncle)) {
            set_color(parent, rb_color::black);
            set_color(uncle, rb_color::black);
            set_color(grandparent, rb_color::red);
            z = grandparent;
        } else {
            if (z == (parent_is_left ? block_view(parent).right() : block_view(parent).left())) {
                z = parent;
                parent_is_left ? rotate_left(ctrl, z) : rotate_right(ctrl, z);
            }
            set_color(block_view(z).parent(), rb_color::black);
            set_color(block_view(block_view(z).parent()).parent(), rb_color::red);
            parent_is_left ? rotate_right(ctrl, block_view(block_view(z).parent()).parent())
                          : rotate_left(ctrl, block_view(block_view(z).parent()).parent());
        }
    }

    set_color(ctrl.root(), rb_color::black);
}

bool less_block(void *left, void *right) noexcept {
    const auto left_size = block_view(left).size();
    const auto right_size = block_view(right).size();

    if (left_size != right_size) {
        return left_size < right_size;
    }

    return reinterpret_cast<std::uintptr_t>(left) < reinterpret_cast<std::uintptr_t>(right);
}

void insert_node(control_block_view &control, void *node) noexcept {
    void *parent = nullptr;
    void *current = control.root();

    while (current) {
        parent = current;
        current = less_block(node, current) ? block_view(current).left() : block_view(current).right();
    }

    block_view block(node);
    block.parent() = parent;
    block.left() = nullptr;
    block.right() = nullptr;
    block.set_color(rb_color::red);

    if (!parent) {
        control.root() = node;
    } else if (less_block(node, parent)) {
        block_view(parent).left() = node;
    } else {
        block_view(parent).right() = node;
    }

    insert_fixup(control, node);
}

void transplant(control_block_view &control, void *u, void *v) noexcept {
    block_view block_u(u);
    if (!block_u.parent()) {
        control.root() = v;
    } else if (u == block_view(block_u.parent()).left()) {
        block_view(block_u.parent()).left() = v;
    } else {
        block_view(block_u.parent()).right() = v;
    }

    if (v) {
        block_view(v).parent() = block_u.parent();
    }
}

void *tree_min(void *node) noexcept {
    while (node && block_view(node).left()) {
        node = block_view(node).left();
    }
    
    return node;
}

void delete_fixup(control_block_view &control, void *x, void *x_parent) noexcept {
    while (x != control.root() && !is_red(x)) {
        bool x_is_left = (x == block_view(x_parent).left());
        void *w = x_is_left ? block_view(x_parent).right() : block_view(x_parent).left();

        if (is_red(w)) {
            set_color(w, rb_color::black);
            set_color(x_parent, rb_color::red);
            x_is_left ? rotate_left(control, x_parent) : rotate_right(control, x_parent);
            w = x_is_left ? block_view(x_parent).right() : block_view(x_parent).left();
        }

        if (!is_red(block_view(w).left()) && !is_red(block_view(w).right())) {
            set_color(w, rb_color::red);
            x = x_parent;
            x_parent = block_view(x_parent).parent();
        } else {
            if (!is_red(x_is_left ? block_view(w).right() : block_view(w).left())) {
                set_color(x_is_left ? block_view(w).left() : block_view(w).right(), rb_color::black);
                set_color(w, rb_color::red);
                x_is_left ? rotate_right(control, w) : rotate_left(control, w);
                w = x_is_left ? block_view(x_parent).right() : block_view(x_parent).left();
            }
            set_color(w, block_view(x_parent).color());
            set_color(x_parent, rb_color::black);
            set_color(x_is_left ? block_view(w).right() : block_view(w).left(), rb_color::black);
            x_is_left ? rotate_left(control, x_parent) : rotate_right(control, x_parent);
            x = control.root();
            x_parent = block_view(x).parent();
        }
    }
    
    set_color(x, rb_color::black);
}

void delete_node(control_block_view &control, void *z) noexcept {
    block_view bz(z);
    void *y = z;
    auto y_original_color = bz.color();
    void *x = nullptr;
    void *x_parent = nullptr;

    if (!bz.left()) {
        x = bz.right();
        x_parent = bz.parent();
        transplant(control, z, x);
    } else if (!bz.right()) {
        x = bz.left();
        x_parent = bz.parent();
        transplant(control, z, x);
    } else {
        y = tree_min(bz.right());
        block_view by(y);
        y_original_color = by.color();
        x = by.right();

        if (by.parent() == z) {
            x_parent = y;
            if (x) block_view(x).parent() = y;
        } else {
            x_parent = by.parent();
            transplant(control, y, x);
            by.right() = bz.right();
            block_view(bz.right()).parent() = y;
        }

        transplant(control, z, y);
        by.left() = bz.left();
        block_view(bz.left()).parent() = y;
        by.set_color(bz.color());
    }

    if (y_original_color == rb_color::black) {
        delete_fixup(control, x, x_parent);
    }
}

void *tree_successor(void *node) noexcept {
    block_view block(node);

    if (block.right()) {
        return tree_min(block.right());
    }

    void *parent = block.parent();
    while (parent && node == block_view(parent).right()) {
        node = parent;
        parent = block_view(parent).parent();
    }

    return parent;
}

void *find_block(control_block_view &control, size_t required_size, allocator_with_fit_mode::fit_mode mode) noexcept {
    void *candidate = nullptr;

    for (void *node = tree_min(control.root()); node; node = tree_successor(node)) {
        const auto size = block_view(node).size();
        
        if (size < required_size) {
            continue;
        }

        if (!candidate) {
            candidate = node;
            
            if (mode == allocator_with_fit_mode::fit_mode::first_fit) {
                break;
            }

            continue;
        }

        bool is_better = false;
        is_better |= (mode == allocator_with_fit_mode::fit_mode::the_best_fit && size < block_view(candidate).size());
        is_better |= (mode == allocator_with_fit_mode::fit_mode::the_worst_fit && size > block_view(candidate).size());

        if (is_better) {
            candidate = node;
        }
    }

    return candidate;
}

void *create_storage(size_t space_size, std::pmr::memory_resource *parent, allocator_with_fit_mode::fit_mode fit) {
    if (!parent) {
        parent = std::pmr::get_default_resource();
    }

    void *memory = parent->allocate(control_block_view::metadata_size + space_size);
    if (!memory) {
        return nullptr;
    }

    control_block_view control(memory);
    control.parent_allocator() = parent;
    control.fit_mode() = fit;
    control.space_size() = space_size;
    new(&control.mutex()) std::mutex();
    control.root() = nullptr;

    block_view initial(control.arena_begin());
    initial.size() = space_size;
    initial.set_occupied(false);
    initial.set_color(rb_color::black);
    initial.left() = nullptr;
    initial.right() = nullptr;
    initial.parent() = nullptr;
    initial.next() = nullptr;
    control.root() = control.arena_begin();

    return memory;
}

void destroy_storage(void *&memory) noexcept {
    if (!memory) {
        return;
    }

    control_block_view control(memory);
    control.mutex().~mutex();

    auto *parent = control.parent_allocator();
    parent->deallocate(memory, control_block_view::metadata_size + control.space_size());

    memory = nullptr;
}

allocator_red_black_tree::~allocator_red_black_tree() {
    destroy_storage(_trusted_memory);
}

allocator_red_black_tree::allocator_red_black_tree(allocator_red_black_tree &&other) noexcept
    : _trusted_memory(other._trusted_memory) {
    other._trusted_memory = nullptr;
}

allocator_red_black_tree &allocator_red_black_tree::operator=(allocator_red_black_tree &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy_storage(_trusted_memory);
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    return *this;
}

allocator_red_black_tree::allocator_red_black_tree(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
    : _trusted_memory(create_storage(space_size, parent_allocator, allocate_fit_mode)) {}

allocator_red_black_tree::allocator_red_black_tree(const allocator_red_black_tree &other)
    : _trusted_memory(nullptr) {
    if (!other._trusted_memory) return;

    auto *other_memory = const_cast<void *>(other._trusted_memory);
    control_block_view source(other_memory);

    std::lock_guard lock(source.mutex());

    _trusted_memory = create_storage(source.space_size(), source.parent_allocator(), source.fit_mode());

    control_block_view target(_trusted_memory);
    std::memcpy(target.arena_begin(), source.arena_begin(), source.space_size());
    target.root() = nullptr;

    for (auto iterator = begin(); iterator != end(); ++iterator) {
        block_view block(*iterator);

        if (block.occupied()) {
            continue;
        }

        block.left() = nullptr;
        block.right() = nullptr;
        block.parent() = nullptr;
        block.next() = nullptr;

        insert_node(target, block.raw());
    }
}

allocator_red_black_tree &allocator_red_black_tree::operator=(const allocator_red_black_tree &other) {
    if (this == &other) {
        return *this;
    }
    
    allocator_red_black_tree copy(other);
    
    *this = std::move(copy);

    return *this;
}

bool allocator_red_black_tree::do_is_equal(const std::pmr::memory_resource &other) const noexcept {
    return typeid(*this) == typeid(other);
}

void *allocator_red_black_tree::do_allocate_sm(size_t size) {
    control_block_view ctrl(_trusted_memory);
    std::lock_guard lock(ctrl.mutex());

    const size_t required_total = size + block_view::occupied_meta_size;
    void *selected = find_block(ctrl, required_total, ctrl.fit_mode());
    if (!selected) throw std::bad_alloc();

    delete_node(ctrl, selected);

    block_view selected_block(selected);
    const size_t selected_size = selected_block.size();

    if (selected_size >= required_total + block_view::free_meta_size + 1) {
        void *remaining_ptr = byte_ptr(selected) + required_total;
        const size_t remaining_size = selected_size - required_total;

        block_view remaining(remaining_ptr);
        remaining.size() = remaining_size;
        remaining.set_occupied(false);
        remaining.set_color(rb_color::black);
        remaining.left() = nullptr;
        remaining.right() = nullptr;
        remaining.parent() = nullptr;
        remaining.next() = nullptr;

        insert_node(ctrl, remaining_ptr);
        selected_block.size() = required_total;
    }

    selected_block.set_occupied(true);
    selected_block.set_color(rb_color::black);
    selected_block.left() = nullptr;
    selected_block.right() = nullptr;

    return selected_block.payload();
}

void allocator_red_black_tree::do_deallocate_sm(void *at) {
    if (!at) return;

    control_block_view ctrl(_trusted_memory);
    std::lock_guard lock(ctrl.mutex());

    block_view current = block_view::from_payload(at);
    current.set_occupied(false);

    auto *arena_begin = ctrl.arena_begin();
    auto *arena_end = ctrl.arena_end();

    void *prev_ptr = nullptr;
    for (auto *it = arena_begin; it && it < byte_ptr(current.raw());) {
        block_view it_block(it);
        auto *next = byte_ptr(it) + it_block.size();
        if (next == byte_ptr(current.raw())) {
            prev_ptr = it;
            break;
        }
        if (next <= it || next > arena_end) break;
        it = next;
    }

    auto *next_ptr = byte_ptr(current.raw()) + current.size();
    if (next_ptr >= arena_end) next_ptr = nullptr;

    if (next_ptr) {
        block_view next_block(next_ptr);
        if (!next_block.occupied()) {
            delete_node(ctrl, next_ptr);
            current.size() += next_block.size();
        }
    }

    if (prev_ptr) {
        block_view prev_block(prev_ptr);
        if (!prev_block.occupied()) {
            delete_node(ctrl, prev_ptr);
            prev_block.size() += current.size();
            current = prev_block;
        }
    }

    current.set_color(rb_color::black);
    current.left() = nullptr;
    current.right() = nullptr;
    current.parent() = nullptr;
    current.next() = nullptr;

    insert_node(ctrl, current.raw());
}

void allocator_red_black_tree::set_fit_mode(allocator_with_fit_mode::fit_mode mode) {
    control_block_view control(_trusted_memory);

    std::lock_guard lock(control.mutex());
    
    control.fit_mode() = mode;
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info() const {
    control_block_view control(_trusted_memory);

    std::lock_guard lock(control.mutex());
    
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info_inner() const {
    std::vector<allocator_test_utils::block_info> result;
    
    for (auto iterator = begin(); iterator != end(); ++iterator) {
        result.push_back({iterator.size(), iterator.occupied()});
    }
    
    return result;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::begin() const noexcept {
    return rb_iterator(_trusted_memory);
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::end() const noexcept {
    return {};
}

allocator_red_black_tree::rb_iterator::rb_iterator()
    : _block_ptr(nullptr), _trusted(nullptr) {}

allocator_red_black_tree::rb_iterator::rb_iterator(void *trusted)
    : _block_ptr(nullptr), _trusted(trusted) {
    if (!_trusted) {
        return;
    }

    control_block_view control(_trusted);
    
    _block_ptr = control.arena_begin();
}

bool allocator_red_black_tree::rb_iterator::operator==(const rb_iterator &other) const noexcept {
    return _block_ptr == other._block_ptr;
}

bool allocator_red_black_tree::rb_iterator::operator!=(const rb_iterator &other) const noexcept {
    return !(*this == other);
}

allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept {
    if (!_block_ptr || !_trusted) {
        return *this;
    }

    control_block_view control(_trusted);
    block_view block(_block_ptr);
    
    void *next = byte_ptr(_block_ptr) + block.size();

    _block_ptr = (next >= control.arena_end()) ? nullptr : next;

    return *this;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int) {
    auto copy = *this;

    ++(*this);
    
    return copy;
}

size_t allocator_red_black_tree::rb_iterator::size() const noexcept {
    return _block_ptr ? block_view(_block_ptr).size() : 0;
}

void *allocator_red_black_tree::rb_iterator::operator*() const noexcept {
    return _block_ptr;
}

bool allocator_red_black_tree::rb_iterator::occupied() const noexcept {
    return _block_ptr && block_view(_block_ptr).occupied();
}
