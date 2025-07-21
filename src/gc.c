/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * Language Runtime
 */

/*
 * [configuration]
 *  - NO_COMPILATION ... No source compilation features.
 *  - USE_DEBUGGER   ... gdb-like debugger feature.
 */

#include "runtime.h"
#include "gc.h"

#include <stdlib.h>
#include <assert.h>

/* Unlink an element from a list. */
#define UNLINK_FROM_LIST(elem, list)	\
	do { \
		if (elem->prev != NULL) { \
			elem->prev->next = elem->next; \
			if (elem->next != NULL) \
				elem->next->prev = elem->prev; \
		} else { \
			if (elem->next != NULL) \
				elem->next->prev = NULL; \
			list = elem->next; \
		} \
		elem->prev = NULL; \
		elem->next = NULL; \
	} while (0)

/* Insert an element to a list. */
#define INSERT_TO_LIST(elem, list)	\
	do { \
		elem->next = list; \
		elem->prev = NULL; \
		if (list != NULL) \
			list->prev = elem; \
		list = elem; \
	} while (0)			

/* Forward declaration. */
static void rt_recursively_mark_object(struct rt_env *rt, struct rt_object_header *obj);
static void rt_free_object(struct rt_env *rt, struct rt_object_header *obj);

/*
 * Do a shallow GC for nursery space.
 */
bool
noct_shallow_gc(
	struct rt_env *rt)
{
	struct rt_object_header *obj, *next_obj;

	/*
	 * A nursery space belongs to a calling frame.
	 * An object first created in a nursery space,
	 * and when it get a strong reference,
	 * it is moved to the tenured space.
	 * Objects in a nursery space are released by rt_leave_frame(),
	 * and are moved to the garbage list.
	 * The shallow GC sweeps such objects in the garbage list.
	 */

	obj = rt->garbage_list;
	while (obj != NULL) {
		next_obj = obj->next;
		rt_free_object(rt, obj);
		obj = next_obj;
	}
	rt->garbage_list = NULL;

	return true;
}

/*
 * Do a deep GC. (tenured space GC)
 */
bool
noct_deep_gc(
	struct rt_env *rt)
{
	struct rt_object_header *obj, *next_obj;
	struct rt_bindglobal *global;
	struct rt_frame *frame;

	/*
	 * We do a full mark-and-sweep GC.
	 */

	/*
	 * Clear phase.
	 */

	/* Clear the marks of all objects in the young list. */
	obj = rt->young_list;
	while (obj != NULL) {
		obj->is_marked = false;
		obj = obj->next;
	}

	/* Clear the marks of all objects in the all frames' shallow lists. */
	frame = rt->frame;
	while (frame != NULL) {
		/* Clear the marks of all strings in the frame's shallow list. */
		obj = frame->shallow_list;
		while (obj != NULL) {
			obj->is_marked = false;
			obj = obj->next;
		}
		frame = frame->next;
	}

	/*
	 * Mark phase.
	 */

	/* Recursively mark all objects that are referenced by the global variables. */
	global = rt->global;
	while (global != NULL) {
		if (global->val.type == NOCT_VALUE_STRING ||
		    global->val.type == NOCT_VALUE_ARRAY ||
		    global->val.type == NOCT_VALUE_DICT)
			rt_recursively_mark_object(rt, global->val.val.obj);
		global = global->next;
	}

	/* Recursively mark all objects in the all shallow lists. */
	frame = rt->frame;
	while (frame != NULL) {
		/* Recursively mark all objects in the frame's shallow list. */
		obj = frame->shallow_list;
		while (obj != NULL) {
			rt_recursively_mark_object(rt, obj);
			obj = obj->next;
		}
		frame = frame->next;
	}

	/* Recursively mark all objects in the young list that have native references. */
	obj = rt->young_list;
	while (obj != NULL) {
		if (obj->has_native_ref)
			rt_recursively_mark_object(rt, obj);
		obj = obj->next;
	}

	/*
	 * Sweep phase.
	 */

	/* Sweep non-marked objects in the young list. */
	obj = rt->young_list;
	while (obj != NULL) {
		next_obj = obj->next;
		if (!obj->is_marked) {
			/* Unlink. */
			UNLINK_FROM_LIST(obj, rt->young_list);

			/* Remove. */
			rt_free_object(rt, obj);
		}
		obj = next_obj;
	}

	/* Sweep non-marked objects in the all frames' shallow lists. */
	frame = rt->frame;
	while (frame != NULL) {
		/* Sweep non-marked strings in the frame's shallow list. */
		obj = frame->shallow_list;
		while (obj != NULL) {
			next_obj = obj->next;
			if (!obj->is_marked) {
				/* Unlink from the shallow list. */
				UNLINK_FROM_LIST(obj, frame->shallow_list);

				/* Remove. */
				rt_free_object(rt, obj);
			}
			obj = next_obj;
		}
		frame = frame->next;
	}

	return true;
}

/* Mark objects recursively as used. */
static void
rt_recursively_mark_object(
	struct rt_env *rt,
	struct rt_object_header *obj)
{
	struct rt_array *arr;
	struct rt_dict *dict;
	int i;

	if (obj->is_marked)
		return;

	if (obj->type == NOCT_VALUE_STRING) {
		obj->is_marked = true;
	} else if (obj->type == NOCT_VALUE_ARRAY) {
		obj->is_marked = true;
		arr = (struct rt_array *)obj;
		for (i = 0; i < arr->size; i++) {
			struct rt_value *val;
			val = &arr->table[i];
			if (val->type == NOCT_VALUE_STRING ||
			    val->type == NOCT_VALUE_ARRAY ||
			    val->type == NOCT_VALUE_DICT) {
				rt_recursively_mark_object(rt, val->val.obj);
			}
		}
	} else {
		obj->is_marked = true;
		dict = (struct rt_dict *)obj;
		for (i = 0; i < dict->size; i++) {
			struct rt_value *val;
			val = &dict->value[i];
			if (val->type == NOCT_VALUE_STRING ||
			    val->type == NOCT_VALUE_ARRAY ||
			    val->type == NOCT_VALUE_DICT) {
				rt_recursively_mark_object(rt, val->val.obj);
			}
		}
	}
}

/* Free a string, array, or dictionary object. */
static void
rt_free_object(
	struct rt_env *rt,
	struct rt_object_header *obj)
{
	UNUSED_PARAMETER(rt);

	if (obj->type == NOCT_VALUE_STRING) {
		struct rt_string *str;
		str = (struct rt_string *)obj;
		free(str->s);
		free(str);
	} else if (obj->type == NOCT_VALUE_ARRAY) {
		struct rt_array *arr;
		arr = (struct rt_array *)obj;
		free(arr->table);
		free(arr);
	} else if (obj->type == NOCT_VALUE_DICT) {
		struct rt_dict *dict;
		dict = (struct rt_dict *)obj;
		free(dict->key);
		free(dict->value);
		free(dict);
	}
}

/*
 * Insert a new object to the shallow of young list.
 */
void
rt_insert_new_object_to_list(
	struct rt_env *rt,
	struct rt_object_header *obj,
	int type)
{
	if (rt->frame != NULL && type == NOCT_VALUE_STRING) {
		/* Insert the object to the top of the shallow list. */
		if (rt->frame->shallow_list == NULL) {
			obj->type = NOCT_VALUE_STRING;
			obj->prev = NULL;
			obj->next = NULL;
			obj->gc_type = RT_GC_SHALLOW;
			rt->frame->shallow_list = obj;
			rt->frame->shallow_list_tail = obj;
		} else {
			obj->type = NOCT_VALUE_STRING;
			obj->prev = NULL;
			obj->next = rt->frame->shallow_list;
			obj->gc_type = RT_GC_SHALLOW;
			rt->frame->shallow_list->prev = obj;
			rt->frame->shallow_list = obj;
		}
	} else {
		/* Insert the object to the top of the young list. */
		if (rt->young_list == NULL) {
			obj->type = type;
			obj->prev = NULL;
			obj->next = NULL;
			obj->gc_type = RT_GC_YOUNG;
			rt->young_list = obj;
		} else {
			obj->type = type;
			obj->prev = NULL;
			obj->next = rt->young_list;
			obj->gc_type = RT_GC_YOUNG;
			rt->young_list->prev = obj;
			rt->young_list = obj;
		}
	}
}

/*
 * Move an object from the shallow list to the young list.
 */
void
rt_move_shallow_to_young_list(
	struct rt_env *rt,
	struct rt_value *val)
{
	struct rt_object_header *obj;

	if (rt->frame == NULL)
		return;
	if (val->type == NOCT_VALUE_INT || val->type == NOCT_VALUE_FLOAT)
		return;
	if (val->val.obj->gc_type != RT_GC_SHALLOW)
		return;

	/* Unlink from the shallow list. */
	UNLINK_FROM_LIST(val->val.obj, rt->frame->shallow_list);

	/* Relink to the young list. */
	INSERT_TO_LIST(val->val.obj, rt->young_list);

	/* Make the object young. */
	val->val.obj->gc_type = RT_GC_YOUNG;
}

/*
 * Move all shallow objects to the garbage list.
 */
void
rt_move_shallow_to_garbage_list(
	struct rt_env *rt)
{
	if (rt->frame->shallow_list == NULL)
		return;

	if (rt->garbage_list == NULL) {
		rt->garbage_list = rt->frame->shallow_list;
	} else {
		rt->frame->shallow_list_tail->next = rt->garbage_list;
		rt->garbage_list->prev = rt->frame->shallow_list_tail;
		rt->garbage_list = rt->frame->shallow_list;
	}
}

/*
 * Set a native reference status of an object.
 */
bool
noct_set_native_reference(
	NoctEnv *rt,
	NoctValue *val,
	bool is_enabled)
{
	struct rt_object_header *obj;

	assert(rt != NULL);
	assert(val != NULL);

	if (val->type == NOCT_VALUE_INT ||
	    val->type == NOCT_VALUE_FLOAT ||
	    val->type == NOCT_VALUE_FUNC)
		return true;

	obj = (struct rt_object_header *)val->val.obj;
	obj->has_native_ref = is_enabled;

	return true;
}

/*
 * Free all young objects.
 */
void
rt_free_young_objects(
	struct rt_env *rt)
{
	struct rt_object_header *obj, *next_obj;

	obj = rt->young_list;
	while (obj != NULL) {
		next_obj = obj->next;
		rt_free_object(rt, obj);
		obj = next_obj;
	}
}

/*
 * Increment heap usage on new object.
 */
void
rt_increment_heap_usage_on_new_object(
	struct rt_env *rt,
	int type,
	size_t len)
{
	if (type == NOCT_VALUE_STRING) {
		rt->heap_usage +=
			sizeof(struct rt_string) +
			len;
	} else if (type == NOCT_VALUE_ARRAY) {
		rt->heap_usage +=
			sizeof(struct rt_array) +
			sizeof(struct rt_value) * len;
	} else if (type == NOCT_VALUE_DICT) {
		rt->heap_usage +=
			sizeof(struct rt_dict) +
			sizeof(char *) * len +
			sizeof(struct rt_value) * len;
	}
}

/*
 * Increment heap usage on array expand.
 */
void
rt_increment_heap_usage_by_array_expand(
	struct rt_env *rt,
	size_t expand_len)
{
	rt->heap_usage +=
		sizeof(struct rt_value) * expand_len;
}

/*
 * Increment heap usage on dict expand.
 */
void
rt_increment_heap_usage_by_dict_expand(
	struct rt_env *rt,
	size_t expand_len)
{
	rt->heap_usage +=
		sizeof(char *) * expand_len +
		sizeof(struct rt_value) * expand_len;
}

/*
 * Increment heap usage on dict add.
 */
void
rt_increment_heap_usage_by_dict_add(
	struct rt_env *rt,
	size_t key_len)
{
	rt->heap_usage += key_len;
}

/*
 * Increment heap usage on dict unset.
 */
void
rt_increment_heap_usage_by_dict_unset(
	struct rt_env *rt,
	size_t key_len)
{
	assert(rt->heap_usage > key_len);

	rt->heap_usage -= key_len;
}

/*
 * Get an approximate memory usage in bytes.
 */
bool
noct_get_heap_usage(
	struct rt_env *rt,
	size_t *ret)
{
	*ret = rt->heap_usage;
	return true;
}
