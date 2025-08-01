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

/*
 * False Assertion
 */
#define NEVER_COME_HERE		0
#define PINNED_VAR_NOT_FOUND	0

/*
 *Link an element to a list.
 */
#define INSERT_TO_LIST(elem, list, prev, next)				\
	do { 								\
		(elem)->prev = NULL;					\
		(elem)->next = (list);					\
		if ((list) != NULL)					\
			(list)->prev = (elem);				\
		(list) = (elem);					\
	} while (0)

/*
 * Unlink an element from a list.
 */
#define UNLINK_FROM_LIST(elem, list, prev, next)			\
	do {								\
		if ((elem)->prev != NULL) {				\
			(elem)->prev->next = (elem)->next;		\
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

/*
 * Check if a value is a reference type.
 */
#define IS_REF_VAL(v)			((v)->type >= NOCT_VALUE_STRING && (v)->type <= NOCT_VALUE_DICT)

/*
 * Check if an object is in the nursery or graduate region.
 */
#define IS_YOUNG_OBJ(o)			((o)->region < RT_GC_REGION_TENURE)

/*
 * Forward declaration.
 */
static struct rt_string *rt_gc_alloc_string_graduate(struct rt_env *env, size_t len, const char *data);
static struct rt_string *rt_gc_alloc_string_tenure(struct rt_env *env, size_t len, const char *data);
static struct rt_array *rt_gc_alloc_array_graduate(struct rt_env *env, size_t size);
static struct rt_array *rt_gc_alloc_array_tenure(struct rt_env *env, size_t size);
static struct rt_dict *rt_gc_alloc_dict_graduate(struct rt_env *env, size_t size);
static struct rt_dict *rt_gc_alloc_dict_tenure(struct rt_env *env, size_t size);
static void rt_gc_young_gc(struct rt_env *env);
static bool rt_gc_copy_young_object_recursively(struct rt_env *env, struct rt_gc_object **obj);
static struct rt_gc_object *rt_gc_promote_object(struct rt_env *env, struct rt_gc_object *obj);
static struct rt_gc_object *rt_gc_promote_string(struct rt_env *env, struct rt_gc_object *obj);
static struct rt_gc_object *rt_gc_promote_array(struct rt_env *env, struct rt_gc_object *obj);
static struct rt_gc_object *rt_gc_promote_dict(struct rt_env *env, struct rt_gc_object *obj);
struct rt_gc_object *rt_gc_copy_string_to_graduate(struct rt_env *env, struct rt_string *old_obj);
struct rt_gc_object *rt_gc_copy_array_to_graduate(struct rt_env *env, struct rt_array *old_obj);
struct rt_gc_object *rt_gc_copy_dict_to_graduate(struct rt_env *env, struct rt_dict *old_obj);
static void rt_gc_old_gc(struct rt_env *env);
static void rt_gc_mark_old_object_recursively(struct rt_env *env, struct rt_gc_object *obj);
static void rt_gc_free_old_object(struct rt_env *env, struct rt_gc_object *obj);
static bool rt_gc_compact_gc(struct rt_env *env);
static void rt_gc_update_tenure_ref_recursively(struct rt_env *env, struct rt_gc_object **obj);
static void *nursery_alloc(struct rt_env *env, size_t size);
static void *graduate_alloc(struct rt_env *env, size_t size);
static void *rt_gc_tenure_alloc(struct rt_env *env, size_t size);
static void rt_gc_tenure_free(struct rt_env *env, void *p);

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

	/* Initialize the tenure allocator.  */
	vm->gc.tenure_freelist.top = malloc(RT_GC_TENURE_SIZE);
	if (vm->gc.tenure_freelist.top == NULL)
		return false;
	memset(vm->gc.tenure_freelist.top, 0, RT_GC_TENURE_SIZE);
	vm->gc.tenure_freelist.end = vm->gc.tenure_freelist.top + RT_GC_TENURE_SIZE;
	vm->gc.tenure_freelist.cur = vm->gc.tenure_freelist.top;

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
	/* Cleanup the nursery allocator. */
	arena_cleanup(&vm->gc.nursery_arena);

	/* Cleanup the graduate allocators. */
	arena_cleanup(&vm->gc.graduate_arena[0]);
	arena_cleanup(&vm->gc.graduate_arena[1]);

	/* Cleanup the tenure allocator. */
	free(vm->gc.tenure_freelist.top);
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
	int retry;

	/*
	 * [Large Object Promotion]
	 *  - If the string is large, allocate in the tenure region.
	 */
	if (len >= RT_GC_LOP_THRESHOLD)
		return rt_gc_alloc_string_tenure(env, len, data);

	/* Allocate in the nursery region. */
	for (retry = 0; retry <= 1; retry++) {
		/* Allocate a rt_string buffer. */
		rts = nursery_alloc(env, sizeof(struct rt_string) + len);
		if (rts == NULL) {
			/* Retry. */
			if (retry == 0) {
				rt_gc_young_gc(env);
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Get the string top address. */
		s = (char *)rts + sizeof(struct rt_string);

		/* Copy the string. */
		memcpy(s, data, len);

		/* Setup the struct. */
		memset(&rts->head, 0, sizeof(struct rt_gc_object));
		rts->head.type = RT_GC_TYPE_STRING;
		rts->head.region = RT_GC_REGION_NURSERY;
		rts->head.size = sizeof(struct rt_string) + len;
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

	do {
		/* Try allocating a rt_string buffer in the graduate region. */
		rts = graduate_alloc(env, sizeof(struct rt_string) + len);
		if (rts == NULL)
			break;

		/* Get the string top address. */
		s = (char *)rts + sizeof(struct rt_string);

		/* Copy the string. */
		memcpy(s, data, len);

		/* Setup the struct. */
		memset(&rts->head, 0, sizeof(struct rt_gc_object));
		rts->head.type = RT_GC_TYPE_STRING;
		rts->head.region = RT_GC_REGION_GRADUATE;
		rts->head.size = sizeof(struct rt_string) + len;
		INSERT_TO_LIST(&rts->head, env->vm->gc.graduate_new_list, prev, next);
		rts->data = s;
		rts->len = len;

		/* Succeeded. (graduate) */
		return rts;
	} while (0);

	/* Failed. Try allocating in the tenure region. */
	rts = rt_gc_alloc_string_tenure(env, len, data);
	if (rts == NULL)
		return NULL;

	/* Succeeded. (tenure) */
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
	int retry;

	/* Allocate in the tenure region. */
	for (retry = 0; retry <= 2; retry++) {
		/* Allocate a rt_string buffer. */
		rts = rt_gc_tenure_alloc(env, sizeof(struct rt_string) + len);
		if (rts == NULL) {
			/* Retry. */
			if (retry == 0) {
				rt_gc_old_gc(env);
				continue;
			} if (retry == 1) {
				rt_gc_compact_gc(env);
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Get the string top address. */
		s = (char *)rts + sizeof(struct rt_string);

		/* Copy the string. */
		memcpy(s, data, len);

		/* Setup the struct. */
		memset(&rts->head, 0, sizeof(struct rt_gc_object));
		rts->head.type = RT_GC_TYPE_STRING;
		rts->head.region = RT_GC_REGION_TENURE;
		rts->head.size = sizeof(struct rt_string) + len;
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
	int retry;

	/*
	 * [Large Object Promotion]
	 *  - If the array is large, allocate in the tenure region.
	 */
	len = size * sizeof(struct rt_value);
	if (len >= RT_GC_LOP_THRESHOLD)
		return rt_gc_alloc_array_tenure(env, size);

	/* Allocate in the nursery region. */
	for (retry = 0; retry <= 1; retry++) {
		/* Allocate a rt_array buffer. */
		arr = nursery_alloc(env, sizeof(struct rt_array) + size * sizeof(struct rt_value));
		if (arr == NULL) {
			/* Retry. */
			if (retry == 0) {
				rt_gc_young_gc(env);
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Get the address of the table. */
		table = (struct rt_value *)((char *)arr + sizeof(struct rt_array));

		/* Setup the struct. */
		memset(&arr->head, 0, sizeof(struct rt_gc_object));
		arr->head.type = RT_GC_TYPE_ARRAY;
		arr->head.region = RT_GC_REGION_NURSERY;
		arr->head.size = sizeof(struct rt_array) + size * sizeof(struct rt_value);
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
		arr = graduate_alloc(env, sizeof(struct rt_array) + size * sizeof(struct rt_value));
		if (arr == NULL)
			break;

		/* Get the address of the table. */
		arr->table = (struct rt_value *)((char *)arr + sizeof(struct rt_array));

		/* Setup the struct. */
		memset(&arr->head, 0, sizeof(struct rt_gc_object));
		arr->head.type = RT_GC_TYPE_ARRAY;
		arr->head.region = RT_GC_REGION_GRADUATE;
		arr->head.size = sizeof(struct rt_array) + size * sizeof(struct rt_value);
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
	if (arr == NULL)
		return NULL;

	/* Succeeded. (tenure) */
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
	int retry;

	/* Allocate in the tenure region. */
	for (retry = 0; retry <= 2; retry++) {
		/* Allocate a rt_array buffer. */
		arr = rt_gc_tenure_alloc(env, sizeof(struct rt_array) + size * sizeof(struct rt_value));
		if (arr == NULL) {
			/* Retry. */
			if (retry == 0) {
				rt_gc_old_gc(env);
				continue;
			} if (retry == 1) {
				rt_gc_compact_gc(env);
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Get the address of the table. */
		table = (struct rt_value *)((char *)arr + sizeof(struct rt_array));

		/* Setup the struct. */
		memset(&arr->head, 0, sizeof(struct rt_gc_object));
		arr->head.type = RT_GC_TYPE_ARRAY;
		arr->head.region = RT_GC_REGION_TENURE;
		arr->head.size = sizeof(struct rt_array) + size * sizeof(struct rt_value);
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
	struct rt_value *key_table;
	struct rt_value *value_table;
	int retry;

	/*
	 * [Large Object Promotion]
	 *  - If the array is large, allocate in the tenure region.
	 */
	if (size * sizeof(char *) + size * sizeof(struct rt_value *) >= RT_GC_LOP_THRESHOLD)
		return rt_gc_alloc_dict_tenure(env, size);

	/* Allocate in the nursery region. */
	for (retry = 0; retry <= 1; retry++) {
		/* Allocate a rt_dict buffer. */
		dict = nursery_alloc(env,
				     sizeof(struct rt_dict) +
				     size * sizeof(struct rt_value) +
				     size * sizeof(struct rt_value));
		if (dict == NULL) {
			/* Retry. */
			if (retry == 0) {
				rt_gc_young_gc(env);
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Get the address of the key array block. */
		key_table = (struct rt_value *)((char *)dict + sizeof(struct rt_dict));

		/* Get the address of the value array block. */
		value_table = (struct rt_value *)((char *)dict + sizeof(struct rt_dict) + size * sizeof(struct rt_value));

		/* Setup the struct. */
		memset(&dict->head, 0, sizeof(struct rt_gc_object));
		dict->head.type = RT_GC_TYPE_DICT;
		dict->head.region = RT_GC_REGION_NURSERY;
		dict->head.size = sizeof(struct rt_dict) + 2 * size * sizeof(struct rt_value);
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
		dict = graduate_alloc(env,
				      sizeof(struct rt_dict) +
				      size * sizeof(struct rt_value) +
				      size * sizeof(struct rt_value));
		if (dict == NULL)
			break;

		/* Get the address of the key array block. */
		dict->key = (struct rt_value *)((char *)dict + sizeof(struct rt_dict));

		/* Get the address of the value array block. */
		dict->value = (struct rt_value *)((char *)dict + sizeof(struct rt_dict) + size * sizeof(struct rt_value));

		/* Setup a struct. */
		memset(&dict->head, 0, sizeof(struct rt_gc_object));
		dict->head.type = RT_GC_TYPE_DICT;
		dict->head.region = RT_GC_REGION_GRADUATE;
		dict->head.size = sizeof(struct rt_dict) + 2 * size * sizeof(struct rt_value);
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
	if (dict == NULL)
		return NULL;

	/* Succeeded. (tenure) */
	return dict;
}

/* Allocates a dictionary object in the tenure region. */
static struct rt_dict *
rt_gc_alloc_dict_tenure(
	struct rt_env *env,
	size_t size)
{
	struct rt_dict *dict;
	struct rt_value *key_table;
	struct rt_value *value_table;
	int retry;

	/* Allocate in the tenure region. */
	for (retry = 0; retry <= 2; retry++) {
		/* Allocate the rt_dict buffer. */
		dict = rt_gc_tenure_alloc(env,
				    sizeof(struct rt_dict) +
				    size * sizeof(struct rt_value) +
				    size * sizeof(struct rt_value));
		if (dict == NULL) {
			/* Retry. */
			if (retry == 0) {
				rt_gc_old_gc(env);
				continue;
			} if (retry == 1) {
				rt_gc_compact_gc(env);
				continue;
			} else {
				rt_out_of_memory(env);
				return NULL;
			}
		}

		/* Get the address of the key array block. */
		key_table = (struct rt_value *)((char *)dict + sizeof(struct rt_dict));

		/* Get the address of the value array block. */
		value_table = (struct rt_value *)((char *)dict + sizeof(struct rt_dict) + size * sizeof(struct rt_value));

		/* Setup a value. */
		memset(&dict->head, 0, sizeof(struct rt_gc_object));
		dict->head.type = RT_GC_TYPE_DICT;
		dict->head.region = RT_GC_REGION_TENURE;
		dict->head.size = sizeof(struct rt_dict) + 2 * size * sizeof(struct rt_value);
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
	struct rt_gc_object *obj;
	struct rt_bindglobal *global;
	struct rt_frame *frame;
	int i;

	env->vm->gc.graduate_new_list = NULL;

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

	/* Clear marks of the tenure objects. */
	obj = env->vm->gc.tenure_list;
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
			if (!rt_gc_copy_young_object_recursively(env, &global->val.val.obj))
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
				if (!rt_gc_copy_young_object_recursively(env, &frame->tmpvar[i].val.obj))
					return;
			}
		}

		/* For all pinned C local variables in the frame. */
		for (i = 0; i < frame->pinned_count; i++) {
			if (IS_REF_VAL(frame->pinned[i])) {
				if (!rt_gc_copy_young_object_recursively(env, &frame->pinned[i]->val.obj))
					return;
			}
		}

		frame = frame->next;
	}

	/* For all pinned C global variables. */
	for (i = 0; i < env->vm->pinned_count; i++) {
		if (IS_REF_VAL(env->vm->pinned[i])) {
			if (!rt_gc_copy_young_object_recursively(env, &env->vm->pinned[i]->val.obj))
				return;
		}
	}

	/* For all remember set objects. */
	obj = env->vm->gc.remember_set;
	while (obj != NULL) {
		if (!rt_gc_copy_young_object_recursively(env, &obj))
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
				if (dict->key[i].val.obj->region == RT_GC_REGION_GRADUATE &&
				    dict->key[i].val.obj->forward != NULL) {
					dict->key[i].val.obj = dict->key[i].val.obj->forward;
				}
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
				if (IS_YOUNG_OBJ(dict->key[i].val.obj)) {
					has_cross_gen_ref = true;
					break;
				}
				if (IS_REF_VAL(&dict->value[i]) &&
				    IS_YOUNG_OBJ(dict->value[i].val.obj)) {
					has_cross_gen_ref = true;
					break;
				}
			}
		}
		if (!has_cross_gen_ref) {
			/* Unlink from the remember set list. */
			obj->rem_flg = false;
			UNLINK_FROM_LIST(obj, env->vm->gc.remember_set, rem_prev, rem_next);
		}
		obj = obj->rem_next;
	}

	/*
	 * Finalize.
	 */

	/* Clear the nursery. */
	arena_unwind(&env->vm->gc.nursery_arena);
		   
	/* Clear the graduate (from) */
	arena_unwind(&env->vm->gc.graduate_arena[env->vm->gc.cur_grad_from]);

	/* Clear the nursery list. */
	env->vm->gc.nursery_list = NULL;

	/* Swap "to" and "from". */
	if (env->vm->gc.cur_grad_from == 0) {
		env->vm->gc.cur_grad_from = 1;
		env->vm->gc.cur_grad_to = 0;
	} else {
		env->vm->gc.cur_grad_from = 0;
		env->vm->gc.cur_grad_to = 1;
	}
	env->vm->gc.graduate_list = env->vm->gc.graduate_new_list;
	env->vm->gc.graduate_new_list = NULL;
}

/* Marks-and-copies objects recursively. */
static bool
rt_gc_copy_young_object_recursively(
	struct rt_env *env,
	struct rt_gc_object **obj)
{
	struct rt_gc_object *new_obj;
	struct rt_array *arr;
	struct rt_dict *dict;
	bool is_promoted;
	int i;

	/* If already processed. */
	if ((*obj)->is_marked) {
		/* And if this is a young object. */
		if (IS_YOUNG_OBJ(*obj) && (*obj)->forward != NULL) {
			/* Rewrite the reference. */
			*obj = (*obj)->forward;
		}

		/* Skip. */
		return true;
	}

	/* Copy or promote. */
	is_promoted = false;
	if ((*obj)->region != RT_GC_REGION_TENURE) {
		/* Check for the promotion. */
		if ((*obj)->promotion_count < RT_GC_PROMOTION_THRESHOLD) {
			/* No promotion, just copy the object. */
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

			/* Set the forwarding pointer. */
			(*obj)->forward = new_obj;

			/* Increment the promotion count. */
			new_obj->promotion_count = (*obj)->promotion_count + 1;
		} else {
			/* Promote the object. */
			new_obj = rt_gc_promote_object(env, *obj);
			if (new_obj == NULL)
				return false;

			/* Mark as promoted. */
			is_promoted = true;

			/* The forwarding pointer is set in rt_gc_promote_object(). */
		}

		/* Rewrite the reference. */
		*obj = new_obj;

		/* Mark as processed. */
		(*obj)->is_marked = true;
	}

	/* Recursively copy. */
	switch ((*obj)->type) {
	case RT_GC_TYPE_STRING:
		break;
	case RT_GC_TYPE_ARRAY:
		arr = (struct rt_array *)*obj;
		for (i = 0; i < arr->size; i++) {
			if (IS_REF_VAL(&arr->table[i])) {
				if (!rt_gc_copy_young_object_recursively(env, &arr->table[i].val.obj))
					return false;
			}
		}
		break;
	case RT_GC_TYPE_DICT:
		dict = (struct rt_dict *)*obj;
		for (i = 0; i < dict->size; i++) {
			if (!rt_gc_copy_young_object_recursively(env, &dict->key[i].val.obj))
				return false;
			if (IS_REF_VAL(&dict->value[i])) {
				if (!rt_gc_copy_young_object_recursively(env, &dict->value[i].val.obj))
					return false;
			}
		}
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	/* When promoted, check for cross-generation references. */
	if (is_promoted) {
		if ((*obj)->type == RT_GC_TYPE_ARRAY) {
			/* Check for array cross-generation references. */
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
					INSERT_TO_LIST(&arr->head, env->vm->gc.remember_set,rem_prev, rem_next);
					break;
				}
			}
		} else if ((*obj)->type == RT_GC_TYPE_DICT) {
			/* Check for dictionary cross-generation references. */
			dict = (struct rt_dict *)*obj;
			for (i = 0; i < dict->size; i++) {
				/* If the key is young generation. */
				if (IS_YOUNG_OBJ(dict->key[i].val.obj)) {
					/* And if the element is not promoted to the tenure region. */
					if (dict->key[i].val.obj->forward != NULL &&
					    dict->key[i].val.obj->forward->region == RT_GC_REGION_TENURE)
						continue;

					/* Add to remember set. */
					dict->head.rem_flg = true;
					INSERT_TO_LIST(&dict->head, env->vm->gc.remember_set,rem_prev, rem_next);
					break;
				}

				/* If the value is young generation. */
				if (IS_REF_VAL(&dict->value[i]) &&
				    IS_YOUNG_OBJ(dict->value[i].val.obj)) {
					/* And if the element is not promoted to the tenure region. */
					if (dict->value[i].val.obj->forward != NULL &&
					    dict->value[i].val.obj->forward->region == RT_GC_REGION_TENURE)
						continue;

					/* Add to remember set. */
					dict->head.rem_flg = true;
					INSERT_TO_LIST(&dict->head, env->vm->gc.remember_set,rem_prev, rem_next);
					break;
				}
			}
		}
	}

	return true;
}

/* Promotes an object. */
static struct rt_gc_object *
rt_gc_promote_object(
	struct rt_env *env,
	struct rt_gc_object *obj)
{
	struct rt_gc_object *ret;

	switch (obj->type) {
	case RT_GC_TYPE_STRING:
		ret = rt_gc_promote_string(env, obj);
		if (ret == NULL)
			return NULL;
		break;
	case RT_GC_TYPE_ARRAY:
		ret = rt_gc_promote_array(env, obj);
		if (ret == NULL)
			return NULL;
		break;
	case RT_GC_TYPE_DICT:
		ret = rt_gc_promote_dict(env, obj);
		if (ret == NULL)
			return NULL;
		break;
	default:
		assert(NEVER_COME_HERE);
		ret = NULL;
		break;
	}

	return ret;
}

/* Promotes a string object. */
static struct rt_gc_object *
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

	/* Set the forwarding pointer. */
	obj->forward = &new_rts->head;

	return &new_rts->head;
}

/* Promotes an array object. */
static struct rt_gc_object *
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
	new_arr->size = old_arr->size;
	memcpy(new_arr->table, old_arr->table, old_arr->size * sizeof(struct rt_value));

	/* Set the forwarding pointer. */
	obj->forward = &new_arr->head;

	return &new_arr->head;
}

/* Promotes a dictionary object. */
static struct rt_gc_object *
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
	new_dict->size = old_dict->size;
	memcpy(new_dict->key, old_dict->key, old_dict->size * sizeof(struct rt_value));

	/* Copy the values. */
	memcpy(new_dict->value, old_dict->value, old_dict->size * sizeof(struct rt_value));

	/* Set the forwarding pointer. */
	obj->forward = &new_dict->head;

	return &new_dict->head;
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
	new_obj->size = old_obj->size;
	memcpy(new_obj->table, old_obj->table, old_obj->size * sizeof(struct rt_value));

	/* Check for cross-generation references. */
	if (new_obj->head.region == RT_GC_REGION_TENURE) {
		for (i = 0; i < new_obj->size; i++) {
			if (IS_REF_VAL(&new_obj->table[i]) &&
			    IS_YOUNG_OBJ(new_obj->table[i].val.obj)) {
				new_obj->head.rem_flg = true;
				INSERT_TO_LIST(&new_obj->head, env->vm->gc.remember_set,rem_prev, rem_next);
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
	bool retry;
	int i;

	/*
	 * Arrays larger than RT_GC_LOP_THRESHOLD/sizeof(struct rt_value *)/2 must not be in the
	 * nursery or graduate regions.
	 */
	assert(old_obj->size < RT_GC_LOP_THRESHOLD / sizeof(struct rt_value *) / 2);

	retry = false;
	do {
		/* Allocate in the graduate region. (If failed, in the tenure region.) */
		if (!retry) {
			new_obj = rt_gc_alloc_dict_graduate(env, old_obj->size);
		} else {
			new_obj = rt_gc_alloc_dict_tenure(env, old_obj->size);
			if (new_obj == NULL) {
				/* Tenure is full. Retry. */
				rt_gc_level2_gc(env);
				new_obj = rt_gc_alloc_dict_tenure(env, old_obj->size);
			}
		}
		if (new_obj == NULL)
			return NULL;

		/* When fallback to the tenure on the first time, go with retry mode. */
		if (!retry && new_obj->head.region == RT_GC_REGION_TENURE)
			retry = true;

		/* Copy the keys. */
		new_obj->size = old_obj->size;
		memcpy(new_obj->key, old_obj->key, old_obj->size * sizeof(struct rt_value));

		/* Copy the value. */
		memcpy(new_obj->value, old_obj->value, old_obj->size * sizeof(struct rt_value));

		/* Check for cross-generation references. */
		if (new_obj->head.region == RT_GC_REGION_TENURE) {
			for (i = 0; i < new_obj->size; i++) {
				if (IS_YOUNG_OBJ(new_obj->key[i].val.obj)) {
					new_obj->head.rem_flg = true;
					INSERT_TO_LIST(&new_obj->head, env->vm->gc.remember_set,rem_prev, rem_next);
					break;
				}

				if (IS_REF_VAL(&new_obj->value[i]) &&
				    IS_YOUNG_OBJ(new_obj->value[i].val.obj)) {
					new_obj->head.rem_flg = true;
					INSERT_TO_LIST(&new_obj->head, env->vm->gc.remember_set,rem_prev, rem_next);
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

/* Executes an old GC. */
static void
rt_gc_old_gc(
	struct rt_env *env)
{
	struct rt_gc_object *obj, *next_obj;
	struct rt_bindglobal *global;
	struct rt_frame *frame;
	int tenure_count;
	int i;

	/*
	 * Clear marks.
	 */

	/* Clear marks of the tenure objects. */
	obj = env->vm->gc.tenure_list;
	while (obj != NULL) {
		obj->is_marked = false;
		obj = obj->next;
	}

	/*
	 * Mark.
	 */

	/* For all global variables. */
	global = env->vm->global;
	while (global != NULL) {
		if (IS_REF_VAL(&global->val))
			rt_gc_mark_old_object_recursively(env, global->val.val.obj);
		global = global->next;
	}

	/* For all call frames. */
	frame = env->frame;
	while (frame != NULL) {
		/* For all temporary variables in the frame. */
		for (i = 0; i < frame->tmpvar_size; i++) {
			if (IS_REF_VAL(&frame->tmpvar[i]))
				rt_gc_mark_old_object_recursively(env, frame->tmpvar[i].val.obj);
		}

		/* For all pinned C local variables in the frame. */
		for (i = 0; i < frame->pinned_count; i++) {
			if (IS_REF_VAL(frame->pinned[i]))
				rt_gc_mark_old_object_recursively(env, frame->pinned[i]->val.obj);
		}

		frame = frame->next;
	}

	/* For all pinned C global variables. */
	for (i = 0; i < env->vm->pinned_count; i++) {
		if (IS_REF_VAL(env->vm->pinned[i]))
			rt_gc_mark_old_object_recursively(env, env->vm->pinned[i]->val.obj);
	}

	/*
	 * Sweep.
	 */

	/* For all tenure objects. */
	obj = env->vm->gc.tenure_list;
	while (obj != NULL) {
		next_obj = obj->next;

		/* Free if not marked. */
		if (!obj->is_marked)
			rt_gc_free_old_object(env, obj);

		obj = next_obj;
	}
}

/* Mark object recursively for the old GC. */
static void
rt_gc_mark_old_object_recursively(
	struct rt_env *env,
	struct rt_gc_object *obj)
{
	int i;

	/* If the object is a tenure object. */
	if (obj->region == RT_GC_REGION_TENURE) {
		/* If already marked, just return. */
		if (obj->is_marked)
			return;

		/* Mark. */
		obj->is_marked = true;
	}

	/* Mark recursively. */
	if (obj->type == RT_GC_TYPE_ARRAY) {
		struct rt_array *arr = (struct rt_array *)obj;
		for (i = 0; i < arr->size; i++) {
			if (IS_REF_VAL(&arr->table[i]))
				rt_gc_mark_old_object_recursively(env, arr->table[i].val.obj);
		}
	} else if (obj->type == RT_GC_TYPE_DICT) {
		struct rt_dict *dict = (struct rt_dict *)obj;
		for (i = 0; i < dict->size; i++) {
			rt_gc_mark_old_object_recursively(env, dict->key[i].val.obj);
			if (IS_REF_VAL(&dict->value[i]))
				rt_gc_mark_old_object_recursively(env, dict->value[i].val.obj);
		}
	}
}

/* Free a string, array, or dictionary object. */
static void
rt_gc_free_old_object(
	struct rt_env *env,
	struct rt_gc_object *obj)
{
	int i;

	assert(obj->region == RT_GC_REGION_TENURE);

	/*
	 * Nursery and graduate objects are allocated by arena
	 * allocater, and no need for freeing.
	 */
	if (obj->region != RT_GC_REGION_TENURE)
		return;

	/* Unlink from the tenure list. */
	UNLINK_FROM_LIST(obj, env->vm->gc.tenure_list, prev, next);

	/* Unlink from the remember set. */
	if (obj->rem_flg)
		UNLINK_FROM_LIST(obj, env->vm->gc.remember_set, rem_prev, rem_next);

	/* Free. */
	if (obj->type == RT_GC_TYPE_STRING) {
		struct rt_string *str;
		str = (struct rt_string *)obj;
		rt_gc_tenure_free(env, str);
	} else if (obj->type == RT_GC_TYPE_ARRAY) {
		struct rt_array *arr;
		arr = (struct rt_array *)obj;
		rt_gc_tenure_free(env, arr);
	} else if (obj->type == RT_GC_TYPE_DICT) {
		struct rt_dict *dict;
		dict = (struct rt_dict *)obj;
		rt_gc_tenure_free(env, dict);
	}
}

/* Create the compaction table. */
static bool
rt_gc_compact_gc(
	struct rt_env *env)
{
	struct rt_gc_object *obj;
	char *cur_blk, *remap_top;
	int i, index;
	struct rt_bindglobal *global;
	struct rt_frame *frame;

	/*
	 * Count all tenure objects.
	 */

	env->vm->gc.compact_count = 0;
	obj = env->vm->gc.tenure_list;
	while (obj != NULL) {
		env->vm->gc.compact_count++;
		obj = obj->next;
	}
	if (env->vm->gc.compact_count == 0)
		return true;

	/*
	 * Initialize the compaction table.
	 */

	/* Allocate the compaction table. */
	env->vm->gc.compact_before = malloc(env->vm->gc.compact_count * sizeof(void *));
	if (env->vm->gc.compact_before == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	env->vm->gc.compact_after = malloc(env->vm->gc.compact_count * sizeof(void *));
	if (env->vm->gc.compact_after == NULL) {
		free(env->vm->gc.compact_before);
		rt_out_of_memory(env);
		return false;
	}

	/* Traverse the tenure region. */
	cur_blk = env->vm->gc.tenure_freelist.top;
	remap_top = env->vm->gc.tenure_freelist.top;
	index = 0;
	while (cur_blk < env->vm->gc.tenure_freelist.end) {
		size_t blk_size, real_size;
		bool blk_used;
		struct rt_gc_object *obj;

		blk_size = *(size_t *)cur_blk;
		blk_used = blk_size & RT_GC_FREELIST_USED_BIT ? true : false;
		obj = (struct rt_gc_object *)(cur_blk + sizeof(size_t));

		/* Check for the end of the list. */
		if (blk_size == 0)
			break;

		/* Skip if unused. */
		if (!blk_used) {
			cur_blk += sizeof(size_t) + blk_size;
			continue;
		}

		/* Record. */
		env->vm->gc.compact_before[index] = obj;
		env->vm->gc.compact_after[index] = remap_top + sizeof(size_t);
		remap_top += sizeof(size_t) + obj->size;
		index++;

		cur_blk += sizeof(size_t) + blk_size;
	}
	assert(cur_blk < env->vm->gc.tenure_freelist.end);
	assert(index == env->vm->gc.compact_count);

	/* Update the free list top. */
	env->vm->gc.tenure_freelist.cur = remap_top;

	/*
	 * Slide tenure objects.
	 */

	for (i = 0; i < env->vm->gc.compact_count; i++) {
		/* Get the real object size. */
		size_t obj_size = ((struct rt_gc_object *)env->vm->gc.compact_before[i])->size;

		/* Move. */
		memmove(env->vm->gc.compact_after[i],
			env->vm->gc.compact_before[i],
			obj_size);

		/* Store the size header. */
		*(size_t *)((char *)env->vm->gc.compact_after[i] - sizeof(size_t)) = obj_size;

		/* Fill the reminder. */
		if (i == env->vm->gc.compact_count - 1) {
			memset(env->vm->gc.compact_after[i] + obj_size,
			       0,
			       env->vm->gc.tenure_freelist.end - ((char *)env->vm->gc.compact_after[i] + obj_size));
		}
	}

	/*
	 * Rewrite all references.
	 */

	/* For all global variables. */
	global = env->vm->global;
	while (global != NULL) {
		if (IS_REF_VAL(&global->val))
			rt_gc_update_tenure_ref_recursively(env, &global->val.val.obj);
		global = global->next;
	}

	/* For all call frames. */
	frame = env->frame;
	while (frame != NULL) {
		/* For all temporary variables in the frame. */
		for (i = 0; i < frame->tmpvar_size; i++) {
			if (IS_REF_VAL(&frame->tmpvar[i]))
				rt_gc_update_tenure_ref_recursively(env, &frame->tmpvar[i].val.obj);
		}

		/* For all pinned C local variables in the frame. */
		for (i = 0; i < frame->pinned_count; i++) {
			if (IS_REF_VAL(frame->pinned[i]))
				rt_gc_update_tenure_ref_recursively(env, &frame->pinned[i]->val.obj);
		}

		frame = frame->next;
	}

	/* For all pinned C global variables. */
	for (i = 0; i < env->vm->pinned_count; i++) {
		if (IS_REF_VAL(env->vm->pinned[i]))
			rt_gc_update_tenure_ref_recursively(env, &env->vm->pinned[i]->val.obj);
	}

	/*
	 * Cleanup the compaction table.
	 */

	if (env->vm->gc.compact_before != NULL) {
		free(env->vm->gc.compact_before);
		env->vm->gc.compact_before = NULL;
	}
	if (env->vm->gc.compact_after != NULL) {
		free(env->vm->gc.compact_after);
		env->vm->gc.compact_after = NULL;
	}

	return true;
}

/* Recursively update the tenure references for compaction. */
static void
rt_gc_update_tenure_ref_recursively(
	struct rt_env *env,
	struct rt_gc_object **obj)
{
	int i;

	/* Search in the compaction table. */
	for (i = 0; i < env->vm->gc.compact_count; i++) {
		if (env->vm->gc.compact_before[i] == *obj)
			break;
	}
	if (i == env->vm->gc.compact_count)
		return;	/* Not found. */

	/* Update the reference. */
	*obj = env->vm->gc.compact_after[i];

	/* Recursively update. */
	if ((*obj)->type == RT_GC_TYPE_ARRAY) {
		struct rt_array *arr = (struct rt_array *)*obj;
		for (i = 0; i < arr->size; i++) {
			if (IS_REF_VAL(&arr->table[i]))
				rt_gc_update_tenure_ref_recursively(env, &arr->table[i].val.obj);
		}
	} else if ((*obj)->type == RT_GC_TYPE_DICT) {
		struct rt_dict *dict = (struct rt_dict *)*obj;
		for (i = 0; i < dict->size; i++) {
			rt_gc_update_tenure_ref_recursively(env, &dict->key[i].val.obj);
			if (IS_REF_VAL(&dict->value[i]))
				rt_gc_update_tenure_ref_recursively(env, &dict->value[i].val.obj);
		}
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
			memmove(&env->vm->pinned[i], &env->vm->pinned[i+1], (RT_GLOBAL_PIN_MAX - i - 1) * sizeof(struct rt_value *));

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

/*
 * Manually trigger the young GC.
 */
void
rt_gc_level1_gc(struct rt_env *env)
{
	rt_gc_young_gc(env);
}

/*
 * Manually trigger the old GC.
 */
void rt_gc_level2_gc(struct rt_env *env)
{
	rt_gc_young_gc(env);
	rt_gc_old_gc(env);
}

/*
 * Manually triggers a  GC.
 */
void rt_gc_level3_gc(struct rt_env *env)
{
	rt_gc_young_gc(env);
	rt_gc_old_gc(env);
	rt_gc_compact_gc(env);
}

static void *
nursery_alloc(
	struct rt_env *env,
	size_t size)
{
	/* Allocate from the nursery arena. */
	return arena_alloc(&env->vm->gc.nursery_arena, size);
}

static void *
graduate_alloc(
	struct rt_env *env,
	size_t size)
{
	/* Allocate from the graduate arena. */
	return arena_alloc(&env->vm->gc.graduate_arena[env->vm->gc.cur_grad_to], size);
}

/*
 * Allocate a tenure block.
 *
 * The allocator for the tenure region.
 * Each block has its size at the block top.
 * The LSB of the block size indicates the block is used (set) or freed (clear).
 */
static void *
rt_gc_tenure_alloc(
	struct rt_env *env,
	size_t size)
{
	char *cur;
	char *p;

	assert(size > 0);
	if (size == 0)
		return NULL;

	/* Align. */
	size = (size + RT_GC_FREELIST_ALIGN - 1) & ~(RT_GC_FREELIST_ALIGN - 1);

	/* The second blk */
	cur = env->vm->gc.tenure_freelist.top;

	/* Search for the first match. */
	while (*cur) {
		size_t blk_size = *(size_t *)cur;

		/* Check for the end of the list. */
		if (blk_size == 0)
			break;

		/* Check if the block is used or the size is small. */
		if ((blk_size & RT_GC_FREELIST_USED_BIT) ||
		    (size > blk_size)) {
			cur = cur + sizeof(size_t) + (blk_size & RT_GC_FREELIST_SIZE_MASK);
			continue;
		}

		/* Reuse this block. */
		blk_size |= RT_GC_FREELIST_USED_BIT;
		*(size_t *)cur = blk_size;

		/* Return the address of the block top + the size of the size header. */
		return cur + sizeof(size_t);
	}

	/* Check if the remaining size fits. */
	if (size > env->vm->gc.tenure_freelist.end - cur - sizeof(size_t))
		return NULL;

	/* Allocate at the end of the free list. */
	*(size_t *)cur = size | RT_GC_FREELIST_USED_BIT;
	env->vm->gc.tenure_freelist.cur += size + sizeof(size_t);
	return cur + sizeof(size_t);
}

/* Free a tenure block. */
static void
rt_gc_tenure_free(
	struct rt_env *env,
	void *p)
{
	size_t *header;
	size_t size;

	/* Get the header address. */
	header = (size_t *)((char *)p - sizeof(size_t));

	/* Get the block size. */
	size = *header;

	/* Block must be used. (check the used bit.) */
	assert(size & RT_GC_FREELIST_USED_BIT);

	/* Erase the used bit. */
	*header = size & RT_GC_FREELIST_SIZE_MASK;
}
