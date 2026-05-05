#include <cstring>
#include <new>
#include <typeinfo>
#include <cstdint>

#include "../include/allocator_red_black_tree.h"

namespace {

// Utility functions
inline std::uint8_t *byte_ptr(void *ptr) noexcept {
    return static_cast<std::uint8_t *>(ptr);
}

inline const std::uint8_t *byte_ptr(const void *ptr) noexcept {
    return static_cast<const std::uint8_t *>(ptr);
}

template <typename T>
T read_at(const void *base, size_t offset) noexcept {
    T value;
    std::memcpy(&value, byte_ptr(base) + offset, sizeof(T));
    return value;
}

template <typename T>
void write_at(void *base, size_t offset, const T &value) noexcept {
    std::memcpy(byte_ptr(base) + offset, &value, sizeof(T));
}

// Control block layout and access
class control_block {
    void *memory_;

    static constexpr size_t parent_allocator_offset = 0;
    static constexpr size_t fit_mode_offset = parent_allocator_offset + sizeof(std::pmr::memory_resource *);
    static constexpr size_t space_size_offset = fit_mode_offset + sizeof(allocator_with_fit_mode::fit_mode);
    static constexpr size_t mutex_offset = space_size_offset + sizeof(size_t);
    static constexpr size_t root_offset = mutex_offset + sizeof(std::mutex);

public:
    static constexpr size_t metadata_size = root_offset + sizeof(void *);

    explicit control_block(void *memory) noexcept : memory_(memory) {}

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

// Block metadata layout and access
enum class rb_color : std::uint8_t { red = 0, black = 1 };

class block {
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

    explicit block(void *memory) noexcept : memory_(memory) {}

    std::uint8_t data() const noexcept {
        return read_at<std::uint8_t>(memory_, data_offset);
    }

    void set_data(std::uint8_t value) const noexcept {
        write_at(memory_, data_offset, value);
    }

    bool occupied() const noexcept {
        return (data() & 0x0F) != 0;
    }

    void set_occupied(bool value) const noexcept {
        set_data((data() & 0xF0) | (value ? 1 : 0));
    }

    rb_color color() const noexcept {
        return static_cast<rb_color>((data() >> 4) & 0x0F);
    }

    void set_color(rb_color value) const noexcept {
        set_data((data() & 0x0F) | (static_cast<std::uint8_t>(value) << 4));
    }

    size_t size() const noexcept {
        return read_at<size_t>(memory_, size_offset);
    }

    void set_size(size_t value) const noexcept {
        write_at(memory_, size_offset, value);
    }

    void *left() const noexcept {
        return read_at<void *>(memory_, left_offset);
    }

    void set_left(void *value) const noexcept {
        write_at(memory_, left_offset, value);
    }

    void *right() const noexcept {
        return read_at<void *>(memory_, right_offset);
    }

    void set_right(void *value) const noexcept {
        write_at(memory_, right_offset, value);
    }

    void *parent() const noexcept {
        return read_at<void *>(memory_, parent_offset);
    }

    void set_parent(void *value) const noexcept {
        write_at(memory_, parent_offset, value);
    }

    void *next() const noexcept {
        return read_at<void *>(memory_, next_offset);
    }

    void set_next(void *value) const noexcept {
        write_at(memory_, next_offset, value);
    }

    void *payload() const noexcept {
        return byte_ptr(memory_) + occupied_meta_size;
    }

    void *raw() const noexcept {
        return memory_;
    }

    static block from_payload(void *payload) noexcept {
        return block(byte_ptr(payload) - occupied_meta_size);
    }
};

// Red-Black Tree operations
inline bool is_red(void *node) noexcept {
    return node && block(node).color() == rb_color::red;
}

inline void set_color(void *node, rb_color color) noexcept {
    if (node) block(node).set_color(color);
}

inline void *left_of(void *node) noexcept {
    return node ? block(node).left() : nullptr;
}

inline void *right_of(void *node) noexcept {
    return node ? block(node).right() : nullptr;
}

inline void *parent_of(void *node) noexcept {
    return node ? block(node).parent() : nullptr;
}

void rotate_left(control_block &ctrl, void *x) noexcept {
    void *y = right_of(x);
    block bx(x), by(y);

    bx.set_right(left_of(y));
    if (left_of(y)) block(left_of(y)).set_parent(x);

    by.set_parent(parent_of(x));
    if (!parent_of(x)) {
        ctrl.root() = y;
    } else if (x == left_of(parent_of(x))) {
        block(parent_of(x)).set_left(y);
    } else {
        block(parent_of(x)).set_right(y);
    }

    by.set_left(x);
    bx.set_parent(y);
}

void rotate_right(control_block &ctrl, void *x) noexcept {
    void *y = left_of(x);
    block bx(x), by(y);

    bx.set_left(right_of(y));
    if (right_of(y)) block(right_of(y)).set_parent(x);

    by.set_parent(parent_of(x));
    if (!parent_of(x)) {
        ctrl.root() = y;
    } else if (x == right_of(parent_of(x))) {
        block(parent_of(x)).set_right(y);
    } else {
        block(parent_of(x)).set_left(y);
    }

    by.set_right(x);
    bx.set_parent(y);
}

void insert_fixup(control_block &ctrl, void *z) noexcept {
    while (is_red(parent_of(z))) {
        void *parent = parent_of(z);
        void *grandparent = parent_of(parent);

        if (parent == left_of(grandparent)) {
            void *uncle = right_of(grandparent);
            if (is_red(uncle)) {
                set_color(parent, rb_color::black);
                set_color(uncle, rb_color::black);
                set_color(grandparent, rb_color::red);
                z = grandparent;
            } else {
                if (z == right_of(parent)) {
                    z = parent;
                    rotate_left(ctrl, z);
                    parent = parent_of(z);
                    grandparent = parent_of(parent);
                }
                set_color(parent, rb_color::black);
                set_color(grandparent, rb_color::red);
                rotate_right(ctrl, grandparent);
            }
        } else {
            void *uncle = left_of(grandparent);
            if (is_red(uncle)) {
                set_color(parent, rb_color::black);
                set_color(uncle, rb_color::black);
                set_color(grandparent, rb_color::red);
                z = grandparent;
            } else {
                if (z == left_of(parent)) {
                    z = parent;
                    rotate_right(ctrl, z);
                    parent = parent_of(z);
                    grandparent = parent_of(parent);
                }
                set_color(parent, rb_color::black);
                set_color(grandparent, rb_color::red);
                rotate_left(ctrl, grandparent);
            }
        }
    }
    set_color(ctrl.root(), rb_color::black);
}

bool less_block(void *lhs, void *rhs) noexcept {
    const auto lhs_size = block(lhs).size();
    const auto rhs_size = block(rhs).size();
    return lhs_size != rhs_size ? lhs_size < rhs_size
                                : reinterpret_cast<std::uintptr_t>(lhs) < reinterpret_cast<std::uintptr_t>(rhs);
}

void insert_node(control_block &ctrl, void *node) noexcept {
    void *parent = nullptr;
    void *current = ctrl.root();

    while (current) {
        parent = current;
        current = less_block(node, current) ? left_of(current) : right_of(current);
    }

    block bn(node);
    bn.set_parent(parent);
    bn.set_left(nullptr);
    bn.set_right(nullptr);
    bn.set_color(rb_color::red);

    if (!parent) {
        ctrl.root() = node;
    } else if (less_block(node, parent)) {
        block(parent).set_left(node);
    } else {
        block(parent).set_right(node);
    }

    insert_fixup(ctrl, node);
}

void transplant(control_block &ctrl, void *u, void *v) noexcept {
    if (!parent_of(u)) {
        ctrl.root() = v;
    } else if (u == left_of(parent_of(u))) {
        block(parent_of(u)).set_left(v);
    } else {
        block(parent_of(u)).set_right(v);
    }
    if (v) block(v).set_parent(parent_of(u));
}

void *tree_min(void *node) noexcept {
    while (node && left_of(node)) {
        node = left_of(node);
    }
    return node;
}

void delete_fixup(control_block &ctrl, void *x, void *x_parent) noexcept {
    while (x != ctrl.root() && !is_red(x)) {
        if (x == left_of(x_parent)) {
            void *w = right_of(x_parent);
            if (is_red(w)) {
                set_color(w, rb_color::black);
                set_color(x_parent, rb_color::red);
                rotate_left(ctrl, x_parent);
                w = right_of(x_parent);
            }
            if (!is_red(left_of(w)) && !is_red(right_of(w))) {
                set_color(w, rb_color::red);
                x = x_parent;
                x_parent = parent_of(x_parent);
            } else {
                if (!is_red(right_of(w))) {
                    set_color(left_of(w), rb_color::black);
                    set_color(w, rb_color::red);
                    rotate_right(ctrl, w);
                    w = right_of(x_parent);
                }
                set_color(w, block(x_parent).color());
                set_color(x_parent, rb_color::black);
                set_color(right_of(w), rb_color::black);
                rotate_left(ctrl, x_parent);
                x = ctrl.root();
                x_parent = parent_of(x);
            }
        } else {
            void *w = left_of(x_parent);
            if (is_red(w)) {
                set_color(w, rb_color::black);
                set_color(x_parent, rb_color::red);
                rotate_right(ctrl, x_parent);
                w = left_of(x_parent);
            }
            if (!is_red(left_of(w)) && !is_red(right_of(w))) {
                set_color(w, rb_color::red);
                x = x_parent;
                x_parent = parent_of(x_parent);
            } else {
                if (!is_red(left_of(w))) {
                    set_color(right_of(w), rb_color::black);
                    set_color(w, rb_color::red);
                    rotate_left(ctrl, w);
                    w = left_of(x_parent);
                }
                set_color(w, block(x_parent).color());
                set_color(x_parent, rb_color::black);
                set_color(left_of(w), rb_color::black);
                rotate_right(ctrl, x_parent);
                x = ctrl.root();
                x_parent = parent_of(x);
            }
        }
    }
    set_color(x, rb_color::black);
}

void delete_node(control_block &ctrl, void *z) noexcept {
    void *y = z;
    auto y_original_color = block(y).color();
    void *x = nullptr;
    void *x_parent = nullptr;

    if (!left_of(z)) {
        x = right_of(z);
        x_parent = parent_of(z);
        transplant(ctrl, z, x);
    } else if (!right_of(z)) {
        x = left_of(z);
        x_parent = parent_of(z);
        transplant(ctrl, z, x);
    } else {
        y = tree_min(right_of(z));
        y_original_color = block(y).color();
        x = right_of(y);

        if (parent_of(y) == z) {
            x_parent = y;
            if (x) block(x).set_parent(y);
        } else {
            x_parent = parent_of(y);
            transplant(ctrl, y, x);
            block(y).set_right(right_of(z));
            block(right_of(z)).set_parent(y);
        }

        transplant(ctrl, z, y);
        block(y).set_left(left_of(z));
        block(left_of(z)).set_parent(y);
        block(y).set_color(block(z).color());
    }

    if (y_original_color == rb_color::black) {
        delete_fixup(ctrl, x, x_parent);
    }
}

void *tree_successor(void *node) noexcept {
    if (right_of(node)) {
        return tree_min(right_of(node));
    }

    void *parent = parent_of(node);
    while (parent && node == right_of(parent)) {
        node = parent;
        parent = parent_of(parent);
    }
    return parent;
}

void *find_block(control_block &ctrl, size_t required_size, allocator_with_fit_mode::fit_mode mode) noexcept {
    void *candidate = nullptr;

    for (void *node = tree_min(ctrl.root()); node; node = tree_successor(node)) {
        const auto size = block(node).size();
        if (size < required_size) continue;

        if (!candidate) {
            candidate = node;
            if (mode == allocator_with_fit_mode::fit_mode::first_fit) break;
            continue;
        }

        if (mode == allocator_with_fit_mode::fit_mode::the_best_fit && size < block(candidate).size()) {
            candidate = node;
        } else if (mode == allocator_with_fit_mode::fit_mode::the_worst_fit && size > block(candidate).size()) {
            candidate = node;
        }
    }

    return candidate;
}

void *create_storage(size_t space_size, std::pmr::memory_resource *parent, allocator_with_fit_mode::fit_mode fit) {
    if (!parent) parent = std::pmr::get_default_resource();

    void *memory = parent->allocate(control_block::metadata_size + space_size);
    if (!memory) return nullptr;

    control_block ctrl(memory);
    ctrl.parent_allocator() = parent;
    ctrl.fit_mode() = fit;
    ctrl.space_size() = space_size;
    new(&ctrl.mutex()) std::mutex();
    ctrl.root() = nullptr;

    if (space_size >= block::free_meta_size) {
        block initial(ctrl.arena_begin());
        initial.set_size(space_size);
        initial.set_occupied(false);
        initial.set_color(rb_color::black);
        initial.set_left(nullptr);
        initial.set_right(nullptr);
        initial.set_parent(nullptr);
        initial.set_next(nullptr);
        ctrl.root() = ctrl.arena_begin();
    }

    return memory;
}

void destroy_storage(void *&memory) noexcept {
    if (!memory) return;

    control_block ctrl(memory);
    ctrl.mutex().~mutex();

    auto *parent = ctrl.parent_allocator();
    parent->deallocate(memory, control_block::metadata_size + ctrl.space_size());

    memory = nullptr;
}

} // namespace

allocator_red_black_tree::~allocator_red_black_tree() {
    destroy_storage(_trusted_memory);
}

allocator_red_black_tree::allocator_red_black_tree(allocator_red_black_tree &&other) noexcept
    : _trusted_memory(other._trusted_memory) {
    other._trusted_memory = nullptr;
}

allocator_red_black_tree &allocator_red_black_tree::operator=(allocator_red_black_tree &&other) noexcept {
    if (this != &other) {
        destroy_storage(_trusted_memory);
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
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
    control_block source(other_memory);
    std::lock_guard lock(source.mutex());

    _trusted_memory = create_storage(source.space_size(), source.parent_allocator(), source.fit_mode());

    control_block target(_trusted_memory);
    std::memcpy(target.arena_begin(), source.arena_begin(), source.space_size());
    target.root() = nullptr;

    for (auto it = rb_iterator(_trusted_memory); it != rb_iterator(); ++it) {
        block blk(*it);
        if (blk.occupied()) continue;

        blk.set_left(nullptr);
        blk.set_right(nullptr);
        blk.set_parent(nullptr);
        blk.set_next(nullptr);
        insert_node(target, blk.raw());
    }
}

allocator_red_black_tree &allocator_red_black_tree::operator=(const allocator_red_black_tree &other) {
    if (this != &other) {
        allocator_red_black_tree copy(other);
        *this = std::move(copy);
    }
    return *this;
}

bool allocator_red_black_tree::do_is_equal(const std::pmr::memory_resource &other) const noexcept {
    return typeid(*this) == typeid(other);
}

void *allocator_red_black_tree::do_allocate_sm(size_t size) {
    control_block ctrl(_trusted_memory);
    std::lock_guard lock(ctrl.mutex());

    const size_t required_total = size + block::occupied_meta_size;
    void *selected = find_block(ctrl, required_total, ctrl.fit_mode());
    if (!selected) throw std::bad_alloc();

    delete_node(ctrl, selected);

    block selected_block(selected);
    const size_t selected_size = selected_block.size();

    if (selected_size >= required_total + block::free_meta_size + 1) {
        void *remaining_ptr = byte_ptr(selected) + required_total;
        const size_t remaining_size = selected_size - required_total;

        block remaining(remaining_ptr);
        remaining.set_size(remaining_size);
        remaining.set_occupied(false);
        remaining.set_color(rb_color::black);
        remaining.set_left(nullptr);
        remaining.set_right(nullptr);
        remaining.set_parent(nullptr);
        remaining.set_next(nullptr);

        insert_node(ctrl, remaining_ptr);
        selected_block.set_size(required_total);
    }

    selected_block.set_occupied(true);
    selected_block.set_color(rb_color::black);
    selected_block.set_left(nullptr);
    selected_block.set_right(nullptr);

    return selected_block.payload();
}

void allocator_red_black_tree::do_deallocate_sm(void *at) {
    if (!at) return;

    control_block ctrl(_trusted_memory);
    std::lock_guard lock(ctrl.mutex());

    block current = block::from_payload(at);
    current.set_occupied(false);

    auto *arena_begin = ctrl.arena_begin();
    auto *arena_end = ctrl.arena_end();

    // Find previous block
    void *prev_ptr = nullptr;
    for (auto *it = arena_begin; it && it < byte_ptr(current.raw());) {
        block it_block(it);
        auto *next = byte_ptr(it) + it_block.size();
        if (next == byte_ptr(current.raw())) {
            prev_ptr = it;
            break;
        }
        if (next <= it || next > arena_end) break;
        it = next;
    }

    // Find next block
    auto *next_ptr = byte_ptr(current.raw()) + current.size();
    if (next_ptr >= arena_end) next_ptr = nullptr;

    // Merge with next
    if (next_ptr) {
        block next_block(next_ptr);
        if (!next_block.occupied()) {
            delete_node(ctrl, next_ptr);
            current.set_size(current.size() + next_block.size());
        }
    }

    // Merge with previous
    if (prev_ptr) {
        block prev_block(prev_ptr);
        if (!prev_block.occupied()) {
            delete_node(ctrl, prev_ptr);
            prev_block.set_size(prev_block.size() + current.size());
            current = prev_block;
        }
    }

    current.set_color(rb_color::black);
    current.set_left(nullptr);
    current.set_right(nullptr);
    current.set_parent(nullptr);
    current.set_next(nullptr);

    insert_node(ctrl, current.raw());
}

void allocator_red_black_tree::set_fit_mode(allocator_with_fit_mode::fit_mode mode) {
    control_block ctrl(_trusted_memory);
    std::lock_guard lock(ctrl.mutex());
    ctrl.fit_mode() = mode;
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info() const {
    control_block ctrl(_trusted_memory);
    std::lock_guard lock(ctrl.mutex());
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info_inner() const {
    std::vector<allocator_test_utils::block_info> result;
    for (auto it = begin(); it != end(); ++it) {
        result.push_back({it.size(), it.occupied()});
    }
    return result;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::begin() const noexcept {
    return rb_iterator(_trusted_memory);
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::end() const noexcept {
    return {};
}

// Iterator implementation
allocator_red_black_tree::rb_iterator::rb_iterator()
    : _block_ptr(nullptr), _trusted(nullptr) {}

allocator_red_black_tree::rb_iterator::rb_iterator(void *trusted)
    : _block_ptr(nullptr), _trusted(trusted) {
    if (_trusted) {
        control_block ctrl(_trusted);
        if (ctrl.arena_begin() < ctrl.arena_end()) {
            _block_ptr = ctrl.arena_begin();
        }
    }
}

bool allocator_red_black_tree::rb_iterator::operator==(const rb_iterator &other) const noexcept {
    return _block_ptr == other._block_ptr;
}

bool allocator_red_black_tree::rb_iterator::operator!=(const rb_iterator &other) const noexcept {
    return !(*this == other);
}

allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept {
    if (!_block_ptr || !_trusted) return *this;

    control_block ctrl(_trusted);
    block blk(_block_ptr);
    void *next = byte_ptr(_block_ptr) + blk.size();

    _block_ptr = (next >= ctrl.arena_end()) ? nullptr : next;
    return *this;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int) {
    auto copy = *this;
    ++(*this);
    return copy;
}

size_t allocator_red_black_tree::rb_iterator::size() const noexcept {
    return _block_ptr ? block(_block_ptr).size() : 0;
}

void *allocator_red_black_tree::rb_iterator::operator*() const noexcept {
    return _block_ptr;
}

bool allocator_red_black_tree::rb_iterator::occupied() const noexcept {
    return _block_ptr && block(_block_ptr).occupied();
}
