#ifndef _schedule_h_
#define _schedule_h_

struct schedule;

struct schedule *schedule_new(int thread);
void schedule_free(struct schedule *);
int schedule_thread(struct schedule *);

int schedule_task_new(struct schedule *, void *ud);
void schedule_task_free(struct schedule *, int tid);
void *schedule_task_grab(struct schedule *, int thread);
void schedule_task_release(struct schedule *, int tid);

int schedule_channel_new(struct schedule *);
void schedule_channel_free(struct schedule *, int cid);
int schedule_closed(struct schedule *, int cid);
int schedule_select(struct schedule *, int tid, int n, int *channels);

void *schedule_read(struct schedule *, int cid);
void schedule_write(struct schedule *, int cid, void *msg);

#endif // _schedule_h_
