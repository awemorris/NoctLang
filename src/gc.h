/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * Garbage Collector
 *
 * The heap is divided into three generations, with an additional remember set:
 *
 * - Nursery Generation:
 *   For newly allocated small objects.
 *   Managed in a dedicated 2MB memory region.
 *   Collected via copying GC along with the graduate generation.
 *
 * - Graduate Generation:
 *   For surviving nursery objects.
 *   Uses semi-space copying GC with two 256KB regions (from and to spaces).
 *
 * - Tenure Generation:
 *   For long-lived, large, or native-referenced objects.
 *   Collected using mark-and-sweep GC.
 *
 * - Remember Set:
 *   Tracks tenure-generation arrays or dictionaries that reference
 *   young-generation objects (in the nursery or graduate space).
 *   Maintained via a write barrier triggered on container updates.
 *   Ensures proper GC reachability across generations.
 */

#ifndef NOCT_GC_H
#define NOCT_GC_H

#include <noct/noct.h>
#include "c89compat.h"
#include "arena.h"

/*
 * Region Size
 */
#define RT_GC_NURSERY_SIZE		(2 * 1024 * 1024)
#define RT_GC_GRADUATE_SIZE		(256 * 1024)
#define RT_GC_TENURE_SIZE		(512 * 1024 * 1024)

/*
 * Large Object Promotion Threshold - A new object that has a size
 * beyond this value will be allocated directly in the tenure region.
 */
#define RT_GC_LOP_THRESHOLD		(32768)

/*
 * Regions.
 */
enum gt_gc_generation {
	RT_GC_REGION_NURSERY,
	RT_GC_REGION_GRADUATE,
	RT_GC_REGION_TENURE,
};

/*
 * Object Types.
 */
enum rt_gc_object_type {
	RT_GC_TYPE_STRING,
	RT_GC_TYPE_ARRAY,
	RT_GC_TYPE_DICT,
};

/*
 * Garbage Collector state structure that is embedded to struct rt_vm.
 */
struct rt_gc_info {
	/* Memory region for the nursery generation (2MB). */
	struct arena_info nursery_arena;

	/*
	 * Memory regions for the graduate generation:
	 * [0] = from-space, [1] = to-space (256KB each).
	 */
	struct arena_info graduate_arena[2];
	int cur_grad_from;
	int cur_grad_to;

	/*
	 * Memory region for the tenure generation.
	 * TODO:
	 *  - Currently we use malloc() directly for tenute object generation.
	 *  - We'll implement an allocator for the tenure generation soon.
	 */
	/* struct tenure_allocator tenure_allocator; */

	/* Linked list of objects in the nursery generation. */
	struct rt_gc_object *nursery_list;

	/* Linked list of objects in the graduate generation. */
	struct rt_gc_object *graduate_list;
	struct rt_gc_object *graduate_new_list;

	/* Linked list of objects in the tenure generation. */
	struct rt_gc_object *tenure_list;

	/* Linked list of the remember set. (see struct rt_gc_remember_set) */
	struct rt_gc_object *remember_set;
};

/*
 * GC-managed object header for strings, arrays, and dictionaries.
 * This struct is embedded at the beginning of each managed object.
 */
struct rt_gc_object {
	/*
	 * Object type: one of RT_GC_TYPE_STRING, RT_GC_TYPE_ARRAY, or
	 * RT_GC_TYPE_DICT.
	 */
	int type;

	/*
	 * Region tag: indicates which GC generation this object
	 * belongs to.
	 */
	int region;

	/*
	 * Intrusive doubly-linked list for the corresponding
	 * region's list (nursery, graduate, or tenure).
	 */
	struct rt_gc_object *prev;
	struct rt_gc_object *next;

	/*
	 * Intrusive doubly-linked list for the remember set.
	 */
	struct rt_gc_remember_set *rem_prev;
	struct rt_gc_remember_set *rem_next; 
	bool rem_flg;

	/* Mark bit used in mark-and-sweep GC for the tenure generation. */
	bool is_marked;

	/* Promotion count. */
	int promotion_count;

	/* Forwarding pointer. */
	struct rt_gc_object *forward;
};

/*
 * Node structure for the GC remember set.
 *
 * Each node tracks an array or dictionary object in the tenure generation
 * that holds references to young-generation objects (nursery or graduate).
 * The list is maintained as a non-intrusive doubly-linked list.
 */
struct rt_gc_remember_set {
	/* Tenure-generation array or dictionary object tracked by this node. */
	struct rt_gc_object *obj;

        /* Previous node in the remember set list. */
	struct rt_gc_remember_set *prev;

	/* Next node in the remember set list. */
	struct rt_gc_remember_set *next;
};

/*
 * The following functions are part of the GC interface used by runtime.c.
 */

/* Initialize the garbage collector and allocate memory regions. */
bool rt_gc_init(struct rt_vm *vm);

/* Cleanup the garbage collector and deallocate memory regions. */
void rt_gc_cleanup(struct rt_vm *vm);

/* Allocate a string object in the appropriate GC generation. */
struct rt_string *rt_gc_alloc_string(struct rt_env *env, size_t len, const char *data);

/* Allocate an array object in the appropriate GC generation. */
struct rt_array *rt_gc_alloc_array(struct rt_env *env, size_t size);

/* Allocate a dictionary object in the appropriate GC generation. */
struct rt_dict *rt_gc_alloc_dict(struct rt_env *env, size_t size);

/* Write barrier: register container in the remember set if it references a younger object. */
void rt_gc_array_write_barrier(struct rt_env *env, struct rt_array *arr, int index, struct rt_value *val);

/* Write barrier: register container in the remember set if it references a younger object. */
void rt_gc_dict_write_barrier(struct rt_env *env, struct rt_dict *dict,int index, struct rt_value *val);

/* Manually trigger a young-generation GC (nursery + graduate, copying GC). */
void rt_gc_level1_gc(struct rt_env *env);

/* Manually trigger an old-generation GC (tenure, mark-and-sweep). */
void rt_gc_level2_gc(struct rt_env *env);

/* Manually trigger a full GC. */
void rt_gc_level3_gc(struct rt_env *env);

#endif
