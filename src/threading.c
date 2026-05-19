#include "include/threading.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF 4096

/* ─── WorkQueue ─────────────────────────────────────────────────────────── */

int wq_init(WorkQueue* q, size_t initial_cap)
{
	q->paths = (char**) malloc(sizeof(char*) * initial_cap);
	if (!q->paths) {
		return -1;
	}
	q->cap = initial_cap;
	q->head = 0;
	q->tail = 0;
	q->count = 0;
	q->finished = false;
	pthread_mutex_init(&q->lock, NULL);
	pthread_cond_init(&q->not_empty, NULL);
	pthread_cond_init(&q->not_full, NULL);
	return 0;
}

void wq_push(WorkQueue* q, char* path)
{
	pthread_mutex_lock(&q->lock);
	while (q->count == q->cap) {
		size_t new_cap = q->cap * 2;
		char** new_buf = (char**) malloc(sizeof(char*) * new_cap);
		if (!new_buf) {
			pthread_cond_wait(&q->not_full, &q->lock);
			continue;
		}
		for (size_t i = 0; i < q->count; i++) {
			new_buf[i] = q->paths[(q->head + i) % q->cap];
		}
		free((void*) q->paths);
		q->paths = new_buf;
		q->head = 0;
		q->tail = q->count;
		q->cap = new_cap;
	}
	q->paths[q->tail] = path;
	q->tail = (q->tail + 1) % q->cap;
	q->count++;
	pthread_cond_signal(&q->not_empty);
	pthread_mutex_unlock(&q->lock);
}

char* wq_pop(WorkQueue* q)
{
	pthread_mutex_lock(&q->lock);
	while (q->count == 0 && !q->finished) {
		pthread_cond_wait(&q->not_empty, &q->lock);
	}
	if (q->count == 0) {
		pthread_cond_broadcast(&q->not_empty);
		pthread_mutex_unlock(&q->lock);
		return NULL;
	}
	char* path = q->paths[q->head];
	q->head = (q->head + 1) % q->cap;
	q->count--;
	pthread_cond_signal(&q->not_full);
	pthread_mutex_unlock(&q->lock);
	return path;
}

int wq_pop_batch(WorkQueue* q, char** batch, int batch_size)
{
	pthread_mutex_lock(&q->lock);
	while (q->count == 0 && !q->finished) {
		pthread_cond_wait(&q->not_empty, &q->lock);
	}
	if (q->count == 0) {
		pthread_cond_broadcast(&q->not_empty);
		pthread_mutex_unlock(&q->lock);
		return 0;
	}
	int n_popped = 0;
	while (n_popped < batch_size && q->count > 0) {
		batch[n_popped] = q->paths[q->head];
		q->head = (q->head + 1) % q->cap;
		q->count--;
		n_popped++;
	}
	pthread_cond_broadcast(&q->not_full);
	pthread_mutex_unlock(&q->lock);
	return n_popped;
}

void wq_finish(WorkQueue* q)
{
	pthread_mutex_lock(&q->lock);
	q->finished = true;
	pthread_cond_broadcast(&q->not_empty);
	pthread_mutex_unlock(&q->lock);
}

COLD_ATTR void wq_destroy(WorkQueue* q)
{
	free((void*) q->paths);
	pthread_cond_destroy(&q->not_full);
	pthread_cond_destroy(&q->not_empty);
	pthread_mutex_destroy(&q->lock);
}

/* ─── DirQueue ───────────────────────────────────────────────────────────── */

int dq_init(DirQueue* q, size_t initial_cap)
{
	q->data = (void**) malloc(sizeof(void*) * initial_cap);
	q->depths = (int*) malloc(sizeof(int) * initial_cap);
	if (!q->data || !q->depths) {
		return -1;
	}
	q->cap = initial_cap;
	q->head = 0;
	q->tail = 0;
	q->count = 0;
	q->active_count = 0;
	q->finished = false;
	pthread_mutex_init(&q->lock, NULL);
	pthread_cond_init(&q->not_empty, NULL);
	pthread_cond_init(&q->not_full, NULL);
	return 0;
}

/*
 * dq_push — grow the ring buffer in-place instead of blocking on not_full.
 *
 * The original code had no growth path for DirQueue; if the queue filled it
 * would deadlock (the traversal thread pushing would block, but all consumer
 * threads were also waiting for work).  We now double the buffer whenever it
 * is full, matching the WorkQueue behaviour.
 *
 * active_count is incremented here, under the lock, so that the count is
 * already correct by the time any worker thread wakes and tries to pop.
 * This closes the race in the original where active_count was set in main()
 * before the root node was pushed, allowing threads to see
 * active_count==0 && count==0 and declare termination prematurely.
 */
void dq_push(DirQueue* q, void* data, int depth)
{
	pthread_mutex_lock(&q->lock);
	printf("DEBUG: dq_push: pushing node, old count=%zu, old active=%d\n",
	 q->count, q->active_count);

	/* Grow the ring buffer if full */
	if (q->count == q->cap) {
		size_t new_cap = q->cap * 2;
		void** new_data = (void**) malloc(sizeof(void*) * new_cap);
		int* new_depths = (int*) malloc(sizeof(int) * new_cap);
		if (!new_data || !new_depths) {
			/* Allocation failure: fall back to blocking on not_full.
			 * Free whichever succeeded to avoid a leak. */
			free((void*) new_data);
			free((void*) new_depths);
			while (q->count == q->cap) {
				pthread_cond_wait(&q->not_full, &q->lock);
			}
		} else {
			for (size_t i = 0; i < q->count; i++) {
				new_data[i] = q->data[(q->head + i) % q->cap];
				new_depths[i] = q->depths[(q->head + i) % q->cap];
			}
			free((void*) q->data);
			free((void*) q->depths);
			q->data = new_data;
			q->depths = new_depths;
			q->head = 0;
			q->tail = q->count;
			q->cap = new_cap;
		}
	}

	q->data[q->tail] = data;
	q->depths[q->tail] = depth;
	q->tail = (q->tail + 1) % q->cap;
	q->count++;

	/*
	 * Increment active_count here so it accurately reflects the number of
	 * nodes in-flight (queued or being processed).  dq_pop decrements it
	 * when a thread finishes a node and is about to block waiting for more
	 * work.  When active_count reaches 0 with an empty queue, all work is
	 * done.
	 */
	q->active_count++;
	printf("DEBUG: dq_push: pushed node, new count=%zu, new active=%d\n",
	 q->count, q->active_count);

	pthread_cond_signal(&q->not_empty);
	pthread_mutex_unlock(&q->lock);
}

/*
 * dq_pop — decrement active_count AFTER finishing the previous item.
 *
 * A thread calls dq_pop when it has finished processing one node and wants
 * the next.  At that point it is no longer "active" on anything, so we
 * decrement before blocking.  If we are the last thread to go idle and the
 * queue is empty, we set finished and wake everyone so they can exit.
 */
bool dq_pop(DirQueue* q, void** data, int* depth)
{
	pthread_mutex_lock(&q->lock);
	printf("DEBUG: dq_pop: attempting pop, count=%zu, active=%d, finished=%d\n",
	 q->count, q->active_count, q->finished);

	while (q->count == 0 && !q->finished) {
		pthread_cond_wait(&q->not_empty, &q->lock);
	}

	if (q->count == 0) {
		printf("DEBUG: dq_pop: no items, finished=%d\n", q->finished);
		pthread_mutex_unlock(&q->lock);
		return false;
	}

	/*
	 * We have successfully taken an item.
	 * We were "inactive" before, but now we are "active" on this new node.
	 */
	*data = q->data[q->head];
	*depth = q->depths[q->head];
	q->head = (q->head + 1) % q->cap;
	q->count--;

	printf("DEBUG: dq_pop: popped item, count=%zu, active=%d\n", q->count,
	 q->active_count);

	/*
	 * If we were previously waiting (idle), our exit from wait means we
	 * are no longer idle. But the logic here is slightly different because
	 * dq_push already accounted for the 'active' count.
	 *
	 * The real issue: `q->active_count--` should only happen when a worker
	 * finishes a task, NOT when it picks one up.
	 */
	pthread_cond_signal(&q->not_full);
	pthread_mutex_unlock(&q->lock);
	return true;
}

/*
 * dq_node_done - call this when the work on a popped node is finished.
 */
void dq_node_done(DirQueue* q)
{
	pthread_mutex_lock(&q->lock);
	q->active_count--;
	printf("DEBUG: dq_node_done: active_count=%d, count=%zu\n", q->active_count,
	 q->count);
	if (q->active_count == 0 && q->count == 0) {
		q->finished = true;
		printf("DEBUG: dq_node_done: setting finished=true and broadcasting\n");
		pthread_cond_broadcast(&q->not_empty);
	}
	pthread_mutex_unlock(&q->lock);
}

COLD_ATTR void dq_destroy(DirQueue* q)
{
	free((void*) q->data);
	free((void*) q->depths);
	pthread_cond_destroy(&q->not_full);
	pthread_cond_destroy(&q->not_empty);
	pthread_mutex_destroy(&q->lock);
}
