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

/* Insert a new object to the shallow of young list. */
void rt_insert_new_object_to_list(struct rt_env *rt, struct rt_object_header *obj, int type);

/* Move an object from the shallow list to the young list. */
void rt_move_shallow_to_young_list(struct rt_env *rt, struct rt_value *val);

/* Move all shallow objects to the garbage list. */
void rt_move_shallow_to_garbage_list(struct rt_env *rt);

/* Free all young objects. */
void rt_free_young_objects(struct rt_env *rt);

/* Increment heap usage on new object. */
void rt_increment_heap_usage_on_new_object(struct rt_env *rt, int type, size_t len);

/* Increment heap usage on array expand. */
void rt_increment_heap_usage_by_array_expand(struct rt_env *rt, size_t expand_len);

/* Increment heap usage on dict expand. */
void rt_increment_heap_usage_by_dict_expand(struct rt_env *rt, size_t expand_len);

/* Increment heap usage on dict add. */
void rt_increment_heap_usage_by_dict_add(struct rt_env *rt, size_t key_len);

/* Increment heap usage on dict unset. */
void rt_increment_heap_usage_by_dict_unset(struct rt_env *rt, size_t key_len);

#endif
