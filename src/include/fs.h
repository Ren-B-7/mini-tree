#ifndef FS_H
#define FS_H

#include <stdbool.h>

#include "threading.h"
#include "types.h"

TreeNode* create_node(const char* name);
void aggregate_totals(TreeNode* node);
void* traverse_directory(void* arg);

TreeNode* process_path(const char* path, bool recurse, WorkQueue* wq, DirQueue* dq);

#endif
