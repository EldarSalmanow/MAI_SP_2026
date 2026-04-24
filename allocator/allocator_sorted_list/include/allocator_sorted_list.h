#ifndef MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_SORTED_LIST_H
#define MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_SORTED_LIST_H

#include <pp_allocator.h>
#include <allocator_test_utils.h>
#include <allocator_with_fit_mode.h>
#include <cstdint>
#include <iterator>


class allocator_sorted_list final :
        public smart_mem_resource,
        public allocator_test_utils,
        public allocator_with_fit_mode {
private:
    class sorted_free_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = void *;
        using reference         = void *&;
        using pointer           = void **;
        using difference_type   = ptrdiff_t;

        sorted_free_iterator() noexcept;
        explicit sorted_free_iterator(void *free_block) noexcept;

        size_t size() const noexcept;

        bool operator==(const sorted_free_iterator &other) const noexcept;
        bool operator!=(const sorted_free_iterator &other) const noexcept;
        sorted_free_iterator &operator++() noexcept;
        sorted_free_iterator operator++(int);

        void *operator*() const noexcept;

    private:
        void *free_block_;
    };

    class sorted_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = void *;
        using reference         = void *&;
        using pointer           = void **;
        using difference_type   = ptrdiff_t;

        sorted_iterator() noexcept;
        explicit sorted_iterator(void *memory) noexcept;

        size_t size() const noexcept;
        bool occupied() const noexcept;

        bool operator==(const sorted_iterator &other) const noexcept;
        bool operator!=(const sorted_iterator &other) const noexcept;
        sorted_iterator &operator++() noexcept;
        sorted_iterator operator++(int);

        void *operator*() const noexcept;

    private:
        void *current_block_;
        void *next_free_block_;
        std::uint8_t *arena_end_;
    };

public:
    explicit allocator_sorted_list(size_t space_size,
                                   std::pmr::memory_resource *parent_allocator         = nullptr,
                                   allocator_with_fit_mode::fit_mode allocate_fit_mode =
                                           allocator_with_fit_mode::fit_mode::first_fit);

    allocator_sorted_list(allocator_sorted_list const &other);

    allocator_sorted_list(allocator_sorted_list &&other) noexcept;

public:
    ~allocator_sorted_list() override;

private:
    [[nodiscard]] void *do_allocate_sm(size_t size) override;

    void do_deallocate_sm(void *at) override;

    bool do_is_equal(const std::pmr::memory_resource &) const noexcept override;

    inline void set_fit_mode(allocator_with_fit_mode::fit_mode mode) override;

    std::vector<allocator_test_utils::block_info> get_blocks_info() const noexcept override;

private:
    std::vector<allocator_test_utils::block_info> get_blocks_info_inner() const override;

    sorted_free_iterator free_begin() const noexcept;
    sorted_free_iterator free_end() const noexcept;

    sorted_iterator begin() const noexcept;
    sorted_iterator end() const noexcept;

public:
    allocator_sorted_list &operator=(allocator_sorted_list const &other);

    allocator_sorted_list &operator=(allocator_sorted_list &&other) noexcept;

private:
    void *memory_;
};

#endif //MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_SORTED_LIST_H
