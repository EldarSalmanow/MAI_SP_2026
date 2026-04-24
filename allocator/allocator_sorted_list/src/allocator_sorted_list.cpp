#include <cstring>
#include <new>
#include <typeinfo>
#include "../include/allocator_sorted_list.h"


std::uint8_t *byte_ptr(void *ptr) noexcept {
    return reinterpret_cast<std::uint8_t *>(ptr);
}

const std::uint8_t *byte_ptr(const void *ptr) noexcept {
    return reinterpret_cast<const std::uint8_t *>(ptr);
}

class block_view {
public:
    static constexpr size_t next_offset = 0;
    static constexpr size_t size_offset = sizeof(void *);
    static constexpr size_t metadata_size = sizeof(void *) + sizeof(size_t);

public:
    explicit block_view(void *memory) noexcept
            : memory_(memory) {}

public:
    static block_view from_payload(void *payload) noexcept {
        return block_view(byte_ptr(payload) - metadata_size);
    }

public:
    void *raw() const noexcept {
        return memory_;
    }

    void *&next() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + next_offset);
    }

    size_t &size() const noexcept {
        return *reinterpret_cast<size_t *>(byte_ptr(memory_) + size_offset);
    }

    void *payload() const noexcept {
        return byte_ptr(memory_) + metadata_size;
    }

    bool is_adjacent_to(block_view other) const noexcept {
        return byte_ptr(payload()) + size() == byte_ptr(other.raw());
    }

private:
    void *memory_;
};

class control_block_view {
public:
    static constexpr size_t parent_allocator_offset = 0;
    static constexpr size_t fit_mode_offset = parent_allocator_offset + sizeof(std::pmr::memory_resource *);
    static constexpr size_t space_size_offset = fit_mode_offset + sizeof(allocator_with_fit_mode::fit_mode);
    static constexpr size_t mutex_offset = space_size_offset + sizeof(size_t);
    static constexpr size_t free_head_offset = mutex_offset + sizeof(std::mutex);
    static constexpr size_t metadata_size = free_head_offset + sizeof(void *);

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

    void *&free_head() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + free_head_offset);
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

void destroy_storage(void *&memory) noexcept {
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

    auto *memory = parent->allocate(control_block_view::metadata_size + space_size);

    if (!memory) {
        return nullptr;
    }

    control_block_view control(memory);
    control.parent_allocator() = parent;
    control.fit_mode() = fit;
    control.space_size() = space_size;
    new(&control.mutex()) std::mutex();

    block_view first(control.arena_begin());
    first.next() = nullptr;
    first.size() = space_size - block_view::metadata_size;
    control.free_head() = first.raw();

    return memory;
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator() noexcept
        : free_block_(nullptr) {}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator(void *free_block) noexcept
        : free_block_(free_block) {}

bool allocator_sorted_list::sorted_free_iterator::operator==(const sorted_free_iterator &other) const noexcept {
    return free_block_ == other.free_block_;
}

bool allocator_sorted_list::sorted_free_iterator::operator!=(const sorted_free_iterator &other) const noexcept {
    return !(*this == other);
}

allocator_sorted_list::sorted_free_iterator &allocator_sorted_list::sorted_free_iterator::operator++() noexcept {
    if (free_block_) {
        free_block_ = block_view(free_block_).next();
    }

    return *this;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::sorted_free_iterator::operator++(int) {
    auto copy = *this;

    ++(*this);

    return copy;
}

size_t allocator_sorted_list::sorted_free_iterator::size() const noexcept {
    if (!free_block_) {
        return 0;
    }

    return block_view(free_block_).size();
}

void *allocator_sorted_list::sorted_free_iterator::operator*() const noexcept {
    return free_block_;
}

allocator_sorted_list::sorted_iterator::sorted_iterator() noexcept
    : current_block_(nullptr), next_free_block_(nullptr), arena_end_(nullptr) {}

allocator_sorted_list::sorted_iterator::sorted_iterator(void *memory) noexcept
    : current_block_(nullptr), next_free_block_(nullptr), arena_end_(nullptr) {
    control_block_view control(memory);
    current_block_ = control.arena_begin();
    next_free_block_ = control.free_head();
    arena_end_ = control.arena_end();

    while (next_free_block_ && byte_ptr(next_free_block_) < byte_ptr(current_block_)) {
        next_free_block_ = block_view(next_free_block_).next();
    }
}

bool allocator_sorted_list::sorted_iterator::operator==(const sorted_iterator &other) const noexcept {
    return current_block_ == other.current_block_;
}

bool allocator_sorted_list::sorted_iterator::operator!=(const sorted_iterator &other) const noexcept {
    return !(*this == other);
}

allocator_sorted_list::sorted_iterator &allocator_sorted_list::sorted_iterator::operator++() noexcept {
    if (!current_block_ || !arena_end_) {
        return *this;
    }

    void *old = current_block_;
    block_view current(current_block_);
    current_block_ = byte_ptr(current_block_) + block_view::metadata_size + current.size();
    if (next_free_block_ == old) {
        next_free_block_ = block_view(next_free_block_).next();
    }

    if (byte_ptr(current_block_) >= arena_end_) {
        current_block_   = nullptr;
        next_free_block_ = nullptr;
        return *this;
    }

    while (next_free_block_ && byte_ptr(next_free_block_) < byte_ptr(current_block_)) {
        next_free_block_ = block_view(next_free_block_).next();
    }

    return *this;
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::sorted_iterator::operator++(int) {
    auto copy = *this;

    ++(*this);

    return copy;
}

size_t allocator_sorted_list::sorted_iterator::size() const noexcept {
    if (!current_block_) {
        return 0;
    }

    return block_view(current_block_).size();
}

bool allocator_sorted_list::sorted_iterator::occupied() const noexcept {
    return current_block_ && current_block_ != next_free_block_;
}

void *allocator_sorted_list::sorted_iterator::operator*() const noexcept {
    return current_block_;
}

allocator_sorted_list::allocator_sorted_list(size_t space_size,
                                             std::pmr::memory_resource *parent_allocator,
                                             allocator_with_fit_mode::fit_mode allocate_fit_mode)
    : memory_(create_storage(space_size, parent_allocator, allocate_fit_mode)) {}

allocator_sorted_list::allocator_sorted_list(const allocator_sorted_list &other)
        : memory_(nullptr) {
    if (!other.memory_) {
        return;
    }

    auto *other_memory = const_cast<void *>(other.memory_);

    control_block_view source(other_memory);

    std::lock_guard lock(source.mutex());

    memory_ = create_storage(source.space_size(), source.parent_allocator(), source.fit_mode());

    control_block_view target(memory_);

    std::memcpy(target.arena_begin(), source.arena_begin(), source.space_size());

    std::vector<size_t> free_offsets;
    for (auto iterator = sorted_free_iterator(source.free_head()); iterator != sorted_free_iterator(); ++iterator) {
        free_offsets.push_back(static_cast<size_t>(byte_ptr(*iterator) - source.arena_begin()));
    }

    if (free_offsets.empty()) {
        target.free_head() = nullptr;

        return;
    }

    target.free_head() = target.arena_begin() + free_offsets.front();
    for (size_t i = 0; i < free_offsets.size(); ++i) {
        auto current_offset = target.arena_begin() + free_offsets[i];
        auto next_offset = target.arena_begin() + free_offsets[i + 1];

        block_view(current_offset).next() = i + 1 < free_offsets.size() ? static_cast<void *>(next_offset) : nullptr;
    }
}

allocator_sorted_list::allocator_sorted_list(allocator_sorted_list &&other) noexcept
        : memory_(other.memory_) {
    other.memory_ = nullptr;
}

allocator_sorted_list::~allocator_sorted_list() {
    destroy_storage(memory_);
}

[[nodiscard]] void *allocator_sorted_list::do_allocate_sm(size_t size) {
    control_block_view control(memory_);

    std::lock_guard lock(control.mutex());

    sorted_free_iterator selected = free_end();
    sorted_free_iterator selected_prev = free_end();
    sorted_free_iterator prev = free_end();

    for (auto iterator = free_begin(); iterator != free_end(); prev = iterator, ++iterator) {
        const auto current_size = iterator.size();

        if (current_size < size) {
            continue;
        }

        bool is_better = false;

        is_better |= selected == free_end();
        is_better |= control.fit_mode() == fit_mode::first_fit;
        is_better |= control.fit_mode() == fit_mode::the_best_fit && current_size < selected.size();
        is_better |= control.fit_mode() == fit_mode::the_worst_fit && current_size > selected.size();

        if (!is_better) {
            continue;
        }

        selected = iterator;
        selected_prev = prev;

        if (control.fit_mode() == fit_mode::first_fit) {
            break;
        }
    }

    if (selected == free_end()) {
        throw std::bad_alloc();
    }

    block_view selected_block(*selected);
    void *next = selected_block.next();
    const auto selected_size = selected_block.size();

    if (selected_size >= size + block_view::metadata_size + 1) {
        block_view remaining(byte_ptr(*selected) + block_view::metadata_size + size);
        remaining.next() = next;
        remaining.size() = selected_size - size - block_view::metadata_size;

        selected_block.size() = size;
        selected_block.next() = nullptr;

        if (selected_prev != free_end()) {
            block_view(*selected_prev).next() = remaining.raw();
        } else {
            control.free_head() = remaining.raw();
        }
    } else {
        if (selected_prev != free_end()) {
            block_view(*selected_prev).next() = next;
        } else {
            control.free_head() = next;
        }
    }

    return selected_block.payload();
}

void allocator_sorted_list::do_deallocate_sm(void *at) {
    if (!at) {
        return;
    }

    control_block_view control(memory_);

    std::lock_guard lock(control.mutex());

    block_view block = block_view::from_payload(at);

    sorted_free_iterator prev_iterator = free_end();
    sorted_free_iterator current_iterator = free_begin();

    while (current_iterator != free_end() && byte_ptr(*current_iterator) < byte_ptr(block.raw())) {
        prev_iterator = current_iterator;
        ++current_iterator;
    }

    void *current_ptr = (current_iterator != free_end()) ? *current_iterator : nullptr;
    void *prev_ptr = (prev_iterator != free_end()) ? *prev_iterator : nullptr;

    block.next() = current_ptr;

    if (prev_ptr) {
        block_view(prev_ptr).next() = block.raw();
    } else {
        control.free_head() = block.raw();
    }

    if (current_ptr && block.is_adjacent_to(block_view(current_ptr))) {
        block.size() += block_view::metadata_size + block_view(current_ptr).size();
        block.next() = block_view(current_ptr).next();
    }

    if (prev_ptr && block_view(prev_ptr).is_adjacent_to(block)) {
        block_view prev(prev_ptr);

        prev.size() += block_view::metadata_size + block.size();
        prev.next() = block.next();
    }
}

bool allocator_sorted_list::do_is_equal(const std::pmr::memory_resource &other) const noexcept {
    return typeid(*this) == typeid(other);
}

inline void allocator_sorted_list::set_fit_mode(allocator_with_fit_mode::fit_mode mode) {
    control_block_view control(memory_);

    std::lock_guard lock(control.mutex());

    control.fit_mode() = mode;
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept {
    control_block_view control(memory_);

    std::lock_guard lock(control.mutex());

    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info_inner() const {
    std::vector<allocator_test_utils::block_info> result;

    for (auto iterator = begin(); iterator != end(); ++iterator) {
        result.push_back({iterator.size(), iterator.occupied()});
    }

    return result;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_begin() const noexcept {
    return sorted_free_iterator(control_block_view(memory_).free_head());
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_end() const noexcept {
    return {};
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::begin() const noexcept {
    return sorted_iterator(memory_);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::end() const noexcept {
    return {};
}

allocator_sorted_list &allocator_sorted_list::operator=(const allocator_sorted_list &other) {
    if (this == &other) {
        return *this;
    }

    allocator_sorted_list copy(other);

    *this = std::move(copy);

    return *this;
}

allocator_sorted_list &allocator_sorted_list::operator=(allocator_sorted_list &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy_storage(memory_);

    memory_       = other.memory_;
    other.memory_ = nullptr;

    return *this;
}
