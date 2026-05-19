#include "include/fs.h"

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "include/threading.h"
#include "include/types.h"

#ifdef _WIN32
#define LSTAT stat
#else
#define LSTAT lstat
#endif

extern bool g_show_hidden;
extern bool g_follow_links;

/*
 * Per-node spinlock helpers.
 *
 * Each TreeNode carries its own atomic lock word, eliminating the single
 * global mutex that previously serialized every add_file / add_subdir call
 * across all threads.  A spinlock is appropriate here because the critical
 * sections are very short (a pointer store + counter bump) and contention on
 * any individual node is rare — only threads that happen to be populating the
 * same directory compete.
 */
static inline void node_lock_acquire(TreeNode* n)
{
	int zero = 0;
	while (!__atomic_compare_exchange_n(&n->node_lock, &zero, 1, 0,
	 __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
		zero = 0;
		/* Yield the CPU briefly to avoid starving other threads on
		 * hyperthreaded cores. */
#if defined(__x86_64__) || defined(__i386__)
		__asm__ volatile("pause" ::: "memory");
#endif
	}
}

static inline void node_lock_release(TreeNode* n)
{
	__atomic_store_n(&n->node_lock, 0, __ATOMIC_RELEASE);
}

TreeNode* create_node(const char* name)
{
	printf("Created a new node!\n");
	TreeNode* node = malloc(sizeof(TreeNode));
	if (!node) {
		return NULL;
	}
	node->name = strdup(name);
	node->files = NULL;
	node->file_count = 0;
	node->file_capacity = 0;
	node->subdirectories = NULL;
	node->dir_count = 0;
	node->dir_capacity = 0;
	node->total_files = 0;
	node->total_dirs = 0;
	node->total_size = 0;
	node->node_lock = 0;
	return node;
}

void aggregate_totals(TreeNode* node)
{
	for (int i = 0; i < node->dir_count; i++) {
		aggregate_totals(node->subdirectories[i]);
		node->total_files += node->subdirectories[i]->total_files;
		node->total_dirs += node->subdirectories[i]->total_dirs;
		node->total_size += node->subdirectories[i]->total_size;
	}
}

/*
 * add_file — lock-free on the fast path.
 *
 * A thread owns a node exclusively while it is the active traversal thread
 * for that directory (it dequeued it from DirQueue).  No other thread calls
 * add_file on the same node concurrently, so no lock is needed here at all.
 * The per-node spinlock is only taken in add_subdir, where a child node is
 * being linked into a parent that may be shared briefly.
 */
static void add_file(TreeNode* node, const char* name, long size)
{
	if (node->file_count == node->file_capacity) {
		int new_cap = (node->file_capacity == 0) ? 8 : node->file_capacity * 2;
		char** nf = (char**) realloc((void*) node->files,
		 sizeof(char*) * (size_t) new_cap);
		if (!nf) {
			return;
		}
		node->files = nf;
		node->file_capacity = new_cap;
	}
	node->files[node->file_count++] = strdup(name);
	node->total_files++;
	node->total_size += size;
}

/*
 * add_subdir — takes the per-node spinlock only on the parent.
 *
 * The child node is brand-new and not yet visible to any other thread, so it
 * needs no protection.  We only lock the parent long enough to append the
 * pointer and bump total_dirs, which is a handful of instructions.
 */
static bool add_subdir(TreeNode* parent, TreeNode* child)
{
	node_lock_acquire(parent);
	if (parent->dir_count == parent->dir_capacity) {
		int new_cap =
		 (parent->dir_capacity == 0) ? 4 : parent->dir_capacity * 2;
		TreeNode** ns = (TreeNode**) realloc((void*) parent->subdirectories,
		 sizeof(TreeNode*) * (size_t) new_cap);
		if (!ns) {
			node_lock_release(parent);
			return false;
		}
		parent->subdirectories = ns;
		parent->dir_capacity = new_cap;
	}
	parent->subdirectories[parent->dir_count++] = child;
	parent->total_dirs++;
	node_lock_release(parent);
	return true;
}

/*
 * traverse_recursive_hybrid
 *
 * Key changes vs. the original:
 *  - No global mutex anywhere in the traversal hot path.
 *  - The depth < 2 gate is removed: we always push subdirectories onto the
 *    DirQueue so worker threads can steal them immediately.  This gives better
 *    load balancing on wide trees (e.g. the kernel's drivers/ subtree).
 *  - The g_follow_links branch is hoisted out of the readdir loop via a
 *    function pointer selected once per call, removing a branch that the
 *    predictor sees as almost-always-the-same but still pays mis-prediction
 *    cost on the first entry of each directory.
 */
static void traverse_recursive_hybrid(TreeNode* node, int depth, DirQueue* dq)
{
	printf("Start hybrid walker\n");
	(void) depth; /* depth tracking no longer needed without the gate */

	DIR* dir = opendir(node->name);
	if (!dir) {
		return;
	}

	/* Select stat variant once, outside the loop. */
	int (*do_stat)(const char*, struct stat*) = g_follow_links ? stat : LSTAT;

	size_t path_len = strlen(node->name);
	struct dirent* entry;

	while ((entry = readdir(dir)) != NULL) {
		/* Skip dot entries */
		if (entry->d_name[0] == '.') {
			if (entry->d_name[1] == '\0' ||
			 (entry->d_name[1] == '.' && entry->d_name[2] == '\0')) {
				continue;
			}
			if (!g_show_hidden) {
				continue;
			}
		}

		size_t nlen = strlen(entry->d_name);
		if (path_len + 1 + nlen >= PATH_BUF) {
			continue;
		}

		char sub_path[PATH_BUF];
		memcpy(sub_path, node->name, path_len);
		sub_path[path_len] = '/';
		memcpy(sub_path + path_len + 1, entry->d_name, nlen + 1);

		struct stat st;
		if (do_stat(sub_path, &st) != 0) {
			continue;
		}

		if (S_ISDIR(st.st_mode)) {
			TreeNode* child = create_node(sub_path);
			if (!child) {
				continue;
			}
			if (!add_subdir(node, child)) {
				free(child->name);
				free(child);
				continue;
			}
			/* Always push to the queue; let workers steal.
			 * Falls back to inline recursion only when dq is NULL
			 * (single-threaded path via traverse_directory). */
			if (dq) {
				dq_push(dq, child, 0);
			} else {
				traverse_recursive_hybrid(child, 0, NULL);
			}
		} else if (S_ISREG(st.st_mode)) {
			add_file(node, entry->d_name, (long) st.st_size);
		}
	}

	closedir(dir);
}

void* traverse_directory(void* arg)
{
	traverse_recursive_hybrid((TreeNode*) arg, 0, NULL);
	return NULL;
}

void walk_dir_task(DirQueue* dq)
{
	void* data;
	int depth;
	while (dq_pop(dq, &data, &depth)) {
		traverse_recursive_hybrid((TreeNode*) data, depth, dq);
		dq_node_done(dq);
	}
}

TreeNode*
process_path(const char* path, bool recurse, WorkQueue* wq, DirQueue* dq)
{
	(void) recurse;
	(void) wq;
	struct stat st;
	if (LSTAT(path, &st) != 0) {
		return NULL;
	}
	if (S_ISDIR(st.st_mode)) {
		TreeNode* root = create_node(path);
		if (!root) {
			return NULL;
		}
		if (dq) {
			dq_push(dq, root, 0);
		}
		return root;
	}
	return NULL;
}
