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

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

/* Link an element to a list. */
#define INSERT_TO_LIST(elem, list, prev, next)				\
	do { 								\
		(elem)->prev = NULL;					\
		(elem)->next = (list);					\
		if ((list) == NULL)					\
			(list)->prev = (elem);				\
		(list) = (elem);					\
	} while (0)

/* Unlink an element from a list. */
#define UNLINK_FROM_LIST(elem, list, prev, next)			\
	do {								\
		if ((elem)->prev != NULL) {				\
			(elem)->prev->next = elem->next;		\
			if ((elem)->next != NULL)			\
				(elem)->next->prev = (elem)->prev;	\
		} else {						\
			if ((elem)->next != NULL)			\
				(elem)->next->prev = NULL;		\
			(list) = (elem)->next;				\
		}							\
		(elem)->prev = NULL;					\
		(elem)->next = NULL;					\
	} while (0)

/* Check if a value is a reference type. */
#define IS_REF_VAL(v)			((v)->type >= NOCT_VALUE_STRING)

/* Check if an object is in the nursery or graduate region. */
#define IS_YOUNG_OBJ(o)			((o)->region < RT_GC_REGION_TENURE)

/* Convert an allocation retry count (3,2,1) to an GC level (0, 1, 2). */
#define RETRY_COUNT_TO_GC_LEVEL(rc)	(3 - rc)

/* Forward declaration. */
static struct rt_string *rt_gc_alloc_string_graduate(struct rt_env *env, size_t len, const char *data);
static struct rt_string *rt_gc_alloc_string_tenure(struct rt_env *env, size_t len, const char *data);
static struct rt_array *rt_gc_alloc_array_graduate(struct rt_env *env, size_t size);
static struct rt_array *rt_gc_alloc_array_tenure(struct rt_env *env, size_t size);
static struct rt_dict *rt_gc_alloc_dict_graduate(struct rt_env *env, size_t size);
static struct rt_dict *rt_gc_alloc_dict_tenure(struct rt_env *env, size_t size);
static void rt_gc_young_gc(struct rt_env *env);
static bool rt_gc_mark_and_copy_object(struct rt_env *env, struct rt_gc_object **obj);
static bool rt_gc_promote_object(struct rt_env *env, struct rt_gc_object *obj);
static bool rt_gc_promote_string(struct rt_env *env, struct rt_gc_object *obj);
static bool rt_gc_promote_array(struct rt_env *env, struct rt_gc_object *obj);
static bool rt_gc_promote_dict(struct rt_env *env, struct rt_gc_object *obj);
struct rt_gc_object *rt_gc_copy_string_to_graduate(struct rt_env *env, struct rt_string *old_obj);
struct rt_gc_object *rt_gc_copy_array_to_graduate(struct rt_env *env, struct rt_array *old_obj);
struct rt_gc_object *rt_gc_copy_dict_to_graduate(struct rt_env *env, struct rt_dict *old_obj);
static void rt_gc_free_object(struct rt_env *env, struct rt_gc_object *obj);
static void rt_gc_run_gc(struct rt_env *env, int level);
static void *nursery_alloc(struct rt_env *env, size_t size);
static void *graduate_alloc(struct rt_env *env, size_t size);
static void *tenure_alloc(struct rt_env *env, size_t size);
static void tenure_free(struct rt_env *env, void *p);

/*
 * Initializes the garbage collector and allocate memory regions.
 */
bool
rt_gc_init(
	struct rt_vm *vm)
{
	memset(&vm->gc, 0, sizeof(struct rt_gc_info));

	/* Initialize the nursery allocator. */
	if (!arena_init(&vm->gc.nursery_arena, RT_GC_NURSERY_SIZE))
		return false;

	/* Initialize the graduate allocators. */
	if (!arena_init(&vm->gc.graduate_arena[0], RT_GC_GRADUATE_SIZE))
		return false;
	if (!arena_init(&vm->gc.graduate_arena[1], RT_GC_GRADUATE_SIZE))
		return false;
	vm->gc.cur_grad_from = 0;
	vm->gc.cur_grad_to = 1;

	/* TODO: Initialize the tenure allocator.  */

	return true;
}

void
rt_gc_cleanup(
	struct rt_vm *vm)
{
}

/*
 * Cleanups the garbage collector and deallocate memory regions.
 */
void env_gc_cleanup(struct rt_vm *vm)
{
	arena_cleanup(&vm->gc.nursery_arena);
	arena_cleanup(&vm->gc.graduate_arena[0]);
	arena_cleanup(&vm->gc.graduate_arena[1]);

	/* TODO: Cleanup the tenure allocator. */
}

/*
 * Allocates a string object in the appropriate region.
 */
struct rt_string *
rt_gc_alloc_string(
	struct rt_env *env,
	size_t len,
	const char *data)
{
	struct rt_string *rts;
	char *s;
	int retry_count;

	/*
	 * [Large Object Promotion]
	 *  - If the string is large, allocate in the tenure region.
	 */
	if (len >= RT_GC_LOP_THRESHOLD)
		return rt_gc_alloc_string_tenure(env, len, data);

	/* Allocate in the nursery region. */
	retry_count = 3;
	while (retry_count >= 0) {
		/* Allocate a rt_string buffer. */
		rts = nursery_alloc(env, sizeof(struct rt_string));
		if (rts == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Allocate a string block. */
		s = nursery_alloc(env, len);
		if (s == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Copy the string. */
		memcpy(s, data, len);

		/* Setup the struct. */
		memset(&rts->head, 0, sizeof(struct rt_gc_object));
		rts->head.type = RT_GC_TYPE_STRING;
		rts->head.region = RT_GC_REGION_NURSERY;
		INSERT_TO_LIST(&rts->head, env->vm->gc.nursery_list, prev, next);
		rts->data = s;
		rts->len = len;

		/* Succeeded. */
		return rts;
	}

	/* Failed. */
	rt_out_of_memory(env);
	return NULL;
}

/* Allocates a string object in the graduate region. */
static struct rt_string *
rt_gc_alloc_string_graduate(
	struct rt_env *env,
	size_t len,
	const char *data)
{
	struct rt_string *rts;
	char *s;

	/*
	 * This function is only called from the young GC,
	 * and thus, we don't use young GC for a retry here.
	 */

	/* Try allocating in the graduate region. */
	do {
		/* Allocate a rt_string buffer. */
		rts = graduate_alloc(env, sizeof(struct rt_string));
		if (rts == NULL)
			break;

		/* Allocate a string block. */
		s = graduate_alloc(env, len);
		if (s == NULL)
			break;

		/* Copy the string. */
		memcpy(s, data, len);

		/* Setup the struct. */
		memset(&rts->head, 0, sizeof(struct rt_gc_object));
		rts->head.type = RT_GC_TYPE_STRING;
		rts->head.region = RT_GC_REGION_GRADUATE;
		INSERT_TO_LIST(&rts->head, env->vm->gc.graduate_new_list, prev, next);
		rts->data = s;
		rts->len = len;

		/* Succeeded. (graduate) */
		return rts;
	} while (0);

	/*
	 * Failed to allocate in the graduate region.
	 * Try allocating in the tenure region.
	 */
	rts = rt_gc_alloc_string_tenure(env, len, data);
	if (rts != NULL)
		return NULL;

	/* Partially succeeded. (tenure) */
	return rts;
}

/* Allocates a large string in the tenure region. */
static struct rt_string *
rt_gc_alloc_string_tenure(
	struct rt_env *env,
	size_t len,
	const char *data)
{
	struct rt_string *rts;
	char *s;
	int retry_count;

	/* Allocate in the tenure region. */
	retry_count = 2;
	while (retry_count >= 0) {
		/* Allocate a rt_string buffer. */
		rts = tenure_alloc(env, sizeof(struct rt_string));
		if (rts == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Allocate a string block. */
		s = tenure_alloc(env, len);
		if (s == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Copy the string. */
		memcpy(s, data, len);

		/* Setup the struct. */
		memset(&rts->head, 0, sizeof(struct rt_gc_object));
		rts->head.type = RT_GC_TYPE_STRING;
		rts->head.region = RT_GC_REGION_TENURE;
		INSERT_TO_LIST(&rts->head, env->vm->gc.tenure_list, prev, next);
		rts->data = s;
		rts->len = len;

		/* Succeeded. */
		return rts;
	}

	/* Failed. */
	rt_out_of_memory(env);
	return NULL;
}

/*
 * Allocates an array object in the appropriate region.
 */
struct rt_array *
rt_gc_alloc_array(
	struct rt_env *env,
	size_t size)
{
	struct rt_array *arr;
	size_t len;
	struct rt_value *table;
	int retry_count;

	/*
	 * [Large Object Promotion]
	 *  - If the array is large, allocate in the tenure region.
	 */
	len = size * sizeof(struct rt_value *);
	if (len >= RT_GC_LOP_THRESHOLD)
		return rt_gc_alloc_array_tenure(env, size);

	/* Allocate in the nursery region. */
	retry_count = 3;
	while (retry_count > 0) {
		/* Allocate a rt_array buffer. */
		arr = nursery_alloc(env, sizeof(struct rt_array));
		if (arr == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Allocate an array block. */
		table = nursery_alloc(env, len);
		if (table == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Setup the struct. */
		memset(&arr->head, 0, sizeof(struct rt_gc_object));
		arr->head.type = RT_GC_TYPE_ARRAY;
		arr->head.region = RT_GC_REGION_NURSERY;
		INSERT_TO_LIST(&arr->head, env->vm->gc.nursery_list, prev, next);
		arr->alloc_size = size;
		arr->size = 0;
		arr->table = table;

		/* Succeeded. */
		return arr;
	}

	/* Failed. */
	rt_out_of_memory(env);
	return NULL;
}

/* Allocates an array object in the graduate region. */
static struct rt_array *
rt_gc_alloc_array_graduate(
	struct rt_env *env,
	size_t size)
{
	struct rt_array *arr;

	/*
	 * This function is only called from the young GC,
	 * and thus, we don't use young GC for a retry here.
	 */

	/* Try allocating in the graduate region. */
	do {
		/* Allocate a rt_arrary buffer. */
		arr = graduate_alloc(env, sizeof(struct rt_array));
		if (arr == NULL)
			break;

		/* Allocate a table block. */
		arr->table = graduate_alloc(env, size * sizeof(struct rt_value *));
		if (arr->table == NULL)
			break;

		/* Setup the struct. */
		memset(&arr->head, 0, sizeof(struct rt_gc_object));
		arr->head.type = RT_GC_TYPE_ARRAY;
		arr->head.region = RT_GC_REGION_GRADUATE;
		INSERT_TO_LIST(&arr->head, env->vm->gc.graduate_new_list, prev, next);
		arr->size = size;

		/* Succeeded. (graduate) */
		return arr;
	} while (0);

	/*
	 * Failed to allocate in the graduate region.
	 * Try allocating in the tenure region.
	 */
	arr = rt_gc_alloc_array_tenure(env, size);
	if (arr != NULL)
		return NULL;

	/* Partially succeeded. (tenure) */
	return arr;
}

/* Allocates a large array in the tenure region. */
static struct rt_array *
rt_gc_alloc_array_tenure(
	struct rt_env *env,
	size_t size)
{
	struct rt_array *arr;
	size_t len;
	struct rt_value *table;
	int retry_count;

	/* Allocate in the tenure region. */
	retry_count = 2;
	while (retry_count > 0) {
		/* Allocate a rt_array buffer. */
		arr = tenure_alloc(env, sizeof(struct rt_array));
		if (arr == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Allocate an array block. */
		table = tenure_alloc(env, size);
		if (table == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Setup the struct. */
		memset(&arr->head, 0, sizeof(struct rt_gc_object));
		arr->head.type = RT_GC_TYPE_ARRAY;
		arr->head.region = RT_GC_REGION_TENURE;
		INSERT_TO_LIST(&arr->head, env->vm->gc.tenure_list, prev, next);
		arr->alloc_size = size;
		arr->size = 0;
		arr->table = table;

		/* Succeeded. */
		return arr;
	}

	/* Failed. */
	rt_out_of_memory(env);
	return NULL;
}

/*
 * Allocates a dictionary object in the appropriate region.
 */
struct rt_dict *
rt_gc_alloc_dict(
	struct rt_env *env,
	size_t size)
{
	struct rt_dict *dict;
	size_t len;
	char **key_table;
	struct rt_value *value_table;
	int retry_count;

	/*
	 * [Large Object Promotion]
	 *  - If the array is large, allocate in the tenure region.
	 */
	if (size * sizeof(char *) + size * sizeof(struct rt_value *) >= RT_GC_LOP_THRESHOLD)
		return rt_gc_alloc_dict_tenure(env, size);

	/* Allocate in the nursery region. */
	retry_count = 3;
	while (retry_count > 0) {
		/* Allocate a rt_dict buffer. */
		dict = nursery_alloc(env, sizeof(struct rt_dict));
		if (dict == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Allocate the key array block. */
		key_table = nursery_alloc(env, size * sizeof(char *));
		if (key_table == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Allocate the value array block. */
		value_table = nursery_alloc(env, size * sizeof(struct rt_value *));
		if (value_table == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Setup the struct. */
		memset(&dict->head, 0, sizeof(struct rt_gc_object));
		dict->head.type = RT_GC_TYPE_DICT;
		dict->head.region = RT_GC_REGION_NURSERY;
		INSERT_TO_LIST(&dict->head, env->vm->gc.nursery_list, prev, next);
		dict->alloc_size = size;
		dict->size = 0;
		dict->key = key_table;
		dict->value = value_table;

		/* Succeeded. */
		return dict;
	}

	/* Failed. */
	rt_out_of_memory(env);
	return NULL;
}

/* Allocates an array object in the graduate region. */
static struct rt_dict *
rt_gc_alloc_dict_graduate(
	struct rt_env *env,
	size_t size)
{
	struct rt_dict *dict;

	/*
	 * This function is only called from the young GC,
	 * and thus, we don't use young GC for a retry here.
	 */

	/* Try allocating in the graduate region. */
	do {
		/* Allocate a rt_dict buffer. */
		dict = graduate_alloc(env, sizeof(struct rt_dict));
		if (dict == NULL)
			break;

		/* Allocate the key and value blocks. */
		dict->key = graduate_alloc(env, size * sizeof(struct rt_value *));
		if (dict->key == NULL)
			break;
		dict->value = graduate_alloc(env, size * sizeof(struct rt_value *));
		if (dict->value == NULL)
			break;

		/* Setup a struct. */
		memset(&dict->head, 0, sizeof(struct rt_gc_object));
		dict->head.type = RT_GC_TYPE_DICT;
		dict->head.region = RT_GC_REGION_GRADUATE;
		INSERT_TO_LIST(&dict->head, env->vm->gc.graduate_new_list, prev, next);
		dict->size = size;

		/* Succeeded. (graduate) */
		return dict;
	} while (0);

	/*
	 * Failed to allocate in the graduate region.
	 * Try allocating in the tenure region.
	 */
	dict = rt_gc_alloc_dict_tenure(env, size);
	if (dict != NULL)
		return NULL;

	/* Partially succeeded. (tenure) */
	return dict;
}

/* Allocates a dictionary object in the tenure region. */
static struct rt_dict *
rt_gc_alloc_dict_tenure(
	struct rt_env *env,
	size_t size)
{
	struct rt_dict *dict;
	char **key_table;
	struct rt_value *value_table;
	int retry_count;

	/* Allocate in the tenure region. */
	retry_count = 1;
	while (retry_count > 0) {
		/* Allocate the rt_dict buffer. */
		dict = tenure_alloc(env, sizeof(struct rt_dict));
		if (dict == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Allocate the key array block. */
		key_table = tenure_alloc(env, size * sizeof(char *));
		if (key_table == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Allocate the value array block. */
		value_table = tenure_alloc(env, size * sizeof(struct rt_value *));
		if (value_table == NULL) {
			/* Retry. */
			retry_count--;
			if (retry_count > 0) {
				rt_gc_run_gc(env, 1);
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
			continue;
		}

		/* Setup a value. */
		memset(&dict->head, 0, sizeof(struct rt_gc_object));
		dict->head.type = RT_GC_TYPE_DICT;
		dict->head.region = RT_GC_REGION_TENURE;
		INSERT_TO_LIST(&dict->head, env->vm->gc.tenure_list, prev, next);
		dict->alloc_size = size;
		dict->size = 0;
		dict->key = key_table;
		dict->value = value_table;

		/* Succeeded. */
		return dict;
	}

	/* Failed. */
	rt_out_of_memory(env);
	return NULL;
}

/*
 * Allocates for a dictionary key.
 */
char *
rt_gc_alloc_dict_key(
	struct rt_env *env,
	struct rt_dict *dict,
	const char *key)
{
	size_t len;
	char *ret;
	int retry_count;

	/* Get the length of the key. */
	len = strlen(key) + 1;

	retry_count = dict->head.region == RT_GC_REGION_TENURE ? 2 : 3;
	while (retry_count >= 0) {
		/* Try allocation from a region. */
		switch (dict->head.region) {
		case RT_GC_REGION_NURSERY:
			ret = nursery_alloc(env, len);
			break;
		case RT_GC_REGION_GRADUATE:
			ret = nursery_alloc(env, len);
			break;
		case RT_GC_REGION_TENURE:
			ret = tenure_alloc(env, len);
			break;
		}

		if (ret == NULL) {
			if (retry_count > 0) {
				rt_gc_run_gc(env, RETRY_COUNT_TO_GC_LEVEL(retry_count));
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Succeeded. */
		memcpy(ret, key, len);
		return ret;
	}

	/* Failed. */
	return NULL;
}

/*
 * Frees a dictionary key.
 */
void
rt_gc_free_dict_key(
	struct rt_env *env,
	struct rt_dict *dict,
	char *key)
{
	/* Free by the region. */
	switch (dict->head.region) {
	case RT_GC_REGION_NURSERY:
	case RT_GC_REGION_GRADUATE:
		/* No action. (arena allocator) */
		break;
	case RT_GC_REGION_TENURE:
		tenure_free(env, key);
		break;
	}
}

/*
 * Write barrier: registers a container in the remember set if it
 * references a young object.
 */
void
rt_gc_array_write_barrier(
	struct rt_env *env,
	struct rt_array *arr,
	int index,
	struct rt_value *val)
{
	/*
	 * If all of the following are satisfied, add the array to the
	 * remember set.
	 *  - the array is a tenure object
	 *  - the array is not in the remember set
	 *  - the element is a reference
	 *  - the element is a nursery or graduate object
	 */
	if (arr->head.region == RT_GC_REGION_TENURE &&
	    !arr->head.rem_flg &&
	    IS_REF_VAL(val) &&
	    IS_YOUNG_OBJ(val->val.obj)) {
		arr->head.rem_flg = true;
		INSERT_TO_LIST(&arr->head, env->vm->gc.remember_set, rem_prev, rem_next);
	}
}

/*
 * Write barrier: registers a container in the remember set if it
 * references a younger object.
 */
void
rt_gc_dict_write_barrier(
	struct rt_env *env,
	struct rt_dict *dict,
	int index,
	struct rt_value *val)
{
	/*
	 * If all of the following are satisfied, add the array to the
	 * remember set.
	 *  - the array is a tenure objectf
	 *  - the array is not in the remember set
	 *  - the element is a reference
	 *  - the element is a nursery or graduate object
	 */
	if (dict->head.region == RT_GC_REGION_TENURE &&
	    !dict->head.rem_flg &&
	    IS_REF_VAL(val) &&
	    IS_YOUNG_OBJ(val->val.obj)) {
		dict->head.rem_flg = true;
		INSERT_TO_LIST(&dict->head, env->vm->gc.remember_set, rem_prev, rem_next);
	}
}

/* Executes a young GC. */
static void
rt_gc_young_gc(
	struct rt_env *env)
{
	int from, to;
	struct rt_gc_object *obj;
	struct rt_bindglobal *global;
	struct rt_frame *frame;
	int i;

	/*
	 * Clear marks.
	 */

	/* Clear marks of the nursery objects. */
	obj = env->vm->gc.nursery_list;
	while (obj != NULL) {
		obj->is_marked = false;
		obj->forward = NULL;
		obj = obj->next;
	}

	/* Clear marks of the graduate (from) objects. */
	obj = env->vm->gc.graduate_list;
	while (obj != NULL) {
		obj->is_marked = false;
		obj->forward = NULL;
		obj = obj->next;
	}

	/*
	 * Mark and Copy objects.
	 *  - Recursively visit root objects.
	 *  - Copy nursery objects to the graduate region.
	 *  - Promote graduate objects that satisfy the criteria to the tenure region.
	 */

	/* For all global variables. */
	global = env->vm->global;
	while (global != NULL) {
		if (IS_REF_VAL(&global->val)) {
			if (!rt_gc_mark_and_copy_object(env, &global->val.val.obj))
				return;
		}
		global = global->next;
	}

	/* For all call frames. */
	frame = env->frame;
	while (frame != NULL) {
		/* For all temporary variables in the frame. */
		for (i = 0; i < frame->tmpvar_size; i++) {
			if (IS_REF_VAL(&frame->tmpvar[i])) {
				if (!rt_gc_mark_and_copy_object(env, &frame->tmpvar[i].val.obj))
					return;
			}
		}

		/* For all pinned C local variables in the frame. */
		for (i = 0; i < frame->pinned_count; i++) {
			if (IS_REF_VAL(frame->pinned[i])) {
				if (!rt_gc_mark_and_copy_object(env, &(*frame->pinned[i]).val.obj))
					return;
			}
		}

		frame = frame->next;
	}

	/* For all pinned C global variables. */
	for (i = 0; i < env->vm->pinned_count; i++) {
		if (IS_REF_VAL(env->vm->pinned[i])) {
			if (!rt_gc_mark_and_copy_object(env, &(*env->vm->pinned[i]).val.obj))
				return;
		}
	}

	/* For all remember set objects. */
	obj = env->vm->gc.remember_set;
	while (obj != NULL) {
		if (!rt_gc_mark_and_copy_object(env, &obj))
			return;
		obj = obj->rem_next;
	}

	/*
	 * Update references from the remember set.
	 */

	/* For each remember set object, update the addresses of the inner elements using the forwarding pointer technique. */
	obj = env->vm->gc.remember_set;
	while (obj != NULL) {
		if (obj->type == RT_GC_TYPE_ARRAY) {
			struct rt_array *arr = (struct rt_array *)obj;
			for (i = 0; i < arr->size; i++) {
				if (IS_REF_VAL(&arr->table[i]) &&
				    arr->table[i].val.obj->region == RT_GC_REGION_GRADUATE &&
				    arr->table[i].val.obj->forward != NULL) {
					arr->table[i].val.obj = arr->table[i].val.obj->forward;
				}
			}
		} else {
			struct rt_dict *dict = (struct rt_dict *)obj;
			for (i = 0; i < dict->size; i++) {
				if (IS_REF_VAL(&dict->value[i]) &&
				    dict->value[i].val.obj->region == RT_GC_REGION_GRADUATE &&
				    dict->value[i].val.obj->forward != NULL) {
					dict->value[i].val.obj = dict->value[i].val.obj->forward;
				}
			}
		}
		obj = obj->rem_next;
	}

	/* For each remember set object, remove from the remember set if the object doesn't have a cross generation reference. */
	obj = env->vm->gc.remember_set;
	while (obj != NULL) {
		bool has_cross_gen_ref = false;
		if (obj->type == RT_GC_TYPE_ARRAY) {
			struct rt_array *arr = (struct rt_array *)obj;
			for (i = 0; i < arr->size; i++) {
				if (IS_REF_VAL(&arr->table[i]) &&
				    IS_YOUNG_OBJ(arr->table[i].val.obj)) {
					has_cross_gen_ref = true;
					break;
				}
			}
		} else {
			struct rt_dict *dict = (struct rt_dict *)obj;
			for (i = 0; i < dict->size; i++) {
				if (IS_REF_VAL(&dict->value[i]) &&
				    IS_YOUNG_OBJ(dict->value[i].val.obj)) {
					has_cross_gen_ref = true;
					break;
				}
			}
		}
		if (!has_cross_gen_ref) {
			/* Unlink from the remember set list. */
			if (obj->rem_prev != NULL)
				obj->rem_prev->rem_next = obj->rem_next;
			else
				env->vm->gc.remember_set = obj->rem_next;
			if (obj->rem_next != NULL)
				obj->rem_next->rem_prev = obj->rem_prev;
		}
		obj = obj->rem_next;
	}

	/*
	 * Finalize.
	 */

	/* Clear the nursery. */
	arena_cleanup(&env->vm->gc.nursery_arena);
		   
	/* Clear the graduate (from) */
	arena_cleanup(&env->vm->gc.graduate_arena[from]);

	/* Swap "to" and "from". */
	env->vm->gc.cur_grad_from = to;
	env->vm->gc.cur_grad_to = from;
	env->vm->gc.graduate_list = env->vm->gc.graduate_new_list;
	env->vm->gc.graduate_new_list = NULL;
}

/* Marks-and-copies objects recursively. */
static bool
rt_gc_mark_and_copy_object(
	struct rt_env *env,
	struct rt_gc_object **obj)
{
	struct rt_gc_object *new_obj;
	struct rt_array *arr;
	struct rt_dict *dict;
	bool is_promoted;
	int i;

	/* Return if already processed. */
	if ((*obj)->is_marked)
		return true;

	/* Copy or promote. */
	is_promoted = false;
	if ((*obj)->region != RT_GC_REGION_TENURE) {
		/* Check for the promotion. */
		if ((*obj)->promotion_count < RT_GC_PROMOTION_THRESHOLD) {
			/* Copy the object */
			switch ((*obj)->type) {
			case RT_GC_TYPE_STRING:
				new_obj = rt_gc_copy_string_to_graduate(env, (struct rt_string *)*obj);
				break;
			case RT_GC_TYPE_ARRAY:
				new_obj = rt_gc_copy_array_to_graduate(env, (struct rt_array *)*obj);
				break;
			case RT_GC_TYPE_DICT:
				new_obj = rt_gc_copy_dict_to_graduate(env, (struct rt_dict *)*obj);
				break;
			default:
				assert(NEVER_COME_HERE);
				break;
			}
			(*obj)->forward = new_obj;
			new_obj->promotion_count++;
		} else {
			/* Promote the object. */
			if (!rt_gc_promote_object(env, *obj))
				return false;
			is_promoted = true;
		}
		*obj = new_obj;
		(*obj)->is_marked = true;
	}

	/* Recursively mark and copy. */
	switch ((*obj)->type) {
	case RT_GC_TYPE_STRING:
		break;
	case RT_GC_TYPE_ARRAY:
		arr = (struct rt_array *)*obj;
		for (i = 0; i < arr->size; i++) {
			if (IS_REF_VAL(&arr->table[i])) {
				if (!rt_gc_mark_and_copy_object(env, &arr->table[i].val.obj))
					return false;
			}
		}
		break;	
	case RT_GC_TYPE_DICT:
		dict = (struct rt_dict *)*obj;
		for (i = 0; i < dict->size; i++) {
			if (IS_REF_VAL(&dict->value[i])) {
				if (!rt_gc_mark_and_copy_object(env, &dict->value[i].val.obj))
					return false;
			}
		}
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	/* Check for the remember set operation. */
	if (is_promoted) {
		if ((*obj)->type == RT_GC_TYPE_ARRAY) {
			/* Check for cross-generation references. */
			arr = (struct rt_array *)*obj;
			for (i = 0; i < arr->size; i++) {
				/* If the element is young generation. */
				if (IS_REF_VAL(&arr->table[i]) &&
				    IS_YOUNG_OBJ(arr->table[i].val.obj)) {
					/* And if the element is not promoted to the tenure region. */
					if (arr->table[i].val.obj->forward != NULL &&
					    arr->table[i].val.obj->forward->region == RT_GC_REGION_TENURE)
						continue;

					/* Add to remember set. */
					arr->head.rem_flg = true;
					INSERT_TO_LIST(arr->table[i].val.obj, env->vm->gc.remember_set,rem_prev, rem_next);
					break;
				}
			}
		} else if ((*obj)->type == RT_GC_TYPE_DICT) {
			/* Check for cross-generation references. */
			dict = (struct rt_dict *)*obj;
			for (i = 0; i < dict->size; i++) {
				/* If the element is young generation. */
				if (IS_REF_VAL(&dict->value[i]) &&
				    IS_YOUNG_OBJ(dict->value[i].val.obj)) {
					/* And if the element is not promoted to the tenure region. */
					if (dict->value[i].val.obj->forward != NULL &&
					    dict->value[i].val.obj->forward->region == RT_GC_REGION_TENURE)
						continue;

					/* Add to remember set. */
					dict->head.rem_flg = true;
					INSERT_TO_LIST(dict->value[i].val.obj, env->vm->gc.remember_set,rem_prev, rem_next);
					break;
				}
			}
		}
	}

	return true;
}

/* Promotes an object. */
static bool
rt_gc_promote_object(
	struct rt_env *env,
	struct rt_gc_object *obj)
{
	switch (obj->type) {
	case RT_GC_TYPE_STRING:
		if (!rt_gc_promote_string(env, obj))
			return false;
		break;
	case RT_GC_TYPE_ARRAY:
		if (!rt_gc_promote_array(env, obj))
			return false;
		break;
	case RT_GC_TYPE_DICT:
		if (!rt_gc_promote_dict(env, obj))
			return false;
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	return true;
}

/* Promotes a string object. */
static bool
rt_gc_promote_string(
	struct rt_env *env,
	struct rt_gc_object *obj)
{
	struct rt_string *old_rts, *new_rts;

	/* Allocate a string object. */
	old_rts = (struct rt_string *)obj;
	new_rts = rt_gc_alloc_string_tenure(env, old_rts->len, old_rts->data);
	if (new_rts == NULL)
		return false;

	obj->forward = &new_rts->head;

	return true;
}

/* Promotes an array object. */
static bool
rt_gc_promote_array(
	struct rt_env *env,
	struct rt_gc_object *obj)
{
	struct rt_array *old_arr, *new_arr;

	/* Allocate an array object. */
	old_arr = (struct rt_array *)obj;
	new_arr = rt_gc_alloc_array_tenure(env, old_arr->size);
	if (new_arr == NULL)
		return false;

	/* Copy the table. */
	memcpy(new_arr->table, old_arr->table, old_arr->size);

	obj->forward = &new_arr->head;

	return true;
}

/* Promotes a dictionary object. */
static bool
rt_gc_promote_dict(
	struct rt_env *env,
	struct rt_gc_object *obj)
{
	struct rt_dict *old_dict, *new_dict;
	int i;
	size_t len;

	/* Allocate a dictionary object. */
	old_dict = (struct rt_dict *)obj;
	new_dict = rt_gc_alloc_dict_tenure(env, old_dict->size);
	if (new_dict == NULL)
		return false;

	/* Copy the keys. */
	for (i = 0; i < old_dict->size; i++) {
		len = strlen(old_dict->key[i]);
		new_dict->key[i] = tenure_alloc(env, len + 1);
		if (new_dict->key[i] == NULL)
			return false;

		strcpy(new_dict->key[i], old_dict->key[i]);
	}

	/* Copy the values. */
	memcpy(new_dict->value, old_dict->value, old_dict->size);

	obj->forward = &new_dict->head;

	return true;
}

/* Copies a string object to the graduate region. */
struct rt_gc_object *
rt_gc_copy_string_to_graduate(
	struct rt_env *env,
	struct rt_string *old_obj)
{
	struct rt_string *new_obj;
	char *s;

	/*
	 * Strings larger than RT_GC_LOP_THRESHOLD must not be in the
	 * nursery or graduate regions.
	 */
	assert(old_obj->len < RT_GC_LOP_THRESHOLD);

	/* Allocate in the graduate region. */
	new_obj = rt_gc_alloc_string_graduate(env, old_obj->len, old_obj->data);
	if (new_obj == NULL)
		return NULL;

	/* Succeeded. */
	return &new_obj->head;
}

/* Copies an array object to the graduate region. */
struct rt_gc_object *
rt_gc_copy_array_to_graduate(
	struct rt_env *env,
	struct rt_array *old_obj)
{
	struct rt_array *new_obj;
	int i;

	/*
	 * Arrays larger than RT_GC_LOP_THRESHOLD/sizeof(struct rt_value *) must not be in the
	 * nursery or graduate regions.
	 */
	assert(old_obj->size < RT_GC_LOP_THRESHOLD / sizeof(struct rt_value *));

	/* Allocate in the graduate region. (If failed, in the tenure region.) */
	new_obj = rt_gc_alloc_array_graduate(env, old_obj->size);
	if (new_obj == NULL)
		return NULL;

	/* Copy the table. */
	memcpy(new_obj->table, old_obj->table, old_obj->size * sizeof(struct rt_value *));

	/* Check for cross-generation references. */
	if (new_obj->head.region == RT_GC_REGION_TENURE) {
		for (i = 0; i < new_obj->size; i++) {
			if (IS_REF_VAL(&new_obj->table[i]) &&
			    IS_YOUNG_OBJ(new_obj->table[i].val.obj)) {
				new_obj->head.rem_flg = true;
				INSERT_TO_LIST(new_obj->table[i].val.obj, env->vm->gc.remember_set,rem_prev, rem_next);
				break;
			}
		}
	}

	return &new_obj->head;
}

/* Copies a dictionary object to the graduate region. */
struct rt_gc_object *
rt_gc_copy_dict_to_graduate(
	struct rt_env *env,
	struct rt_dict *old_obj)
{
	struct rt_dict *new_obj;
	int retry;
	int i;
	bool need_retry;

	/*
	 * Arrays larger than RT_GC_LOP_THRESHOLD/sizeof(struct rt_value *)/2 must not be in the
	 * nursery or graduate regions.
	 */
	assert(old_obj->size < RT_GC_LOP_THRESHOLD / sizeof(struct rt_value *) / 2);

	do {
		/* Allocate in the graduate region. (If failed, in the tenure region.) */
		new_obj = rt_gc_alloc_dict_graduate(env, old_obj->size);
		if (new_obj == NULL)
			return NULL;

		/* Copy the keys. */
		if (new_obj->head.region == RT_GC_REGION_GRADUATE) {
			for (i = 0; i < new_obj->size; i++) {
				new_obj->key[i] = graduate_alloc(env, strlen(old_obj->key[i]) + 1);
				if (new_obj->key[i] == NULL) {
					/* Graduate is full. Retry with the tenure region. */
					need_retry = true;
					new_obj = NULL;
					break;
				}
				strcpy(new_obj->key[i], old_obj->key[i]);
			}
			if (need_retry)
				break;
		} else if (new_obj->head.region == RT_GC_REGION_TENURE) {
			for (i = 0; i < new_obj->size; i++) {
				new_obj->key[i] = tenure_alloc(env, strlen(old_obj->key[i]) + 1);
				if (new_obj->key[i] == NULL) {
					/* Tenure is full. */
					rt_out_of_memory(env);
					return NULL;
				}
				strcpy(new_obj->key[i], old_obj->key[i]);
			}
		} else {
			assert(NEVER_COME_HERE);
		}

		/* Copy the value. */
		memcpy(new_obj->value, old_obj->value, old_obj->size * sizeof(struct rt_value *));

		/* Check for cross-generation references. */
		if (new_obj->head.region == RT_GC_REGION_TENURE) {
			for (i = 0; i < new_obj->size; i++) {
				if (IS_REF_VAL(&new_obj->value[i]) &&
				    IS_YOUNG_OBJ(new_obj->value[i].val.obj)) {
					new_obj->head.rem_flg = true;
					INSERT_TO_LIST(new_obj->value[i].val.obj, env->vm->gc.remember_set,rem_prev, rem_next);
					break;
				}
			}
		}
	} while (0);

	/* Check for failure. */
	if (new_obj == NULL)
		return NULL;

	/* Succeeded. */
	return &new_obj->head;
}

static void
rt_gc_old_gc(
	struct rt_env *env)
{
	/* TODO: implement. */
}

/* Free a string, array, or dictionary object. */
static void
rt_gc_free_object(
	struct rt_env *env,
	struct rt_gc_object *obj)
{
	int i;

	UNUSED_PARAMETER(env);

	/*
	 * Nursery and graduate objects are allocated by arena
	 * allocater, and no need for freeing.
	 */
	if (obj->region != RT_GC_REGION_TENURE)
		return;

	if (obj->type == NOCT_VALUE_STRING) {
		struct rt_string *str;
		str = (struct rt_string *)obj;
		tenure_free(env, str->data);
		tenure_free(env, str);
	} else if (obj->type == NOCT_VALUE_ARRAY) {
		struct rt_array *arr;
		arr = (struct rt_array *)obj;
		tenure_free(env, arr->table);
		tenure_free(env, arr);
	} else if (obj->type == NOCT_VALUE_DICT) {
		struct rt_dict *dict;
		dict = (struct rt_dict *)obj;
		for (i = 0; i < dict->size; i++)
			tenure_free(env, dict->key[i]);
		tenure_free(env, dict->key);
		tenure_free(env, dict->value);
		tenure_free(env, dict);
	}
}

/*
 * Pins a C global variable.
 */
bool
rt_gc_pin_global(
	struct rt_env *env,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (env->vm->pinned_count == RT_GLOBAL_PIN_MAX) {
		rt_error(env, _("Too many pinned global variables."));
		return false;
	}

	env->vm->pinned[env->vm->pinned_count++] = val;

	return true;
}

/*
 * Unpins a C global variable.
 */
bool
rt_gc_unpin_global(
	struct rt_env *env,
	struct rt_value *val)
{
	int i;

	assert(env != NULL);
	assert(val != NULL);

	for (i = 0; i < env->vm->pinned_count; i++) {
		if (env->vm->pinned[i] == val) {
			memmove(&env->vm->pinned[i+1], &env->vm->pinned[i], (RT_GLOBAL_PIN_MAX - i - 1) * sizeof(struct rt_value *));

			/* Succeeded. */
			return true;
		}
	}

	/* Failed. */
	assert(PINNED_VAR_NOT_FOUND);
	return false;
}

/*
 * Pins a C local variable.
 */
bool
rt_gc_pin_local(
	struct rt_env *env,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (env->frame->pinned_count == RT_LOCAL_PIN_MAX) {
		rt_error(env, _("Too many pinned local variables."));
		return false;
	}

	env->frame->pinned[env->frame->pinned_count++] = val;

	return true;
}

/*
 * Retrieves the approximate memory usage, in bytes.
 */
bool
rt_gc_get_heap_usage(
	struct rt_env *env,
	size_t *ret)
{
	assert(env != NULL);
	assert(ret != NULL);

	/* TODO */
	*ret = 0;

	return true;
}

/* Run GC at the specified level. */
static void
rt_gc_run_gc(
	struct rt_env *env,
	int level)
{
	switch (level) {
	case 0:
		rt_gc_young_gc(env);
		break;
	case 1:
		rt_gc_old_gc(env);
		break;
	case 2:
		rt_gc_old_gc(env);
		rt_gc_young_gc(env);
		break;
	}
}

/*
 * Manually trigger the young GC (nursery and graduate regions, copying GC).
 */
void
rt_gc_level1_gc(struct rt_env *env)
{
	rt_gc_young_gc(env);
}

/*
 * Manually trigger the old GC (tenure region, mark-and-sweep GC).
 */
void rt_gc_level2_gc(struct rt_env *env)
{
	rt_gc_old_gc(env);
}

/*
 * Manually triggers a full GC. (tenure, nursery + graduate)
 */
void rt_gc_level3_gc(struct rt_env *env)
{
	rt_gc_old_gc(env);
	rt_gc_young_gc(env);
}

static void *
nursery_alloc(
	struct rt_env *env,
	size_t size)
{
	return arena_alloc(&env->vm->gc.nursery_arena, size);
}

static void *
graduate_alloc(
	struct rt_env *env,
	size_t size)
{
	return arena_alloc(&env->vm->gc.graduate_arena[env->vm->gc.cur_grad_to], size);
}

static void *
tenure_alloc(
	struct rt_env *env,
	size_t size)
{
	return malloc(size);
}

static void
tenure_free(
	struct rt_env *env,
	void *p)
{
}
