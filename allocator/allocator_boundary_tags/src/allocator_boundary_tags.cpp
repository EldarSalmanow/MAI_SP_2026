#include <cstring>
#include <new>
#include <typeinfo>
#include "../include/allocator_boundary_tags.h"

std::uint8_t *byte_ptr(void *ptr) noexcept {
    return reinterpret_cast<std::uint8_t *>(ptr);
}

const std::uint8_t *byte_ptr(const void *ptr) noexcept {
    return reinterpret_cast<const std::uint8_t *>(ptr);
}

class block_view {
public:
    static constexpr size_t prev_offset = 0;
    static constexpr size_t next_offset = sizeof(void *);
    static constexpr size_t prev_free_offset = sizeof(void *) * 2;
    static constexpr size_t size_offset = sizeof(void *) * 3;
    static constexpr size_t metadata_size = sizeof(void *) * 3 + sizeof(size_t);

public:
    explicit block_view(void *memory) noexcept : memory_(memory) {}

    static block_view from_payload(void *payload) noexcept {
        return block_view(byte_ptr(payload) - metadata_size);
    }

    void *raw() const noexcept {
        return memory_;
    }

    void *&prev() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + prev_offset);
    }

    void *&next() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + next_offset);
    }

    void *&prev_free() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + prev_free_offset);
    }

    size_t &size() const noexcept {
        return *reinterpret_cast<size_t *>(byte_ptr(memory_) + size_offset);
    }

    void *payload() const noexcept {
        return byte_ptr(memory_) + metadata_size;
    }

    bool is_occupied() const noexcept {
        return prev_free() != nullptr;
    }

    void mark_occupied() const noexcept {
        prev_free() = reinterpret_cast<void *>(0x1);
    }

    void mark_free() const noexcept {
        prev_free() = nullptr;
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
    static constexpr size_t first_block_offset = mutex_offset + sizeof(std::mutex);
    static constexpr size_t metadata_size = first_block_offset + sizeof(void *);

public:
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

    void *&first_block() const noexcept {
        return *reinterpret_cast<void **>(byte_ptr(memory_) + first_block_offset);
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
    first.prev() = nullptr;
    first.next() = nullptr;
    first.mark_free();
    first.size() = space_size - block_view::metadata_size;
    control.first_block() = first.raw();

    return memory;
}

allocator_boundary_tags::boundary_iterator::boundary_iterator()
    : _occupied_ptr(nullptr), _occupied(false), _trusted_memory(nullptr) {}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void *trusted)
    : _occupied_ptr(nullptr), _occupied(false), _trusted_memory(trusted) {
    if (trusted) {
        control_block_view control(trusted);
        _occupied_ptr = control.first_block();
        if (_occupied_ptr) {
            _occupied = block_view(_occupied_ptr).is_occupied();
        }
    }
}

bool allocator_boundary_tags::boundary_iterator::operator==(const boundary_iterator &other) const noexcept {
    return _occupied_ptr == other._occupied_ptr;
}

bool allocator_boundary_tags::boundary_iterator::operator!=(const boundary_iterator &other) const noexcept {
    return !(*this == other);
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator++() & noexcept {
    if (_occupied_ptr) {
        block_view current(_occupied_ptr);
        _occupied_ptr = current.next();
        if (_occupied_ptr) {
            _occupied = block_view(_occupied_ptr).is_occupied();
        }
    }
    return *this;
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator--() & noexcept {
    if (_occupied_ptr) {
        block_view current(_occupied_ptr);
        _occupied_ptr = current.prev();
        if (_occupied_ptr) {
            _occupied = block_view(_occupied_ptr).is_occupied();
        }
    }
    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int) {
    auto copy = *this;
    ++(*this);
    return copy;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int) {
    auto copy = *this;
    --(*this);
    return copy;
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept {
    if (!_occupied_ptr) {
        return 0;
    }
    return block_view(_occupied_ptr).size();
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept {
    return _occupied;
}

void *allocator_boundary_tags::boundary_iterator::operator*() const noexcept {
    return _occupied_ptr;
}

void *allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept {
    return _occupied_ptr;
}

allocator_boundary_tags::allocator_boundary_tags(size_t space_size,
                                                 std::pmr::memory_resource *parent_allocator,
                                                 allocator_with_fit_mode::fit_mode allocate_fit_mode)
    : _trusted_memory(create_storage(space_size, parent_allocator, allocate_fit_mode)) {}

allocator_boundary_tags::allocator_boundary_tags(const allocator_boundary_tags &other)
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

    std::ptrdiff_t offset = target.arena_begin() - source.arena_begin();

    for (auto it = boundary_iterator(_trusted_memory); it != boundary_iterator(); ++it) {
        block_view block(*it);
        if (block.prev()) {
            block.prev() = byte_ptr(block.prev()) + offset;
        }
        if (block.next()) {
            block.next() = byte_ptr(block.next()) + offset;
        }
    }

    if (target.first_block()) {
        target.first_block() = byte_ptr(target.first_block()) + offset;
    }
}

allocator_boundary_tags::allocator_boundary_tags(allocator_boundary_tags &&other) noexcept
    : _trusted_memory(other._trusted_memory) {
    other._trusted_memory = nullptr;
}

allocator_boundary_tags::~allocator_boundary_tags() {
    destroy_storage(_trusted_memory);
}

[[nodiscard]] void *allocator_boundary_tags::do_allocate_sm(size_t size) {
    control_block_view control(_trusted_memory);

    std::lock_guard lock(control.mutex());

    boundary_iterator selected = end();

    for (auto it = begin(); it != end(); ++it) {
        if (it.occupied()) {
            continue;
        }

        const auto current_size = it.size();

        if (current_size < size) {
            continue;
        }

        bool is_better = false;

        is_better |= selected == end();
        is_better |= control.fit_mode() == fit_mode::first_fit;
        is_better |= control.fit_mode() == fit_mode::the_best_fit && current_size < selected.size();
        is_better |= control.fit_mode() == fit_mode::the_worst_fit && current_size > selected.size();

        if (!is_better) {
            continue;
        }

        selected = it;

        if (control.fit_mode() == fit_mode::first_fit) {
            break;
        }
    }

    if (selected == end()) {
        throw std::bad_alloc();
    }

    block_view selected_block(*selected);
    const auto selected_size = selected_block.size();

    if (selected_size >= size + block_view::metadata_size + 1) {
        block_view remaining(byte_ptr(*selected) + block_view::metadata_size + size);
        remaining.prev() = selected_block.raw();
        remaining.next() = selected_block.next();
        remaining.mark_free();
        remaining.size() = selected_size - size - block_view::metadata_size;

        if (selected_block.next()) {
            block_view(selected_block.next()).prev() = remaining.raw();
        }

        selected_block.next() = remaining.raw();
        selected_block.size() = size;
    }

    selected_block.mark_occupied();

    return selected_block.payload();
}

void allocator_boundary_tags::do_deallocate_sm(void *at) {
    if (!at) {
        return;
    }

    control_block_view control(_trusted_memory);

    std::lock_guard lock(control.mutex());

    block_view block = block_view::from_payload(at);
    block.mark_free();

    void *prev_ptr = block.prev();
    void *next_ptr = block.next();

    if (next_ptr && !block_view(next_ptr).is_occupied()) {
        block_view next_block(next_ptr);
        block.size() += block_view::metadata_size + next_block.size();
        block.next() = next_block.next();

        if (next_block.next()) {
            block_view(next_block.next()).prev() = block.raw();
        }
    }

    if (prev_ptr && !block_view(prev_ptr).is_occupied()) {
        block_view prev_block(prev_ptr);
        prev_block.size() += block_view::metadata_size + block.size();
        prev_block.next() = block.next();

        if (block.next()) {
            block_view(block.next()).prev() = prev_block.raw();
        }
    }
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource &other) const noexcept {
    return typeid(*this) == typeid(other);
}

inline void allocator_boundary_tags::set_fit_mode(allocator_with_fit_mode::fit_mode mode) {
    control_block_view control(_trusted_memory);

    std::lock_guard lock(control.mutex());

    control.fit_mode() = mode;
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const {
    control_block_view control(_trusted_memory);

    std::lock_guard lock(control.mutex());

    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const {
    std::vector<allocator_test_utils::block_info> result;

    for (auto it = begin(); it != end(); ++it) {
        result.push_back({it.size() + block_view::metadata_size, it.occupied()});
    }

    return result;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept {
    return boundary_iterator(_trusted_memory);
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept {
    return {};
}

allocator_boundary_tags &allocator_boundary_tags::operator=(const allocator_boundary_tags &other) {
    if (this == &other) {
        return *this;
    }

    allocator_boundary_tags copy(other);
    *this = std::move(copy);

    return *this;
}

allocator_boundary_tags &allocator_boundary_tags::operator=(allocator_boundary_tags &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy_storage(_trusted_memory);

    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    return *this;
}
