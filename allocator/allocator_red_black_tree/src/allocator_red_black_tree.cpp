#include <cstring>
#include <new>
#include <typeinfo>
#include <cstdint>

#include "../include/allocator_red_black_tree.h"

namespace {
std::uint8_t *byte_ptr(void *ptr) noexcept {
    return reinterpret_cast<std::uint8_t *>(ptr);
}

const std::uint8_t *byte_ptr(const void *ptr) noexcept {
    return reinterpret_cast<const std::uint8_t *>(ptr);
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

class control_block_view {
public:
    static constexpr size_t parent_allocator_offset = 0;
    static constexpr size_t fit_mode_offset = parent_allocator_offset + sizeof(std::pmr::memory_resource *);
    static constexpr size_t space_size_offset = fit_mode_offset + sizeof(allocator_with_fit_mode::fit_mode);
    static constexpr size_t mutex_offset = space_size_offset + sizeof(size_t);
    static constexpr size_t root_offset = mutex_offset + sizeof(std::mutex);
    static constexpr size_t metadata_size = root_offset + sizeof(void *);

public:
    explicit control_block_view(void *memory) noexcept
        : memory_(memory) {}

public:
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

    void *root() const noexcept {
        return read_at<void *>(memory_, root_offset);
    }

    void set_root(void *ptr) const noexcept {
        write_at<void *>(memory_, root_offset, ptr);
    }

    std::uint8_t *arena_begin() const noexcept {
        return byte_ptr(memory_) + metadata_size;
    }

    std::uint8_t *arena_end() const noexcept {
        return arena_begin() + space_size();
    }

private:
    void *memory_;
};

enum class rb_color : std::uint8_t { red = 0, black = 1 };

class block_view {
public:
    static constexpr size_t data_size = sizeof(std::uint8_t);
    static constexpr size_t data_offset = 0;
    static constexpr size_t size_offset = data_size;
    static constexpr size_t left_offset = size_offset + sizeof(void *);
    static constexpr size_t right_offset = left_offset + sizeof(void *);
    static constexpr size_t parent_offset = right_offset + sizeof(void *);
    static constexpr size_t next_offset = parent_offset + sizeof(void *);

    static constexpr size_t occupied_meta_size = data_size + 3 * sizeof(void *);
    static constexpr size_t free_meta_size = data_size + 5 * sizeof(void *);

public:
    explicit block_view(void *memory) noexcept
        : memory_(memory) {}

public:
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
        auto d = data();
        d = static_cast<std::uint8_t>((d & 0xF0) | (value ? 1 : 0));
        set_data(d);
    }

    rb_color color() const noexcept {
        return static_cast<rb_color>((data() >> 4) & 0x0F);
    }

    void set_color(rb_color value) const noexcept {
        auto d = data();
        d = static_cast<std::uint8_t>((d & 0x0F) | (static_cast<std::uint8_t>(value) << 4));
        set_data(d);
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

    static block_view from_payload(void *payload) noexcept {
        return block_view(byte_ptr(payload) - occupied_meta_size);
    }

private:
    void *memory_;
};

void init_free_block(void *ptr, size_t total_size) noexcept {
    block_view block(ptr);
    block.set_size(total_size);
    block.set_occupied(false);
    block.set_color(rb_color::black);
    block.set_left(nullptr);
    block.set_right(nullptr);
    block.set_parent(nullptr);
    block.set_next(nullptr);
}

void init_occupied_block(void *ptr, size_t total_size) noexcept {
    block_view block(ptr);
    block.set_size(total_size);
    block.set_occupied(true);
    block.set_color(rb_color::black);
    block.set_left(nullptr);
    block.set_right(nullptr);
}

bool less_block(void *lhs, void *rhs) noexcept {
    const auto lhs_size = block_view(lhs).size();
    const auto rhs_size = block_view(rhs).size();
    if (lhs_size != rhs_size) {
        return lhs_size < rhs_size;
    }

    return reinterpret_cast<std::uintptr_t>(lhs) < reinterpret_cast<std::uintptr_t>(rhs);
}

bool is_red(void *node) noexcept {
    if (!node) {
        return false;
    }

    return block_view(node).color() == rb_color::red;
}

bool is_black(void *node) noexcept {
    return !is_red(node);
}

void set_color(void *node, rb_color color) noexcept {
    if (!node) {
        return;
    }

    block_view(node).set_color(color);
}

void *left_of(void *node) noexcept {
    return node ? block_view(node).left() : nullptr;
}

void *right_of(void *node) noexcept {
    return node ? block_view(node).right() : nullptr;
}

void *parent_of(void *node) noexcept {
    return node ? block_view(node).parent() : nullptr;
}

void rotate_left(const control_block_view &control, void *x) noexcept {
    void *y = right_of(x);
    block_view(x).set_right(left_of(y));
    if (left_of(y)) {
        block_view(left_of(y)).set_parent(x);
    }

    block_view(y).set_parent(parent_of(x));
    if (!parent_of(x)) {
        control.set_root(y);
    } else if (x == left_of(parent_of(x))) {
        block_view(parent_of(x)).set_left(y);
    } else {
        block_view(parent_of(x)).set_right(y);
    }

    block_view(y).set_left(x);
    block_view(x).set_parent(y);
}

void rotate_right(const control_block_view &control, void *x) noexcept {
    void *y = left_of(x);
    block_view(x).set_left(right_of(y));
    if (right_of(y)) {
        block_view(right_of(y)).set_parent(x);
    }

    block_view(y).set_parent(parent_of(x));
    if (!parent_of(x)) {
        control.set_root(y);
    } else if (x == right_of(parent_of(x))) {
        block_view(parent_of(x)).set_right(y);
    } else {
        block_view(parent_of(x)).set_left(y);
    }

    block_view(y).set_right(x);
    block_view(x).set_parent(y);
}

void insert_fixup(const control_block_view &control, void *z) noexcept {
    while (is_red(parent_of(z))) {
        if (parent_of(z) == left_of(parent_of(parent_of(z)))) {
            void *y = right_of(parent_of(parent_of(z)));
            if (is_red(y)) {
                set_color(parent_of(z), rb_color::black);
                set_color(y, rb_color::black);
                set_color(parent_of(parent_of(z)), rb_color::red);
                z = parent_of(parent_of(z));
            } else {
                if (z == right_of(parent_of(z))) {
                    z = parent_of(z);
                    rotate_left(control, z);
                }
                set_color(parent_of(z), rb_color::black);
                set_color(parent_of(parent_of(z)), rb_color::red);
                rotate_right(control, parent_of(parent_of(z)));
            }
        } else {
            void *y = left_of(parent_of(parent_of(z)));
            if (is_red(y)) {
                set_color(parent_of(z), rb_color::black);
                set_color(y, rb_color::black);
                set_color(parent_of(parent_of(z)), rb_color::red);
                z = parent_of(parent_of(z));
            } else {
                if (z == left_of(parent_of(z))) {
                    z = parent_of(z);
                    rotate_right(control, z);
                }
                set_color(parent_of(z), rb_color::black);
                set_color(parent_of(parent_of(z)), rb_color::red);
                rotate_left(control, parent_of(parent_of(z)));
            }
        }
    }

    set_color(control.root(), rb_color::black);
}

void insert_node(const control_block_view &control, void *node) noexcept {
    void *parent = nullptr;
    void *current = control.root();

    while (current) {
        parent = current;
        current = less_block(node, current) ? left_of(current) : right_of(current);
    }

    block_view(node).set_parent(parent);
    block_view(node).set_left(nullptr);
    block_view(node).set_right(nullptr);
    block_view(node).set_color(rb_color::red);

    if (!parent) {
        control.set_root(node);
    } else if (less_block(node, parent)) {
        block_view(parent).set_left(node);
    } else {
        block_view(parent).set_right(node);
    }

    insert_fixup(control, node);
}

void transplant(const control_block_view &control, void *u, void *v) noexcept {
    if (!parent_of(u)) {
        control.set_root(v);
    } else if (u == left_of(parent_of(u))) {
        block_view(parent_of(u)).set_left(v);
    } else {
        block_view(parent_of(u)).set_right(v);
    }

    if (v) {
        block_view(v).set_parent(parent_of(u));
    }
}

void *tree_min(void *node) noexcept {
    if (!node) {
        return nullptr;
    }

    while (left_of(node)) {
        node = left_of(node);
    }

    return node;
}

void delete_fixup(const control_block_view &control, void *x, void *x_parent) noexcept {
    while (x != control.root() && is_black(x)) {
        if (x == left_of(x_parent)) {
            void *w = right_of(x_parent);
            if (is_red(w)) {
                set_color(w, rb_color::black);
                set_color(x_parent, rb_color::red);
                rotate_left(control, x_parent);
                w = right_of(x_parent);
            }
            if (is_black(left_of(w)) && is_black(right_of(w))) {
                set_color(w, rb_color::red);
                x = x_parent;
                x_parent = parent_of(x_parent);
            } else {
                if (is_black(right_of(w))) {
                    set_color(left_of(w), rb_color::black);
                    set_color(w, rb_color::red);
                    rotate_right(control, w);
                    w = right_of(x_parent);
                }
                set_color(w, block_view(x_parent).color());
                set_color(x_parent, rb_color::black);
                set_color(right_of(w), rb_color::black);
                rotate_left(control, x_parent);
                x = control.root();
                x_parent = parent_of(x);
            }
        } else {
            void *w = left_of(x_parent);
            if (is_red(w)) {
                set_color(w, rb_color::black);
                set_color(x_parent, rb_color::red);
                rotate_right(control, x_parent);
                w = left_of(x_parent);
            }
            if (is_black(left_of(w)) && is_black(right_of(w))) {
                set_color(w, rb_color::red);
                x = x_parent;
                x_parent = parent_of(x_parent);
            } else {
                if (is_black(left_of(w))) {
                    set_color(right_of(w), rb_color::black);
                    set_color(w, rb_color::red);
                    rotate_left(control, w);
                    w = left_of(x_parent);
                }
                set_color(w, block_view(x_parent).color());
                set_color(x_parent, rb_color::black);
                set_color(left_of(w), rb_color::black);
                rotate_right(control, x_parent);
                x = control.root();
                x_parent = parent_of(x);
            }
        }
    }

    set_color(x, rb_color::black);
}

void delete_node(const control_block_view &control, void *z) noexcept {
    void *y = z;
    auto y_original_color = block_view(y).color();
    void *x = nullptr;
    void *x_parent = nullptr;

    if (!left_of(z)) {
        x = right_of(z);
        x_parent = parent_of(z);
        transplant(control, z, right_of(z));
    } else if (!right_of(z)) {
        x = left_of(z);
        x_parent = parent_of(z);
        transplant(control, z, left_of(z));
    } else {
        y = tree_min(right_of(z));
        y_original_color = block_view(y).color();
        x = right_of(y);
        if (parent_of(y) == z) {
            x_parent = y;
            if (x) {
                block_view(x).set_parent(y);
            }
        } else {
            x_parent = parent_of(y);
            transplant(control, y, right_of(y));
            block_view(y).set_right(right_of(z));
            block_view(right_of(z)).set_parent(y);
        }

        transplant(control, z, y);
        block_view(y).set_left(left_of(z));
        block_view(left_of(z)).set_parent(y);
        block_view(y).set_color(block_view(z).color());
    }

    if (y_original_color == rb_color::black) {
        delete_fixup(control, x, x_parent);
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

void *find_block(const control_block_view &control, size_t required_size, allocator_with_fit_mode::fit_mode mode) noexcept {
    void *candidate = nullptr;

    if (!control.root()) {
        return nullptr;
    }

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

        if (mode == allocator_with_fit_mode::fit_mode::the_best_fit && size < block_view(candidate).size()) {
            candidate = node;
        }

        if (mode == allocator_with_fit_mode::fit_mode::the_worst_fit && size > block_view(candidate).size()) {
            candidate = node;
        }
    }

    return candidate;
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

void *create_storage(size_t space_size,
                     std::pmr::memory_resource *parent,
                     allocator_with_fit_mode::fit_mode fit) {
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
    control.set_root(nullptr);

    if (space_size >= block_view::free_meta_size) {
        init_free_block(control.arena_begin(), space_size);
        control.set_root(control.arena_begin());
    }

    return memory;
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
    if (!other._trusted_memory) {
        return;
    }

    auto *other_memory = const_cast<void *>(other._trusted_memory);
    control_block_view source(other_memory);
    std::lock_guard lock(source.mutex());

    _trusted_memory = create_storage(source.space_size(), source.parent_allocator(), source.fit_mode());

    control_block_view target(_trusted_memory);
    std::memcpy(target.arena_begin(), source.arena_begin(), source.space_size());
    target.set_root(nullptr);

    for (auto iterator = rb_iterator(_trusted_memory); iterator != rb_iterator(); ++iterator) {
        block_view block(*iterator);
        if (block.occupied()) {
            continue;
        }
        block.set_left(nullptr);
        block.set_right(nullptr);
        block.set_parent(nullptr);
        block.set_next(nullptr);
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

[[nodiscard]] void *allocator_red_black_tree::do_allocate_sm(size_t size) {
    control_block_view control(_trusted_memory);
    std::lock_guard lock(control.mutex());

    const size_t required_total = size + block_view::occupied_meta_size;
    void *selected = find_block(control, required_total, control.fit_mode());
    if (!selected) {
        throw std::bad_alloc();
    }

    delete_node(control, selected);

    block_view selected_block(selected);
    const size_t selected_size = selected_block.size();

    if (selected_size >= required_total + block_view::free_meta_size + 1) {
        void *remaining_ptr = byte_ptr(selected) + required_total;
        const size_t remaining_size = selected_size - required_total;
        init_free_block(remaining_ptr, remaining_size);
        insert_node(control, remaining_ptr);
        selected_block.set_size(required_total);
    }

    init_occupied_block(selected, selected_block.size());
    return block_view(selected).payload();
}

void allocator_red_black_tree::do_deallocate_sm(void *at) {
    if (!at) {
        return;
    }

    control_block_view control(_trusted_memory);
    std::lock_guard lock(control.mutex());

    block_view current = block_view::from_payload(at);
    current.set_occupied(false);

    auto *arena_begin = control.arena_begin();
    auto *arena_end = control.arena_end();

    void *prev_ptr = nullptr;
    for (auto *it = arena_begin; it && it < byte_ptr(current.raw());) {
        block_view it_block(it);
        auto *next = byte_ptr(it) + it_block.size();
        if (next == byte_ptr(current.raw())) {
            prev_ptr = it;
            break;
        }
        if (next <= it || next > arena_end) {
            break;
        }
        it = next;
    }

    auto *next_ptr = byte_ptr(current.raw()) + current.size();
    if (next_ptr >= arena_end) {
        next_ptr = nullptr;
    }

    if (next_ptr) {
        block_view next_block(next_ptr);
        if (!next_block.occupied()) {
            delete_node(control, next_ptr);
            current.set_size(current.size() + next_block.size());
        }
    }

    if (prev_ptr) {
        block_view prev_block(prev_ptr);
        if (!prev_block.occupied()) {
            delete_node(control, prev_ptr);
            prev_block.set_size(prev_block.size() + current.size());
            current = prev_block;
        }
    }

    init_free_block(current.raw(), current.size());
    insert_node(control, current.raw());
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

bool allocator_red_black_tree::rb_iterator::operator==(const allocator_red_black_tree::rb_iterator &other) const noexcept {
    return _block_ptr == other._block_ptr;
}

bool allocator_red_black_tree::rb_iterator::operator!=(const allocator_red_black_tree::rb_iterator &other) const noexcept {
    return !(*this == other);
}

allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept {
    if (!_block_ptr || !_trusted) {
        return *this;
    }

    control_block_view control(_trusted);
    block_view block(_block_ptr);
    void *next = byte_ptr(_block_ptr) + block.size();
    if (next >= control.arena_end()) {
        _block_ptr = nullptr;
    } else {
        _block_ptr = next;
    }

    return *this;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int) {
    auto copy = *this;
    ++(*this);
    return copy;
}

size_t allocator_red_black_tree::rb_iterator::size() const noexcept {
    if (!_block_ptr) {
        return 0;
    }

    return block_view(_block_ptr).size();
}

void *allocator_red_black_tree::rb_iterator::operator*() const noexcept {
    return _block_ptr;
}

allocator_red_black_tree::rb_iterator::rb_iterator()
    : _block_ptr(nullptr), _trusted(nullptr) {}

allocator_red_black_tree::rb_iterator::rb_iterator(void *trusted)
    : _block_ptr(nullptr), _trusted(trusted) {
    if (!_trusted) {
        return;
    }

    control_block_view control(_trusted);
    if (control.arena_begin() < control.arena_end()) {
        _block_ptr = control.arena_begin();
    }
}

bool allocator_red_black_tree::rb_iterator::occupied() const noexcept {
    return _block_ptr && block_view(_block_ptr).occupied();
}
