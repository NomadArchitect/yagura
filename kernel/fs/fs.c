#include "fs.h"
#include <kernel/api/dirent.h>
#include <kernel/api/err.h>
#include <kernel/api/fcntl.h>
#include <kernel/api/mman.h>
#include <kernel/api/stat.h>
#include <kernel/kmalloc.h>
#include <kernel/panic.h>
#include <string.h>

int file_descriptor_table_init(file_descriptor_table* table) {
    table->entries = kmalloc(FD_TABLE_CAPACITY * sizeof(file_description*));
    if (!table->entries)
        return -ENOMEM;

    for (size_t i = 0; i < FD_TABLE_CAPACITY; ++i)
        table->entries[i] = NULL;
    return 0;
}

int file_descriptor_table_clone_from(file_descriptor_table* to,
                                     const file_descriptor_table* from) {
    to->entries = kmalloc(FD_TABLE_CAPACITY * sizeof(file_description*));
    if (!to->entries)
        return -ENOMEM;

    memcpy(to->entries, from->entries,
           FD_TABLE_CAPACITY * sizeof(file_description*));

    for (size_t i = 0; i < FD_TABLE_CAPACITY; ++i) {
        if (from->entries[i])
            ++from->entries[i]->ref_count;
    }
    return 0;
}

struct file* fs_lookup(struct file* file, const char* name) {
    if (!file->lookup || !S_ISDIR(file->mode))
        return ERR_PTR(-ENOTDIR);
    return file->lookup(file, name);
}

struct file* fs_create_child(struct file* file, const char* name, mode_t mode) {
    if (!file->create_child || !S_ISDIR(file->mode))
        return ERR_PTR(-ENOTDIR);
    ASSERT(mode & S_IFMT);
    return file->create_child(file, name, mode);
}

file_description* fs_open(struct file* file, int flags, mode_t mode) {
    if (S_ISDIR(file->mode) && (flags & O_WRONLY))
        return ERR_PTR(-EISDIR);
    if (file->open) {
        int rc = file->open(file, flags, mode);
        if (IS_ERR(rc))
            return ERR_PTR(rc);
    }
    file_description* desc = kmalloc(sizeof(file_description));
    if (!desc)
        return ERR_PTR(-ENOMEM);
    desc->file = file;
    desc->offset = 0;
    desc->flags = flags;
    desc->ref_count = 1;
    return desc;
}

int fs_stat(struct file* file, struct stat* buf) {
    if (file->stat)
        return file->stat(file, buf);
    buf->st_rdev = file->device_id;
    buf->st_mode = file->mode;
    buf->st_size = 0;
    return 0;
}

int fs_close(file_description* desc) {
    ASSERT(desc->ref_count > 0);
    if (--desc->ref_count > 0)
        return 0;
    struct file* file = desc->file;
    if (file->close)
        return file->close(desc);
    return 0;
}

ssize_t fs_read(file_description* desc, void* buffer, size_t count) {
    struct file* file = desc->file;
    if (S_ISDIR(file->mode))
        return -EISDIR;
    if (!file->read)
        return -EINVAL;
    if (!(desc->flags & O_RDONLY))
        return -EBADF;
    return file->read(desc, buffer, count);
}

ssize_t fs_write(file_description* desc, const void* buffer, size_t count) {
    struct file* file = desc->file;
    if (S_ISDIR(file->mode))
        return -EISDIR;
    if (!file->write)
        return -EINVAL;
    if (!(desc->flags & O_WRONLY))
        return -EBADF;
    return file->write(desc, buffer, count);
}

uintptr_t fs_mmap(file_description* desc, uintptr_t vaddr, size_t length,
                  int prot, off_t offset, bool shared) {
    struct file* file = desc->file;
    if (!file->mmap)
        return -ENODEV;
    if (!(desc->flags & O_RDONLY))
        return -EACCES;
    if (shared && (prot | PROT_WRITE) && ((desc->flags & O_RDWR) != O_RDWR))
        return -EACCES;
    return file->mmap(desc, vaddr, length, prot, offset, shared);
}

int fs_truncate(file_description* desc, off_t length) {
    struct file* file = desc->file;
    if (S_ISDIR(file->mode))
        return -EISDIR;
    if (!file->truncate)
        return -EROFS;
    if (!(desc->flags & O_WRONLY))
        return -EBADF;
    return file->truncate(desc, length);
}

int fs_ioctl(file_description* desc, int request, void* argp) {
    struct file* file = desc->file;
    if (!file->ioctl)
        return -ENOTTY;
    return file->ioctl(desc, request, argp);
}

long fs_readdir(file_description* desc, void* dirp, unsigned int count) {
    struct file* file = desc->file;
    if (!file->readdir || !S_ISDIR(file->mode))
        return -ENOTDIR;
    return file->readdir(desc, dirp, count);
}

uint8_t mode_to_dirent_type(mode_t mode) {
    switch (mode & S_IFMT) {
    case S_IFDIR:
        return DT_DIR;
    case S_IFCHR:
        return DT_CHR;
    case S_IFBLK:
        return DT_BLK;
    case S_IFREG:
        return DT_REG;
    case S_IFIFO:
        return DT_FIFO;
    case S_IFLNK:
        return DT_LNK;
    case S_IFSOCK:
        return DT_SOCK;
    }
    UNREACHABLE();
}
