#pragma once

#include <common/extra.h>
#include <kernel/forward.h>
#include <kernel/lock.h>
#include <stddef.h>
#include <stdint.h>

// kernel heap starts right after the quickmap page
#define KERNEL_HEAP_START (KERNEL_VADDR + 1024 * PAGE_SIZE)

// last 4MiB is for recursive mapping
#define KERNEL_HEAP_END 0xffc00000

typedef struct range_allocator {
    uintptr_t start;
    uintptr_t end;
    struct range* ranges;
    mutex lock;
} range_allocator;

NODISCARD int range_allocator_init(range_allocator* allocator, uintptr_t start,
                                   uintptr_t end);
uintptr_t range_allocator_alloc(range_allocator* allocator, size_t size);
NODISCARD int range_allocator_free(range_allocator* allocator, uintptr_t addr,
                                   size_t size);

extern range_allocator kernel_vaddr_allocator;

#define PAGE_WRITE 0x2
#define PAGE_USER 0x4
#define PAGE_PAT 0x80
#define PAGE_GLOBAL 0x100

// we use an unused bit in page table entries to indicate the page should be
// linked, not copied, when cloning a page directory
#define PAGE_SHARED 0x200

void paging_init(const multiboot_info_t*);

uintptr_t paging_virtual_to_physical_addr(uintptr_t virtual_addr);

page_directory* paging_current_page_directory(void);
page_directory* paging_create_page_directory(void);
page_directory* paging_clone_current_page_directory(void);
void paging_destroy_current_page_directory(void);
void paging_switch_page_directory(page_directory* pd);

NODISCARD int paging_map_to_free_pages(uintptr_t virtual_addr, uintptr_t size,
                                       uint16_t flags);
NODISCARD int paging_map_to_physical_range(uintptr_t virtual_addr,
                                           uintptr_t physical_addr,
                                           uintptr_t size, uint16_t flags);
NODISCARD int paging_copy_mapping(uintptr_t to_virtual_addr,
                                  uintptr_t from_virtual_addr, uintptr_t size,
                                  uint16_t flags);
void paging_unmap(uintptr_t virtual_addr, uintptr_t size);

void* kmalloc(size_t size);
void* kaligned_alloc(size_t alignment, size_t size);
void* krealloc(void* ptr, size_t new_size);
void kfree(void* ptr);

char* kstrdup(const char*);
char* kstrndup(const char*, size_t n);

struct physical_memory_info {
    size_t total;
    size_t free;
};

void page_allocator_init(const multiboot_info_t* mb_info);
uintptr_t page_allocator_alloc(void);
void page_allocator_ref_page(uintptr_t physical_addr);
void page_allocator_unref_page(uintptr_t physical_addr);
void page_allocator_get_info(struct physical_memory_info* out_memory_info);
