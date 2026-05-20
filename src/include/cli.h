#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

extern int g_max_depth;
extern bool g_no_format;
extern bool g_show_hidden;
extern bool g_json;
extern bool g_xml;
extern bool g_no_color;
extern bool g_no_indent;
extern bool g_prune;
extern bool g_show_size;
extern bool g_dirs_only;
extern bool g_no_count;
extern bool g_follow_links;
extern bool g_single_thread;
extern int g_max_thread;

typedef struct {
	const char* dir;
} TreeConfig;

int cb_json(int argc, char** argv, void* user_data);
int cb_xml(int argc, char** argv, void* user_data);
int cb_no_color(int argc, char** argv, void* user_data);
int cb_no_indent(int argc, char** argv, void* user_data);
int cb_prune(int argc, char** argv, void* user_data);
int cb_show_size(int argc, char** argv, void* user_data);
int cb_dirs_only(int argc, char** argv, void* user_data);
int cb_no_count(int argc, char** argv, void* user_data);
int cb_follow_links(int argc, char** argv, void* user_data);
int cb_max_depth(int argc, char** argv, void* user_data);
int cb_show_hidden(int argc, char** argv, void* user_data);
int cb_no_format(int argc, char** argv, void* user_data);
int cb_single_thread(int argc, char** argv, void* user_data);
int cb_max_threads(int argc, char** argv, void* user_data);

#endif
