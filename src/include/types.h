#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

typedef struct TreeNode {
	char* name;
	char** files;
	int file_count;
	int file_capacity;
	struct TreeNode** subdirectories;
	int dir_count;
	int dir_capacity;
} TreeNode;

#endif
