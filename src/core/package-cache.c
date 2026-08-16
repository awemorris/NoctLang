/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* Best-effort, content-addressed package bytecode cache. */

#include "package-cache.h"
#include "ast.h"
#include "sha256.h"
#include <noct/backend.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(NOCT_TARGET_WINDOWS)
#include <direct.h>
#include <process.h>
#define package_mkdir(path) _mkdir(path)
#define package_getpid() _getpid()
#else
#include <unistd.h>
#define package_mkdir(path) mkdir((path), 0700)
#define package_getpid() getpid()
#endif

#define PACKAGE_CACHE_SCHEMA "Noct-NBP-1"

static char *cache_strdup(const char *s)
{
	char *copy;
	size_t size;
	size = strlen(s) + 1;
	copy = malloc(size);
	if (copy != NULL)
		memcpy(copy, s, size);
	return copy;
}

static bool cache_clone_resolution(const struct module_resolution *src,
				   struct module_resolution *dst)
{
	memset(dst, 0, sizeof(*dst));
	dst->physical = cache_strdup(src->physical);
	dst->logical = cache_strdup(src->logical);
	dst->data = cache_strdup(src->data);
	dst->package_name = src->package_name != NULL ?
		cache_strdup(src->package_name) : NULL;
	dst->package_dir = src->package_dir != NULL ?
		cache_strdup(src->package_dir) : NULL;
	dst->is_package = src->is_package;
	if (dst->physical == NULL || dst->logical == NULL || dst->data == NULL ||
	    (src->package_name != NULL && dst->package_name == NULL) ||
	    (src->package_dir != NULL && dst->package_dir == NULL)) {
		module_resolution_cleanup(dst);
		return false;
	}
	return true;
}

static struct package_cache_node *cache_find(struct package_cache_result *r,
					      const char *physical)
{
	struct package_cache_node *node;
	for (node = r->node; node != NULL; node = node->next)
		if (strcmp(node->module.physical, physical) == 0)
			return node;
	return NULL;
}

static bool cache_collect(struct package_cache_result *r,
			  const struct module_paths *paths,
			  struct package_cache_node *node)
{
	char **require_name;
	uint32_t count;
	uint32_t i;
	bool ok;

	if (node->visit_state == 2)
		return true;
	if (node->visit_state == 1)
		return false;
	node->visit_state = 1;
	require_name = NULL;
	count = 0;
	ok = false;
	if (!ast_build(node->module.logical, node->module.data))
		goto done;
	count = ast_get_require_count();
	if (count != 0) {
		require_name = calloc(count, sizeof(*require_name));
		if (require_name == NULL)
			goto done;
		for (i = 0; i < count; i++) {
			require_name[i] = cache_strdup(ast_get_require_name(i));
			if (require_name[i] == NULL)
				goto done;
		}
	}
	ast_cleanup();
	for (i = 0; i < count; i++) {
		struct module_resolution resolution;
		struct package_cache_node *dependency;
		if (!module_resolve_ex(paths, require_name[i], &resolution))
			goto names_done;
		dependency = cache_find(r, resolution.physical);
		if (dependency == NULL) {
			dependency = calloc(1, sizeof(*dependency));
			if (dependency == NULL) {
				module_resolution_cleanup(&resolution);
				goto names_done;
			}
			dependency->module = resolution;
			dependency->next = r->node;
			r->node = dependency;
		} else {
			module_resolution_cleanup(&resolution);
		}
		if (!cache_collect(r, paths, dependency))
			goto names_done;
	}
	node->visit_state = 2;
	ok = true;
names_done:
	for (i = 0; i < count; i++)
		free(require_name[i]);
	free(require_name);
	return ok;
done:
	ast_cleanup();
	for (i = 0; i < count; i++)
		free(require_name != NULL ? require_name[i] : NULL);
	free(require_name);
	return false;
}

static bool cache_read(const char *path, uint8_t **data, size_t *size)
{
	FILE *fp;
	long end;
	uint8_t *buffer;
	fp = fopen(path, "rb");
	if (fp == NULL)
		return false;
	if (fseek(fp, 0, SEEK_END) != 0 || (end = ftell(fp)) < 0 ||
	    (uint64_t)end > UINT32_MAX || fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return false;
	}
	buffer = malloc((size_t)end + 1);
	if (buffer == NULL) {
		fclose(fp);
		return false;
	}
	if (fread(buffer, 1, (size_t)end, fp) != (size_t)end) {
		free(buffer);
		fclose(fp);
		return false;
	}
	buffer[end] = 0;
	fclose(fp);
	*data = buffer;
	*size = (size_t)end;
	return true;
}

static bool cache_validate(const char *path, uint8_t **data, size_t *size)
{
	char *sum_path;
	uint8_t *sum_data;
	size_t sum_size;
	uint8_t digest[32];
	char hex[65];
	bool ok;
	sum_path = malloc(strlen(path) + 5);
	if (sum_path == NULL)
		return false;
	sprintf(sum_path, "%s.sum", path);
	sum_data = NULL;
	ok = cache_read(path, data, size) &&
	     cache_read(sum_path, &sum_data, &sum_size);
	free(sum_path);
	if (!ok) {
		free(*data);
		*data = NULL;
		free(sum_data);
		return false;
	}
	noct_sha256_bytes(*data, *size, digest);
	noct_sha256_hex(digest, hex);
	ok = sum_size >= 64 && memcmp(sum_data, hex, 64) == 0 &&
	     *size >= strlen(NOCT_BYTECODE_HEADER) &&
	     memcmp(*data, NOCT_BYTECODE_HEADER,
		    strlen(NOCT_BYTECODE_HEADER)) == 0;
	free(sum_data);
	if (!ok) {
		free(*data);
		*data = NULL;
		*size = 0;
	}
	return ok;
}

static bool cache_make_dir(const char *path)
{
	return package_mkdir(path) == 0 || errno == EEXIST;
}

static char *cache_directory(const char *package_name)
{
	const char *home;
	char *p1, *p2, *p3, *p4;
	size_t n;
#if defined(NOCT_TARGET_WINDOWS)
	home = getenv("USERPROFILE");
#else
	home = getenv("HOME");
#endif
	if (home == NULL || home[0] == '\0')
		return NULL;
	n = strlen(home) + strlen("/.noct") + 1;
	p1 = malloc(n); if (p1 == NULL) return NULL;
	sprintf(p1, "%s/.noct", home);
	p2 = malloc(strlen(p1) + strlen("/cache") + 1);
	p3 = malloc(strlen(p1) + strlen("/cache/packages") + 1);
	p4 = malloc(strlen(p1) + strlen("/cache/packages/") +
		    strlen(package_name) + 1);
	if (p2 == NULL || p3 == NULL || p4 == NULL) {
		free(p1); free(p2); free(p3); free(p4); return NULL;
	}
	sprintf(p2, "%s/cache", p1);
	sprintf(p3, "%s/cache/packages", p1);
	sprintf(p4, "%s/cache/packages/%s", p1, package_name);
	if (!cache_make_dir(p1) || !cache_make_dir(p2) ||
	    !cache_make_dir(p3) || !cache_make_dir(p4)) {
		free(p4); p4 = NULL;
	}
	free(p1); free(p2); free(p3);
	return p4;
}

static void cache_hash_graph(struct package_cache_result *r,
			     int optimize_level, bool lineinfo, char hex[65])
{
	struct noct_sha256 hash;
	struct package_cache_node *node;
	uint8_t digest[32];
	noct_sha256_init(&hash);
	noct_sha256_update(&hash, PACKAGE_CACHE_SCHEMA,
			   strlen(PACKAGE_CACHE_SCHEMA) + 1);
	noct_sha256_update(&hash, &optimize_level, sizeof(optimize_level));
	noct_sha256_update(&hash, &lineinfo, sizeof(lineinfo));
	for (node = r->node; node != NULL; node = node->next) {
		noct_sha256_update(&hash, node->module.logical,
				   strlen(node->module.logical) + 1);
		noct_sha256_update(&hash, node->module.data,
				   strlen(node->module.data) + 1);
	}
	noct_sha256_final(&hash, digest);
	noct_sha256_hex(digest, hex);
}

static bool cache_write_checksum(const char *path)
{
	uint8_t *data;
	size_t size;
	uint8_t digest[32];
	char hex[65];
	char *sum_path;
	char *tmp_path;
	FILE *fp;
	bool ok;
	data = NULL;
	if (!cache_read(path, &data, &size))
		return false;
	noct_sha256_bytes(data, size, digest);
	noct_sha256_hex(digest, hex);
	free(data);
	sum_path = malloc(strlen(path) + 5);
	tmp_path = malloc(strlen(path) + 32);
	if (sum_path == NULL || tmp_path == NULL) {
		free(sum_path); free(tmp_path); return false;
	}
	sprintf(sum_path, "%s.sum", path);
	sprintf(tmp_path, "%s.sum.tmp.%ld", path, (long)package_getpid());
	fp = fopen(tmp_path, "wb");
	ok = fp != NULL && fwrite(hex, 1, 64, fp) == 64 &&
	     fputc('\n', fp) != EOF;
	if (fp != NULL && fclose(fp) != 0)
		ok = false;
	if (ok)
		ok = rename(tmp_path, sum_path) == 0;
	if (!ok)
		remove(tmp_path);
	free(sum_path); free(tmp_path);
	return ok;
}

bool
package_cache_prepare(const struct module_paths *paths,
		      const char *package_name,
		      const struct module_resolution *root,
		      int optimize_level, bool lineinfo,
		      struct package_cache_result *result)
{
	struct package_cache_node *root_node;
	char hex[65];
	char *dir;
	char *path;
	char logical[256];
	uint32_t i;
	bool built;

	memset(result, 0, sizeof(*result));
	root_node = calloc(1, sizeof(*root_node));
	if (root_node == NULL || !cache_clone_resolution(root, &root_node->module)) {
		free(root_node); return false;
	}
	result->node = root_node;
	if (!cache_collect(result, paths, root_node))
		return false;
	cache_hash_graph(result, optimize_level, lineinfo, hex);
	dir = cache_directory(package_name);
	if (dir == NULL)
		return false;
	path = malloc(strlen(dir) + 1 + 64 + 5);
	if (path == NULL) { free(dir); return false; }
	sprintf(path, "%s/%s.nbp", dir, hex);
	free(dir);
	if (cache_validate(path, &result->bytecode, &result->bytecode_size)) {
		if (getenv("NOCT_PACKAGE_CACHE_DEBUG") != NULL)
			fprintf(stderr, "noct-package-cache: %s: hit\n", package_name);
		free(path); return true;
	}
	noct_bcback_set_optimize_level(optimize_level);
	noct_bcback_set_lineinfo(lineinfo);
	snprintf(logical, sizeof(logical), "@package-cache/%s.nbp", package_name);
	built = noct_bcback_package_start(path, logical);
	for (i = 0; built && i < paths->count; i++)
		built = noct_bcback_app_add_require_path(paths->item[i]);
	if (built)
		built = noct_bcback_app_add_source(root->logical, root->data);
	if (built)
		built = noct_bcback_app_finalize();
	else
		noct_bcback_app_abort();
	if (built)
		built = cache_write_checksum(path);
	if (built)
		built = cache_validate(path, &result->bytecode,
				       &result->bytecode_size);
	if (built && getenv("NOCT_PACKAGE_CACHE_DEBUG") != NULL)
		fprintf(stderr, "noct-package-cache: %s: rebuilt\n", package_name);
	free(path);
	return built;
}

void
package_cache_cleanup(struct package_cache_result *result)
{
	struct package_cache_node *node;
	while (result->node != NULL) {
		node = result->node;
		result->node = node->next;
		module_resolution_cleanup(&node->module);
		free(node);
	}
	free(result->bytecode);
	memset(result, 0, sizeof(*result));
}
