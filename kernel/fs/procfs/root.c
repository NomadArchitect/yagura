#include "procfs_private.h"
#include <common/stdio.h>
#include <common/stdlib.h>
#include <kernel/api/dirent.h>
#include <kernel/api/sys/sysmacros.h>
#include <kernel/fs/dentry.h>
#include <kernel/interrupts.h>
#include <kernel/panic.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/system.h>
#include <kernel/time.h>
#include <kernel/vec.h>

static int populate_cmdline(file_description* desc, struct vec* vec) {
    (void)desc;
    return vec_printf(vec, "%s\n", cmdline_get_raw());
}

static int populate_kallsyms(file_description* desc, struct vec* vec) {
    (void)desc;
    const struct symbol* symbol = NULL;
    while ((symbol = ksyms_next(symbol))) {
        if (vec_printf(vec, "%08x %c %s\n", symbol->addr, symbol->type,
                       symbol->name) < 0)
            return -ENOMEM;
    }
    return 0;
}

static int populate_meminfo(file_description* desc, struct vec* vec) {
    (void)desc;
    struct memory_stats stats;
    memory_get_stats(&stats);

    return vec_printf(vec,
                      "MemTotal: %8u kB\n"
                      "MemFree:  %8u kB\n",
                      stats.total, stats.free);
}

static int populate_self(file_description* desc, struct vec* vec) {
    (void)desc;
    return vec_printf(vec, "%d", current->pid);
}

NODISCARD static int sprintf_ticks(struct vec* vec, int64_t ticks) {
    int32_t r;
    int32_t q = divmodi64(ticks, CLK_TCK, &r);
    r = r * 100 / CLK_TCK; // Map [0, CLK_TCK) to [0, 100)
    return vec_printf(vec, "%d.%02d", q, r);
}

static int populate_uptime(file_description* desc, struct vec* vec) {
    (void)desc;
    int rc = sprintf_ticks(vec, uptime);
    if (IS_ERR(rc))
        return rc;
    rc = vec_append(vec, " ", 1);
    if (IS_ERR(rc))
        return rc;
    rc = sprintf_ticks(vec, idle_ticks);
    if (IS_ERR(rc))
        return rc;
    return vec_append(vec, "\n", 1);
}

static int populate_version(file_description* desc, struct vec* vec) {
    (void)desc;
    return vec_printf(vec, "%s version %s %s\n", utsname()->sysname,
                      utsname()->release, utsname()->version);
}

static procfs_item_def root_items[] = {{"cmdline", S_IFREG, populate_cmdline},
                                       {"kallsyms", S_IFREG, populate_kallsyms},
                                       {"meminfo", S_IFREG, populate_meminfo},
                                       {"self", S_IFLNK, populate_self},
                                       {"uptime", S_IFREG, populate_uptime},
                                       {"version", S_IFREG, populate_version}};
#define NUM_ITEMS ARRAY_SIZE(root_items)

static struct inode* procfs_root_lookup_child(struct inode* inode,
                                              const char* name) {
    if (str_is_uint(name)) {
        pid_t pid = atoi(name);
        return procfs_pid_dir_inode_create((procfs_dir_inode*)inode, pid);
    }
    return procfs_dir_lookup_child(inode, name);
}

static int procfs_root_getdents(file_description* desc,
                                getdents_callback_fn callback, void* ctx) {
    procfs_dir_inode* node = (procfs_dir_inode*)desc->inode;

    mutex_lock(&desc->offset_lock);
    if ((size_t)desc->offset < NUM_ITEMS) {
        int rc = dentry_getdents(desc, node->children, callback, ctx);
        if (IS_ERR(rc)) {
            mutex_unlock(&desc->offset_lock);
            return rc;
        }
    }
    if ((size_t)desc->offset < NUM_ITEMS) {
        mutex_unlock(&desc->offset_lock);
        return 0;
    }

    bool int_flag = push_cli();

    pid_t offset_pid = (pid_t)(desc->offset - NUM_ITEMS);
    struct process* it = all_processes;
    while (it->pid <= offset_pid) {
        it = it->next_in_all_processes;
        if (!it)
            break;
    }

    while (it) {
        char name[16];
        (void)snprintf(name, sizeof(name), "%d", it->pid);
        if (!callback(name, DT_DIR, ctx))
            break;
        desc->offset = it->pid + NUM_ITEMS;
        it = it->next_in_all_processes;
    }

    pop_cli(int_flag);
    mutex_unlock(&desc->offset_lock);
    return 0;
}

static int add_item(procfs_dir_inode* parent, const procfs_item_def* item_def) {
    procfs_item_inode* node = kmalloc(sizeof(procfs_item_inode));
    if (!node) {
        inode_unref((struct inode*)parent);
        return -ENOMEM;
    }
    *node = (procfs_item_inode){0};

    node->populate = item_def->populate;

    struct inode* inode = &node->inode;
    inode->dev = parent->inode.dev;
    inode->fops = &procfs_item_fops;
    inode->mode = item_def->mode;
    inode->ref_count = 1;

    int rc = dentry_append(&parent->children, item_def->name, inode);
    inode_unref((struct inode*)parent);
    return rc;
}

struct inode* procfs_create_root(void) {
    procfs_dir_inode* root = kmalloc(sizeof(procfs_dir_inode));
    if (!root)
        return ERR_PTR(-ENOMEM);
    *root = (procfs_dir_inode){0};

    static file_ops fops = {
        .destroy_inode = procfs_dir_destroy_inode,
        .lookup_child = procfs_root_lookup_child,
        .getdents = procfs_root_getdents,
    };

    struct inode* inode = &root->inode;
    inode->dev = vfs_generate_unnamed_device_number();
    inode->fops = &fops;
    inode->mode = S_IFDIR;
    inode->ref_count = 1;

    for (size_t i = 0; i < NUM_ITEMS; ++i) {
        inode_ref(inode);
        int rc = add_item(root, root_items + i);
        if (IS_ERR(rc))
            return ERR_PTR(rc);
    }

    return inode;
}
