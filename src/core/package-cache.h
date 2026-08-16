/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#ifndef NOCT_PACKAGE_CACHE_H
#define NOCT_PACKAGE_CACHE_H

#include "module.h"

struct package_cache_node {
	struct module_resolution module;
	int visit_state;
	struct package_cache_node *next;
};

struct package_cache_result {
	struct package_cache_node *node;
	uint8_t *bytecode;
	size_t bytecode_size;
};

bool package_cache_prepare(const struct module_paths *paths,
			   const char *package_name,
			   const struct module_resolution *root,
			   int optimize_level, bool lineinfo,
			   struct package_cache_result *result);
void package_cache_cleanup(struct package_cache_result *result);

#endif
