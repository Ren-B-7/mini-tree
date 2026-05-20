#ifndef OUTPUT_H
#define OUTPUT_H

#include <stddef.h>

#include "types.h"

#define PRINT_RESET "\033[0m"
#define PRINT_BOLD "\033[1m"
#define PRINT_RED "\033[31m"
#define PRINT_GREEN "\033[32m"
#define PRINT_YELLOW "\033[33m"
#define PRINT_BLUE "\033[34m"
#define PRINT_CYAN "\033[36m"

void print_tree(TreeNode* node, int level, bool is_last, const char* prefix);
void print_json(TreeNode* node, int level);
void print_xml(TreeNode* node, int level);
void format_size(long size, char* buf, size_t buf_size);

#endif
