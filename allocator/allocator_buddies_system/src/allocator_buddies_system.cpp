#include <cstring>
#include <new>
#include <typeinfo>
#include <stdexcept>
#include "../include/allocator_buddies_system.h"


std::uint8_t *byte_ptr(void *ptr) noexcept {
    return reinterpret_cast<std::uint8_t *>(ptr);
}

class control_block {
    void *memory_;

public:
    static constexpr size_t parent_offset = 0;
    static constexpr size_t fit_mode_offset = parent_offset + sizeof(std::pmr::memory_resource *);
    static constexpr size_t power_offset = fit_mode_offset + sizeof(allocator_with_fit_mode::fit_mode);
    static constexpr size_t mutex_offset = power_offset + sizeof(unsigned char);
    static constexpr size_t metadata_size = mutex_offset + sizeof(std::mutex);

    explicit control_block(void *memory) noexcept : memory_(memory) {}

    std::pmr::memory_resource *&parent() const noexcept {
        return *reinterpret_cast<std::pmr::memory_resource **>(byte_ptr(memory_) + parent_offset);
    }

    allocator_with_fit_mode::fit_mode &fit_mode() const noexcept {
        return *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(byte_ptr(memory_) + fit_mode_offset);
    }

    unsigned char &power() const noexcept {
        return *reinterpret_cast<unsigned char *>(byte_ptr(memory_) + power_offset);
    }

    std::mutex &mutex() const noexcept {
        return *reinterpret_cast<std::mutex *>(byte_ptr(memory_) + mutex_offset);
    }

    std::uint8_t *arena() const noexcept {
        return byte_ptr(memory_) + metadata_size;
    }

    size_t arena_size() const noexcept {
        return static_cast<size_t>(1) << power();
    }
};

class block_header {
public:
    explicit block_header(void *block) noexcept
            : block_(block) {}

    static block_header from_payload(void *payload) noexcept {
        return block_header(byte_ptr(payload) - allocator_buddies_system::occupied_block_metadata_size);
    }

    allocator_buddies_system::block_metadata &metadata() const noexcept {
        return *static_cast<allocator_buddies_system::block_metadata *>(block_);
    }

    bool occupied() const noexcept {
        return metadata().occupied;
    }

    void set_occupied(bool value) noexcept {
        metadata().occupied = value;
    }
    
    unsigned char power() const noexcept {
        return metadata().size;
    }
    
    void set_power(unsigned char value) noexcept {
        metadata().size = value;
    }
    
    void *raw() const noexcept {
        return block_;
    }

    void *payload() const noexcept {
        return byte_ptr(block_) + allocator_buddies_system::occupied_block_metadata_size;
    }

private:
    void *block_;
};

void destroy_allocator(void *&memory) noexcept {
    if (!memory) {
        return;
    }

    control_block control(memory);

    control.mutex().~mutex();
    control.parent()->deallocate(memory, control_block::metadata_size + control.arena_size());

    memory = nullptr;
}

allocator_buddies_system::allocator_buddies_system(
    size_t space_size,
    std::pmr::memory_resource *parent_allocator,
    allocator_with_fit_mode::fit_mode allocate_fit_mode) {
    if (space_size < (static_cast<size_t>(1) << min_k)) {
        throw std::logic_error("Space size is too small");
    }

    if (!parent_allocator) {
        parent_allocator = std::pmr::get_default_resource();
    }

    unsigned char power = __detail::nearest_greater_k_of_2(space_size);
    size_t actual_size = static_cast<size_t>(1) << power;

    _trusted_memory = parent_allocator->allocate(control_block::metadata_size + actual_size);

    control_block control(_trusted_memory);
    control.parent() = parent_allocator;
    control.fit_mode() = allocate_fit_mode;
    control.power() = power;
    new(&control.mutex()) std::mutex();

    block_header first(control.arena());
    first.set_occupied(false);
    first.set_power(power);
}

allocator_buddies_system::allocator_buddies_system(const allocator_buddies_system &other)
    : _trusted_memory(nullptr) {
    if (!other._trusted_memory) {
        return;
    }

    control_block source(other._trusted_memory);

    std::lock_guard<std::mutex> lock(source.mutex());

    auto *parent = source.parent();
    size_t total_size = control_block::metadata_size + source.arena_size();

    _trusted_memory = parent->allocate(total_size);

    control_block target(_trusted_memory);
    target.parent() = parent;
    target.fit_mode() = source.fit_mode();
    target.power() = source.power();
    new(&target.mutex()) std::mutex();

    std::memcpy(target.arena(), source.arena(), source.arena_size());
}

allocator_buddies_system &allocator_buddies_system::operator=(const allocator_buddies_system &other) {
    if (this == &other) {
        return *this;
    }

    auto copy = allocator_buddies_system(other);

    *this = std::move(copy);

    return *this;
}

allocator_buddies_system::allocator_buddies_system(allocator_buddies_system &&other) noexcept
    : _trusted_memory(other._trusted_memory) {
    other._trusted_memory = nullptr;
}

allocator_buddies_system &allocator_buddies_system::operator=(allocator_buddies_system &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy_allocator(_trusted_memory);

    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    return *this;
}

allocator_buddies_system::~allocator_buddies_system() {
    destroy_allocator(_trusted_memory);
}

[[nodiscard]] void *allocator_buddies_system::do_allocate_sm(size_t size) {
    control_block control(_trusted_memory);
    std::lock_guard<std::mutex> lock(control.mutex());

    size_t required_power = __detail::nearest_greater_k_of_2(size + occupied_block_metadata_size);

    if (required_power < min_k) {
        required_power = min_k;
    }

    void *best_block = nullptr;
    unsigned char best_power = 0;

    for (auto iterator = begin(); iterator != end(); ++iterator) {
        if (iterator.occupied()) {
            continue;
        }

        unsigned char block_power = block_header(*iterator).power();

        if (block_power < required_power) {
            continue;
        }

        bool is_better = false;

        is_better |= !best_block;
        is_better |= control.fit_mode() == fit_mode::first_fit;
        is_better |= control.fit_mode() == fit_mode::the_best_fit && block_power < best_power;
        is_better |= control.fit_mode() == fit_mode::the_worst_fit && block_power > best_power;

        if (!is_better) {
            continue;
        }

        best_block = *iterator;
        best_power = block_power;

        if (control.fit_mode() == fit_mode::first_fit) {
            break;
        }
    }

    if (!best_block) {
        throw std::bad_alloc();
    }

    while (best_power > required_power) {
        --best_power;
        size_t half_size = static_cast<size_t>(1) << best_power;

        block_header left(best_block);
        left.set_power(best_power);

        block_header right(byte_ptr(best_block) + half_size);
        right.set_occupied(false);
        right.set_power(best_power);
    }

    block_header(best_block).set_occupied(true);

    return block_header(best_block).payload();
}

void allocator_buddies_system::do_deallocate_sm(void *at) {
    if (!at) {
        return;
    }

    control_block control(_trusted_memory);
    
    std::lock_guard<std::mutex> lock(control.mutex());

    block_header block = block_header::from_payload(at);
    block.set_occupied(false);

    std::uint8_t *arena = control.arena();

    while (block.power() < control.power()) {
        size_t block_size = static_cast<size_t>(1) << block.power();
        size_t block_offset = byte_ptr(block.raw()) - arena;
        size_t buddy_offset = block_offset ^ block_size;

        if (buddy_offset >= control.arena_size()) {
            break;
        }

        block_header buddy(arena + buddy_offset);

        if (buddy.occupied() || buddy.power() != block.power()) {
            break;
        }

        void *merged = (block_offset < buddy_offset) ? block.raw() : buddy.raw();
        block = block_header(merged);
        block.set_occupied(false);
        block.set_power(block.power() + 1);
    }
}

bool allocator_buddies_system::do_is_equal(const std::pmr::memory_resource &other) const noexcept {
    return typeid(*this) == typeid(other);
}

void allocator_buddies_system::set_fit_mode(allocator_with_fit_mode::fit_mode mode) {
    control_block control(_trusted_memory);

    std::lock_guard<std::mutex> lock(control.mutex());
    
    control.fit_mode() = mode;
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept {
    control_block control(_trusted_memory);
    
    std::lock_guard<std::mutex> lock(control.mutex());
    
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const {
    std::vector<allocator_test_utils::block_info> result;

    for (auto iterator = begin(); iterator != end(); ++iterator) {
        result.push_back({iterator.size(), iterator.occupied()});
    }
    
    return result;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept {
    return buddy_iterator(_trusted_memory);
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept {
    return buddy_iterator();
}

allocator_buddies_system::buddy_iterator::buddy_iterator()
    : _block(nullptr), _memory(nullptr) {}

allocator_buddies_system::buddy_iterator::buddy_iterator(void *memory)
    : _block(memory ? control_block(memory).arena() : nullptr), _memory(memory) {}

bool allocator_buddies_system::buddy_iterator::operator==(const buddy_iterator &other) const noexcept {
    return _block == other._block;
}

bool allocator_buddies_system::buddy_iterator::operator!=(const buddy_iterator &other) const noexcept {
    return !(*this == other);
}

allocator_buddies_system::buddy_iterator &allocator_buddies_system::buddy_iterator::operator++() & noexcept {
    if (!_block || !_memory) {
        return *this;
    }

    control_block control(_memory);
    size_t block_size = static_cast<size_t>(1) << block_header(_block).power();
    std::uint8_t *next_block = byte_ptr(_block) + block_size;
    std::uint8_t *arena_end = control.arena() + control.arena_size();

    _block = (next_block >= arena_end) ? nullptr : next_block;

    return *this;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int) {
    auto copy = *this;

    ++(*this);
    
    return copy;
}

size_t allocator_buddies_system::buddy_iterator::size() const noexcept {
    if (!_block) {
        return 0;
    }

    return static_cast<size_t>(1) << block_header(_block).power();
}

bool allocator_buddies_system::buddy_iterator::occupied() const noexcept {
    return _block && block_header(_block).occupied();
}

void *allocator_buddies_system::buddy_iterator::operator*() const noexcept {
    return _block;
}
