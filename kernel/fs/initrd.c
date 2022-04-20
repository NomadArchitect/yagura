#include "fs.h"
#include <common/extra.h>
#include <kernel/api/fcntl.h>
#include <kernel/boot_defs.h>
#include <kernel/memory/memory.h>
#include <kernel/panic.h>
#include <string.h>

struct cpio_odc_header {
    char c_magic[6];
    char c_dev[6];
    char c_ino[6];
    char c_mode[6];
    char c_uid[6];
    char c_gid[6];
    char c_nlink[6];
    char c_rdev[6];
    char c_mtime[11];
    char c_namesize[6];
    char c_filesize[11];
} __attribute__((packed));

static size_t parse_octal(const char* s, size_t len) {
    size_t res = 0;
    for (size_t i = 0; i < len; ++i) {
        res += s[i] - '0';
        if (i < len - 1)
            res *= 8;
    }
    return res;
}

#define PARSE(field) parse_octal(field, sizeof(field))

void initrd_populate_root_fs(uintptr_t paddr, size_t size) {
    uintptr_t vaddr = memory_alloc_kernel_virtual_addr_range(size);
    ASSERT_OK(vaddr);

    uintptr_t region_start = round_down(paddr, PAGE_SIZE);
    uintptr_t region_end = round_up(paddr + size, PAGE_SIZE);
    ASSERT_OK(memory_map_to_physical_range(
        vaddr, region_start, region_end - region_start, MEMORY_SHARED));

    uintptr_t cursor = vaddr + (paddr - region_start);
    for (;;) {
        const struct cpio_odc_header* header =
            (const struct cpio_odc_header*)cursor;
        ASSERT(!strncmp(header->c_magic, "070707", 6));

        size_t name_size = PARSE(header->c_namesize);
        const char* filename =
            (const char*)(cursor + sizeof(struct cpio_odc_header));
        if (!strncmp(filename, "TRAILER!!!", name_size))
            break;

        size_t mode = PARSE(header->c_mode);
        size_t rdev = PARSE(header->c_rdev);

        file_description* desc =
            vfs_open(filename, O_CREAT | O_EXCL | O_WRONLY, mode);
        ASSERT_OK(desc);

        desc->file->device_id = rdev;

        size_t file_size = PARSE(header->c_filesize);
        const unsigned char* file_content =
            (const unsigned char*)(cursor + sizeof(struct cpio_odc_header) +
                                   name_size);
        for (size_t count = 0; count < file_size;) {
            ssize_t nwritten =
                fs_write(desc, file_content + count, file_size - count);
            ASSERT_OK(nwritten);
            count += nwritten;
        }

        ASSERT_OK(fs_close(desc));
        cursor += sizeof(struct cpio_odc_header) + name_size + file_size;
    }
}
