#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "include/output.h"
#include "include/types.h"
#include "include/minicli.h"

extern TreeNode* create_node(const char* name);
extern void* traverse_directory(void* arg);

int g_max_depth = -1;
bool g_no_format = false;
bool g_show_hidden = false;

typedef struct {
    const char* dir;
} TreeConfig;

static int cb_max_depth(int argc, char** argv, void* user_data) {
    if (argc > 0) {
        g_max_depth = (int)strtol(argv[0], NULL, 10);
        return 1;
    }
    return 0;
}

static int cb_show_hidden(int argc, char** argv, void* user_data) {
    (void)argc; (void)argv; (void)user_data;
    g_show_hidden = true;
    return 0;
}

static int cb_no_format(int argc, char** argv, void* user_data) {
    (void)argc; (void)argv; (void)user_data;
    g_no_format = true;
    return 0;
}

bool is_binary(const char* path)
{
	return (access(path, X_OK) == 0);
}

void print_tree(TreeNode* node, int level, bool is_last, const char* prefix)
{
	if (g_max_depth != -1 && level > g_max_depth) {
		return;
	}

	if (level > 0) {
		printf("%s%s-- %s%s%s\n", prefix, is_last ? "`" : "|", PRINT_GREEN,
		 node->name, PRINT_RESET);
	} else {
		printf("%s%s%s\n", PRINT_GREEN, node->name, PRINT_RESET);
	}

	char new_prefix[1024];
	snprintf(new_prefix, sizeof(new_prefix), "%s%s   ", prefix,
	 is_last ? " " : "|");

	for (int i = 0; i < node->file_count; i++) {
		char full_path[1024];
		snprintf(full_path, sizeof(full_path), "%s/%s", node->name,
		 node->files[i]);

		bool is_binary_file = is_binary(full_path);
		const char* color = is_binary_file ? PRINT_YELLOW : PRINT_BLUE;

		bool is_last_item = (i == node->file_count - 1 && node->dir_count == 0);
		printf("%s%s-- %s%s%s\n", new_prefix, is_last_item ? "`" : "|", color,
		 node->files[i], PRINT_RESET);
	}
	for (int i = 0; i < node->dir_count; i++) {
		bool is_last_item = (i == node->dir_count - 1);
		print_tree(node->subdirectories[i], level + 1, is_last_item,
		 new_prefix);
	}
}

int main(int argc, char* argv[])
{
    TreeConfig cfg = { .dir = "." };
    CliParser parser;
    cli_init(&parser, (CliInitParams){"tree", "A multithreaded directory tree tool"});

    cli_add_argument(&parser, (CliArgument){"--max-depth", NULL, "Max depth of recursion", cb_max_depth, &cfg});
    cli_add_argument(&parser, (CliArgument){"--show-hidden", NULL, "Show hidden files", cb_show_hidden, &cfg});
    cli_add_argument(&parser, (CliArgument){"--no-format", NULL, "Disable formatting", cb_no_format, &cfg});
    cli_add_argument(&parser, (CliArgument){"--help", "-h", "Show help", NULL, NULL});

    int consumed = cli_parse(&parser, argc, argv);
    if (consumed < 0) {
        return 1;
    }
    if (consumed < argc) {
        cfg.dir = argv[consumed];
    }

	TreeNode* root = create_node(cfg.dir);
	traverse_directory(root);

	print_tree(root, 0, true, "");

	return 0;
}
