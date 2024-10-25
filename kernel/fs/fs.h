#pragma once

#include <common/extra.h>
#include <kernel/api/sys/stat.h>
#include <kernel/api/sys/types.h>
#include <kernel/api/time.h>
#include <kernel/lock.h>
#include <stdatomic.h>
#include <stdbool.h>

#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#define ROOT_DIR PATH_SEPARATOR_STR

typedef struct multiboot_info multiboot_info_t;
typedef struct multiboot_mod_list multiboot_module_t;

// Open file description
struct file {
    struct mutex offset_lock;
    struct inode* inode;
    atomic_int flags;
    uint64_t offset;
    void* private_data;
    atomic_size_t ref_count;
};

struct kstat {
    dev_t st_dev;         /* ID of device containing file */
    ino_t st_ino;         /* Inode number */
    mode_t st_mode;       /* File type and mode */
    nlink_t st_nlink;     /* Number of hard links */
    uid_t st_uid;         /* User ID of owner */
    gid_t st_gid;         /* Group ID of owner */
    dev_t st_rdev;        /* Device ID (if special file) */
    uint64_t st_size;     /* Total size, in bytes */
    blksize_t st_blksize; /* Block size for filesystem I/O */
    blkcnt_t st_blocks;   /* Number of 512 B blocks allocated */

    struct timespec st_atim; /* Time of last access */
    struct timespec st_mtim; /* Time of last modification */
    struct timespec st_ctim; /* Time of last status change */
};

typedef bool (*getdents_callback_fn)(const char* name, uint8_t type, void* ctx);

struct file_ops {
    void (*destroy_inode)(struct inode*);

    struct inode* (*lookup_child)(struct inode*, const char* name);
    struct inode* (*create_child)(struct inode*, const char* name, mode_t mode);
    int (*link_child)(struct inode*, const char* name, struct inode* child);
    struct inode* (*unlink_child)(struct inode*, const char* name);
    int (*open)(struct file*, mode_t mode);
    int (*stat)(struct inode*, struct kstat* buf);

    int (*close)(struct file*);
    ssize_t (*read)(struct file*, void* buffer, size_t count);
    ssize_t (*write)(struct file*, const void* buffer, size_t count);
    void* (*mmap)(struct file*, size_t length, uint64_t offset, int flags);
    int (*truncate)(struct file*, uint64_t length);
    int (*ioctl)(struct file*, int request, void* user_argp);
    int (*getdents)(struct file*, getdents_callback_fn callback, void* ctx);
    short (*poll)(struct file*, short events);
};

struct inode {
    const struct file_ops* fops;
    dev_t dev;  // Device number of device containing this inode
    dev_t rdev; // Device number (if this inode is a special file)
    _Atomic(struct inode*) fifo;
    _Atomic(struct unix_socket*) bound_socket;
    mode_t mode;
    _Atomic(nlink_t) num_links;
    atomic_size_t ref_count;
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
NODISCARD struct file* inode_open(struct inode*, int flags, mode_t mode);
NODISCARD int inode_stat(struct inode*, struct kstat* buf);

int file_close(struct file*);
NODISCARD ssize_t file_read(struct file*, void* buffer, size_t count);
NODISCARD ssize_t file_read_to_end(struct file*, void* buffer, size_t count);
NODISCARD ssize_t file_write(struct file*, const void* buffer, size_t count);
NODISCARD ssize_t file_write_all(struct file*, const void* buffer,
                                 size_t count);
NODISCARD void* file_mmap(struct file*, size_t length, uint64_t offset,
                          int flags);
NODISCARD int file_truncate(struct file*, uint64_t length);
NODISCARD loff_t file_seek(struct file*, loff_t offset, int whence);
NODISCARD int file_ioctl(struct file*, int request, void* user_argp);
NODISCARD int file_getdents(struct file*, getdents_callback_fn, void* ctx);
NODISCARD short file_poll(struct file*, short events);

NODISCARD int file_block(struct file*, bool (*unblock)(struct file*),
                         int flags);

void vfs_init(const multiboot_module_t* initrd_mod);

struct path* vfs_get_root(void);

struct file_system {
    char name[16];
    struct inode* (*mount)(const char* source);
    struct file_system* next;
};

extern struct file_system* file_systems;

NODISCARD int vfs_register_file_system(struct file_system*);
NODISCARD int vfs_mount(const char* source, const char* target,
                        const char* type);
NODISCARD int vfs_mount_at(const struct path* base, const char* source,
                           const char* target, const char* type);

NODISCARD int vfs_register_device(const char* name, struct inode* device);
struct inode* vfs_get_device_by_name(const char* name);
dev_t vfs_generate_unnamed_block_device_number(void);

// Return a path even if the last component of the path does not exist.
// The last component of the returned path will have NULL inode in this case.
#define O_ALLOW_NOENT 0x4000

// When combined with O_NOFOLLOW, do not return an error if the last component
// of the path is a symbolic link, and return the symlink itself.
#define O_NOFOLLOW_NOERROR 0x2000

NODISCARD struct file* vfs_open(const char* pathname, int flags, mode_t mode);
NODISCARD struct file* vfs_open_at(const struct path* base,
                                   const char* pathname, int flags,
                                   mode_t mode);
NODISCARD int vfs_stat(const char* pathname, struct kstat* buf, int flags);
NODISCARD int vfs_stat_at(const struct path* base, const char* pathname,
                          struct kstat* buf, int flags);
NODISCARD struct inode* vfs_create(const char* pathname, mode_t mode);
NODISCARD struct inode* vfs_create_at(const struct path* base,
                                      const char* pathname, mode_t mode);

struct path* vfs_resolve_path(const char* pathname, int flags);
struct path* vfs_resolve_path_at(const struct path* base, const char* pathname,
                                 int flags);

struct inode* fifo_create(void);
