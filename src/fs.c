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
static pthread_mutex_t g_tree_mutex = PTHREAD_MUTEX_INITIALIZER;

// Helper to create a new node
TreeNode* create_node(const char* name)
{
	TreeNode* node = malloc(sizeof(TreeNode));
	node->name = strdup(name);
	node->files = NULL;
	node->file_count = 0;
	node->file_capacity = 0;
	node->subdirectories = NULL;
	node->dir_count = 0;
	node->dir_capacity = 0;
	return node;
}

// Function to add a file
static void add_file(TreeNode* node, const char* name)
{
	pthread_mutex_lock(&g_tree_mutex);
	if (node->file_count == node->file_capacity) {
		int new_capacity =
		 (node->file_capacity == 0) ? 4 : node->file_capacity * 2;
		char** new_files = (char**) realloc((void*) node->files,
		 sizeof(char*) * (size_t) new_capacity);
		if (new_files == NULL) {
			pthread_mutex_unlock(&g_tree_mutex);
			return;
		}
		node->files = new_files;
		node->file_capacity = new_capacity;
	}
	node->files[node->file_count++] = strdup(name);
	pthread_mutex_unlock(&g_tree_mutex);
}

// Function to add a subdirectory
static void add_subdir(TreeNode* node, TreeNode* subdir)
{
	pthread_mutex_lock(&g_tree_mutex);
	if (node->dir_count == node->dir_capacity) {
		int new_capacity =
		 (node->dir_capacity == 0) ? 4 : node->dir_capacity * 2;
		TreeNode** new_subs = (TreeNode**) realloc((void*) node->subdirectories,
		 sizeof(TreeNode*) * (size_t) new_capacity);
		if (new_subs == NULL) {
			pthread_mutex_unlock(&g_tree_mutex);
			return;
		}
		node->subdirectories = new_subs;
		node->dir_capacity = new_capacity;
	}
	node->subdirectories[node->dir_count++] = subdir;
	pthread_mutex_unlock(&g_tree_mutex);
}

static void traverse_recursive_hybrid(TreeNode* node, int depth, DirQueue* dq)
{
	DIR* dir = opendir(node->name);
	if (!dir) {
		return;
	}

	size_t path_len = strlen(node->name);
	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') {
			if (strcmp(entry->d_name, ".") == 0 ||
			 strcmp(entry->d_name, "..") == 0) {
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
		if (LSTAT(sub_path, &st) != 0) {
			continue;
		}

		if (S_ISDIR(st.st_mode)) {
			TreeNode* subdir = create_node(sub_path);
			add_subdir(node, subdir);
			if (depth < 2 && dq) {
				dq_push(dq, subdir, depth + 1);
			} else {
				traverse_recursive_hybrid(subdir, depth + 1, dq);
			}
		} else if (S_ISREG(st.st_mode)) {
			add_file(node, entry->d_name);
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
		if (dq) {
			dq_push(dq, root, 0);
		}
		return root;
	}
	return NULL;
}
