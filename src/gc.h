/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * GC
 */

#ifndef NOCT_GC_H
#define NOCT_GC_H

#include <noct/noct.h>
#include "c89compat.h"

/*
 * Global GC info, embedded to global envs.
 */
struct rt_gc_global {
	/* Young object list. */
	struct rt_gc_object_header *young_list;

	/* Garbage object list. */
	struct rt_gc_object_header *garbage_list;

	/* Heap usage in bytes. */
	size_t heap_usage;
};

/*
 * Local GC Info, embedded to call frames.
 */
struct rt_gc_local {
	/* Shallow object list. */
	struct rt_gc_object_header *shallow_list;
	struct rt_gc_object_header *shallow_list_tail;
};

/*
 * Object header, embedded to objects.
 */
struct rt_gc_object_header {
	/* Type. */
	int type;

	/* Which GC list is this object in? */
	int gc_type;

	/* Object list/ (shallow, young, or tenure) */
	struct rt_gc_object_header *prev;
	struct rt_gc_object_header *next;

	/* Native reference count. */
	int native_ref_count;

	/* Is marked? (for mark-and-sweep). */
	bool is_marked_on_gc;
};

/* Allocate a string. */
struct rt_string *rt_gc_allocate_string(struct rt_env *rt, size_t len);

/* Allocate an array. */
struct rt_array *rt_gc_allocate_array(struct rt_env *rt, size_t size);

/* Allocate an array. */
struct rt_dict *rt_gc_allocate_dict(struct rt_env *rt, size_t size);

/* Reallocate an array. */
struct rt_array *rt_gc_reallocate_array(struct rt_env *rt, struct rt_array *arr, size_t size);

/* Insert a new object to the shallow of young list. */
void rt_gc_insert_new_object_to_list(struct rt_env *rt, struct rt_gc_object_header *obj, int type);

/* Move an object from the shallow list to the young list. */
void rt_gc_move_shallow_to_young_list(struct rt_env *rt, struct rt_value *val);

/* Move all shallow objects to the garbage list. */
void rt_gc_move_shallow_to_garbage_list(struct rt_env *rt);

/* Free all young objects. */
void rt_gc_free_young_objects(struct rt_env *rt);

/* Increment heap usage on new object. */
void rt_gc_increment_heap_usage_on_new_object(struct rt_env *rt, int type, size_t len);

/* Increment heap usage on array expand. */
void rt_gc_increment_heap_usage_by_array_expand(struct rt_env *rt, size_t expand_len);

/* Increment heap usage on dict expand. */
void rt_gc_increment_heap_usage_by_dict_expand(struct rt_env *rt, size_t expand_len);

/* Increment heap usage on dict add. */
void rt_gc_increment_heap_usage_by_dict_add(struct rt_env *rt, size_t key_len);

/* Increment heap usage on dict unset. */
void rt_gc_increment_heap_usage_by_dict_unset(struct rt_env *rt, size_t key_len);

#endif
