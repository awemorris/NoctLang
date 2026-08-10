/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/* Shared source-module path and resolver support. */

#ifndef NOCT_MODULE_H
#define NOCT_MODULE_H

#include <noct/c89compat.h>

struct module_paths {
	char **item;
	uint32_t count;
	uint32_t capacity;
};

/* Initialize a path list.  The current directory is always first. */
bool module_paths_init(struct module_paths *paths);
void module_paths_cleanup(struct module_paths *paths);

/* Append a colon-separated path list. Empty elements are ignored. */
bool module_paths_add(struct module_paths *paths, const char *path_list);

/*
 * Resolve NAME as NAME.noct, then NAME.nct, in path order.
 * PHYSICAL is an absolute, lexically normalized duplicate-detection key.
 * LOGICAL is safe to embed in diagnostics and bytecode and contains no host
 * absolute path. DATA is a NUL-terminated source buffer.
 */
bool module_resolve(const struct module_paths *paths, const char *name,
		    char **physical, char **logical, char **data);

/* Return a normalized absolute key for an already named source file. */
char *module_path_key(const char *path);

#endif
