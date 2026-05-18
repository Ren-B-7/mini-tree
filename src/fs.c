#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/types.h"

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
void add_file(TreeNode* node, const char* name)
{
	if (node->file_count == node->file_capacity) {
		int new_capacity =
		 (node->file_capacity == 0) ? 4 : node->file_capacity * 2;
		char** new_files = (char**) realloc((void*) node->files,
		 sizeof(char*) * (size_t) new_capacity);
		if (new_files == NULL) {
			return;
		}
		node->files = new_files;
		node->file_capacity = new_capacity;
	}
	node->files[node->file_count++] = strdup(name);
}

// Function to add a subdirectory
void add_subdir(TreeNode* node, TreeNode* subdir)
{
	if (node->dir_count == node->dir_capacity) {
		int new_capacity =
		 (node->dir_capacity == 0) ? 4 : node->dir_capacity * 2;
		TreeNode** new_subs = (TreeNode**) realloc((void*) node->subdirectories,
		 sizeof(TreeNode*) * (size_t) new_capacity);
		if (new_subs == NULL) {
			return;
		}
		node->subdirectories = new_subs;
		node->dir_capacity = new_capacity;
	}
	node->subdirectories[node->dir_count++] = subdir;
}

extern bool g_show_hidden;

// Recursive task function
void* traverse_directory(void* arg)
{
	TreeNode* node = (TreeNode*) arg;
	DIR* dir = opendir(node->name);
	if (!dir) {
		return NULL;
	}

	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
		 strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		if (!g_show_hidden && entry->d_name[0] == '.') {
			continue;
		}

		char path[1024];
		snprintf(path, sizeof(path), "%s/%s", node->name, entry->d_name);

		if (entry->d_type == DT_DIR) {
			TreeNode* subdir = create_node(path);
			add_subdir(node, subdir);

			// Spawn new thread for subdirectory
			pthread_t thread;
			pthread_create(&thread, NULL, traverse_directory, subdir);
			pthread_join(thread, NULL);
		} else {
			add_file(node, entry->d_name);
		}
	}

	closedir(dir);
	return NULL;
}
