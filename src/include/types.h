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
} TreeNode;

#endif
