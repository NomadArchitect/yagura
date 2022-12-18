#pragma once

#include <common/extra.h>
#include <kernel/api/sys/stat.h>
#include <kernel/api/sys/types.h>
#include <kernel/forward.h>
#include <kernel/lock.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#define ROOT_DIR PATH_SEPARATOR_STR

#define OPEN_MAX 1024

typedef struct file_description {
    mutex offset_lock;
    struct inode* inode;
    atomic_int flags;
    off_t offset;
    void* private_data;
    atomic_size_t ref_count;
} file_description;

typedef struct file_descriptor_table {
    file_description** entries;
} file_descriptor_table;

NODISCARD int file_descriptor_table_init(file_descriptor_table*);
void file_descriptor_table_destroy(file_descriptor_table*);
NODISCARD int
file_descriptor_table_clone_from(file_descriptor_table* to,
                                 const file_descriptor_table* from);

typedef void (*destroy_inode_fn)(struct inode*);

typedef struct inode* (*lookup_child_fn)(struct inode*, const char* name);
typedef struct inode* (*create_child_fn)(struct inode*, const char* name,
                                         mode_t mode);
typedef int (*link_child_fn)(struct inode*, const char* name,
                             struct inode* child);
typedef struct inode* (*unlink_child_fn)(struct inode*, const char* name);
typedef int (*stat_fn)(struct inode*, struct stat* buf);

typedef int (*open_fn)(file_description*, int flags, mode_t mode);
typedef int (*close_fn)(file_description*);
typedef ssize_t (*read_fn)(file_description*, void* buffer, size_t count);
typedef ssize_t (*write_fn)(file_description*, const void* buffer,
                            size_t count);
typedef uintptr_t (*mmap_fn)(file_description*, uintptr_t addr, size_t length,
                             off_t offset, uint16_t page_flags);
typedef int (*truncate_fn)(file_description*, off_t length);
typedef int (*ioctl_fn)(file_description*, int request, void* argp);

struct getdents_ctx;
typedef bool (*getdents_callback_fn)(struct getdents_ctx*, const char* name,
                                     uint8_t type);
typedef int (*getdents_fn)(struct getdents_ctx*, file_description*,
                           getdents_callback_fn callback);

typedef struct file_ops {
    destroy_inode_fn destroy_inode;

    lookup_child_fn lookup_child;
    create_child_fn create_child;
    link_child_fn link_child;
    unlink_child_fn unlink_child;
    open_fn open;
    stat_fn stat;

    close_fn close;
    read_fn read;
    write_fn write;
    mmap_fn mmap;
    truncate_fn truncate;
    ioctl_fn ioctl;
    getdents_fn getdents;
} file_ops;

struct inode {
    struct inode* fs_root_inode;
    file_ops* fops;
    dev_t device_id;
    _Atomic(unix_socket*) bound_socket;
    _Atomic(nlink_t) num_links;
    atomic_size_t ref_count;
    mode_t mode;
};

void inode_ref(struct inode*);
void inode_unref(struct inode*);

void inode_destroy(struct inode*);
NODISCARD struct inode* inode_lookup_child(struct inode*, const char* name);
NODISCARD struct inode* inode_create_child(struct inode*, const char* name,
                                           mode_t mode);
NODISCARD int inode_link_child(struct inode*, const char* name,
                               struct inode* child);
NODISCARD int inode_unlink_child(struct inode*, const char* name);
NODISCARD file_description* inode_open(struct inode*, int flags, mode_t mode);
NODISCARD int inode_stat(struct inode*, struct stat* buf);

int file_description_close(file_description*);
NODISCARD ssize_t file_description_read(file_description*, void* buffer,
                                        size_t count);
NODISCARD ssize_t file_description_write(file_description*, const void* buffer,
                                         size_t count);
NODISCARD uintptr_t file_description_mmap(file_description*, uintptr_t addr,
                                          size_t length, off_t offset,
                                          uint16_t page_flags);
NODISCARD int file_description_truncate(file_description*, off_t length);
NODISCARD off_t file_description_lseek(file_description*, off_t offset,
                                       int whence);
NODISCARD int file_description_ioctl(file_description*, int request,
                                     void* argp);
NODISCARD long file_description_getdents(file_description*, void* dirp,
                                         unsigned int count);

NODISCARD int file_description_block(file_description*,
                                     bool (*should_unblock)(file_description*));

NODISCARD int vfs_mount(const char* path, struct inode* fs_root);
struct inode* vfs_get_root(void);
NODISCARD int vfs_register_device(struct inode* device);
NODISCARD file_description* vfs_open(const char* pathname, int flags,
                                     mode_t mode);
NODISCARD int vfs_stat(const char* pathname, struct stat* buf);
NODISCARD struct inode* vfs_create(const char* pathname, mode_t mode);
char* vfs_canonicalize_path(const char* pathname);
struct inode* vfs_resolve_path(const char* pathname, struct inode** out_parent,
                               char** out_basename);

uint8_t mode_to_dirent_type(mode_t);

struct inode* fifo_create(void);

void initrd_populate_root_fs(uintptr_t physical_addr, size_t size);

struct inode* tmpfs_create_root(void);
struct inode* procfs_create_root(void);
