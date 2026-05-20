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

#include "include/cli.h"
#include "include/fs.h"
#include "include/minicli.h"
#include "include/output.h"
#include "include/threading.h"
#include "include/types.h"

int main(int argc, char* argv[])
{
	TreeConfig cfg = {.dir = "."};
	CliParser parser;
	cli_init(&parser,
	 (CliInitParams) {"tree", "A small cli directory tree tool"});

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
	 (CliArgument) {"--single-thread", "-S",
	     "Run in single threaded mode (Overrides --max-threads)",
	     cb_single_thread, &cfg});
	cli_add_argument(&parser,
	 (CliArgument) {"--max_threads", NULL,
	     "Run with max amount of threads (Clamped to 0 < THREADS < nproc)",
	     cb_max_threads, &cfg});

	int consumed = cli_parse(&parser, argc, argv);
	if (consumed < 0) {
		return 1;
	}
	if (consumed < argc) {
		cfg.dir = argv[consumed];
	}

	DirQueue dq;
	dq_init(&dq, 1024);

	TreeNode* root = process_path(cfg.dir, true, NULL, &dq);

	long nproc = (g_max_thread < 1) ? GET_NPROC() : g_max_thread;
	if (nproc < 1) {
		nproc = 1;
	}
	if (nproc > MAX_THREADS) {
		nproc = MAX_THREADS;
	}

	if (g_single_thread) {
		/* Single-threaded mode: process all tasks in the current thread */
		while (dq.active_count > 0 || !dq.finished) {
			walker_thread(&dq);
		}
	} else {
		pthread_t threads[MAX_THREADS];
		for (long i = 0; i < nproc; i++) {
			pthread_create(&threads[i], NULL, walker_thread, &dq);
		}
		for (long i = 0; i < nproc; i++) {
			pthread_join(threads[i], NULL);
		}
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
