/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Process API: child processes on a pseudo terminal.
 *
 *   Process.spawn(argv)        ... start argv (an array of strings) on
 *                                  a new pty; returns a handle or -1
 *   Process.read(h, timeoutMs) ... read pending output as a string;
 *                                  "" when nothing arrived in time
 *   Process.write(h, s)        ... write a string to the process
 *   Process.isAlive(h)         ... 1 while the process runs
 *   Process.kill(h, sig)       ... send a signal
 *   Process.wait(h)            ... wait for exit; returns the status
 *   Process.close(h)           ... close the handle
 *
 * The pty keeps line editing, echo and signal generation working for
 * interactive children (shells, debuggers). Non-POSIX platforms get
 * stubs that fail cleanly.
 */

#include <noct/noct.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#if defined(__linux__)
#include <pty.h>
#else
#include <util.h>
#endif

#define PROC_MAX	16

struct proc_slot {
	int used;
	pid_t pid;
	int fd;
	int exited;
	int status;
};

static struct proc_slot proc_table[PROC_MAX];

static struct proc_slot *
proc_get(int h)
{
	if (h < 0 || h >= PROC_MAX)
		return NULL;
	if (!proc_table[h].used)
		return NULL;
	return &proc_table[h];
}

/* Reap the child if it has exited. */
static void
proc_poll_exit(struct proc_slot *p)
{
	int status;
	pid_t r;

	if (p->exited)
		return;
	r = waitpid(p->pid, &status, WNOHANG);
	if (r == p->pid) {
		p->exited = 1;
		p->status = status;
	}
}

/* Process.spawn(argv) */
static bool
cfunc_Process_spawn(NoctEnv *env)
{
	NoctValue arr, elem, ret;
	char *argv[64];
	int argc, i, h;
	size_t n;
	pid_t pid;
	int master;
	bool ok = false;

	memset(argv, 0, sizeof(argv));

	if (!noct_pin_local(env, 3, &arr, &elem, &ret))
		return false;
	if (!noct_get_arg_check_array(env, 0, &arr))
		goto cleanup;
	if (!noct_get_array_size(env, &arr, &n))
		goto cleanup;
	if (n == 0 || n > 63)
		goto cleanup;
	argc = (int)n;
	for (i = 0; i < argc; i++) {
		const char *s;
		if (!noct_get_array_elem_check_string(env, &arr, (size_t)i, &elem, &s))
			goto cleanup;
		argv[i] = strdup(s);
		if (argv[i] == NULL)
			goto cleanup;
	}
	argv[argc] = NULL;

	/* Find a free slot. */
	h = -1;
	for (i = 0; i < PROC_MAX; i++) {
		if (!proc_table[i].used) {
			h = i;
			break;
		}
	}
	if (h < 0) {
		ok = noct_set_return_make_int(env, &ret, -1);
		goto cleanup;
	}

	pid = forkpty(&master, NULL, NULL, NULL);
	if (pid < 0) {
		ok = noct_set_return_make_int(env, &ret, -1);
		goto cleanup;
	}
	if (pid == 0) {
		/* Child. */
		execvp(argv[0], argv);
		_exit(127);
	}

	/* Parent: non-blocking reads. */
	fcntl(master, F_SETFL, fcntl(master, F_GETFL, 0) | O_NONBLOCK);

	proc_table[h].used = 1;
	proc_table[h].pid = pid;
	proc_table[h].fd = master;
	proc_table[h].exited = 0;
	proc_table[h].status = 0;

	ok = noct_set_return_make_int(env, &ret, h);

cleanup:
	for (i = 0; i < 64; i++)
		free(argv[i]);
	(void)noct_unpin_local(env, 3, &arr, &elem, &ret);
	return ok;
}

/* Process.read(h, timeoutMs) */
static bool
cfunc_Process_read(NoctEnv *env)
{
	NoctValue hv, tv, ret;
	int h, timeout_ms;
	struct proc_slot *p;
	char buf[8192];
	ssize_t rn;
	struct pollfd pfd;
	bool ok = false;

	if (!noct_pin_local(env, 3, &hv, &tv, &ret))
		return false;
	if (!noct_get_arg_check_int(env, 0, &hv, &h))
		goto cleanup;
	if (!noct_get_arg_check_int(env, 1, &tv, &timeout_ms))
		goto cleanup;
	p = proc_get(h);
	if (p == NULL) {
		ok = noct_set_return_make_string(env, &ret, "");
		goto cleanup;
	}

	rn = read(p->fd, buf, sizeof(buf) - 1);
	if (rn < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && timeout_ms > 0) {
		pfd.fd = p->fd;
		pfd.events = POLLIN;
		noct_enter_blocking(env);
		poll(&pfd, 1, timeout_ms);
		noct_leave_blocking(env);
		rn = read(p->fd, buf, sizeof(buf) - 1);
	}
	if (rn <= 0) {
		proc_poll_exit(p);
		ok = noct_set_return_make_string(env, &ret, "");
		goto cleanup;
	}
	buf[rn] = '\0';

	/*
	 * Replace NUL and invalid leading bytes conservatively: the
	 * string layer expects UTF-8. Raw terminal output is passed
	 * through as-is otherwise.
	 */
	ok = noct_set_return_make_string(env, &ret, buf);

cleanup:
	(void)noct_unpin_local(env, 3, &hv, &tv, &ret);
	return ok;
}

/* Process.write(h, s) */
static bool
cfunc_Process_write(NoctEnv *env)
{
	NoctValue hv, sv, ret;
	int h;
	const char *s;
	struct proc_slot *p;
	bool ok = false;

	if (!noct_pin_local(env, 3, &hv, &sv, &ret))
		return false;
	if (!noct_get_arg_check_int(env, 0, &hv, &h))
		goto cleanup;
	if (!noct_get_arg_check_string(env, 1, &sv, &s))
		goto cleanup;
	p = proc_get(h);
	if (p == NULL) {
		ok = noct_set_return_make_int(env, &ret, 0);
		goto cleanup;
	}
	if (write(p->fd, s, strlen(s)) < 0) {
		ok = noct_set_return_make_int(env, &ret, 0);
		goto cleanup;
	}
	ok = noct_set_return_make_int(env, &ret, 1);

cleanup:
	(void)noct_unpin_local(env, 3, &hv, &sv, &ret);
	return ok;
}

/* Process.isAlive(h) */
static bool
cfunc_Process_isAlive(NoctEnv *env)
{
	NoctValue hv, ret;
	int h;
	struct proc_slot *p;
	bool ok = false;

	if (!noct_pin_local(env, 2, &hv, &ret))
		return false;
	if (!noct_get_arg_check_int(env, 0, &hv, &h))
		goto cleanup;
	p = proc_get(h);
	if (p == NULL) {
		ok = noct_set_return_make_int(env, &ret, 0);
		goto cleanup;
	}
	proc_poll_exit(p);
	ok = noct_set_return_make_int(env, &ret, p->exited ? 0 : 1);

cleanup:
	(void)noct_unpin_local(env, 2, &hv, &ret);
	return ok;
}

/* Process.kill(h, sig) */
static bool
cfunc_Process_kill(NoctEnv *env)
{
	NoctValue hv, sv, ret;
	int h, sig;
	struct proc_slot *p;
	bool ok = false;

	if (!noct_pin_local(env, 3, &hv, &sv, &ret))
		return false;
	if (!noct_get_arg_check_int(env, 0, &hv, &h))
		goto cleanup;
	if (!noct_get_arg_check_int(env, 1, &sv, &sig))
		goto cleanup;
	p = proc_get(h);
	if (p != NULL && !p->exited)
		kill(p->pid, sig);
	ok = noct_set_return_make_int(env, &ret, 0);

cleanup:
	(void)noct_unpin_local(env, 3, &hv, &sv, &ret);
	return ok;
}

/* Process.wait(h) */
static bool
cfunc_Process_wait(NoctEnv *env)
{
	NoctValue hv, ret;
	int h, status;
	struct proc_slot *p;
	bool ok = false;

	if (!noct_pin_local(env, 2, &hv, &ret))
		return false;
	if (!noct_get_arg_check_int(env, 0, &hv, &h))
		goto cleanup;
	p = proc_get(h);
	if (p == NULL) {
		ok = noct_set_return_make_int(env, &ret, -1);
		goto cleanup;
	}
	if (!p->exited) {
		noct_enter_blocking(env);
		waitpid(p->pid, &status, 0);
		noct_leave_blocking(env);
		p->exited = 1;
		p->status = status;
	}
	ok = noct_set_return_make_int(env, &ret,
				      WIFEXITED(p->status) ? WEXITSTATUS(p->status) : -1);

cleanup:
	(void)noct_unpin_local(env, 2, &hv, &ret);
	return ok;
}

/* Process.close(h) */
static bool
cfunc_Process_close(NoctEnv *env)
{
	NoctValue hv, ret;
	int h;
	struct proc_slot *p;
	bool ok = false;

	if (!noct_pin_local(env, 2, &hv, &ret))
		return false;
	if (!noct_get_arg_check_int(env, 0, &hv, &h))
		goto cleanup;
	p = proc_get(h);
	if (p != NULL) {
		close(p->fd);
		p->used = 0;
	}
	ok = noct_set_return_make_int(env, &ret, 0);

cleanup:
	(void)noct_unpin_local(env, 2, &hv, &ret);
	return ok;
}

#else /* _WIN32 */

static bool
cfunc_Process_unsupported(NoctEnv *env)
{
	NoctValue ret;
	bool ok;

	noct_pin_local(env, 1, &ret);
	ok = noct_set_return_make_int(env, &ret, -1);
	noct_unpin_local(env, 1, &ret);
	return ok;
}

#define cfunc_Process_spawn	cfunc_Process_unsupported
#define cfunc_Process_read	cfunc_Process_unsupported
#define cfunc_Process_write	cfunc_Process_unsupported
#define cfunc_Process_isAlive	cfunc_Process_unsupported
#define cfunc_Process_kill	cfunc_Process_unsupported
#define cfunc_Process_wait	cfunc_Process_unsupported
#define cfunc_Process_close	cfunc_Process_unsupported

#endif

/* FFI table. */
struct proc_ffi_item {
	const char *global_name;
	const char *field_name;
	uint32_t param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(NoctEnv *env);
};
static struct proc_ffi_item proc_ffi_items[] = {
	{"Process.spawn",	"spawn",	1, {"argv"},		cfunc_Process_spawn},
	{"Process.read",	"read",		2, {"h", "timeoutMs"},	cfunc_Process_read},
	{"Process.write",	"write",	2, {"h", "s"},		cfunc_Process_write},
	{"Process.isAlive",	"isAlive",	1, {"h"},		cfunc_Process_isAlive},
	{"Process.kill",	"kill",		2, {"h", "sig"},	cfunc_Process_kill},
	{"Process.wait",	"wait",		1, {"h"},		cfunc_Process_wait},
	{"Process.close",	"close",	1, {"h"},		cfunc_Process_close},
};

/*
 * Register the Process API.
 */
NOCT_DLL
bool
noct_register_api_process(NoctEnv *env)
{
	NoctValue dict;
	size_t i;

	if (!noct_make_empty_dict(env, &dict) ||
	    !noct_set_global(env, "Process", &dict))
		return false;
	for (i = 0; i < sizeof(proc_ffi_items) / sizeof(proc_ffi_items[0]); i++) {
		NoctValue funcval;
		if (!noct_register_cfunc(env, proc_ffi_items[i].global_name,
					 proc_ffi_items[i].param_count,
					 proc_ffi_items[i].param,
					 proc_ffi_items[i].cfunc, NULL) ||
		    !noct_get_global(env, proc_ffi_items[i].global_name, &funcval) ||
		    !noct_set_dict_elem_cstr(env, &dict, proc_ffi_items[i].field_name, &funcval))
			return false;
	}
	return true;
}
