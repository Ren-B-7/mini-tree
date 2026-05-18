#ifndef THREADING_H
#define THREADING_H

#include <stdbool.h>
#include <stddef.h>

/* Compiler attribute compat */
#ifndef COLD_ATTR
#if defined(__GNUC__) || defined(__clang__)
#define COLD_ATTR __attribute__((cold))
#else
#define COLD_ATTR
#endif
#endif

/* pthread: available natively on Linux/macOS; on Windows requires MinGW
 * winpthreads */
#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
#error \
 "Windows builds require MinGW (for pthread/dirent support). MSVC is not supported."
#else
#include <pthread.h>
#endif

#define QUEUE_INIT_CAP 4096
#define MAX_THREADS 64
#define LOCAL_INIT_CAP 1024

typedef struct {
	char** paths;
	size_t cap;
	size_t head;
	size_t tail;
	size_t count;
	bool finished;
	pthread_mutex_t lock;
	pthread_cond_t not_empty;
	pthread_cond_t not_full;
} WorkQueue;

typedef struct {
	void** data;
	int* depths;
	size_t cap;
	size_t head;
	size_t tail;
	size_t count;
	int active_count;
	bool finished;
	pthread_mutex_t lock;
	pthread_cond_t not_empty;
	pthread_cond_t not_full;
} DirQueue;

int wq_init(WorkQueue* q, size_t initial_cap);
void wq_push(WorkQueue* q, char* path);
char* wq_pop(WorkQueue* q);
int wq_pop_batch(WorkQueue* q, char** batch, int batch_size);
void wq_finish(WorkQueue* q);
void wq_destroy(WorkQueue* q);

int dq_init(DirQueue* q, size_t initial_cap);
void dq_push(DirQueue* q, void* data, int depth);
bool dq_pop(DirQueue* q, void** data, int* depth);
void dq_finish(DirQueue* q);
void dq_destroy(DirQueue* q);

void walk_dir_task(DirQueue* dq);

#endif
