/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * NoctLang
 */

#ifndef NOCT_RUNTIME_H
#define NOCT_RUNTIME_H

#include <noct/noct.h>
#include "c89compat.h"

/*
 * Butecode OP definition.
 */
enum rt_bytecode {
	ROP_NOP,		/* 0x00: nop */
	ROP_ASSIGN,		/* 0x01: dst = src */
	ROP_ICONST,		/* 0x02: dst = integer constant */
	ROP_FCONST,		/* 0x03: dst = floating-point constant */
	ROP_SCONST,		/* 0x04: dst = string constant */
	ROP_ACONST,		/* 0x05: dst = empty array */
	ROP_DCONST,		/* 0x06: dst = empty dictionary */
	ROP_INC,		/* 0x07: dst = src + 1 */
	ROP_NEG,		/* 0x08: dst = -src */
	ROP_NOT,		/* 0x09: dst = !src */
	ROP_ADD,		/* 0x0a: dst = src1 + src2 */
	ROP_SUB,		/* 0x0b: dst = src1 - src2 */
	ROP_MUL,		/* 0x0c: dst = src1 * src2 */
	ROP_DIV,		/* 0x0d: dst = src1 / src2 */
	ROP_MOD,		/* 0x0e: dst = src1 % src2 */
	ROP_AND,		/* 0x0f: dst = src1 & src2 */
	ROP_OR,			/* 0x10: dst = src1 | src2 */
	ROP_XOR,		/* 0x11: dst = src1 ^ src2 */
	ROP_LT,			/* 0x12: dst = src1 <  src2 [0 or 1] */
	ROP_LTE,		/* 0x13: dst = src1 <= src2 [0 or 1] */
	ROP_GT,			/* 0x14: dst = src1 >  src2 [0 or 1] */
	ROP_GTE,		/* 0x15: dst = src1 >= src2 [0 or 1] */
	ROP_EQ,			/* 0x16: dst = src1 == src2 [0 or 1] */
	ROP_NEQ,		/* 0x17: dst = src1 != src2 [0 or 1] */
	ROP_EQI,		/* 0x18: dst = src1 == src2 [0 or 1], integers */
	ROP_LOADARRAY,		/* 0x19: dst = src1[src2] */
	ROP_STOREARRAY,		/* 0x1a: opr1[opr2] = opr3 */
	ROP_LEN,		/* 0x1b: dst = len(src) */
	ROP_GETDICTKEYBYINDEX,	/* 0x1c: dst = src1.keyAt(src2) */
	ROP_GETDICTVALBYINDEX,	/* 0x1d: dst = src1.valAt(src2) */
	ROP_STOREDOT,		/* 0x1e: obj.access = src */
	ROP_LOADDOT,		/* 0x1f: dst = obj.access */
	ROP_STORESYMBOL,	/* 0x20: setSymbol(dst, src) */
	ROP_LOADSYMBOL,		/* 0x21: dst = getSymbol(src) */
	ROP_CALL,		/* 0x22: func(arg1, ...) */
	ROP_THISCALL,		/* 0x23: obj->func(arg1, ...) */
	ROP_JMP,		/* 0x24: PC = src */
	ROP_JMPIFTRUE,		/* 0x25: PC = src1 if src2 != 0 */
	ROP_JMPIFFALSE,		/* 0x26: PC = src1 if src2 == 0 */
	ROP_JMPIFEQ,		/* 0x27: PC = src1 if src2 indicates eq */
	ROP_LINEINFO,		/* 0x28: line = src */
};

/*
 * GC List Type
 */
enum {
	RT_GC_SHALLOW,
	RT_GC_YOUNG,
	RT_GC_TENURE,
};

struct rt_env;
struct rt_frame;
struct rt_value;
struct rt_object_header;
struct rt_string;
struct rt_array;
struct rt_dict;
struct rt_func;
struct rt_bindglobal;

/*
 * Runtime environment.
 */
struct rt_env {
	/* Stack. (Do not move. JIT assumes the offset 0.) */
	struct rt_frame *frame;

	/* Execution line. (Do not move. JIT assumes the offset 8.) */
	int line;

	/* Global symbols. */
	struct rt_bindglobal *global;

	/* Function list. */
	struct rt_func *func_list;

	/* Heap usage in bytes. */
	size_t heap_usage;

	/* Young object list. */
	struct rt_object_header *young_list;

	/* Garbage object list. */
	struct rt_object_header *garbage_list;

	/* Execution file. */
	char file_name[1024];

	/* Error message. */
	char error_message[4096];
};

/*
 * Calling frame.
 */
struct rt_frame {
	/* tmpvar (Do not move. JIT assumes the offset 0.) */
	struct rt_value *tmpvar;
	int tmpvar_size;

	/* function */
	struct rt_func *func;

	/* Shallow object list. */
	struct rt_object_header *shallow_list;

	/* Next frame. */
	struct rt_frame *next;
};

/*
 * Object header.
 */
struct rt_object_header {
	/* Type. */
	int type;

	/* String list (shallow, young, or tenure). */
	struct rt_object_header *prev;
	struct rt_object_header *next;
	int gc_type;

	/* Is marked? (for mark-and-sweep GC). */
	bool is_marked;

	/* Is referenced by native code? */
	bool has_native_ref;
};

/*
 * String object.
 */
struct rt_string {
	struct rt_object_header head;

	char *s;
};

/*
 * Array object.
 */
struct rt_array {
	struct rt_object_header head;

	int alloc_size;
	int size;
	struct rt_value *table;
};

/*
 * Dictionary object.
 */
struct rt_dict {
	struct rt_object_header head;

	int alloc_size;
	int size;
	char **key;
	struct rt_value *value;
};

/*
 * Function object.
 */
struct rt_func {
	char *name;
	int param_count;
	char *param_name[NOCT_ARG_MAX];

	char *file_name;

	/* Bytecode for a function. (if not a cfunc) */
	int bytecode_size;
	uint8_t *bytecode;
	int tmpvar_size;

	/* JIT-generated code. */
	SYSVABI bool (*jit_code)(struct rt_env *env);

	/* Function pointer. (if a cfunc) */
	bool (*cfunc)(struct rt_env *env);

	/* Next. */
	struct rt_func *next;
};

/*
 * Global variable entry.
 */
struct rt_bindglobal {
	char *name;
	struct rt_value val;
	struct rt_bindglobal *next;
};

/* Enter a new calling frame. */
bool rt_enter_frame(struct rt_env *rt, struct rt_func *func);

/* Leave the current calling frame. */
void rt_leave_frame(struct rt_env *rt, struct rt_value *ret);

/* Find a global variable. */
bool rt_find_global(struct rt_env *rt, const char *name, struct rt_bindglobal **global);

/* Add a global variable. */
bool rt_add_global(struct rt_env *rt, const char *name, struct rt_bindglobal **global);

/* Move an object from the shallow list to the young list. */
void rt_move_shallow_to_young_list(struct rt_env *rt, struct rt_value *val);

/* Output an out-of-memory message. */
void rt_out_of_memory(struct rt_env *rt);

#endif
