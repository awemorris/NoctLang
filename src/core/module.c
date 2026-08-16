/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* Shared source-module path and resolver support. */

#include "module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(NOCT_TARGET_WINDOWS) || defined(NOCT_TARGET_DOS4G)
#include <direct.h>
#define module_getcwd(buf, size) _getcwd((buf), (int)(size))
#elif defined(NOCT_TARGET_POSIX) || defined(NOCT_TARGET_PC98BE)
#include <unistd.h>
#define module_getcwd(buf, size) getcwd((buf), (size))
#else
#define module_getcwd(buf, size) NULL
#endif

static char *
module_strdup(const char *s)
{
	size_t n;
	char *p;

	n = strlen(s) + 1;
	p = malloc(n);
	if (p != NULL)
		memcpy(p, s, n);
	return p;
}

static bool
module_is_absolute(const char *path)
{
	return path[0] == '/' || path[0] == '\\' ||
	       (isalpha((unsigned char)path[0]) && path[1] == ':');
}

static bool
module_valid_package_name(const char *name)
{
	const unsigned char *p;
	unsigned char c;

	if (name == NULL)
		return false;
	c = (unsigned char)name[0];
	if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
	      c == '_'))
		return false;
	for (p = (const unsigned char *)name + 1; *p != '\0'; p++)
		if (!((*p >= 'A' && *p <= 'Z') ||
		      (*p >= 'a' && *p <= 'z') ||
		      (*p >= '0' && *p <= '9') || *p == '_'))
			return false;
	return true;
}

/* Lexically normalize slashes, '.', and '..'. */
static char *
module_normalize_absolute(const char *path)
{
	char cwd[4096];
	char *joined;
	char *out;
	char *part[512];
	uint32_t count;
	char *p;
	char *start;
	size_t n;
	size_t prefix;
	uint32_t i;

	if (module_is_absolute(path)) {
		joined = module_strdup(path);
	} else {
		if (module_getcwd(cwd, sizeof(cwd)) == NULL)
			return NULL;
		n = strlen(cwd) + 1 + strlen(path) + 1;
		joined = malloc(n);
		if (joined == NULL)
			return NULL;
		snprintf(joined, n, "%s/%s", cwd, path);
	}
	if (joined == NULL)
		return NULL;
	for (p = joined; *p != '\0'; p++)
		if (*p == '\\')
			*p = '/';
	prefix = (isalpha((unsigned char)joined[0]) && joined[1] == ':') ? 2 : 0;
	p = joined + prefix;
	while (*p == '/')
		p++;
	count = 0;
	while (*p != '\0') {
		start = p;
		while (*p != '\0' && *p != '/')
			p++;
		if (*p == '/')
			*p++ = '\0';
		while (*p == '/')
			p++;
		if (strcmp(start, ".") == 0 || start[0] == '\0')
			continue;
		if (strcmp(start, "..") == 0) {
			if (count != 0)
				count--;
			continue;
		}
		if (count == (uint32_t)(sizeof(part) / sizeof(part[0]))) {
			free(joined);
			return NULL;
		}
		part[count++] = start;
	}
	n = prefix + 2;
	for (i = 0; i < count; i++)
		n += strlen(part[i]) + 1;
	out = malloc(n);
	if (out == NULL) {
		free(joined);
		return NULL;
	}
	p = out;
	if (prefix != 0) {
		*p++ = joined[0];
		*p++ = ':';
	}
	*p++ = '/';
	for (i = 0; i < count; i++) {
		size_t len = strlen(part[i]);
		memcpy(p, part[i], len);
		p += len;
		if (i + 1 != count)
			*p++ = '/';
	}
	*p = '\0';
	free(joined);
	return out;
}

char *
module_path_key(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return NULL;
	return module_normalize_absolute(path);
}

static bool
module_paths_append(struct module_paths *paths, const char *path, size_t len)
{
	char **new_item;
	char *copy;
	uint32_t capacity;

	if (len == 0)
		return true;
	copy = malloc(len + 1);
	if (copy == NULL)
		return false;
	memcpy(copy, path, len);
	copy[len] = '\0';
	if (paths->count == paths->capacity) {
		capacity = paths->capacity == 0 ? 4 : paths->capacity * 2;
		new_item = realloc(paths->item, sizeof(*new_item) * capacity);
		if (new_item == NULL) {
			free(copy);
			return false;
		}
		paths->item = new_item;
		paths->capacity = capacity;
	}
	paths->item[paths->count++] = copy;
	return true;
}

bool
module_paths_init(struct module_paths *paths)
{
	memset(paths, 0, sizeof(*paths));
	return module_paths_append(paths, ".", 1);
}

void
module_paths_cleanup(struct module_paths *paths)
{
	uint32_t i;
	for (i = 0; i < paths->count; i++)
		free(paths->item[i]);
	free(paths->item);
	memset(paths, 0, sizeof(*paths));
}

bool
module_paths_add(struct module_paths *paths, const char *path_list)
{
	const char *start;
	const char *p;

	if (path_list == NULL)
		return false;
	start = path_list;
	for (p = path_list; ; p++) {
		bool drive_colon;
		drive_colon = *p == ':' && p == start + 1 &&
			      isalpha((unsigned char)start[0]) &&
			      (p[1] == '/' || p[1] == '\\');
		if ((*p == ':' && !drive_colon) || *p == '\0') {
			if (!module_paths_append(paths, start, (size_t)(p - start)))
				return false;
			if (*p == '\0')
				break;
			start = p + 1;
		}
	}
	return true;
}

static bool
module_read_file(const char *path, char **data)
{
	FILE *in;
	long end;
	char *buf;

	in = fopen(path, "rb");
	if (in == NULL)
		return false;
	if (fseek(in, 0, SEEK_END) != 0 || (end = ftell(in)) < 0 ||
	    fseek(in, 0, SEEK_SET) != 0) {
		fclose(in);
		return false;
	}
	buf = malloc((size_t)end + 1);
	if (buf == NULL) {
		fclose(in);
		return false;
	}
	if (fread(buf, 1, (size_t)end, in) != (size_t)end) {
		free(buf);
		fclose(in);
		return false;
	}
	buf[end] = '\0';
	fclose(in);
	*data = buf;
	return true;
}

void
module_resolution_cleanup(struct module_resolution *result)
{
	if (result == NULL)
		return;
	free(result->physical);
	free(result->logical);
	free(result->data);
	free(result->package_name);
	free(result->package_dir);
	memset(result, 0, sizeof(*result));
}

static bool
module_try_flat(const char *dir, const char *name, const char *suffix,
		struct module_resolution *result)
{
	char *candidate;
	char *source;
	size_t n;

	n = strlen(dir) + 1 + strlen(name) + strlen(suffix) + 1;
	candidate = malloc(n);
	if (candidate == NULL)
		return false;
	snprintf(candidate, n, "%s/%s%s", dir, name, suffix);
	if (!module_read_file(candidate, &source)) {
		free(candidate);
		return false;
	}
	result->physical = module_path_key(candidate);
	free(candidate);
	if (result->physical == NULL) {
		free(source);
		return false;
	}
	n = strlen("@require/") + strlen(name) + strlen(suffix) + 1;
	result->logical = malloc(n);
	if (result->logical == NULL) {
		free(result->physical);
		free(source);
		result->physical = NULL;
		return false;
	}
	snprintf(result->logical, n, "@require/%s%s", name, suffix);
	result->data = source;
	return true;
}

static bool
module_try_package(const char *root, const char *name, const char *suffix,
		   struct module_resolution *result)
{
	char *candidate;
	char *source;
	char *dir;
	size_t n;

	n = strlen(root) + 1 + strlen(name) + 1 + strlen(name) +
	    strlen(suffix) + 1;
	candidate = malloc(n);
	if (candidate == NULL)
		return false;
	snprintf(candidate, n, "%s/%s/%s%s", root, name, name, suffix);
	if (!module_read_file(candidate, &source)) {
		free(candidate);
		return false;
	}
	result->physical = module_path_key(candidate);
	free(candidate);
	if (result->physical == NULL) {
		free(source);
		return false;
	}
	n = strlen(root) + 1 + strlen(name) + 1;
	dir = malloc(n);
	if (dir == NULL)
		goto oom;
	snprintf(dir, n, "%s/%s", root, name);
	result->package_dir = module_path_key(dir);
	free(dir);
	if (result->package_dir == NULL)
		goto oom;
	result->package_name = module_strdup(name);
	if (result->package_name == NULL)
		goto oom;
	n = strlen("@package/") + strlen(name) + 1 + strlen(name) +
	    strlen(suffix) + 1;
	result->logical = malloc(n);
	if (result->logical == NULL)
		goto oom;
	snprintf(result->logical, n, "@package/%s/%s%s", name, name, suffix);
	result->data = source;
	result->is_package = true;
	return true;
oom:
	free(source);
	module_resolution_cleanup(result);
	return false;
}

bool
module_resolve_package(const char *name, struct module_resolution *result)
{
	static const char *suffix[] = { ".noct", ".nct" };
	static const char *system_root[] = {
		"/usr/local/share/noct/packages",
		"/usr/share/noct/packages"
	};
	const char *home;
	char *user_root;
	uint32_t i;
	uint32_t j;

	memset(result, 0, sizeof(*result));
	if (!module_valid_package_name(name))
		return false;
#if defined(NOCT_TARGET_WINDOWS)
	home = getenv("USERPROFILE");
#else
	home = getenv("HOME");
#endif
	if (home != NULL && home[0] != '\0') {
		size_t n = strlen(home) + strlen("/.noct/packages") + 1;
		user_root = malloc(n);
		if (user_root == NULL)
			return false;
		snprintf(user_root, n, "%s/.noct/packages", home);
		for (j = 0; j < 2; j++) {
			if (module_try_package(user_root, name, suffix[j], result)) {
				free(user_root);
				return true;
			}
		}
		free(user_root);
	}
#if !defined(NOCT_TARGET_WINDOWS)
	for (i = 0; i < (uint32_t)(sizeof(system_root) / sizeof(system_root[0])); i++)
		for (j = 0; j < 2; j++)
			if (module_try_package(system_root[i], name, suffix[j], result))
				return true;
#else
	(void)system_root;
#endif
	return false;
}

bool
module_resolve_ex(const struct module_paths *paths, const char *name,
		  struct module_resolution *result)
{
	static const char *suffix[] = { ".noct", ".nct" };
	uint32_t i;
	uint32_t j;

	memset(result, 0, sizeof(*result));
	if (name == NULL || name[0] == '\0' || module_is_absolute(name) ||
	    strchr(name, '/') != NULL || strchr(name, '\\') != NULL ||
	    strstr(name, "..") != NULL)
		return false;
	/* Explicit --path modules preserve their historical precedence. */
	for (i = 0; i < paths->count; i++)
		for (j = 0; j < 2; j++)
			if (module_try_flat(paths->item[i], name, suffix[j], result))
				return true;
	return module_resolve_package(name, result);
}

bool
module_resolve(const struct module_paths *paths, const char *name,
		char **physical, char **logical, char **data)
{
	struct module_resolution result;

	*physical = NULL;
	*logical = NULL;
	*data = NULL;
	if (!module_resolve_ex(paths, name, &result))
		return false;
	*physical = result.physical;
	*logical = result.logical;
	*data = result.data;
	free(result.package_name);
	free(result.package_dir);
	return true;
}
