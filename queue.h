#ifndef _queue_h_
#define _queue_h_

struct queue;

struct queue *queue_new(void);
void queue_free(struct queue *, void(*dtor)(void *));

void queue_push(struct queue *, void *msg);
void *queue_pop(struct queue *);

int queue_empty(struct queue *);

#endif // _queue_h_
