#include "include/output.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "include/cli.h"

static inline bool is_binary(const char* path)
{
	return (access(path, X_OK) == 0);
}

void format_size(long size, char* buf, size_t buf_size)
{
	const char* units[] = {"B", "KB", "MB", "GB", "TB"};
	int unit = 0;
	double d_size = (double) size;
	while (d_size >= 1024 && unit < 4) {
		d_size /= 1024;
		unit++;
	}
	if (unit == 0) {
		snprintf(buf, buf_size, "%ld%s", size, units[unit]);
	} else {
		snprintf(buf, buf_size, "%.1f%s", d_size, units[unit]);
	}
}

void print_tree(TreeNode* node, int level, bool is_last, const char* prefix)
{
	if (g_max_depth != -1 && level > g_max_depth) {
		return;
	}

	if (g_prune && node->total_files == 0 && node->total_dirs == 0) {
		return;
	}

	char size_buf[32] = "";
	if (g_show_size) {
		format_size(node->total_size, size_buf, sizeof(size_buf));
	}

	if (level > 0) {
		if (g_no_indent) {
			printf("%s%s%s%s%s\n", g_no_color ? "" : PRINT_GREEN, node->name,
			 g_no_color ? "" : PRINT_RESET, g_show_size ? " (" : "", size_buf);
			if (g_show_size) {
				printf(")");
			}
		} else {
			printf("%s%s-- %s%s%s", prefix, is_last ? "`" : "|",
			 g_no_color ? "" : PRINT_GREEN, node->name,
			 g_no_color ? "" : PRINT_RESET);
			if (g_show_size) {
				printf(" (%s)", size_buf);
			}
			printf("\n");
		}
	} else {
		printf("%s%s%s", g_no_color ? "" : PRINT_GREEN, node->name,
		 g_no_color ? "" : PRINT_RESET);
		if (g_show_size) {
			printf(" (%s)", size_buf);
		}
		printf("\n");
	}

	char new_prefix[4096];
	if (!g_no_indent) {
		if (level == 0) {
			snprintf(new_prefix, sizeof(new_prefix), "%s", prefix);
		} else {
			snprintf(new_prefix, sizeof(new_prefix), "%s%s   ", prefix,
			 is_last ? " " : "|");
		}
	} else {
		new_prefix[0] = '\0';
	}

	if (!g_dirs_only) {
		for (int i = 0; i < node->file_count; i++) {
			char full_path[PATH_BUF];
			snprintf(full_path, sizeof(full_path), "%s/%s", node->name,
			 node->files[i]);

			bool is_binary_file = is_binary(full_path);
			const char* color =
			 g_no_color ? "" : (is_binary_file ? PRINT_YELLOW : PRINT_BLUE);
			bool is_last_item =
			 (i == node->file_count - 1 && node->dir_count == 0);

			if (g_show_size) {
				struct stat st;
				if (stat(full_path, &st) == 0) {
					format_size((long) st.st_size, size_buf, sizeof(size_buf));
				}
			}

			if (g_no_indent) {
				printf("%s%s%s", color, node->files[i],
				 g_no_color ? "" : PRINT_RESET);
				if (g_show_size) {
					printf(" (%s)", size_buf);
				}
				printf("\n");
			} else {
				printf("%s%s-- %s%s%s", new_prefix, is_last_item ? "`" : "|",
				 color, node->files[i], g_no_color ? "" : PRINT_RESET);
				if (g_show_size) {
					printf(" (%s)", size_buf);
				}
				printf("\n");
			}
		}
	}

	for (int i = 0; i < node->dir_count; i++) {
		print_tree(node->subdirectories[i], level + 1, i == node->dir_count - 1,
		 new_prefix);
	}
}

void print_json(TreeNode* node, int level)
{
	printf("%*s{\n", level * 2, "");
	printf("%*s\"name\": \"%s\",\n", (level + 1) * 2, "", node->name);
	printf("%*s\"size\": %ld,\n", (level + 1) * 2, "", node->total_size);
	printf("%*s\"files\": [\n", (level + 1) * 2, "");
	for (int i = 0; i < node->file_count; i++) {
		printf("%*s\"%s\"%s\n", (level + 2) * 2, "", node->files[i],
		 i == node->file_count - 1 ? "" : ",");
	}
	printf("%*s],\n", (level + 1) * 2, "");
	printf("%*s\"directories\": [\n", (level + 1) * 2, "");
	for (int i = 0; i < node->dir_count; i++) {
		print_json(node->subdirectories[i], level + 2);
		if (i < node->dir_count - 1) {
			printf(",\n");
		} else {
			printf("\n");
		}
	}
	printf("%*s]\n", (level + 1) * 2, "");
	printf("%*s}", level * 2, "");
}

void print_xml(TreeNode* node, int level)
{
	printf("%*s<directory name=\"%s\" size=\"%ld\">\n", level * 2, "",
	 node->name, node->total_size);
	for (int i = 0; i < node->file_count; i++) {
		printf("%*s<file name=\"%s\"/>\n", (level + 1) * 2, "", node->files[i]);
	}
	for (int i = 0; i < node->dir_count; i++) {
		print_xml(node->subdirectories[i], level + 1);
	}
	printf("%*s</directory>\n", level * 2, "");
}
