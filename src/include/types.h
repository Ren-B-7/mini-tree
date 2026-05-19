#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

#define PATH_BUF 4096

typedef struct TreeNode {
	char* name;
	char** files;
	int file_count;
	int file_capacity;
	struct TreeNode** subdirectories;
	int dir_count;
	int dir_capacity;

	/* Totals for the subtree */
	long total_files;
	long total_dirs;
	long total_size;

	/*
	 * Per-node spinlock (0 = unlocked, 1 = locked).
	 *
	 * Replaces the single global pthread_mutex that previously serialized
	 * all add_file / add_subdir calls across every thread.  Using one word
	 * per node means threads traversing different directories never contend
	 * at all.  The spinlock is only acquired in add_subdir (linking a new
	 * child pointer into the parent) — add_file is fully lock-free because
	 * only one thread ever owns a node during its traversal.
	 *
	 * Aligned to its own cache line to prevent false sharing between the
	 * hot read fields above and the lock word, which is written under
	 * contention.
	 */
	int node_lock;
} TreeNode;

#endif
