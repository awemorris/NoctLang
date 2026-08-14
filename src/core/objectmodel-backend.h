/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#ifndef NOCT_OBJECTMODEL_BACKEND_H
#define NOCT_OBJECTMODEL_BACKEND_H

#include "runtime.h"

#define OM_JOIN_INNER(a, b) a##b
#define OM_JOIN(a, b) OM_JOIN_INNER(a, b)
#define OM_FN(prefix, name) OM_JOIN(prefix, name)

#define OM_DECLARE_BACKEND(prefix) \
bool OM_FN(prefix, _make_array)(struct rt_env *, struct rt_value *); \
bool OM_FN(prefix, _get_array_size)(struct rt_env *, struct rt_value *, size_t *); \
bool OM_FN(prefix, _read_array)(struct rt_env *, struct rt_value *, size_t, struct rt_value *); \
bool OM_FN(prefix, _write_array)(struct rt_env *, struct rt_value *, size_t, struct rt_value *); \
bool OM_FN(prefix, _resize_array)(struct rt_env *, struct rt_value *, size_t); \
bool OM_FN(prefix, _copy_array)(struct rt_env *, struct rt_value *, struct rt_value *); \
bool OM_FN(prefix, _make_dict)(struct rt_env *, struct rt_value *); \
bool OM_FN(prefix, _get_dict_size)(struct rt_env *, struct rt_value *, size_t *); \
bool OM_FN(prefix, _get_dict_alloc_size)(struct rt_env *, struct rt_value *, size_t *); \
bool OM_FN(prefix, _check_dict_key)(struct rt_env *, struct rt_value *, struct rt_value *, bool *); \
bool OM_FN(prefix, _read_dict)(struct rt_env *, struct rt_value *, struct rt_value *, struct rt_value *); \
bool OM_FN(prefix, _read_dict_with_hash)(struct rt_env *, struct rt_value *, const char *, size_t, uint32_t, struct rt_value *); \
bool OM_FN(prefix, _read_dict_index)(struct rt_env *, struct rt_value *, size_t, struct rt_value *, struct rt_value *); \
bool OM_FN(prefix, _write_dict)(struct rt_env *, struct rt_value *, struct rt_value *, struct rt_value *); \
bool OM_FN(prefix, _write_dict_with_hash)(struct rt_env *, struct rt_value *, const char *, size_t, uint32_t, struct rt_value *); \
bool OM_FN(prefix, _erase_dict_entry)(struct rt_env *, struct rt_value *, struct rt_value *); \
bool OM_FN(prefix, _copy_dict)(struct rt_env *, struct rt_value *, struct rt_value *); \
bool OM_FN(prefix, _merge_dict)(struct rt_env *, struct rt_value *, struct rt_value *, struct rt_value *); \
bool OM_FN(prefix, _freeze_dict)(struct rt_env *, struct rt_value *); \
bool OM_FN(prefix, _enter_gc)(struct rt_env *, int); \
void OM_FN(prefix, _leave_gc)(struct rt_env *); \
void OM_FN(prefix, _safepoint)(struct rt_env *); \
void OM_FN(prefix, _init_env)(struct rt_env *); \
void OM_FN(prefix, _enter_blocking)(struct rt_env *); \
void OM_FN(prefix, _leave_blocking)(struct rt_env *)

OM_DECLARE_BACKEND(om_st);
#if defined(NOCT_USE_MULTITHREAD)
OM_DECLARE_BACKEND(om_mt);
#endif

#undef OM_DECLARE_BACKEND
#undef OM_FN
#undef OM_JOIN
#undef OM_JOIN_INNER

#endif
