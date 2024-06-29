#include "syscall.h"
#include <common/string.h>
#include <kernel/api/dirent.h>
#include <kernel/api/fcntl.h>
#include <kernel/api/sys/limits.h>
#include <kernel/fs/fs.h>
#include <kernel/fs/path.h>
#include <kernel/memory/memory.h>
#include <kernel/panic.h>
#include <kernel/process.h>
#include <kernel/safe_string.h>

static int copy_pathname_from_user(char* dest, const char* user_src) {
    ssize_t pathname_len = strncpy_from_user(dest, user_src, PATH_MAX);
    if (IS_ERR(pathname_len))
        return pathname_len;
    if (pathname_len >= PATH_MAX)
        return -ENAMETOOLONG;
    return 0;
}

int sys_open(const char* user_pathname, int flags, unsigned mode) {
    char pathname[PATH_MAX];
    int rc = copy_pathname_from_user(pathname, user_pathname);
    if (IS_ERR(rc))
        return rc;

    file_description* desc = vfs_open(pathname, flags, (mode & 0777) | S_IFREG);
    if (IS_ERR(desc))
        return PTR_ERR(desc);
    rc = process_alloc_file_descriptor(-1, desc);
    if (IS_ERR(rc))
        file_description_close(desc);
    return rc;
}

int sys_close(int fd) {
    file_description* desc = process_get_file_description(fd);
    if (IS_ERR(desc))
        return PTR_ERR(desc);

    int rc = file_description_close(desc);
    if (IS_ERR(rc))
        return rc;

    return process_free_file_descriptor(fd);
}

ssize_t sys_read(int fd, void* user_buf, size_t count) {
    if (!user_buf || !is_user_range(user_buf, count))
        return -EFAULT;
    file_description* desc = process_get_file_description(fd);
    if (IS_ERR(desc))
        return PTR_ERR(desc);
    return file_description_read(desc, user_buf, count);
}

ssize_t sys_readlink(const char* user_pathname, char* user_buf, size_t bufsiz) {
    char pathname[PATH_MAX];
    int rc = copy_pathname_from_user(pathname, user_pathname);
    if (IS_ERR(rc))
        return rc;

    struct path* path =
        vfs_resolve_path(pathname, O_NOFOLLOW | O_NOFOLLOW_NOERROR);
    if (IS_ERR(path))
        return PTR_ERR(path);

    struct inode* inode = path_into_inode(path);
    if (!S_ISLNK(inode->mode)) {
        inode_unref(inode);
        return -EINVAL;
    }

    file_description* desc = inode_open(inode, O_RDONLY, 0);
    if (IS_ERR(desc))
        return PTR_ERR(desc);

    bufsiz = MIN(bufsiz, SYMLINK_MAX);

    char buf[SYMLINK_MAX];
    size_t buf_len = 0;
    while (buf_len < bufsiz) {
        ssize_t nread = file_description_read(desc, buf + buf_len, bufsiz);
        if (IS_ERR(nread)) {
            file_description_close(desc);
            return nread;
        }
        if (nread == 0)
            break;
        buf_len += nread;
    }
    file_description_close(desc);

    if (!copy_to_user(user_buf, buf, buf_len))
        return -EFAULT;
    return buf_len;
}

ssize_t sys_write(int fd, const void* user_buf, size_t count) {
    if (!user_buf || !is_user_range(user_buf, count))
        return -EFAULT;
    file_description* desc = process_get_file_description(fd);
    if (IS_ERR(desc))
        return PTR_ERR(desc);
    return file_description_write(desc, user_buf, count);
}

int sys_ftruncate(int fd, off_t length) {
    file_description* desc = process_get_file_description(fd);
    if (IS_ERR(desc))
        return PTR_ERR(desc);
    return file_description_truncate(desc, length);
}

off_t sys_lseek(int fd, off_t offset, int whence) {
    file_description* desc = process_get_file_description(fd);
    if (IS_ERR(desc))
        return PTR_ERR(desc);
    return file_description_seek(desc, offset, whence);
}

int sys_lstat(const char* user_pathname, struct stat* user_buf) {
    char pathname[PATH_MAX];
    int rc = copy_pathname_from_user(pathname, user_pathname);
    if (IS_ERR(rc))
        return rc;
    struct stat buf;
    rc = vfs_stat(pathname, &buf, O_NOFOLLOW | O_NOFOLLOW_NOERROR);
    if (IS_ERR(rc))
        return rc;
    if (!copy_to_user(user_buf, &buf, sizeof(struct stat)))
        return -EFAULT;
    return 0;
}

int sys_stat(const char* user_pathname, struct stat* user_buf) {
    char pathname[PATH_MAX];
    int rc = copy_pathname_from_user(pathname, user_pathname);
    if (IS_ERR(rc))
        return rc;
    struct stat buf;
    rc = vfs_stat(pathname, &buf, 0);
    if (IS_ERR(rc))
        return rc;
    if (!copy_to_user(user_buf, &buf, sizeof(struct stat)))
        return -EFAULT;
    return 0;
}

int sys_symlink(const char* user_target, const char* user_linkpath) {
    char target[PATH_MAX];
    int rc = copy_pathname_from_user(target, user_target);
    if (IS_ERR(rc))
        return rc;
    size_t target_len = strlen(target);
    if (target_len > SYMLINK_MAX)
        return -ENAMETOOLONG;

    char linkpath[PATH_MAX];
    rc = copy_pathname_from_user(linkpath, user_linkpath);
    if (IS_ERR(rc))
        return rc;

    struct inode* inode = vfs_create(linkpath, S_IFLNK);
    if (IS_ERR(inode))
        return PTR_ERR(inode);

    file_description* desc = inode_open(inode, O_WRONLY, 0);
    if (IS_ERR(desc))
        return PTR_ERR(desc);
    rc = file_description_write(desc, target, target_len);

    file_description_close(desc);
    inode_unref(inode);
    return rc;
}

int sys_ioctl(int fd, int request, void* user_argp) {
    if (!is_user_address(user_argp))
        return -EFAULT;
    file_description* desc = process_get_file_description(fd);
    if (IS_ERR(desc))
        return PTR_ERR(desc);
    return file_description_ioctl(desc, request, user_argp);
}

int sys_mkdir(const char* user_pathname, mode_t mode) {
    char pathname[PATH_MAX];
    int rc = copy_pathname_from_user(pathname, user_pathname);
    if (IS_ERR(rc))
        return rc;
    struct inode* inode = vfs_create(pathname, (mode & 0777) | S_IFDIR);
    if (IS_ERR(inode))
        return PTR_ERR(inode);
    inode_unref(inode);
    return 0;
}

int sys_mknod(const char* user_pathname, mode_t mode, dev_t dev) {
    switch (mode & S_IFMT) {
    case S_IFREG:
    case S_IFCHR:
    case S_IFBLK:
    case S_IFIFO:
    case S_IFSOCK:
        break;
    default:
        return -EINVAL;
    }

    char pathname[PATH_MAX];
    int rc = copy_pathname_from_user(pathname, user_pathname);
    if (IS_ERR(rc))
        return rc;

    struct inode* inode = vfs_create(pathname, mode);
    if (IS_ERR(inode))
        return PTR_ERR(inode);
    inode->rdev = dev;
    inode_unref(inode);
    return 0;
}

int sys_mount(const mount_params* user_params) {
    mount_params params;
    if (!copy_from_user(&params, user_params, sizeof(mount_params)))
        return -EFAULT;

    char target[PATH_MAX];
    int rc = copy_pathname_from_user(target, params.target);
    if (IS_ERR(rc))
        return rc;

    char fs_type[8];
    ssize_t fs_type_len =
        strncpy_from_user(fs_type, params.filesystemtype, sizeof(fs_type));
    if (IS_ERR(fs_type_len))
        return fs_type_len;
    if ((size_t)fs_type_len >= sizeof(fs_type)) {
        // There is no file system type with such a long name.
        return -ENODEV;
    }

    if (!strcmp(fs_type, "tmpfs")) {
        return vfs_mount(target, tmpfs_create_root());
    }
    if (!strcmp(fs_type, "proc")) {
        return vfs_mount(target, procfs_create_root());
    }
    return -ENODEV;
}

int sys_link(const char* user_oldpath, const char* user_newpath) {
    char old_pathname[PATH_MAX];
    int rc = copy_pathname_from_user(old_pathname, user_oldpath);
    if (IS_ERR(rc))
        return rc;
    char new_pathname[PATH_MAX];
    rc = copy_pathname_from_user(new_pathname, user_newpath);
    if (IS_ERR(rc))
        return rc;

    struct path* old_path = vfs_resolve_path(old_pathname, 0);
    if (IS_ERR(old_path))
        return PTR_ERR(old_path);
    if (S_ISDIR(old_path->inode->mode)) {
        path_destroy_recursive(old_path);
        return -EPERM;
    }

    struct path* new_path = vfs_resolve_path(new_pathname, O_ALLOW_NOENT);
    if (IS_ERR(new_path)) {
        path_destroy_recursive(old_path);
        return PTR_ERR(new_path);
    }
    if (new_path->inode) {
        rc = -EEXIST;
        goto done;
    }
    if (!new_path->parent) {
        rc = -EPERM;
        goto done;
    }

    inode_ref(new_path->parent->inode);
    inode_ref(old_path->inode);
    rc = inode_link_child(new_path->parent->inode, new_path->basename,
                          old_path->inode);

done:
    path_destroy_recursive(new_path);
    path_destroy_recursive(old_path);
    return rc;
}

int sys_unlink(const char* user_pathname) {
    char pathname[PATH_MAX];
    int rc = copy_pathname_from_user(pathname, user_pathname);
    if (IS_ERR(rc))
        return rc;

    struct path* path = vfs_resolve_path(pathname, 0);
    if (IS_ERR(path))
        return PTR_ERR(path);
    if (!path->parent || S_ISDIR(path->inode->mode)) {
        path_destroy_recursive(path);
        return -EPERM;
    }

    inode_ref(path->parent->inode);
    rc = inode_unlink_child(path->parent->inode, path->basename);
    path_destroy_recursive(path);
    return rc;
}

static bool set_has_children(const char* name, uint8_t type, void* ctx) {
    (void)name;
    (void)type;
    *(bool*)ctx = true;
    return false;
}

static int ensure_empty_directory(struct inode* inode) {
    ASSERT(S_ISDIR(inode->mode));

    file_description* desc = inode_open(inode, O_RDONLY, 0);
    if (IS_ERR(desc))
        return PTR_ERR(desc);

    bool has_children = false;
    int rc = file_description_getdents(desc, set_has_children, &has_children);
    file_description_close(desc);
    if (IS_ERR(rc))
        return rc;

    return has_children ? -ENOTEMPTY : 0;
}

int sys_rename(const char* user_oldpath, const char* user_newpath) {
    char old_pathname[PATH_MAX];
    int rc = copy_pathname_from_user(old_pathname, user_oldpath);
    if (IS_ERR(rc))
        return rc;
    char new_pathname[PATH_MAX];
    rc = copy_pathname_from_user(new_pathname, user_newpath);
    if (IS_ERR(rc))
        return rc;

    struct path* old_path = vfs_resolve_path(old_pathname, 0);
    if (IS_ERR(old_path))
        return PTR_ERR(old_path);
    if (!old_path->parent) {
        path_destroy_recursive(old_path);
        return -EPERM;
    }

    struct path* new_path = vfs_resolve_path(new_pathname, O_ALLOW_NOENT);
    if (IS_ERR(new_path)) {
        path_destroy_recursive(old_path);
        return PTR_ERR(new_path);
    }
    if (!new_path->parent) {
        rc = -EPERM;
        goto done;
    }

    if (new_path->inode) {
        if (new_path->inode == old_path->inode)
            goto done;

        if (S_ISDIR(new_path->inode->mode)) {
            if (!S_ISDIR(old_path->inode->mode)) {
                rc = -EISDIR;
                goto done;
            }
            inode_ref(new_path->inode);
            rc = ensure_empty_directory(new_path->inode);
            if (IS_ERR(rc))
                goto done;
        }

        inode_ref(new_path->parent->inode);
        rc = inode_unlink_child(new_path->parent->inode, new_path->basename);
        if (IS_ERR(rc))
            goto done;
    }

    inode_ref(new_path->parent->inode);
    inode_ref(old_path->inode);
    rc = inode_link_child(new_path->parent->inode, new_path->basename,
                          old_path->inode);
    if (IS_ERR(rc))
        goto done;

    inode_ref(old_path->parent->inode);
    rc = inode_unlink_child(old_path->parent->inode, old_path->basename);

done:
    path_destroy_recursive(new_path);
    path_destroy_recursive(old_path);
    return rc;
}

int sys_rmdir(const char* user_pathname) {
    char pathname[PATH_MAX];
    int rc = copy_pathname_from_user(pathname, user_pathname);
    if (IS_ERR(rc))
        return rc;

    struct path* path = vfs_resolve_path(pathname, 0);
    if (IS_ERR(path))
        return PTR_ERR(path);
    if (!path->parent) {
        path_destroy_recursive(path);
        return -EPERM;
    }
    if (!S_ISDIR(path->inode->mode)) {
        path_destroy_recursive(path);
        return -ENOTDIR;
    }
    inode_ref(path->inode);
    rc = ensure_empty_directory(path->inode);
    if (IS_ERR(rc)) {
        path_destroy_recursive(path);
        return rc;
    }
    inode_ref(path->parent->inode);
    rc = inode_unlink_child(path->parent->inode, path->basename);
    path_destroy_recursive(path);
    return rc;
}

struct fill_dir_ctx {
    unsigned char* user_dirp;
    unsigned remaining_count;
    long nwritten;
    int rc;
};

static bool fill_dir(const char* name, uint8_t type, void* raw_ctx) {
    struct fill_dir_ctx* ctx = (struct fill_dir_ctx*)raw_ctx;
    size_t name_len = strlen(name);
    size_t name_size = name_len + 1;
    size_t size = offsetof(struct dirent, d_name) + name_size;
    if (ctx->remaining_count < size) {
        if (ctx->nwritten == 0)
            ctx->rc = -EINVAL;
        return false;
    }

    struct dirent dent = {
        .d_reclen = size, .d_type = type, .d_namlen = name_len};
    struct dirent* user_dent = (struct dirent*)ctx->user_dirp;
    if (!copy_to_user(user_dent, &dent, sizeof(struct dirent))) {
        ctx->rc = -EFAULT;
        return false;
    }
    if (!copy_to_user(user_dent->d_name, name, name_len)) {
        ctx->rc = -EFAULT;
        return false;
    }

    ctx->user_dirp += size;
    ctx->remaining_count -= size;
    ctx->nwritten += size;
    return true;
}

long sys_getdents(int fd, void* user_dirp, size_t count) {
    file_description* desc = process_get_file_description(fd);
    if (IS_ERR(desc))
        return PTR_ERR(desc);

    struct fill_dir_ctx ctx = {.user_dirp = user_dirp,
                               .remaining_count = count,
                               .nwritten = 0,
                               .rc = 0};
    int rc = file_description_getdents(desc, fill_dir, &ctx);
    if (IS_ERR(rc))
        return rc;
    if (IS_ERR(ctx.rc))
        return ctx.rc;
    return ctx.nwritten;
}

int sys_fcntl(int fd, int cmd, uintptr_t arg) {
    file_description* desc = process_get_file_description(fd);
    if (IS_ERR(desc))
        return PTR_ERR(desc);
    switch (cmd) {
    case F_DUPFD: {
        int ret = process_alloc_file_descriptor(-1, desc);
        if (IS_ERR(ret))
            return ret;
        ++desc->ref_count;
        return ret;
    }
    case F_GETFL:
        return desc->flags;
    case F_SETFL:
        desc->flags = arg;
        return 0;
    default:
        return -EINVAL;
    }
}

int sys_dup2(int oldfd, int newfd) {
    file_description* oldfd_desc = process_get_file_description(oldfd);
    if (IS_ERR(oldfd_desc))
        return PTR_ERR(oldfd_desc);
    if (oldfd == newfd)
        return oldfd;
    file_description* newfd_desc = process_get_file_description(newfd);
    if (IS_OK(newfd_desc)) {
        int rc = file_description_close(newfd_desc);
        if (IS_ERR(rc))
            return rc;
        rc = process_free_file_descriptor(newfd);
        if (IS_ERR(rc))
            return rc;
    }
    int ret = process_alloc_file_descriptor(newfd, oldfd_desc);
    if (IS_ERR(ret))
        return ret;
    ++oldfd_desc->ref_count;
    return ret;
}

int sys_pipe(int user_fifofd[2]) {
    struct inode* fifo = fifo_create();
    if (IS_ERR(fifo))
        return PTR_ERR(fifo);

    inode_ref(fifo);
    file_description* reader_desc = inode_open(fifo, O_RDONLY, 0);
    if (IS_ERR(reader_desc)) {
        inode_unref(fifo);
        return PTR_ERR(reader_desc);
    }

    file_description* writer_desc = inode_open(fifo, O_WRONLY, 0);
    if (IS_ERR(writer_desc)) {
        file_description_close(reader_desc);
        return PTR_ERR(writer_desc);
    }

    int rc = 0;
    int writer_fd = -1;

    int reader_fd = process_alloc_file_descriptor(-1, reader_desc);
    if (IS_ERR(reader_fd)) {
        rc = reader_fd;
        goto fail;
    }

    writer_fd = process_alloc_file_descriptor(-1, writer_desc);
    if (IS_ERR(writer_fd)) {
        rc = writer_fd;
        goto fail;
    }

    int fifofd[2] = {reader_fd, writer_fd};
    if (!copy_to_user(user_fifofd, fifofd, sizeof(int[2]))) {
        rc = -EFAULT;
        goto fail;
    }

    ASSERT(IS_OK(rc));
    return 0;

fail:
    ASSERT(IS_ERR(rc));
    if (IS_OK(reader_fd))
        process_free_file_descriptor(reader_fd);
    if (IS_OK(writer_fd))
        process_free_file_descriptor(writer_fd);
    file_description_close(reader_desc);
    file_description_close(writer_desc);
    return rc;
}
