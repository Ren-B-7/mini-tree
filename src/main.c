#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

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

#include "include/minicli.h"
#include "include/output.h"
#include "include/types.h"
#include "include/fs.h"
#include "include/threading.h"

int g_max_depth = -1;
bool g_no_format = false;
bool g_show_hidden = false;

typedef struct {
	const char* dir;
} TreeConfig;

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

static void print_tree(TreeNode* node, int level, bool is_last, const char* prefix)
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

	char new_prefix[4096];
	/* Ensure no truncation, though 4096 should be plenty for deep trees */
	int written = snprintf(new_prefix, sizeof(new_prefix), "%s%s   ", prefix,
	 is_last ? " " : "|");
	(void) written;


	for (int i = 0; i < node->file_count; i++) {
		char full_path[PATH_BUF];
		if (snprintf(full_path, sizeof(full_path), "%s/%s", node->name,
		     node->files[i]) >= (int) sizeof(full_path)) {
			/* Path truncated */
		}

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
	if (nproc < 1) nproc = 1;
	if (nproc > MAX_THREADS) nproc = MAX_THREADS;

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
		print_tree(root, 0, true, "");
	}

	dq_destroy(&dq);
	cli_destroy(&parser);

	return 0;
}
