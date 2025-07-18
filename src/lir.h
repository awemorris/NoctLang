/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * LIR: Low-level intermediate representation
 */

#ifndef NOCT_LIR_H
#define NOCT_LIR_H

#include "c89compat.h"

#define LIR_PARAM_SIZE		32

enum bytecode {
	LOP_NOP,			/* 0x00: nop */

	/* tmpvar assignment */
	LOP_ASSIGN,		/* 0x01: dst = src */
	LOP_ICONST,		/* 0x02: dst = integer constant */
	LOP_FCONST,		/* 0x03: dst = floating-point constant */
	LOP_SCONST,		/* 0x04: dst = string constant */
	LOP_ACONST,		/* 0x05: dst = empty array */
	LOP_DCONST,		/* 0x06: dst = empty dictionary */

	/* tmpvar calc (dst = op src1) */
	LOP_INC,		/* 0x07: dst = src + 1, assume operands are integers */
	LOP_NEG,		/* 0x08: dst = ~src */

	/* tmpvar calc (dst = src1 op src2) */
	LOP_ADD,		/* 0x09: dst = src1 + src2 */
	LOP_SUB,		/* 0x0a: dst = src1 - src2 */
	LOP_MUL,		/* 0x0b: dst = src1 * src2 */
	LOP_DIV,		/* 0x0c: dst = src1 / src2 */
	LOP_MOD,		/* 0x0d: dst = src1 % src2 */
	LOP_AND,		/* 0x0e: dst = src1 & src2 */
	LOP_OR,			/* 0x0f: dst = src1 | src2 */
	LOP_XOR,		/* 0x10: dst = src1 ^ src2 */
	LOP_LT,			/* 0x11: dst = src1 <  src2 [0 or 1] */
	LOP_LTE,		/* 0x12: dst = src1 <= src2 [0 or 1] */
	LOP_GT,			/* 0x13: dst = src1 >  src2 [0 or 1] */
	LOP_GTE,		/* 0x14: dst = src1 >= src2 [0 or 1] */
	LOP_EQ,			/* 0x15: dst = src1 == src2 [0 or 1] */
	LOP_NEQ,		/* 0x16: dst = src1 != src2 [0 or 1] */
	LOP_EQI,		/* 0x17: dst = src1 == src2 [0 or 1], assume operands are integers */

	/* array/dictionary */
	LOP_LOADARRAY,		/* 0x18: dst = src1[src2] */
	LOP_STOREARRAY,		/* 0x19: opr1[opr2] = op3 */
	LOP_LEN,		/* 0x1a: dst = len(src) */

	/* dictionary */
	LOP_GETDICTKEYBYINDEX,	/* 0x1b: dst = src1.keyAt(src2) */
	LOP_GETDICTVALBYINDEX,	/* 0x1c: dst = src1.valAt(src2) */
	LOP_STOREDOT,		/* 0x1d: obj.access = src */
	LOP_LOADDOT,		/* 0x1e: dst = obj.access */

	/* symbol */
	LOP_STORESYMBOL,	/* 0x1f: setSymbol(dst, src) */
	LOP_LOADSYMBOL,		/* 0x20: dst = getSymbol(src) */

	/* call */
	LOP_CALL,		/* 0x21: func(arg1, ...) */
	LOP_THISCALL,		/* 0x22: obj->func(arg1, ...) */

	/* branch */
	LOP_JMP,		/* 0x23: PC = src */
	LOP_JMPIFTRUE,		/* 0x24: PC = src1 if src2 == 1 */
	LOP_JMPIFFALSE,		/* 0x25: PC = src1 if src2 != 1 */
	LOP_JMPIFEQ,		/* 0x25: PC = src1 if src2 indicates eq */

	/* line number */
	LOP_LINEINFO,		/* 0x26: setDebugLine(src) */
};

struct hir_block;

struct lir_func {
	char *file_name;
	char *func_name;
	int param_count;
	char *param_name[LIR_PARAM_SIZE];
	int tmpvar_size;
	int bytecode_size;
	uint8_t *bytecode;
};

/* Build a LIR function from a HIR function. */
bool lir_build(struct hir_block *hir_func, struct lir_func **lir_func);

/* Free a constructed LIR. */
void lir_free(struct lir_func *func);

/* Get a file name. */
const char *lir_get_file_name(void);

/* Get an error line. */
int lir_get_error_line(void);

/* Get an error message. */
const char *lir_get_error_message(void);

/* Dump LIR. */
void lir_dump(struct lir_func *func);

#endif
