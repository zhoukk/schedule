#ifndef _thread_h_
#define _thread_h_

struct thread {
	void(*func)(void *);
	void *ud;
};

static void thread_join(struct thread *threads, int n);

struct thread_event;
static void thread_event_new(struct thread_event *ev);
static void thread_event_free(struct thread_event *ev);
static void thread_event_trigger(struct thread_event *ev);
static void thread_event_wait(struct thread_event *ev);

#ifdef _MSC_VER

#include <Windows.h>
#define inline __inline

static inline DWORD WINAPI _thread_func(LPVOID lpParam) {
	struct thread *t = (struct thread *)lpParam;
	t->func(t->ud);
	return 0;
}

static inline void thread_join(struct thread *threads, int n) {
	int i;
	HANDLE *handle = (HANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n*sizeof(HANDLE));
	for (i = 0; i < n; i++) {
		handle[i] = CreateThread(0, 0, _thread_func, (LPVOID)&threads[i], 0, 0);
		if (handle[i] == 0) {
			HeapFree(GetProcessHeap(), 0, handle);
			return;
		}
	}
	WaitForMultipleObjects(n, handle, TRUE, INFINITE);
	for (i = 0; i < n; i++)
		CloseHandle(handle[i]);
	HeapFree(GetProcessHeap(), 0, handle);
}

struct thread_event {
	HANDLE event;
};

static inline void thread_event_new(struct thread_event *ev) {
	ev->event = CreateEvent(0, FALSE, FALSE, 0);
}

static inline void thread_event_free(struct thread_event *ev) {
	if (ev->event) {
		CloseHandle(ev->event);
		ev->event = 0;
	}
}

static inline void thread_event_trigger(struct thread_event *ev) {
	SetEvent(ev->event);
}

static inline void thread_event_wait(struct thread_event *ev) {
	WaitForSingleObject(ev->event, INFINITE);
}

#else

#include <pthread.h>

static inline void *_thread_func(void *p) {
	struct thread *t = (struct thread *)p;
	t->func(t->ud);
	return 0;
}

static inline void thread_join(struct thread *threads, int n) {
	pthread_t pid[n];
	int i;
	for (i = 0; i < n; i++)
		if (pthread_create(&pid[i], 0, _thread_func, &threads[i])) return;
	for (i = 0; i < n; i++)
		pthread_join(pid[i], 0);
}

struct thread_event {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	int flag;
};

static inline void thread_event_new(struct thread_event *ev) {
	pthread_mutex_init(&ev->mutex, 0);
	pthread_cond_init(&ev->cond, 0);
	ev->flag = 0;
}

static inline void thread_event_free(struct thread_event *ev) {
	pthread_mutex_destroy(&ev->mutex);
	pthread_cond_destroy(&ev->cond);
}

static inline void thread_event_trigger(struct thread_event *ev) {
	pthread_mutex_lock(&ev->mutex);
	ev->flag = 1;
	pthread_mutex_unlock(&ev->mutex);
	pthread_cond_signal(&ev->cond);
}

static inline void thread_event_wait(struct thread_event *ev) {
	pthread_mutex_lock(&ev->mutex);
	while (!ev->flag) pthread_cond_wait(&ev->cond, &ev->mutex);
	ev->flag = 0;
	pthread_mutex_unlock(&ev->mutex);
}

#endif // _MSC_VER


#endif // _thread_h_