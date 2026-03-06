#include <not_implemented.h>
#include "../include/allocator_global_heap.h"

allocator_global_heap::allocator_global_heap() = default;

allocator_global_heap::~allocator_global_heap() = default;

[[nodiscard]] void *allocator_global_heap::do_allocate_sm(
    size_t size)
{
    std::lock_guard lock(global_heap_mutex_);

    void *pointer = new char[size];

    return pointer;
}

void allocator_global_heap::do_deallocate_sm(
    void *at)
{
    std::lock_guard lock(global_heap_mutex_);

    delete[] static_cast<char *>(at);
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return typeid(*this) == typeid(other);
}

allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
{
    return *this;
}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept
{
    return *this;
}
