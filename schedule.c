#include "schedule.h"
#include "queue.h"
#include "index.h"
#include "lock.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define TASK_RUNNING 0
#define TASK_READY 1
#define TASK_BLOCKED 2
#define TASK_DEAD 3
#define TASK_INIT 4

struct schedule {
	int thread;
	int cur;
	struct queue **q;
	struct index *channel;
	struct index *task;
};

struct channel {
	struct queue *q;
	struct queue *r;
};

struct task {
	int tid;
	void *ud;
	int status;
	int thread;
};

struct schedule *schedule_new(int thread) {
	struct schedule *s = (struct schedule *)malloc(sizeof *s);
	if (!s) return 0;
	s->thread = thread;
	s->q = (struct queue **)calloc(thread, sizeof(struct queue *));
	int i;
	for (i = 0; i < thread; i++)
		s->q[i] = queue_new();
	s->channel = index_new();
	s->task = index_new();
	s->cur = 0;
	return s;
}

void schedule_free(struct schedule *s) {
	if (s) {
		int i;
		for (i = 0; i < s->thread; i++) {
			queue_free(s->q[i], 0);
		}
		free(s->q);
		free(s);
	}
}

int schedule_thread(struct schedule *s) {
	return s->thread;
}

static void _schedule_commit_task(struct schedule *s, struct task *t, int status) {
	if (atom_cas_long(&t->status, status, TASK_READY))
		queue_push(s->q[t->thread], (void *)(uintptr_t)(t->tid));
}

int schedule_task_new(struct schedule *s, void *ud) {
	struct task *t = (struct task *)malloc(sizeof *t);
	if (!t) return 0;
	int c = s->cur;
	int next = c + 1;
	if (next >= s->thread) next = 0;
	s->cur = next;
	t->thread = c;
	t->status = TASK_INIT;
	t->ud = ud;
	t->tid = index_regist(s->task, t);
	_schedule_commit_task(s, t, TASK_INIT);
	return t->tid;
}

void schedule_task_free(struct schedule *s, int tid) {
	struct task *t = (struct task *)index_release(s->task, tid);
	if (t) {
		t->status = TASK_DEAD;
		free(t);
	}
}

void *schedule_task_grab(struct schedule *s, int thread) {
	for (;;) {
		int tid = (int)(uintptr_t)queue_pop(s->q[thread]);
		if (tid == 0) {
			int i;
			for (i = 1; i < s->thread; i++) {
				int t = (thread + i) % s->thread;
				tid = (int)(uintptr_t)queue_pop(s->q[t]);
				if (tid) break;
			}
			if (tid == 0) return 0;
		}
		struct task *t = (struct task *)index_grab(s->task, tid);
		if (t) {
			void *ud = t->ud;
			assert(t->status != TASK_RUNNING);
			t->status = TASK_RUNNING;
			t->thread = thread;
			schedule_task_free(s, tid);
			return ud;
		}
	}
}

void schedule_task_release(struct schedule *s, int tid) {
	struct task *t = (struct task *)index_grab(s->task, tid);
	if (t) {
		assert(t->status != TASK_READY);
		if (t->status == TASK_RUNNING)
			_schedule_commit_task(s, t, TASK_RUNNING);
		schedule_task_free(s, tid);
	}
}

int schedule_channel_new(struct schedule *s) {
	struct channel *c = (struct channel *)malloc(sizeof *c);
	if (!c) return 0;
	c->q = queue_new();
	c->r = queue_new();
	return index_regist(s->channel, c);
}

void schedule_channel_free(struct schedule *s, int cid) {
	struct channel *c = (struct channel *)index_release(s->channel, cid);
	if (c) {
		queue_free(c->q, 0);
		queue_free(c->r, 0);
		free(c);
	}
}

int schedule_closed(struct schedule *s, int cid) {
	struct channel *c = (struct channel *)index_grab(s->channel, cid);
	if (c) {
		schedule_channel_free(s, cid);
		return 0;
	}
	return 1;
}

int schedule_select(struct schedule *s, int tid, int n, int *channels) {
	struct task *t = (struct task *)index_grab(s->task, tid);
	if (!t) return 0;
	int i;
	for (i = 0; i < n; i++) {
		int cid = channels[i];
		struct channel *c = (struct channel *)index_grab(s->channel, cid);
		if (!c) continue;
		if (!queue_empty(c->q)) {
			schedule_channel_free(s, cid);
			schedule_task_free(s, tid);
			return cid;
		}
		schedule_channel_free(s, cid);
	}
	t->status = TASK_BLOCKED;
	for (i = 0; i < n; i++) {
		int cid = channels[i];
		struct channel *c = (struct channel *)index_grab(s->channel, cid);
		if (c) {
			queue_push(c->r, (void *)(uintptr_t)tid);
			schedule_channel_free(s, cid);
		}
	}
	schedule_task_free(s, tid);
	return 0;
}

void *schedule_read(struct schedule *s, int cid) {
	struct channel *c = (struct channel *)index_grab(s->channel, cid);
	if (c) {
		void *msg = queue_pop(c->q);
		schedule_channel_free(s, cid);
		return msg;
	}
	return 0;
}

void schedule_write(struct schedule *s, int cid, void *msg) {
	struct channel *c = (struct channel *)index_grab(s->channel, cid);
	if (c) {
		queue_push(c->q, msg);
		for (;;) {
			int tid = (int)(uintptr_t)queue_pop(c->r);
			if (tid == 0) break;
			struct task *t = (struct task *)index_grab(s->task, tid);
			if (t) {
				if (t->status == TASK_BLOCKED)
					_schedule_commit_task(s, t, TASK_BLOCKED);
				schedule_task_free(s, tid);
			}
		}
		schedule_channel_free(s, cid);
	}
}