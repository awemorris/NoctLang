/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * API: HttpServer.*
 *
 * A minimal socket layer for writing HTTP servers in Noct.
 *
 * Design notes:
 *
 *  - Handles (listener sockets, connections and pollers) are
 *    dictionaries carrying a native pointer to a control block.
 *
 *  - A poller is a stateful handle rather than a per-call handle set.
 *    Registrations are added and removed incrementally, which maps
 *    directly onto epoll_ctl()/kqueue() and avoids rebuilding the
 *    watch set on every wait. The poll(2) backend keeps a persistent
 *    pollfd array for the same reason.
 *
 *  - Readiness is level-triggered, so the poll(2) and epoll backends
 *    behave identically. Edge-triggered mode is intentionally absent.
 *
 *  - A socket belongs to at most one poller. Closing a socket detaches
 *    it automatically, so a closed descriptor can never linger in a
 *    watch set.
 *
 *  - The poller stores its socket handles in its own handle dictionary
 *    (the "socks" key), keyed by registration id. The native side only
 *    ever holds file descriptors and registration ids, never raw
 *    object pointers, so a moving GC cannot invalidate anything.
 *
 *  - Blocking calls (accept, recv, send, wait) are wrapped in
 *    noct_enter_blocking()/noct_leave_blocking() so a blocked thread
 *    never stalls a stop-the-world GC. This is also what allows a
 *    future thread-pool design: a connection handle carries no
 *    thread affinity, so the thread that accepts it and the thread
 *    that serves it may differ.
 */

#if defined(_WIN32) && !defined(_WIN32_WINNT)
/* WSAPoll is available starting with Windows Vista. */
#define _WIN32_WINNT	0x0600
#endif

#include <noct/noct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#if defined(NOCT_TARGET_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
typedef int socklen_arg_t;
typedef WSAPOLLFD sock_pollfd_t;
#define SOCK_INVALID		INVALID_SOCKET
#define sock_close		closesocket
#define sock_poll		WSAPoll
#define SOCK_WOULDBLOCK()	(WSAGetLastError() == WSAEWOULDBLOCK)
#define SOCK_INTR()		(WSAGetLastError() == WSAEINTR)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
typedef int sock_t;
typedef socklen_t socklen_arg_t;
typedef struct pollfd sock_pollfd_t;
#define SOCK_INVALID		(-1)
#define sock_close		close
#define sock_poll		poll
#define SOCK_WOULDBLOCK()	(errno == EAGAIN || errno == EWOULDBLOCK)
#define SOCK_INTR()		(errno == EINTR)
#endif

/* Magic numbers for the handle control blocks. */
#define SOCKET_MAGIC	0x536b7431	/* 'Skt1' */
#define POLLER_MAGIC	0x506c7231	/* 'Plr1' */

/* Event flags. Must match the values documented for Noct scripts. */
#define EVENT_READ	0x1
#define EVENT_WRITE	0x2

/* Maximum bytes accepted by a single recv() call. */
#define RECV_MAX	(16 * 1024 * 1024)

/* Socket handle control block. */
struct socket_obj {
	int magic;
	sock_t fd;

	/* Registration id in the owning poller, or -1 if not registered. */
	int reg_id;

	/* Owning poller, or NULL. */
	struct poller_obj *poller;
};

/* One poller registration. */
struct poller_entry {
	int reg_id;
	sock_t fd;
	int events;
};

/* Poller control block. */
struct poller_obj {
	int magic;

	/* Registrations, kept dense. */
	struct poller_entry *entry;
	size_t entry_count;
	size_t entry_alloc;

	/* Persistent pollfd array, rebuilt only when registrations change. */
	sock_pollfd_t *pfd;
	size_t pfd_alloc;
	bool pfd_dirty;

	/* Next registration id. */
	int next_reg_id;
};

/*
 * Forward declaration.
 */

/*
 * [Sockets]
 *
 * var server = HttpServer.listen("0.0.0.0", 8080);
 * var conn = HttpServer.accept(server);
 * var text = HttpServer.recv(conn, 4096);
 * HttpServer.send(conn, "HTTP/1.1 200 OK\r\n\r\nhi");
 * HttpServer.close(conn);
 */
static bool cfunc_HttpServer_listen(NoctEnv *env);
static bool cfunc_HttpServer_connect(NoctEnv *env);
static bool cfunc_HttpServer_accept(NoctEnv *env);
static bool cfunc_HttpServer_recv(NoctEnv *env);
static bool cfunc_HttpServer_send(NoctEnv *env);
static bool cfunc_HttpServer_close(NoctEnv *env);
static bool cfunc_HttpServer_isClosed(NoctEnv *env);
static bool cfunc_HttpServer_setBlocking(NoctEnv *env);
static bool cfunc_HttpServer_getPeer(NoctEnv *env);

/*
 * [Poller]
 *
 * var poller = HttpServer.createPoller();
 * HttpServer.addToPoller(poller, server, HttpServer.READ);
 * var ready = HttpServer.waitPoller(poller, 1000);
 * for (r in ready) {
 *     handle(r.socket, r.events);
 * }
 */
static bool cfunc_HttpServer_createPoller(NoctEnv *env);
static bool cfunc_HttpServer_addToPoller(NoctEnv *env);
static bool cfunc_HttpServer_modifyPoller(NoctEnv *env);
static bool cfunc_HttpServer_removeFromPoller(NoctEnv *env);
static bool cfunc_HttpServer_waitPoller(NoctEnv *env);
static bool cfunc_HttpServer_countPoller(NoctEnv *env);

static void socket_finalizer(void *native_pointer);
static void poller_finalizer(void *native_pointer);

/* FFI table. */
struct ffi_item {
	const char *global_name;
	const char *package_name;
	const char *field_name;
	size_t param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(NoctEnv *env);
};
static struct ffi_item ffi_items[] = {
	{"HttpServer.listen",		"HttpServer",	"listen",		2,	{"host", "port"},		cfunc_HttpServer_listen},
	{"HttpServer.connect",		"HttpServer",	"connect",		2,	{"host", "port"},		cfunc_HttpServer_connect},
	{"HttpServer.accept",		"HttpServer",	"accept",		1,	{"server"},			cfunc_HttpServer_accept},
	{"HttpServer.recv",		"HttpServer",	"recv",			2,	{"conn", "maxBytes"},		cfunc_HttpServer_recv},
	{"HttpServer.send",		"HttpServer",	"send",			2,	{"conn", "data"},		cfunc_HttpServer_send},
	{"HttpServer.close",		"HttpServer",	"close",		1,	{"sock"},			cfunc_HttpServer_close},
	{"HttpServer.isClosed",		"HttpServer",	"isClosed",		1,	{"sock"},			cfunc_HttpServer_isClosed},
	{"HttpServer.setBlocking",	"HttpServer",	"setBlocking",		2,	{"sock", "blocking"},		cfunc_HttpServer_setBlocking},
	{"HttpServer.getPeer",		"HttpServer",	"getPeer",		1,	{"conn"},			cfunc_HttpServer_getPeer},
	{"HttpServer.createPoller",	"HttpServer",	"createPoller",		0,	{NULL},				cfunc_HttpServer_createPoller},
	{"HttpServer.addToPoller",	"HttpServer",	"addToPoller",		3,	{"poller", "sock", "events"},	cfunc_HttpServer_addToPoller},
	{"HttpServer.modifyPoller",	"HttpServer",	"modifyPoller",		3,	{"poller", "sock", "events"},	cfunc_HttpServer_modifyPoller},
	{"HttpServer.removeFromPoller",	"HttpServer",	"removeFromPoller",	2,	{"poller", "sock"},		cfunc_HttpServer_removeFromPoller},
	{"HttpServer.waitPoller",	"HttpServer",	"waitPoller",		2,	{"poller", "timeout"},		cfunc_HttpServer_waitPoller},
	{"HttpServer.countPoller",	"HttpServer",	"countPoller",		1,	{"poller"},			cfunc_HttpServer_countPoller},
};

/*
 * Register "HttpServer.*" functions.
 */
NOCT_DLL
bool
noct_register_api_httpserver(
	NoctEnv *env)
{
	NoctValue srv_dict, tmp;
	size_t i;

	memset(&srv_dict, 0, sizeof(NoctValue));
	memset(&tmp, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &srv_dict, &tmp);

#if defined(NOCT_TARGET_WINDOWS)
	{
		WSADATA wsa;
		static bool initialized = false;
		if (!initialized) {
			if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
				noct_error(env, N_TR("Cannot initialize the socket library."));
				return false;
			}
			initialized = true;
		}
	}
#else
	/* Do not die on a write to a closed connection. */
	signal(SIGPIPE, SIG_IGN);
#endif

	/* Make a global variable "HttpServer". */
	if (!noct_make_empty_dict(env, &srv_dict))
		return false;
	if (!noct_set_global(env, "HttpServer", &srv_dict))
		return false;

	/* Register functions. */
	for (i = 0; i < sizeof(ffi_items) / sizeof(struct ffi_item); i++) {
		NoctValue funcval;

		memset(&funcval, 0, sizeof(NoctValue));

		/* Register a cfunc. */
		if (!noct_register_cfunc(
			    env,
			    ffi_items[i].global_name,
			    ffi_items[i].param_count,
			    ffi_items[i].param,
			    ffi_items[i].cfunc,
			    NULL))
			return false;

		/* Get a function value. */
		if (!noct_get_global(env, ffi_items[i].global_name, &funcval))
			return false;

		/* Make a dictionary element. */
		if (!noct_set_dict_elem_cstr(env, &srv_dict, ffi_items[i].field_name, &funcval))
			return false;
	}

	/* Register the event flags. */
	if (!noct_set_dict_elem_make_int(env, &srv_dict, "READ", &tmp, EVENT_READ))
		return false;
	if (!noct_set_dict_elem_make_int(env, &srv_dict, "WRITE", &tmp, EVENT_WRITE))
		return false;

	noct_unpin_local(env, 2, &srv_dict, &tmp);

	return true;
}

/*
 * Handle helpers
 */

/* Get a control block from a handle dictionary with a magic check. */
static bool
get_handle_native(
	NoctEnv *env,
	NoctValue *handle,
	int magic,
	void **native)
{
	void (*finalizer)(void *);

	if (!noct_get_dict_native_pointer(env, handle, native, &finalizer))
		return false;
	if (*native == NULL || *(int *)*native != magic) {
		noct_error(env, N_TR("Invalid handle."));
		return false;
	}
	return true;
}

/* Get a socket control block that is still open. */
static bool
get_open_socket(
	NoctEnv *env,
	NoctValue *handle,
	struct socket_obj **sock)
{
	if (!get_handle_native(env, handle, SOCKET_MAGIC, (void **)sock))
		return false;
	if ((*sock)->fd == SOCK_INVALID) {
		noct_error(env, N_TR("Socket is already closed."));
		return false;
	}
	return true;
}

/* Make a socket handle dictionary for an open descriptor. */
static bool
make_socket_handle(
	NoctEnv *env,
	NoctValue *handle,
	sock_t fd)
{
	struct socket_obj *obj;

	obj = noct_malloc(sizeof(struct socket_obj));
	if (obj == NULL) {
		noct_out_of_memory(env);
		return false;
	}
	memset(obj, 0, sizeof(struct socket_obj));
	obj->magic = SOCKET_MAGIC;
	obj->fd = fd;
	obj->reg_id = -1;
	obj->poller = NULL;

	if (!noct_make_empty_dict(env, handle)) {
		noct_free(obj);
		return false;
	}
	if (!noct_set_dict_native_pointer(env, handle, obj, socket_finalizer)) {
		noct_free(obj);
		return false;
	}

	return true;
}

static void
socket_finalizer(
	void *native_pointer)
{
	struct socket_obj *obj;

	obj = (struct socket_obj *)native_pointer;
	if (obj == NULL)
		return;

	/*
	 * A socket that is still registered keeps its handle dictionary
	 * alive through the poller, so reaching the finalizer means the
	 * registration is already gone.
	 */
	if (obj->fd != SOCK_INVALID)
		sock_close(obj->fd);

	noct_free(obj);
}

static void
poller_finalizer(
	void *native_pointer)
{
	struct poller_obj *obj;

	obj = (struct poller_obj *)native_pointer;
	if (obj == NULL)
		return;

	noct_free(obj->entry);
	noct_free(obj->pfd);
	noct_free(obj);
}

/*
 * Poller registration table
 */

/* Find a registration by id. Returns the index, or -1. */
static int
poller_find(
	struct poller_obj *poller,
	int reg_id)
{
	size_t i;

	for (i = 0; i < poller->entry_count; i++) {
		if (poller->entry[i].reg_id == reg_id)
			return (int)i;
	}

	return -1;
}

/* Append a registration. */
static bool
poller_add_entry(
	NoctEnv *env,
	struct poller_obj *poller,
	sock_t fd,
	int events,
	int *ret_reg_id)
{
	if (poller->entry_count == poller->entry_alloc) {
		size_t new_alloc;
		struct poller_entry *new_entry;

		new_alloc = poller->entry_alloc == 0 ? 16 : poller->entry_alloc * 2;
		new_entry = noct_realloc(poller->entry, new_alloc * sizeof(struct poller_entry));
		if (new_entry == NULL) {
			noct_out_of_memory(env);
			return false;
		}
		poller->entry = new_entry;
		poller->entry_alloc = new_alloc;
	}

	poller->entry[poller->entry_count].reg_id = poller->next_reg_id++;
	poller->entry[poller->entry_count].fd = fd;
	poller->entry[poller->entry_count].events = events;
	*ret_reg_id = poller->entry[poller->entry_count].reg_id;
	poller->entry_count++;
	poller->pfd_dirty = true;

	return true;
}

/* Remove a registration by index. */
static void
poller_remove_entry(
	struct poller_obj *poller,
	int index)
{
	if (index < 0 || (size_t)index >= poller->entry_count)
		return;

	if ((size_t)index != poller->entry_count - 1) {
		poller->entry[index] = poller->entry[poller->entry_count - 1];
	}
	poller->entry_count--;
	poller->pfd_dirty = true;
}

/* Rebuild the pollfd array if the registrations changed. */
static bool
poller_sync(
	NoctEnv *env,
	struct poller_obj *poller)
{
	size_t i;

	if (!poller->pfd_dirty)
		return true;

	if (poller->entry_count > poller->pfd_alloc) {
		size_t new_alloc;
		sock_pollfd_t *new_pfd;

		new_alloc = poller->pfd_alloc == 0 ? 16 : poller->pfd_alloc;
		while (new_alloc < poller->entry_count)
			new_alloc *= 2;
		new_pfd = noct_realloc(poller->pfd, new_alloc * sizeof(sock_pollfd_t));
		if (new_pfd == NULL) {
			noct_out_of_memory(env);
			return false;
		}
		poller->pfd = new_pfd;
		poller->pfd_alloc = new_alloc;
	}

	for (i = 0; i < poller->entry_count; i++) {
		poller->pfd[i].fd = poller->entry[i].fd;
		poller->pfd[i].events = 0;
		if (poller->entry[i].events & EVENT_READ)
			poller->pfd[i].events |= POLLIN;
		if (poller->entry[i].events & EVENT_WRITE)
			poller->pfd[i].events |= POLLOUT;
		poller->pfd[i].revents = 0;
	}

	poller->pfd_dirty = false;

	return true;
}

/*
 * Detach a socket from its poller.
 *
 * Removes both the native registration and the handle reference kept
 * in the poller's "socks" dictionary.
 */
static bool
detach_socket(
	NoctEnv *env,
	NoctValue *poller_handle,
	struct socket_obj *sock)
{
	NoctValue socks, key;
	int index;

	if (sock->poller == NULL)
		return true;

	memset(&socks, 0, sizeof(NoctValue));
	memset(&key, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &socks, &key);

	index = poller_find(sock->poller, sock->reg_id);
	poller_remove_entry(sock->poller, index);

	if (poller_handle != NULL) {
		char key_s[32];

		if (!noct_get_dict_elem_check_dict(env, poller_handle, "socks", &socks))
			return false;
		snprintf(key_s, sizeof(key_s), "%d", sock->reg_id);
		if (!noct_remove_dict_elem_cstr(env, &socks, key_s))
			return false;
	}

	sock->poller = NULL;
	sock->reg_id = -1;

	noct_unpin_local(env, 2, &socks, &key);

	return true;
}

/*
 * Sockets
 */

/* Implementation of HttpServer.listen() */
static bool
cfunc_HttpServer_listen(
	NoctEnv *env)
{
	NoctValue host, port, handle;
	const char *host_s;
	char port_s[16];
	int port_i;
	struct addrinfo hints, *res, *ai;
	sock_t fd;
	int on;

	memset(&host, 0, sizeof(NoctValue));
	memset(&port, 0, sizeof(NoctValue));
	memset(&handle, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &host, &port, &handle);

	/* Get parameters. */
	if (!noct_get_arg_check_string(env, 0, &host, &host_s))
		return false;
	if (!noct_get_arg_check_int(env, 1, &port, &port_i))
		return false;
	if (port_i < 0 || port_i > 65535) {
		noct_error(env, N_TR("Port number is out-of-range."));
		return false;
	}
	snprintf(port_s, sizeof(port_s), "%d", port_i);

	/* Resolve the address. */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(host_s[0] == '\0' ? NULL : host_s, port_s, &hints, &res) != 0) {
		noct_error(env, N_TR("Cannot resolve the address %s."), host_s);
		return false;
	}

	/* Bind to the first usable address. */
	fd = SOCK_INVALID;
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd == SOCK_INVALID)
			continue;

		on = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));

		if (bind(fd, ai->ai_addr, (socklen_arg_t)ai->ai_addrlen) == 0)
			break;

		sock_close(fd);
		fd = SOCK_INVALID;
	}
	freeaddrinfo(res);

	if (fd == SOCK_INVALID) {
		noct_error(env, N_TR("Cannot bind to %s:%d. (%s)"), host_s, port_i, strerror(errno));
		return false;
	}

	/* Start listening. */
	if (listen(fd, SOMAXCONN) != 0) {
		sock_close(fd);
		noct_error(env, N_TR("Cannot listen on %s:%d."), host_s, port_i);
		return false;
	}

	/* Make a return value. */
	if (!make_socket_handle(env, &handle, fd)) {
		sock_close(fd);
		return false;
	}
	if (!noct_set_return(env, &handle))
		return false;

	noct_unpin_local(env, 3, &host, &port, &handle);

	return true;
}

/* Implementation of HttpServer.connect() */
static bool
cfunc_HttpServer_connect(
	NoctEnv *env)
{
	NoctValue host, port, handle;
	const char *host_s;
	char port_s[16];
	char host_buf[256];
	int port_i;
	struct addrinfo hints, *res, *ai;
	sock_t fd;
	int on;

	memset(&host, 0, sizeof(NoctValue));
	memset(&port, 0, sizeof(NoctValue));
	memset(&handle, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &host, &port, &handle);

	/* Get parameters. */
	if (!noct_get_arg_check_string(env, 0, &host, &host_s))
		return false;
	if (!noct_get_arg_check_int(env, 1, &port, &port_i))
		return false;
	if (port_i < 1 || port_i > 65535) {
		noct_error(env, N_TR("Port number is out-of-range."));
		return false;
	}
	snprintf(port_s, sizeof(port_s), "%d", port_i);

	/* Copy the host name: the resolve and connect below park this thread. */
	strncpy(host_buf, host_s, sizeof(host_buf) - 1);
	host_buf[sizeof(host_buf) - 1] = '\0';

	/* Resolve and connect. */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	fd = SOCK_INVALID;
	noct_enter_blocking(env);
	if (getaddrinfo(host_buf, port_s, &hints, &res) == 0) {
		for (ai = res; ai != NULL; ai = ai->ai_next) {
			fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
			if (fd == SOCK_INVALID)
				continue;
			if (connect(fd, ai->ai_addr, (socklen_arg_t)ai->ai_addrlen) == 0)
				break;
			sock_close(fd);
			fd = SOCK_INVALID;
		}
		freeaddrinfo(res);
	}
	noct_leave_blocking(env);

	if (fd == SOCK_INVALID) {
		noct_error(env, N_TR("Cannot connect to %s:%d."), host_buf, port_i);
		return false;
	}

	on = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(on));

	/* Make a return value. */
	if (!make_socket_handle(env, &handle, fd)) {
		sock_close(fd);
		return false;
	}
	if (!noct_set_return(env, &handle))
		return false;

	noct_unpin_local(env, 3, &host, &port, &handle);

	return true;
}

/* Implementation of HttpServer.accept() */
static bool
cfunc_HttpServer_accept(
	NoctEnv *env)
{
	NoctValue server, handle;
	struct socket_obj *sock;
	sock_t fd, conn_fd;
	int on;

	memset(&server, 0, sizeof(NoctValue));
	memset(&handle, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &server, &handle);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &server))
		return false;
	if (!get_open_socket(env, &server, &sock))
		return false;
	fd = sock->fd;

	/*
	 * Accept. The wait is a blocking region, so the descriptor is
	 * read out of the control block before parking.
	 */
	noct_enter_blocking(env);
	for (;;) {
		conn_fd = accept(fd, NULL, NULL);
		if (conn_fd != SOCK_INVALID)
			break;
		if (SOCK_INTR())
			continue;
		break;
	}
	noct_leave_blocking(env);

	if (conn_fd == SOCK_INVALID) {
		if (SOCK_WOULDBLOCK()) {
			/* Nothing pending on a non-blocking listener. */
			if (!noct_set_return_make_int(env, &handle, 0))
				return false;
			noct_unpin_local(env, 2, &server, &handle);
			return true;
		}
		noct_error(env, N_TR("Cannot accept a connection."));
		return false;
	}

	/* Disable Nagle: responses are written in one shot. */
	on = 1;
	setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(on));

	/* Make a return value. */
	if (!make_socket_handle(env, &handle, conn_fd)) {
		sock_close(conn_fd);
		return false;
	}
	if (!noct_set_return(env, &handle))
		return false;

	noct_unpin_local(env, 2, &server, &handle);

	return true;
}

/* Implementation of HttpServer.recv() */
static bool
cfunc_HttpServer_recv(
	NoctEnv *env)
{
	NoctValue conn, max_bytes, ret;
	struct socket_obj *sock;
	sock_t fd;
	size_t max_n;
	char *buf;
	long received;

	memset(&conn, 0, sizeof(NoctValue));
	memset(&max_bytes, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &conn, &max_bytes, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &conn))
		return false;
	if (!noct_get_arg_check_int_long(env, 1, &max_bytes, &max_n))
		return false;
	if (max_n == 0 || max_n > RECV_MAX) {
		noct_error(env, N_TR("Receive size is out-of-range."));
		return false;
	}
	if (!get_open_socket(env, &conn, &sock))
		return false;
	fd = sock->fd;

	/* Allocate a buffer outside the VM heap: recv() parks this thread. */
	buf = noct_malloc(max_n + 1);
	if (buf == NULL) {
		noct_out_of_memory(env);
		return false;
	}

	/* Receive. */
	noct_enter_blocking(env);
	for (;;) {
		received = (long)recv(fd, buf, max_n, 0);
		if (received >= 0)
			break;
		if (SOCK_INTR())
			continue;
		break;
	}
	noct_leave_blocking(env);

	if (received < 0) {
		noct_free(buf);
		if (SOCK_WOULDBLOCK()) {
			/* No data available on a non-blocking socket. */
			if (!noct_set_return_make_string(env, &ret, ""))
				return false;
			noct_unpin_local(env, 3, &conn, &max_bytes, &ret);
			return true;
		}
		noct_error(env, N_TR("Cannot receive from the connection."));
		return false;
	}

	/*
	 * Return the data as a string. A zero-length result means the
	 * peer closed the connection; use HttpServer.isClosed() or the
	 * length to tell the two apart.
	 */
	buf[received] = '\0';
	if (!noct_set_return_make_string(env, &ret, buf)) {
		noct_free(buf);
		return false;
	}
	noct_free(buf);

	noct_unpin_local(env, 3, &conn, &max_bytes, &ret);

	return true;
}

/* Implementation of HttpServer.send() */
static bool
cfunc_HttpServer_send(
	NoctEnv *env)
{
	NoctValue conn, data, ret;
	struct socket_obj *sock;
	sock_t fd;
	const char *data_s;
	char *buf;
	size_t len, sent;
	long n;
	bool failed;

	memset(&conn, 0, sizeof(NoctValue));
	memset(&data, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &conn, &data, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &conn))
		return false;
	if (!noct_get_arg_check_string(env, 1, &data, &data_s))
		return false;
	if (!get_open_socket(env, &conn, &sock))
		return false;
	fd = sock->fd;

	len = strlen(data_s);
	if (len == 0) {
		if (!noct_set_return_make_int_long(env, &ret, 0))
			return false;
		noct_unpin_local(env, 3, &conn, &data, &ret);
		return true;
	}

	/*
	 * Copy the payload out of the VM heap: send() parks this thread,
	 * during which a GC may move the string.
	 */
	buf = noct_malloc(len);
	if (buf == NULL) {
		noct_out_of_memory(env);
		return false;
	}
	memcpy(buf, data_s, len);

	/* Send everything. */
	failed = false;
	sent = 0;
	noct_enter_blocking(env);
	while (sent < len) {
		n = (long)send(fd, buf + sent, len - sent, 0);
		if (n < 0) {
			if (SOCK_INTR())
				continue;
			failed = true;
			break;
		}
		sent += (size_t)n;
	}
	noct_leave_blocking(env);
	noct_free(buf);

	if (failed && sent == 0 && !SOCK_WOULDBLOCK()) {
		noct_error(env, N_TR("Cannot send to the connection."));
		return false;
	}

	/* Make a return value: the number of bytes actually written. */
	if (!noct_set_return_make_int_long(env, &ret, sent))
		return false;

	noct_unpin_local(env, 3, &conn, &data, &ret);

	return true;
}

/* Implementation of HttpServer.close() */
static bool
cfunc_HttpServer_close(
	NoctEnv *env)
{
	NoctValue handle, ret;
	struct socket_obj *sock;

	memset(&handle, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &handle, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &handle))
		return false;
	if (!get_handle_native(env, &handle, SOCKET_MAGIC, (void **)&sock))
		return false;

	/*
	 * A registered socket cannot be closed directly: the poller
	 * would be left watching a dead descriptor. Removing it first
	 * is the caller's job, but doing it here keeps the invariant
	 * unconditional.
	 */
	if (sock->poller != NULL) {
		noct_error(env, N_TR("Socket is registered to a poller. Remove it first."));
		return false;
	}

	if (sock->fd != SOCK_INVALID) {
		sock_close(sock->fd);
		sock->fd = SOCK_INVALID;
	}

	/* Make a return value. */
	if (!noct_set_return_make_int(env, &ret, 1))
		return false;

	noct_unpin_local(env, 2, &handle, &ret);

	return true;
}

/* Implementation of HttpServer.isClosed() */
static bool
cfunc_HttpServer_isClosed(
	NoctEnv *env)
{
	NoctValue handle, ret;
	struct socket_obj *sock;

	memset(&handle, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &handle, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &handle))
		return false;
	if (!get_handle_native(env, &handle, SOCKET_MAGIC, (void **)&sock))
		return false;

	/* Make a return value. */
	if (!noct_set_return_make_int(env, &ret, sock->fd == SOCK_INVALID ? 1 : 0))
		return false;

	noct_unpin_local(env, 2, &handle, &ret);

	return true;
}

/* Implementation of HttpServer.setBlocking() */
static bool
cfunc_HttpServer_setBlocking(
	NoctEnv *env)
{
	NoctValue handle, blocking, ret;
	struct socket_obj *sock;
	int blocking_i;

	memset(&handle, 0, sizeof(NoctValue));
	memset(&blocking, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &handle, &blocking, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &handle))
		return false;
	if (!noct_get_arg_check_int(env, 1, &blocking, &blocking_i))
		return false;
	if (!get_open_socket(env, &handle, &sock))
		return false;

	/* Change the mode. */
#if defined(NOCT_TARGET_WINDOWS)
	{
		u_long mode = blocking_i ? 0 : 1;
		if (ioctlsocket(sock->fd, FIONBIO, &mode) != 0) {
			noct_error(env, N_TR("Cannot change the socket mode."));
			return false;
		}
	}
#else
	{
		int flags = fcntl(sock->fd, F_GETFL, 0);
		if (flags == -1) {
			noct_error(env, N_TR("Cannot change the socket mode."));
			return false;
		}
		if (blocking_i)
			flags &= ~O_NONBLOCK;
		else
			flags |= O_NONBLOCK;
		if (fcntl(sock->fd, F_SETFL, flags) == -1) {
			noct_error(env, N_TR("Cannot change the socket mode."));
			return false;
		}
	}
#endif

	/* Make a return value. */
	if (!noct_set_return_make_int(env, &ret, 1))
		return false;

	noct_unpin_local(env, 3, &handle, &blocking, &ret);

	return true;
}

/* Implementation of HttpServer.getPeer() */
static bool
cfunc_HttpServer_getPeer(
	NoctEnv *env)
{
	NoctValue handle, ret, tmp;
	struct socket_obj *sock;
	struct sockaddr_storage addr;
	socklen_arg_t addr_len;
	char host[NI_MAXHOST], serv[NI_MAXSERV];

	memset(&handle, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	memset(&tmp, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &handle, &ret, &tmp);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &handle))
		return false;
	if (!get_open_socket(env, &handle, &sock))
		return false;

	/* Query the peer. */
	addr_len = (socklen_arg_t)sizeof(addr);
	host[0] = '\0';
	serv[0] = '\0';
	if (getpeername(sock->fd, (struct sockaddr *)&addr, &addr_len) == 0) {
		getnameinfo((struct sockaddr *)&addr, addr_len,
			    host, sizeof(host), serv, sizeof(serv),
			    NI_NUMERICHOST | NI_NUMERICSERV);
	}

	/* Make a return value. */
	if (!noct_make_empty_dict(env, &ret))
		return false;
	if (!noct_set_dict_elem_make_string(env, &ret, "host", &tmp, host))
		return false;
	if (!noct_set_dict_elem_make_string(env, &ret, "port", &tmp, serv))
		return false;
	if (!noct_set_return(env, &ret))
		return false;

	noct_unpin_local(env, 3, &handle, &ret, &tmp);

	return true;
}

/*
 * Poller
 */

/* Implementation of HttpServer.createPoller() */
static bool
cfunc_HttpServer_createPoller(
	NoctEnv *env)
{
	NoctValue handle, socks;
	struct poller_obj *obj;

	memset(&handle, 0, sizeof(NoctValue));
	memset(&socks, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &handle, &socks);

	/* Make a control block. */
	obj = noct_malloc(sizeof(struct poller_obj));
	if (obj == NULL) {
		noct_out_of_memory(env);
		return false;
	}
	memset(obj, 0, sizeof(struct poller_obj));
	obj->magic = POLLER_MAGIC;
	obj->next_reg_id = 1;

	/* Make a handle dictionary. */
	if (!noct_make_empty_dict(env, &handle)) {
		noct_free(obj);
		return false;
	}
	if (!noct_set_dict_native_pointer(env, &handle, obj, poller_finalizer)) {
		noct_free(obj);
		return false;
	}

	/*
	 * Registered socket handles live here, keyed by registration id.
	 * This is what keeps them reachable for the GC while the native
	 * side only remembers descriptors.
	 */
	if (!noct_make_empty_dict(env, &socks))
		return false;
	if (!noct_set_dict_elem_cstr(env, &handle, "socks", &socks))
		return false;

	/* Make a return value. */
	if (!noct_set_return(env, &handle))
		return false;

	noct_unpin_local(env, 2, &handle, &socks);

	return true;
}

/* Implementation of HttpServer.addToPoller() */
static bool
cfunc_HttpServer_addToPoller(
	NoctEnv *env)
{
	NoctValue poller_h, sock_h, events, socks, ret;
	struct poller_obj *poller;
	struct socket_obj *sock;
	char key_s[32];
	int events_i, reg_id;

	memset(&poller_h, 0, sizeof(NoctValue));
	memset(&sock_h, 0, sizeof(NoctValue));
	memset(&events, 0, sizeof(NoctValue));
	memset(&socks, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 5, &poller_h, &sock_h, &events, &socks, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &poller_h))
		return false;
	if (!noct_get_arg_check_dict(env, 1, &sock_h))
		return false;
	if (!noct_get_arg_check_int(env, 2, &events, &events_i))
		return false;
	if (!get_handle_native(env, &poller_h, POLLER_MAGIC, (void **)&poller))
		return false;
	if (!get_open_socket(env, &sock_h, &sock))
		return false;

	if ((events_i & ~(EVENT_READ | EVENT_WRITE)) != 0 || events_i == 0) {
		noct_error(env, N_TR("Invalid event flags."));
		return false;
	}
	if (sock->poller != NULL) {
		noct_error(env, N_TR("Socket is already registered to a poller."));
		return false;
	}

	/* Register natively. */
	if (!poller_add_entry(env, poller, sock->fd, events_i, &reg_id))
		return false;

	/* Keep the handle reachable through the poller. */
	if (!noct_get_dict_elem_check_dict(env, &poller_h, "socks", &socks)) {
		poller_remove_entry(poller, poller_find(poller, reg_id));
		return false;
	}
	snprintf(key_s, sizeof(key_s), "%d", reg_id);
	if (!noct_set_dict_elem_cstr(env, &socks, key_s, &sock_h)) {
		poller_remove_entry(poller, poller_find(poller, reg_id));
		return false;
	}

	sock->poller = poller;
	sock->reg_id = reg_id;

	/* Make a return value. */
	if (!noct_set_return_make_int(env, &ret, 1))
		return false;

	noct_unpin_local(env, 5, &poller_h, &sock_h, &events, &socks, &ret);

	return true;
}

/* Implementation of HttpServer.modifyPoller() */
static bool
cfunc_HttpServer_modifyPoller(
	NoctEnv *env)
{
	NoctValue poller_h, sock_h, events, ret;
	struct poller_obj *poller;
	struct socket_obj *sock;
	int events_i, index;

	memset(&poller_h, 0, sizeof(NoctValue));
	memset(&sock_h, 0, sizeof(NoctValue));
	memset(&events, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 4, &poller_h, &sock_h, &events, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &poller_h))
		return false;
	if (!noct_get_arg_check_dict(env, 1, &sock_h))
		return false;
	if (!noct_get_arg_check_int(env, 2, &events, &events_i))
		return false;
	if (!get_handle_native(env, &poller_h, POLLER_MAGIC, (void **)&poller))
		return false;
	if (!get_handle_native(env, &sock_h, SOCKET_MAGIC, (void **)&sock))
		return false;

	if ((events_i & ~(EVENT_READ | EVENT_WRITE)) != 0 || events_i == 0) {
		noct_error(env, N_TR("Invalid event flags."));
		return false;
	}
	if (sock->poller != poller) {
		noct_error(env, N_TR("Socket is not registered to this poller."));
		return false;
	}

	index = poller_find(poller, sock->reg_id);
	if (index < 0) {
		noct_error(env, N_TR("Socket is not registered to this poller."));
		return false;
	}
	poller->entry[index].events = events_i;
	poller->pfd_dirty = true;

	/* Make a return value. */
	if (!noct_set_return_make_int(env, &ret, 1))
		return false;

	noct_unpin_local(env, 4, &poller_h, &sock_h, &events, &ret);

	return true;
}

/* Implementation of HttpServer.removeFromPoller() */
static bool
cfunc_HttpServer_removeFromPoller(
	NoctEnv *env)
{
	NoctValue poller_h, sock_h, ret;
	struct poller_obj *poller;
	struct socket_obj *sock;

	memset(&poller_h, 0, sizeof(NoctValue));
	memset(&sock_h, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &poller_h, &sock_h, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &poller_h))
		return false;
	if (!noct_get_arg_check_dict(env, 1, &sock_h))
		return false;
	if (!get_handle_native(env, &poller_h, POLLER_MAGIC, (void **)&poller))
		return false;
	if (!get_handle_native(env, &sock_h, SOCKET_MAGIC, (void **)&sock))
		return false;

	if (sock->poller != poller) {
		noct_error(env, N_TR("Socket is not registered to this poller."));
		return false;
	}

	if (!detach_socket(env, &poller_h, sock))
		return false;

	/* Make a return value. */
	if (!noct_set_return_make_int(env, &ret, 1))
		return false;

	noct_unpin_local(env, 3, &poller_h, &sock_h, &ret);

	return true;
}

/* Implementation of HttpServer.waitPoller() */
static bool
cfunc_HttpServer_waitPoller(
	NoctEnv *env)
{
	NoctValue poller_h, timeout, socks, ret, item, sock_h;
	struct poller_obj *poller;
	int timeout_i, poll_ret;
	size_t i, ready_count;

	memset(&poller_h, 0, sizeof(NoctValue));
	memset(&timeout, 0, sizeof(NoctValue));
	memset(&socks, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	memset(&item, 0, sizeof(NoctValue));
	memset(&sock_h, 0, sizeof(NoctValue));
	noct_pin_local(env, 6, &poller_h, &timeout, &socks, &ret, &item, &sock_h);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &poller_h))
		return false;
	if (!noct_get_arg_check_int(env, 1, &timeout, &timeout_i))
		return false;
	if (!get_handle_native(env, &poller_h, POLLER_MAGIC, (void **)&poller))
		return false;

	/* Refresh the pollfd array if the registrations changed. */
	if (!poller_sync(env, poller))
		return false;

	/* Make the result array up front: allocating cannot follow the wait. */
	if (!noct_make_empty_array(env, &ret))
		return false;

	if (poller->entry_count == 0) {
		/*
		 * Nothing to watch. Returning immediately rather than
		 * sleeping keeps a caller's event loop responsive to its
		 * own shutdown checks.
		 */
		if (!noct_set_return(env, &ret))
			return false;
		noct_unpin_local(env, 6, &poller_h, &timeout, &socks, &ret, &item, &sock_h);
		return true;
	}

	/* Wait. */
	noct_enter_blocking(env);
	for (;;) {
		poll_ret = sock_poll(poller->pfd, (unsigned int)poller->entry_count, timeout_i);
		if (poll_ret >= 0)
			break;
		if (SOCK_INTR())
			continue;
		break;
	}
	noct_leave_blocking(env);

	if (poll_ret < 0) {
		noct_error(env, N_TR("Cannot wait for socket events."));
		return false;
	}

	/* Collect the ready sockets. */
	if (!noct_get_dict_elem_check_dict(env, &poller_h, "socks", &socks))
		return false;

	ready_count = 0;
	for (i = 0; i < poller->entry_count; i++) {
		char key_s[32];
		int ev;

		if (poller->pfd[i].revents == 0)
			continue;

		/*
		 * Report an error or a hang-up as readable: the caller
		 * finds out by getting zero bytes from the next recv().
		 */
		ev = 0;
		if (poller->pfd[i].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL))
			ev |= EVENT_READ;
		if (poller->pfd[i].revents & POLLOUT)
			ev |= EVENT_WRITE;
		if (ev == 0)
			continue;

		snprintf(key_s, sizeof(key_s), "%d", poller->entry[i].reg_id);
		if (!noct_get_dict_elem_check_dict(env, &socks, key_s, &sock_h))
			return false;

		if (!noct_make_empty_dict(env, &item))
			return false;
		if (!noct_set_dict_elem_cstr(env, &item, "socket", &sock_h))
			return false;
		if (!noct_set_dict_elem_make_int(env, &item, "events", &timeout, ev))
			return false;
		if (!noct_set_array_elem(env, &ret, ready_count, &item))
			return false;
		ready_count++;
	}

	/* Make a return value. */
	if (!noct_set_return(env, &ret))
		return false;

	noct_unpin_local(env, 6, &poller_h, &timeout, &socks, &ret, &item, &sock_h);

	return true;
}

/* Implementation of HttpServer.countPoller() */
static bool
cfunc_HttpServer_countPoller(
	NoctEnv *env)
{
	NoctValue poller_h, ret;
	struct poller_obj *poller;

	memset(&poller_h, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &poller_h, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &poller_h))
		return false;
	if (!get_handle_native(env, &poller_h, POLLER_MAGIC, (void **)&poller))
		return false;

	/* Make a return value. */
	if (!noct_set_return_make_int_long(env, &ret, poller->entry_count))
		return false;

	noct_unpin_local(env, 2, &poller_h, &ret);

	return true;
}
