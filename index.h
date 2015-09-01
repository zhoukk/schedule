#ifndef _index_h_
#define _index_h_

struct index;

struct index *index_new(void);
void index_free(struct index *);

int index_regist(struct index *, void *ud);
void *index_grab(struct index *, int id);
void *index_release(struct index *, int id);
int index_list(struct index *, int n, int *list);

#endif // _index_h_
