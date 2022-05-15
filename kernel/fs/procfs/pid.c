#include "procfs.h"
#include <common/string.h>
#include <kernel/fs/dentry.h>
#include <kernel/growable_buf.h>
#include <kernel/interrupts.h>
#include <kernel/panic.h>
#include <kernel/process.h>

typedef struct procfs_pid_item_inode {
    procfs_item_inode item_inode;
    pid_t pid;
} procfs_pid_item_inode;

static int populate_comm(file_description* desc, growable_buf* buf) {
    procfs_pid_item_inode* node = (procfs_pid_item_inode*)desc->inode;
    struct process* process = process_find_process_by_pid(node->pid);
    if (!process)
        return -ENOENT;

    char comm[sizeof(process->comm)];
    bool int_flag = push_cli();
    strlcpy(comm, process->comm, sizeof(process->comm));
    pop_cli(int_flag);

    return growable_buf_printf(buf, "%s\n", comm);
}

static int add_item(procfs_dir_inode* parent, const procfs_item_def* item_def,
                    pid_t pid) {
    procfs_pid_item_inode* node = kmalloc(sizeof(procfs_pid_item_inode));
    if (!node)
        return -ENOMEM;
    *node = (procfs_pid_item_inode){0};

    node->pid = pid;
    node->item_inode.populate = item_def->populate;

    struct inode* inode = &node->item_inode.inode;
    inode->fs_root_inode = parent->inode.fs_root_inode;
    inode->fops = &procfs_item_fops;
    inode->mode = S_IFREG;
    inode->ref_count = 1;

    return dentry_append(&parent->children, item_def->name, inode);
}

static procfs_item_def pid_items[] = {{"comm", populate_comm}};
#define NUM_ITEMS (sizeof(pid_items) / sizeof(procfs_item_def))

struct inode* procfs_pid_dir_inode_create(procfs_dir_inode* parent, pid_t pid) {
    struct process* process = process_find_process_by_pid(pid);
    if (!process)
        return ERR_PTR(-ENOENT);

    procfs_dir_inode* node = kmalloc(sizeof(procfs_dir_inode));
    if (!node)
        return ERR_PTR(-ENOMEM);
    *node = (procfs_dir_inode){0};

    static file_ops fops = {.destroy_inode = procfs_dir_destroy_inode,
                            .lookup_child = procfs_dir_lookup_child,
                            .getdents = procfs_dir_getdents};
    struct inode* inode = &node->inode;
    inode->fs_root_inode = parent->inode.fs_root_inode;
    inode->fops = &fops;
    inode->mode = S_IFDIR;
    inode->ref_count = 1;

    for (size_t i = 0; i < NUM_ITEMS; ++i) {
        int rc = add_item(node, pid_items + i, pid);
        if (IS_ERR(rc))
            return ERR_PTR(rc);
    }

    inode_unref((struct inode*)parent);
    return inode;
}
