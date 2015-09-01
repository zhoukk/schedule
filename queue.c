#include "queue.h"
#include "lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define INIT_LEN 16

struct queue {
	int n;
	int head;
	int tail;
	struct spinlock lock;
	void **list;
};

struct queue *queue_new(void) {
	struct queue *q = (struct queue *)malloc(sizeof *q);
	if (!q) return 0;
	q->n = INIT_LEN;
	q->head = 0;
	q->tail = 0;
	q->list = (void **)malloc(INIT_LEN * sizeof(void *));
	spinlock_init(&q->lock);
	return q;
}

void queue_free(struct queue *q, void(*dtor)(void *)) {
	spinlock_lock(&q->lock);
	if (dtor) {
		int head = q->head;
		int tail = q->tail;
		if (tail < head) tail += q->n;
		int i;
		for (i = head; i < tail; i++)
			dtor(q->list[i%q->n]);
	}
	spinlock_unit(&q->lock);
	free(q->list);
	free(q);
}

void queue_push(struct queue *q, void *msg) {
	spinlock_lock(&q->lock);
	int tail = q->tail;
	q->list[tail] = msg;
	++tail;
	if (tail >= q->n) tail = 0;
	if (tail == q->head) {
		void **list = (void **)malloc(sizeof(void *)*q->n * 2);
		assert(list);
		int i;
		int head = q->head;
		for (i = 0; i < q->n; i++) {
			list[i] = q->list[head%q->n];
			++head;
		}
		free(q->list);
		q->list = list;
		q->head = 0;
		q->tail = q->n;
		q->n *= 2;
	} else {
		q->tail = tail;
	}
	spinlock_unlock(&q->lock);
}

void *queue_pop(struct queue *q) {
	if (q->head == q->tail) return 0;
	spinlock_lock(&q->lock);
	if (q->head == q->tail) {
		spinlock_unlock(&q->lock);
		return 0;
	}
	void *data = q->list[q->head];
	++q->head;
	if (q->head == q->n) q->head = 0;
	spinlock_unlock(&q->lock);
	return data;
}

int queue_empty(struct queue *q) {
	return q->head == q->tail;
}