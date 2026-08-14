/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Namespace the public entry points while compiling one object-model backend.
 *
 * This file intentionally has no include guard.  A backend defines
 * OM_BACKEND_SYMBOL(name), includes this file, then includes objectmodel.h.
 * Definitions and calls in that translation unit are consequently given the
 * backend-specific prefix.
 */

#ifndef OM_BACKEND_SYMBOL
#error "OM_BACKEND_SYMBOL must be defined"
#endif

#define om_make_array			OM_BACKEND_SYMBOL(make_array)
#define om_get_array_size		OM_BACKEND_SYMBOL(get_array_size)
#define om_read_array			OM_BACKEND_SYMBOL(read_array)
#define om_write_array			OM_BACKEND_SYMBOL(write_array)
#define om_resize_array			OM_BACKEND_SYMBOL(resize_array)
#define om_copy_array			OM_BACKEND_SYMBOL(copy_array)
#define om_make_dict			OM_BACKEND_SYMBOL(make_dict)
#define om_get_dict_size		OM_BACKEND_SYMBOL(get_dict_size)
#define om_get_dict_alloc_size		OM_BACKEND_SYMBOL(get_dict_alloc_size)
#define om_check_dict_key		OM_BACKEND_SYMBOL(check_dict_key)
#define om_read_dict			OM_BACKEND_SYMBOL(read_dict)
#define om_read_dict_with_hash		OM_BACKEND_SYMBOL(read_dict_with_hash)
#define om_read_dict_index		OM_BACKEND_SYMBOL(read_dict_index)
#define om_write_dict			OM_BACKEND_SYMBOL(write_dict)
#define om_write_dict_with_hash		OM_BACKEND_SYMBOL(write_dict_with_hash)
#define om_erase_dict_entry		OM_BACKEND_SYMBOL(erase_dict_entry)
#define om_copy_dict			OM_BACKEND_SYMBOL(copy_dict)
#define om_merge_dict			OM_BACKEND_SYMBOL(merge_dict)
#define om_freeze_dict			OM_BACKEND_SYMBOL(freeze_dict)
#define om_enter_gc			OM_BACKEND_SYMBOL(enter_gc)
#define om_leave_gc			OM_BACKEND_SYMBOL(leave_gc)
#define om_safepoint			OM_BACKEND_SYMBOL(safepoint)
#define om_init_env			OM_BACKEND_SYMBOL(init_env)
#define om_enter_blocking		OM_BACKEND_SYMBOL(enter_blocking)
#define om_leave_blocking		OM_BACKEND_SYMBOL(leave_blocking)
