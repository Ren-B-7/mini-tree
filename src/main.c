#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(_WIN32)
#include <windows.h>

static inline long get_nproc_win32(void)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return (long) si.dwNumberOfProcessors;
}

#define GET_NPROC() get_nproc_win32()
#else
#include <unistd.h>
#define GET_NPROC() sysconf(_SC_NPROCESSORS_ONLN)
#endif

#include "include/fs.h"
#include "include/minicli.h"
#include "include/output.h"
#include "include/threading.h"
#include "include/types.h"

int g_max_depth = -1;
bool g_no_format = false;
bool g_show_hidden = false;
bool g_json = false;
bool g_xml = false;
bool g_no_color = false;
bool g_no_indent = false;
bool g_prune = false;
bool g_show_size = false;
bool g_dirs_only = false;
bool g_no_count = false;
bool g_follow_links = false;

typedef struct {
	const char* dir;
} TreeConfig;

static int cb_json(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_json = true;
	return 0;
}

static int cb_xml(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_xml = true;
	return 0;
}

static int cb_no_color(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_no_color = true;
	return 0;
}

static int cb_no_indent(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_no_indent = true;
	return 0;
}

static int cb_prune(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_prune = true;
	return 0;
}

static int cb_show_size(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_show_size = true;
	return 0;
}

static int cb_dirs_only(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_dirs_only = true;
	return 0;
}

static int cb_no_count(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_no_count = true;
	return 0;
}

static int cb_follow_links(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_follow_links = true;
	return 0;
}

static int cb_max_depth(int argc, char** argv, void* user_data)
{
	(void) user_data;
	if (argc > 0) {
		g_max_depth = (int) strtol(argv[0], NULL, 10);
		return 1;
	}
	return 0;
}

static int cb_show_hidden(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_show_hidden = true;
	return 0;
}

static int cb_no_format(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_no_format = true;
	return 0;
}

static bool is_binary(const char* path)
{
	return (access(path, X_OK) == 0);
}

static void format_size(long size, char* buf, size_t buf_size)
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

static void
print_tree(TreeNode* node, int level, bool is_last, const char* prefix)
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
		snprintf(new_prefix, sizeof(new_prefix), "%s%s   ", prefix,
		 is_last ? " " : "|");
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
		bool is_last_item = (i == node->dir_count - 1);
		print_tree(node->subdirectories[i], level + 1, is_last_item,
		 new_prefix);
	}
}

static void print_json(TreeNode* node, int level)
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

static void print_xml(TreeNode* node, int level)
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

static void* walker_thread(void* arg)
{
	DirQueue* dq = (DirQueue*) arg;
	walk_dir_task(dq);
	return NULL;
}

int main(int argc, char* argv[])
{
	TreeConfig cfg = {.dir = "."};
	CliParser parser;
	cli_init(&parser,
	 (CliInitParams) {"tree", "A multithreaded directory tree tool"});

	cli_add_argument(&parser,
	 (CliArgument) {
	     "--max-depth", NULL, "Max depth of recursion", cb_max_depth, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {
	     "--show-hidden", NULL, "Show hidden files", cb_show_hidden, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {
	     "--no-format", NULL, "Disable formatting", cb_no_format, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {"--json", "-j", "Output in JSON format", cb_json, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {"--xml", "-x", "Output in XML format", cb_xml, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {
	     "--no-color", NULL, "Disable coloration", cb_no_color, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {
	     "--no-indent", NULL, "Disable indentation", cb_no_indent, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {
	     "--prune", NULL, "Prune empty directories", cb_prune, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {
	     "--size", "-s", "Show file and directory sizes", cb_show_size, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {
	     "--dirs-only", "-d", "Only show directory names", cb_dirs_only, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {"--no-count", NULL, "Don't show file/directory counts",
	     cb_no_count, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {
	     "--follow-links", "-L", "Follow symlinks", cb_follow_links, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {"--help", "-h", "Show help", NULL, NULL});

	int consumed = cli_parse(&parser, argc, argv);
	if (consumed < 0) {
		return 1;
	}
	if (consumed < argc) {
		cfg.dir = argv[consumed];
	}

	DirQueue dq;
	dq_init(&dq, 1024);

	long nproc = GET_NPROC();
	if (nproc < 1) {
		nproc = 1;
	}
	if (nproc > MAX_THREADS) {
		nproc = MAX_THREADS;
	}

	dq.active_count = (int) nproc;

	TreeNode* root = process_path(cfg.dir, true, NULL, &dq);

	pthread_t threads[MAX_THREADS];
	for (long i = 0; i < nproc; i++) {
		pthread_create(&threads[i], NULL, walker_thread, &dq);
	}

	for (long i = 0; i < nproc; i++) {
		pthread_join(threads[i], NULL);
	}

	if (root) {
		aggregate_totals(root);
		if (g_json) {
			print_json(root, 0);
			printf("\n");
		} else if (g_xml) {
			print_xml(root, 0);
		} else {
			print_tree(root, 0, true, "");
			if (!g_no_count) {
				printf("\n%ld directories, %ld files\n", root->total_dirs,
				 root->total_files);
			}
		}
	}

	dq_destroy(&dq);
	cli_destroy(&parser);

	return 0;
}
