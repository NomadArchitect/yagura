#pragma once

struct path {
    // The inode associated with the path.
    // NULL if vfs_resolve_path was called with O_ALLOW_NOENT and
    // the last component of the path does not exist.
    struct inode* inode;

    // The basename of the path. NULL for the root path.
    char* basename;

    // The parent of the path. NULL for the root path.
    struct path* parent;
};

// Returns a string representation of the path.
char* path_to_string(const struct path*);

// Returns a clone of the path.
struct path* path_dup(const struct path*);

// Joins a path with a basename associated with an inode.
//
// The parent is consumed by the function.
struct path* path_join(struct path* parent, struct inode* inode,
                       const char* basename);

// Extracts the inode from the path and destroys the path.
struct inode* path_into_inode(struct path*);

// Destroys the last component of the path, but not the parents.
void path_destroy_last(struct path*);

// Destroys the path and all its parents.
void path_destroy_recursive(struct path*);
