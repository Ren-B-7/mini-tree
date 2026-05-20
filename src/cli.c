#include "include/cli.h"

#include <stdlib.h>

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
bool g_single_thread = false;
int g_max_thread = -1;

int cb_json(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_json = true;
	return 0;
}

int cb_xml(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_xml = true;
	return 0;
}

int cb_no_color(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_no_color = true;
	return 0;
}

int cb_no_indent(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_no_indent = true;
	return 0;
}

int cb_prune(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_prune = true;
	return 0;
}

int cb_show_size(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_show_size = true;
	return 0;
}

int cb_dirs_only(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_dirs_only = true;
	return 0;
}

int cb_no_count(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_no_count = true;
	return 0;
}

int cb_follow_links(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_follow_links = true;
	return 0;
}

int cb_max_depth(int argc, char** argv, void* user_data)
{
	(void) user_data;
	if (argc > 0) {
		g_max_depth = (int) strtol(argv[0], NULL, 10);
		return 1;
	}
	return 0;
}

int cb_show_hidden(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_show_hidden = true;
	return 0;
}

int cb_no_format(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_no_format = true;
	return 0;
}

int cb_single_thread(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	g_single_thread = true;
	return 0;
}

int cb_max_threads(int argc, char** argv, void* user_data)
{
	(void) argc;
	(void) argv;
	(void) user_data;
	(void) user_data;
	if (argc > 0) {
		g_max_thread = (int) strtol(argv[0], NULL, 10);
		return 1;
	}
	return 0;
}
