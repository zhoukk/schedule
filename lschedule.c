#include "schedule.h"
#include "serial.h"
#include "thread.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include <stdio.h>
#include <stdlib.h>

struct task {
	lua_State *L;
	int tid;
};

struct monitor;

struct worker {
	struct monitor *m;
	struct thread_event event;
	int thread;
	int suspend;
};

struct monitor {
	int thread;
	struct worker *w;
};

static struct schedule *S = 0;

static int linit_task(lua_State *L) {
	luaL_openlibs(L);
	const char *filename = (const char *)lua_touserdata(L, 1);
	if (LUA_OK != luaL_loadfile(L, filename)) {
		lua_error(L);
	}
	return 1;
}

static int linit(lua_State *L) {
	if (S) {
		return luaL_error(L, "schedule already init");
	}
	S = schedule_new((int)luaL_checkinteger(L, 1));
	return 0;
}

static int run_task(struct worker *w) {
	struct task *t = schedule_task_grab(S, w->thread);
	if (!t) {
		return 0;
	}
	lua_State *L = t->L;
	int args = lua_gettop(L) - 1;
	int ret = lua_resume(L, 0, args);
	if (ret == LUA_YIELD) {
		lua_settop(L, 0);
		schedule_task_release(S, t->tid);
		return 1;
	}
	if (ret != LUA_OK) {
		printf("error %s\n", lua_tostring(L, -1));
	}
	t->L = 0;
	lua_close(L);
	schedule_task_free(S, t->tid);
	schedule_task_release(S, t->tid);
	return 1;
}

static void worker(void *p) {
	struct worker *w = (struct worker *)p;
	for (;;) {
		w->suspend = 0;
		while (run_task(w)) {
			struct monitor *m = w->m;
			int i;
			for (i = 0; i < m->thread; i++) {
				struct worker *w = &m->w[i];
				if (w->suspend) {
					thread_event_trigger(&w->event);
				}
			}
		}
		w->suspend = 1;
		thread_event_wait(&w->event);
	}
}

static int lrun(lua_State *L) {
	int thread = schedule_thread(S);
#ifdef _MSC_VER
	struct worker *w = (struct worker *)_alloca(thread*sizeof(struct worker));
	struct thread *t = (struct thread *)_alloca(thread*sizeof(struct thread));
#else
	struct worker w[thread];
	struct thread t[thread];
#endif // _MSC_VER
	struct monitor m;
	m.thread = thread;
	m.w = w;
	int i;
	for (i = 0; i < thread; i++) {
		w[i].thread = i;
		w[i].suspend = 0;
		w[i].m = &m;
		thread_event_new(&w[i].event);
		t[i].func = worker;
		t[i].ud = &w[i];
	}
	thread_join(t, thread);
	for (i = 0; i < thread; i++) {
		thread_event_free(&w[i].event);
	}
	return 0;
}

static int ltask(lua_State *L) {
	lua_State *taskL = luaL_newstate();
	if (!taskL) {
		return luaL_error(L, "luaL_newstate failed");
	}
	const char *filename = luaL_checkstring(L, 1);
	lua_pushcfunction(taskL, linit_task);
	lua_pushlightuserdata(taskL, (void *)filename);
	if (LUA_OK != lua_pcall(taskL, 1, 1, 0)) {
		size_t size;
		const char *msg = lua_tolstring(taskL, -1, &size);
		if (msg) {
			lua_pushlstring(L, msg, size);
			lua_close(taskL);
			lua_error(L);
		} else {
			lua_close(taskL);
			return luaL_error(L, "new task %s error", filename);
		}
	}
	lua_pushcfunction(L, lpack);
	lua_insert(L, 2);
	int top = lua_gettop(L);
	lua_call(L, top - 2, 1);
	void *args = lua_touserdata(L, 2);
	lua_pushcfunction(taskL, lunpack);
	lua_pushlightuserdata(taskL, args);
	if (LUA_OK != lua_pcall(taskL, 1, LUA_MULTRET, 0)) {
		lua_close(taskL);
		return luaL_error(L, "new task %s argments error", filename);
	}
	struct task *t = (struct task *)malloc(sizeof *t);
	t->L = taskL;
	t->tid = schedule_task_new(S, t);
	lua_pushinteger(taskL, t->tid);
	lua_rawsetp(taskL, LUA_REGISTRYINDEX, S);
	lua_pushinteger(L, t->tid);
	return 1;
}

static int lchannel(lua_State *L) {
	lua_pushinteger(L, schedule_channel_new(S));
	return 1;
}

static int lread(lua_State *L) {
	int cid = (int)luaL_checkinteger(L, 1);
	void *msg = schedule_read(S, cid);
	if (!msg) {
		if (schedule_closed(S, cid)) {
			return 0;
		}
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_settop(L, 0);
	lua_pushboolean(L, 1);
	lua_pushcfunction(L, lunpack);
	lua_pushlightuserdata(L, msg);
	lua_call(L, 1, LUA_MULTRET);
	return lua_gettop(L);
}

static int lwrite(lua_State *L) {
	int cid = (int)luaL_checkinteger(L, 1);
	lua_pushcfunction(L, lpack);
	lua_replace(L, 1);
	int top = lua_gettop(L);
	lua_call(L, top - 1, 1);
	void *msg = lua_touserdata(L, 1);
	schedule_write(S, cid, msg);
	return 0;
}

static int lselect(lua_State *L) {
	lua_rawgetp(L, LUA_REGISTRYINDEX, S);
	if (lua_type(L, -1) != LUA_TNUMBER) {
		return luaL_error(L, "select need a task");
	}
	int tid = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);
	int n = lua_gettop(L);
#ifdef _MSC_VER
	int *channels = (int *)_alloca(n*sizeof(int));
#else
	int channels[n];
#endif // _MSC_VER
	int i;
	for (i = 0; i < n; i++) {
		channels[i] = (int)luaL_checkinteger(L, i + 1);
	}
	int cid = schedule_select(S, tid, n, channels);
	if (cid == 0) {
		return 0;
	}
	lua_pushinteger(L, cid);
	return 1;
}

static int lclose(lua_State *L) {
	schedule_channel_free(S, (int)luaL_checkinteger(L, 1));
	return 0;
}

static int ltaskid(lua_State *L) {
	lua_rawgetp(L, LUA_REGISTRYINDEX, S);
	return 1;
}

#ifdef _MSC_VER
#define SCHEDULE_EXPORT __declspec(dllexport)
#else
#define SCHEDULE_EXPORT
#endif // _MSC_VER

SCHEDULE_EXPORT int luaopen_schedule(lua_State *L) {
	luaL_Reg l[] = {
		{"init", linit},
		{"run", lrun},
		{"task", ltask},
		{"channel", lchannel},
		{"select", lselect},
		{"read", lread},
		{"write", lwrite},
		{"close", lclose},
		{"taskid", ltaskid},
		{0, 0},
	};
	luaL_checkversion(L);
	luaL_newlib(L, l);
	return 1;
}
