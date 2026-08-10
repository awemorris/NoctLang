/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/* Shared source-module path and resolver support. */

#include "module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32) || defined(NOCT_TARGET_DOS4G)
#include <direct.h>
#define module_getcwd(buf, size) _getcwd((buf), (int)(size))
#else
#include <unistd.h>
#define module_getcwd(buf, size) getcwd((buf), (size))
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

bool
module_resolve(const struct module_paths *paths, const char *name,
		char **physical, char **logical, char **data)
{
	static const char *suffix[] = { ".noct", ".nct" };
	uint32_t i;
	uint32_t j;

	*physical = NULL;
	*logical = NULL;
	*data = NULL;
	if (name == NULL || name[0] == '\0' || module_is_absolute(name) ||
	    strchr(name, '/') != NULL || strchr(name, '\\') != NULL ||
	    strstr(name, "..") != NULL)
		return false;
	for (i = 0; i < paths->count; i++) {
		for (j = 0; j < 2; j++) {
			char *candidate;
			char *source;
			size_t n;

			n = strlen(paths->item[i]) + 1 + strlen(name) +
			    strlen(suffix[j]) + 1;
			candidate = malloc(n);
			if (candidate == NULL)
				return false;
			snprintf(candidate, n, "%s/%s%s", paths->item[i], name,
				 suffix[j]);
			if (!module_read_file(candidate, &source)) {
				free(candidate);
				continue;
			}
			*physical = module_path_key(candidate);
			free(candidate);
			if (*physical == NULL) {
				free(source);
				return false;
			}
			n = strlen("@require/") + strlen(name) +
			    strlen(suffix[j]) + 1;
			*logical = malloc(n);
			if (*logical == NULL) {
				free(*physical);
				free(source);
				*physical = NULL;
				return false;
			}
			snprintf(*logical, n, "@require/%s%s", name, suffix[j]);
			*data = source;
			return true;
		}
	}
	return false;
}
