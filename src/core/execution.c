/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Execution Helpers
 */

#include <noct/noct.h>
#include "runtime.h"
#include "intrinsics.h"
#include "bytecode.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

/*
 * Some exotic compilers for x86 including Watcom utilize registers to
 * pass function arguments. However, our JIT-generated code for x86
 * uses the stack for function arguments. To bridge this gap, we use
 * the CDECL keyword in this module.
 */

/*
 * Make a string.
 */
NOCT_DLL
bool
CDECL
noct_ex_make_string_with_hash(
	NoctEnv *env,
	NoctValue *val,
	const char *data,
	size_t len,
	uint32_t hash)
{
	/* Be careful: calling convention may be different. */
	return rt_make_string_with_hash(env, val, data, len, hash);
}

/*
 * Make an empty array.
 */
NOCT_DLL
bool
CDECL
noct_ex_make_empty_array(
	NoctEnv *env,
	NoctValue *val)
{
	/* Be careful: calling convention may be different. */
	return rt_make_empty_array(env, val);
}

/*
 * Make an empty dictionary.
 */
NOCT_DLL
bool
CDECL
noct_ex_make_empty_dict(
	NoctEnv *env,
	NoctValue *val)
{
	/* Be careful: calling convention may be different. */
	return rt_make_empty_dict(env, val);
}

/*
 * Add helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_add_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i + src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)src1_val->val.i + src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.i + src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.i + src2_val->val.lf;
			break;
		case NOCT_VALUE_STRING:
			if (!noct_make_string_format(env, dst_val, "%d%s", src1_val->val.i, src2_val->val.str->data))
				return false;
			break;
		default:
			rt_error(env, N_TR("Value is not a number or a string."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = src1_val->val.l + (int64_t)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = src1_val->val.l + src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.l + src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.l + src2_val->val.lf;
			break;
		case NOCT_VALUE_STRING:
			if (!noct_make_string_format(env, dst_val, "%" PRId64 "%s", src1_val->val.l, src2_val->val.str->data))
				return false;
			break;
		default:
			rt_error(env, N_TR("Value is not a number or a string."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f + (float)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f + (float)src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f + src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.f + src2_val->val.lf;
			break;
		case NOCT_VALUE_STRING:
			if (!noct_make_string_format(env, dst_val, "%f%s", src1_val->val.f, src2_val->val.str->data))
				return false;
			break;
		default:
			rt_error(env, N_TR("Value is not a number or a string."));
			return false;
		}
		break;
	case NOCT_VALUE_DOUBLE:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf + (double)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf + (double)src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf + (double)src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf + src2_val->val.lf;
			break;
		case NOCT_VALUE_STRING:
			if (!noct_make_string_format(env, dst_val, "%f%s", src1_val->val.lf, src2_val->val.str->data))
				return false;
			break;
		default:
			rt_error(env, N_TR("Value is not a number or a string."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (!noct_make_string_format(env, dst_val, "%s%d", src1_val->val.str->data, src2_val->val.i))
				return false;
			break;
		case NOCT_VALUE_LONG:
			if (!noct_make_string_format(env, dst_val, "%s%lld", src1_val->val.str->data, src2_val->val.l))
				return false;
			break;
		case NOCT_VALUE_FLOAT:
			if (!noct_make_string_format(env, dst_val, "%s%.7g", src1_val->val.str->data, src2_val->val.f))
				return false;
			break;
		case NOCT_VALUE_DOUBLE:
			if (!noct_make_string_format(env, dst_val, "%s%.15g", src1_val->val.str->data, src2_val->val.lf))
				return false;
			break;
		case NOCT_VALUE_STRING:
			if (!noct_make_string_format(env, dst_val, "%s%s", src1_val->val.str->data, src2_val->val.str->data))
				return false;
			break;
		default:
			rt_error(env, N_TR("Value is not a number or a string."));
			return false;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * Subtract helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_sub_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i - src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)src1_val->val.i - src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.i - src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.i - src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = src1_val->val.l - (int64_t)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = src1_val->val.l - src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.l - src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.l - src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f - (float)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f - (float)src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f - src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.f - src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_DOUBLE:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf - (double)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf - (double)src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf - (double)src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf - src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not a number."));
		return false;
	}

	return true;
}

/*
 * Multiply helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_mul_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i * src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)src1_val->val.i * src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.i * src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.i * src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = src1_val->val.l * (int64_t)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = src1_val->val.l * src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.l * src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.l * src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f * (float)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f * (float)src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f * src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.f * src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_DOUBLE:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf * (double)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf * (double)src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf * (double)src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf * src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not a number."));
		return false;
	}

	return true;
}

/*
 * Multiply helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_div_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	/*
	 * Integer-performed divisions (both operands int/long) error on
	 * a zero divisor.  Float/double-performed divisions are total:
	 * they follow IEEE 754 and yield +/-inf or NaN (design 07
	 * Part 0, D-TOP12).  The FP environment keeps its default
	 * masked-exception state on every target; the runtime never
	 * touches MXCSR/FPCR or their equivalents.
	 */
	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (src2_val->val.i == 0) {
				rt_error(env, N_TR("Division by zero."));
				return false;
			}
			if (src2_val->val.i == -1 &&
			    src1_val->val.i == (-2147483647 - 1)) {
				/* Wraps: -INT_MIN == INT_MIN; the raw C
				   division traps (SIGFPE). */
				dst_val->type = NOCT_VALUE_INT;
				dst_val->val.i = src1_val->val.i;
				break;
			}
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i / src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			if (src2_val->val.l == 0) {
				rt_error(env, N_TR("Division by zero."));
				return false;
			}
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)src1_val->val.i / src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.i / src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.i / src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (src2_val->val.i == 0) {
				rt_error(env, N_TR("Division by zero."));
				return false;
			}
			if (src2_val->val.i == -1 &&
			    src1_val->val.l == (int64_t)((uint64_t)1 << 63)) {
				dst_val->type = NOCT_VALUE_LONG;
				dst_val->val.l = src1_val->val.l;
				break;
			}
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = src1_val->val.l / (int64_t)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			if (src2_val->val.l == 0) {
				rt_error(env, N_TR("Division by zero."));
				return false;
			}
			if (src2_val->val.l == -1 &&
			    src1_val->val.l == (int64_t)((uint64_t)1 << 63)) {
				dst_val->type = NOCT_VALUE_LONG;
				dst_val->val.l = src1_val->val.l;
				break;
			}
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = src1_val->val.l / src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.l / src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.l / src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f / (float)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f / (float)src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f / src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = (double)src1_val->val.f / src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_DOUBLE:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf / (double)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf / (double)src2_val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf / (double)src2_val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_DOUBLE;
			dst_val->val.lf = src1_val->val.lf / src2_val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not a number."));
		return false;
	}

	return true;
}

/*
 * MOD helper. (modulo)
 */
NOCT_DLL
bool
CDECL
noct_ex_mod_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (src2_val->val.i == 0) {
				rt_error(env, N_TR("Division by zero."));
				return false;
			}
			if (src2_val->val.i == -1 &&
			    src1_val->val.i == (-2147483647 - 1)) {
				/* The raw C modulo traps (SIGFPE). */
				dst_val->type = NOCT_VALUE_INT;
				dst_val->val.i = 0;
				break;
			}
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i % src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			if (src2_val->val.l == 0) {
				rt_error(env, N_TR("Division by zero."));
				return false;
			}
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)src1_val->val.i % src2_val->val.l;
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (src2_val->val.i == 0) {
				rt_error(env, N_TR("Division by zero."));
				return false;
			}
			if (src2_val->val.i == -1 &&
			    src1_val->val.l == (int64_t)((uint64_t)1 << 63)) {
				dst_val->type = NOCT_VALUE_LONG;
				dst_val->val.l = 0;
				break;
			}
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = src1_val->val.l % (int64_t)src2_val->val.i;
			break;
		case NOCT_VALUE_LONG:
			if (src2_val->val.l == 0) {
				rt_error(env, N_TR("Division by zero."));
				return false;
			}
			if (src2_val->val.l == -1 &&
			    src1_val->val.l == (int64_t)((uint64_t)1 << 63)) {
				dst_val->type = NOCT_VALUE_LONG;
				dst_val->val.l = 0;
				break;
			}
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = src1_val->val.l % src2_val->val.l;
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * AND helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_and_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (int32_t)((uint32_t)src1_val->val.i & (uint32_t)src2_val->val.i);
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)(uint32_t)src1_val->val.i & (uint64_t)src2_val->val.l);
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)src1_val->val.l & (uint64_t)(uint32_t)src2_val->val.i);
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)src1_val->val.l & (uint64_t)src2_val->val.l);
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * OR helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_or_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (int32_t)((uint32_t)src1_val->val.i | (uint32_t)src2_val->val.i);
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)(uint32_t)src1_val->val.i | (uint64_t)src2_val->val.l);
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)src1_val->val.l | (uint64_t)(uint32_t)src2_val->val.i);
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)src1_val->val.l | (uint64_t)src2_val->val.l);
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * XOR helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_xor_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (int32_t)((uint32_t)src1_val->val.i ^ (uint32_t)src2_val->val.i);
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)(uint32_t)src1_val->val.i ^ (uint64_t)src2_val->val.l);
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)src1_val->val.l ^ (uint64_t)(uint32_t)src2_val->val.i);
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)src1_val->val.l ^ (uint64_t)src2_val->val.l);
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * SHL helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_shl_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (src2_val->val.i < 0 || src2_val->val.i >= 32) {
				rt_error(env, N_TR("Invalid shift amount."));
				return false;
			}
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (int32_t)((uint32_t)src1_val->val.i << src2_val->val.i);
			break;
		case NOCT_VALUE_LONG:
			if (src2_val->val.l < 0 || src2_val->val.l >= 32) {
				rt_error(env, N_TR("Invalid shift amount."));
				return false;
			}
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (int32_t)((uint32_t)src1_val->val.i << src2_val->val.l);
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (src2_val->val.i < 0 || src2_val->val.i >= 32) {
				rt_error(env, N_TR("Invalid shift amount."));
				return false;
			}
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)src1_val->val.l << src2_val->val.i);
			break;
		case NOCT_VALUE_LONG:
			if (src2_val->val.l < 0 || src2_val->val.l >= 32) {
				rt_error(env, N_TR("Invalid shift amount."));
				return false;
			}
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)src1_val->val.l << src2_val->val.l);
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * SHR helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_shr_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (src2_val->val.i < 0 || src2_val->val.i >= 32) {
				rt_error(env, N_TR("Invalid shift amount."));
				return false;
			}
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (int32_t)((uint32_t)src1_val->val.i >> src2_val->val.i);
			break;
		case NOCT_VALUE_LONG:
			if (src2_val->val.l < 0 || src2_val->val.l >= 32) {
				rt_error(env, N_TR("Invalid shift amount."));
				return false;
			}
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (int32_t)((uint32_t)src1_val->val.i >> src2_val->val.l);
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (src2_val->val.i < 0 || src2_val->val.i >= 32) {
				rt_error(env, N_TR("Invalid shift amount."));
				return false;
			}
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)src1_val->val.l >> src2_val->val.i);
			break;
		case NOCT_VALUE_LONG:
			if (src2_val->val.l < 0 || src2_val->val.l >= 32) {
				rt_error(env, N_TR("Invalid shift amount."));
				return false;
			}
			dst_val->type = NOCT_VALUE_LONG;
			dst_val->val.l = (int64_t)((uint64_t)src1_val->val.l >> src2_val->val.l);
			break;
		default:
			rt_error(env, N_TR("Value is not an integer."));
			return false;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * NEG helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_neg_helper(
	NoctEnv *env,
	int dst,
	int src)
{
	struct rt_value *dst_val;
	struct rt_value *src_val;

	dst_val = &env->frame->tmpvar[dst];
	src_val = &env->frame->tmpvar[src];

	switch (src_val->type) {
	case NOCT_VALUE_INT:
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.i = -src_val->val.i;
		break;
	case NOCT_VALUE_LONG:
		dst_val->type = NOCT_VALUE_LONG;
		dst_val->val.l = -src_val->val.l;
		break;
	case NOCT_VALUE_FLOAT:
		dst_val->type = NOCT_VALUE_FLOAT;
		dst_val->val.f = -src_val->val.f;
		break;
	case NOCT_VALUE_DOUBLE:
		dst_val->type = NOCT_VALUE_DOUBLE;
		dst_val->val.lf = -src_val->val.lf;
		break;
	default:
		rt_error(env, N_TR("Value is not a number."));
		return false;
	}

	return true;
}

/*
 * NOT helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_not_helper(
	NoctEnv *env,
	int dst,
	int src)
{
	struct rt_value *dst_val;
	struct rt_value *src_val;

	dst_val = &env->frame->tmpvar[dst];
	src_val = &env->frame->tmpvar[src];

	switch (src_val->type) {
	case NOCT_VALUE_INT:
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.i = src_val->val.i == 0 ? 1 : 0;
		break;
	case NOCT_VALUE_LONG:
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.l = src_val->val.l == 0 ? 1 : 0;
		break;
	default:
		rt_error(env, N_TR("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * LT helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_lt_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i < src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((int64_t)src1_val->val.i < src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i < src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.i < src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.l < (int64_t)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.l < src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.l < src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.l < src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f < (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f < (float)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f < src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.f < src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_DOUBLE:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf < (double)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf < (double)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf < (double)src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf < src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->data, src2_val->val.str->data) < 0 ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a string."));
			break;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * LTE helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_lte_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i <= src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((int64_t)src1_val->val.i <= src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i <= src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.i <= src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.l <= (int64_t)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.l <= src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.l <= src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.l <= src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f <= (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f <= (float)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f <= src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.f <= src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_DOUBLE:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf <= (double)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf <= (double)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf <= (double)src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf <= src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->data, src2_val->val.str->data) <= 0 ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a string."));
			break;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * GT helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_gt_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i > src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((int64_t)src1_val->val.i > src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i > src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.i > src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.l > (int64_t)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.l > src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.l > src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.l > src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f > (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f > (float)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f > src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.f > src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_DOUBLE:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf > (double)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf > (double)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf > (double)src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf > src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->data, src2_val->val.str->data) > 0 ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a string."));
			break;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * GTE helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_gte_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i >= src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((int64_t)src1_val->val.i >= src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i >= src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.i >= (double)src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.l >= (int64_t)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.l >= src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.l >= src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.l >= (double)src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f >= (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f >= (float)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f >= src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.f >= src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_DOUBLE:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf >= (double)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf >= (double)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf >= (double)src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf >= src2_val->val.lf) ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->data, src2_val->val.str->data) >= 0 ? 1 : 0;
			break;
		default:
			rt_error(env, N_TR("Value is not a string."));
			break;
		}
		break;
	default:
		rt_error(env, N_TR("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * EQ helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_eq_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i == src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((uint64_t)(uint32_t)src1_val->val.i == (uint64_t)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i == src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.i == src2_val->val.lf) ? 1 : 0;
			break;
		default:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = 0;
			break;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((uint64_t)src1_val->val.l == (uint64_t)(uint32_t)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.l == src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.l == src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.l == src2_val->val.lf) ? 1 : 0;
			break;
		default:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = 0;
			break;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f == (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f == (float)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f == src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f == src2_val->val.lf) ? 1 : 0;
			break;
		default:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = 0;
			break;
		}
		break;
	case NOCT_VALUE_DOUBLE:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf == (double)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf == (double)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf == (double)src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf == src2_val->val.lf) ? 1 : 0;
			break;
		default:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = 0;
			break;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			rt_cache_string_hash(src1_val->val.str);
			rt_cache_string_hash(src2_val->val.str);

			dst_val->type = NOCT_VALUE_INT;
			if (src1_val->val.str->len == src2_val->val.str->len &&
			    src1_val->val.str->hash == src2_val->val.str->hash)
				dst_val->val.i = strcmp(src1_val->val.str->data, src2_val->val.str->data) == 0 ? 1 : 0;
			else
				dst_val->val.i = 0;
			break;
		default:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = 0;
			break;
		}
		break;
	case NOCT_VALUE_ARRAY:
	case NOCT_VALUE_DICT:
	case NOCT_VALUE_PACKED:
	case NOCT_VALUE_FUNC:
		/* Reference types compare by identity. */
		dst_val->type = NOCT_VALUE_INT;
		if (src2_val->type == src1_val->type &&
		    src1_val->val.obj == src2_val->val.obj)
			dst_val->val.i = 1;
		else
			dst_val->val.i = 0;
		break;
	default:
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.i = 0;
		break;
	}

	return true;
}

/* NEQ helper. */
NOCT_DLL
bool
CDECL
noct_ex_neq_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &env->frame->tmpvar[dst];
	src1_val = &env->frame->tmpvar[src1];
	src2_val = &env->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i != src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((int64_t)src1_val->val.i != src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i != src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.i != src2_val->val.lf) ? 1 : 0;
			break;
		default:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = 1;
			break;
		}
		break;
	case NOCT_VALUE_LONG:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((uint64_t)src1_val->val.l != (uint64_t)(uint32_t)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.l != src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.l != src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.l != src2_val->val.lf) ? 1 : 0;
			break;
		default:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = 1;
			break;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f != (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f != (float)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f != src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((double)src1_val->val.f != src2_val->val.lf) ? 1 : 0;
			break;
		default:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = 1;
			break;
		}
		break;
	case NOCT_VALUE_DOUBLE:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf != (double)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_LONG:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf != (double)src2_val->val.l) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf != (double)src2_val->val.f) ? 1 : 0;
			break;
		case NOCT_VALUE_DOUBLE:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.lf != src2_val->val.lf) ? 1 : 0;
			break;
		default:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = 1;
			break;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->data, src2_val->val.str->data) != 0 ? 1 : 0;
			break;
		default:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = 1;
			break;
		}
		break;
	case NOCT_VALUE_ARRAY:
	case NOCT_VALUE_DICT:
	case NOCT_VALUE_PACKED:
	case NOCT_VALUE_FUNC:
		/* Reference types compare by identity. */
		dst_val->type = NOCT_VALUE_INT;
		if (src2_val->type == src1_val->type &&
		    src1_val->val.obj == src2_val->val.obj)
			dst_val->val.i = 0;
		else
			dst_val->val.i = 1;
		break;
	default:
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.i = 1;
		break;
	}

	return true;
}

/*
 * STOREARRAY helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_storearray_helper(
	NoctEnv *env,
	int arr,
	int subscr,
	int val)
{
	struct rt_value *arr_val;
	struct rt_value *subscr_val;
	struct rt_value *val_val;
	size_t index;

	/* Get the container. */
	arr_val = &env->frame->tmpvar[arr];

	/* Get the subscription. */
	subscr_val = &env->frame->tmpvar[subscr];

	/* Get the value to assign. */
	val_val = &env->frame->tmpvar[val];

	if (arr_val->type == NOCT_VALUE_ARRAY) {
		if (subscr_val->type == NOCT_VALUE_INT) {
			if (subscr_val->val.i < 0) {
				rt_error(env, N_TR("Subscript is negative."));
				return false;
			}
			index = (size_t)(uint32_t)subscr_val->val.i;
		} else if (subscr_val->type == NOCT_VALUE_LONG) {
			if (subscr_val->val.l < 0) {
				rt_error(env, N_TR("Subscript is negative."));
				return false;
			}
			index = (size_t)(uint64_t)subscr_val->val.l;
		} else {
			rt_error(env, N_TR("Subscript not an integer."));
			return false;
		}

		/* Store to the array. */
		if (!rt_set_array_elem(env, arr_val, index, val_val))
			return false;
		return true;
	} else if (arr_val->type == NOCT_VALUE_DICT) {
		if (subscr_val->type != NOCT_VALUE_STRING) {
			rt_error(env, N_TR("Subscript not a string."));
			return false;
		}

		/* Cache the key string hash. */
		rt_cache_string_hash(subscr_val->val.str);

		/* Store to the dictionary. */
		if (!rt_set_dict_elem(env,
				      arr_val,
				      subscr_val,
				      val_val))
			return false;
		return true;
	} else if (arr_val->type == NOCT_VALUE_PACKED) {
		if (subscr_val->type == NOCT_VALUE_INT) {
			if (subscr_val->val.i < 0) {
				rt_error(env, N_TR("Subscript is negative."));
				return false;
			}
			index = (size_t)(uint32_t)subscr_val->val.i;
		} else if (subscr_val->type == NOCT_VALUE_LONG) {
			if (subscr_val->val.l < 0) {
				rt_error(env, N_TR("Subscript is negative."));
				return false;
			}
			index = (size_t)(uint64_t)subscr_val->val.l;
		} else {
			rt_error(env, N_TR("Subscript not an integer."));
			return false;
		}

		/* Store to the packed. */
		if (!rt_set_packed_elem(env, arr_val, index, val_val))
			return false;
		return true;
	}

	rt_error(env, N_TR("Not an array or a dictionary."));
	return false;
}

/*
 * LOADARRAY helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_loadarray_helper(
	NoctEnv *env,
	int dst,
	int arr,
	int subscr)
{
	struct rt_value *dst_val;
	struct rt_value *arr_val;
	struct rt_value *subscr_val;
	size_t index;

	dst_val = &env->frame->tmpvar[dst];
	arr_val = &env->frame->tmpvar[arr];
	subscr_val = &env->frame->tmpvar[subscr];

	/* Check the array type. */
	if (arr_val->type == NOCT_VALUE_ARRAY) {
		if (subscr_val->type == NOCT_VALUE_INT) {
			if (subscr_val->val.i < 0) {
				rt_error(env, N_TR("Subscript is negative."));
				return false;
			}
			index = (size_t)(uint32_t)subscr_val->val.i;
		} else if (subscr_val->type == NOCT_VALUE_LONG) {
			if (subscr_val->val.l < 0) {
				rt_error(env, N_TR("Subscript is negative."));
				return false;
			}
			index = (size_t)(uint64_t)subscr_val->val.l;
		} else {
			rt_error(env, N_TR("Subscript not an integer."));
			return false;
		}

		/* Load the array element. */
		if (!rt_get_array_elem(env, arr_val, index, dst_val))
			return false;
		return true;
	} else if (arr_val->type == NOCT_VALUE_DICT) {
		/* Get the key string. */
		if (subscr_val->type != NOCT_VALUE_STRING) {
			rt_error(env, N_TR("Subscript not a string."));
			return false;
		}

		/* Cache the key string hash. */
		rt_cache_string_hash(subscr_val->val.str);

		/* Get the dictionary element. */
		if (!rt_get_dict_elem(env, arr_val, subscr_val, dst_val))
			return false;
		return true;
	} else if (arr_val->type == NOCT_VALUE_PACKED) {
		if (subscr_val->type == NOCT_VALUE_INT) {
			if (subscr_val->val.i < 0) {
				rt_error(env, N_TR("Subscript is negative."));
				return false;
			}
			index = (size_t)(uint32_t)subscr_val->val.i;
		} else if (subscr_val->type == NOCT_VALUE_LONG) {
			if (subscr_val->val.l < 0) {
				rt_error(env, N_TR("Subscript is negative."));
				return false;
			}
			index = (size_t)(uint64_t)subscr_val->val.l;
		} else {
			rt_error(env, N_TR("Subscript not an integer."));
			return false;
		}

		/* Load the packed element. */
		if (!rt_get_packed_elem(env, arr_val, index, dst_val))
			return false;
		return true;
	}

	rt_error(env, N_TR("Not an array or a dictionary."));
	return false;
}

/*
 * LEN helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_len_helper(
	NoctEnv *env,
	int dst,
	int src)
{
	struct rt_value *dst_val;
	struct rt_value *src_val;
	size_t val;

	dst_val = &env->frame->tmpvar[dst];
	src_val = &env->frame->tmpvar[src];

	switch (src_val->type) {
	case NOCT_VALUE_STRING:
		val = (src_val->val.str->len - 1); /* Exclude NUL */
		break;
	case NOCT_VALUE_ARRAY:
		rt_get_array_size(env, src_val, &val);
		break;
	case NOCT_VALUE_DICT:
		rt_get_dict_size(env, src_val, &val);
		break;
	case NOCT_VALUE_PACKED:
		rt_get_packed_size(env, src_val, &val);
		break;
	default:
		rt_error(env, N_TR("Value is not a string, an array, or a dictionary."));
		return false;
	}

	if (val <= INT_MAX) {
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.i = (int32_t)val;
	} else {
		dst_val->type = NOCT_VALUE_LONG;
		dst_val->val.l = (int64_t)val;
	}

	return true;
}

/*
 * GETDICTKEYBYINDEX helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_getdictkeybyindex_helper(
	NoctEnv *env,
	int dst,
	int dict,
	int subscr)
{
	struct rt_value *dst_val;
	struct rt_value *dict_val;
	struct rt_value *subscr_val;
	struct rt_value val;
	size_t index;

	dst_val = &env->frame->tmpvar[dst];
	dict_val = &env->frame->tmpvar[dict];
	subscr_val = &env->frame->tmpvar[subscr];

	if (dict_val->type != NOCT_VALUE_DICT) {
		rt_error(env, N_TR("Not a dictionary."));
		return false;
	}
	if (subscr_val->type == NOCT_VALUE_INT) {
		index = (size_t)(uint32_t)subscr_val->val.i;
	} else if (subscr_val->type == NOCT_VALUE_LONG) {
		index = (size_t)(uint64_t)subscr_val->val.l;
	} else {
		rt_error(env, N_TR("Subscript not an integer."));
		return false;
	}

	/* Load the element. */
	if (!rt_get_dict_by_index(env, dict_val, index, dst_val, &val))
		return false;

	return true;
}

/*
 * GETDICTVALBYINDEX helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_getdictvalbyindex_helper(
	NoctEnv *env,
	int dst,
	int dict,
	int subscr)
{
	struct rt_value *dst_val;
	struct rt_value *dict_val;
	struct rt_value *subscr_val;
	struct rt_value key;
	size_t index;

	dst_val = &env->frame->tmpvar[dst];
	dict_val = &env->frame->tmpvar[dict];
	subscr_val = &env->frame->tmpvar[subscr];

	if (dict_val->type != NOCT_VALUE_DICT) {
		rt_error(env, N_TR("Not a dictionary."));
		return false;
	}
	if (subscr_val->type == NOCT_VALUE_INT) {
		index = (size_t)(uint32_t)subscr_val->val.i;
	} else if (subscr_val->type == NOCT_VALUE_LONG) {
		index = (size_t)(uint64_t)subscr_val->val.l;
	} else {
		rt_error(env, N_TR("Subscript not an integer."));
		return false;
	}

	/* Load the element. */
	if (!rt_get_dict_by_index(env, dict_val, index, &key, dst_val))
		return false;

	return true;
}

/*
 * LOADSYMBOL helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_loadsymbol_helper(
	NoctEnv *env,
	int dst,
	const char *symbol,
	uint32_t symbol_len,
	uint32_t symbol_hash)
{
	struct rt_value val;

	if (!rt_get_global_with_hash(env, symbol, symbol_len, symbol_hash, &val))
		return false;

	env->frame->tmpvar[dst] = val;

	return true;
}

/*
 * STORESYMBOL helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_storesymbol_helper(
	NoctEnv *env,
	const char *symbol,
	uint32_t symbol_len,
	uint32_t symbol_hash,
	int src)
{
	if (!rt_set_global_with_hash(env, symbol, symbol_len, symbol_hash, &env->frame->tmpvar[src]))
		return false;

	return true;
}

/*
 * LOADDOT helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_loaddot_helper(
	NoctEnv *env,
	int dst,
	int dict,
	const char *field,
	uint32_t field_len,
	uint32_t field_hash)
{
	if (env->frame->tmpvar[dict].type != NOCT_VALUE_DICT) {
		rt_error(env, N_TR("Not a dictionary."));
		return false;
	}

	if (!rt_get_dict_elem_with_hash(env,
					&env->frame->tmpvar[dict],
					field,
					field_len,
					field_hash,
					&env->frame->tmpvar[dst]))
		return false;

	return true;
}

/*
 * STOREDOT helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_storedot_helper(
	NoctEnv *env,
	int dict,
	const char *field,
	uint32_t field_len,
	uint32_t field_hash,
	int src)
{
	struct rt_value *dict_val, *val;

	/* Get the dictionary. */
	dict_val = &env->frame->tmpvar[dict];
	if (dict_val->type != NOCT_VALUE_DICT) {
		rt_error(env, N_TR("Not a dictionary."));
		return false;
	}

	/* Get the source value. */
	val = &env->frame->tmpvar[src];

	/* Store the source value to the dictionary with the key. */
	if (!rt_set_dict_elem_with_hash(env,
					dict_val,
					field,
					field_len,
					field_hash,
					val))
		return false;

	return true;
}

/*
 * CALL helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_call_helper(
	NoctEnv *env,
	int dst,
	int func,
	int arg_count,
	int *arg)
{
	struct rt_value arg_val[NOCT_ARG_MAX];
	struct rt_func *callee;
	struct rt_value ret;
	int i;
	int arg_index;

	/* Get a function. */
	if (env->frame->tmpvar[func].type != NOCT_VALUE_FUNC) {
		rt_error(env, N_TR("Not a function."));
		return false;
	}
	callee = env->frame->tmpvar[func].val.func;

	/*
	 * Pin the argument and result slots.
	 *
	 * These live on the C stack, and rt_call() crosses a safepoint,
	 * so without pinning a collection running in another thread
	 * would move the objects and leave these copies dangling.
	 */
	memset(&ret, 0, sizeof(ret));
	for (i = 0; i < arg_count; i++)
		memset(&arg_val[i], 0, sizeof(arg_val[i]));
	if (!rt_pin_local(env, &ret))
		return false;
	for (i = 0; i < arg_count; i++) {
		if (!rt_pin_local(env, &arg_val[i]))
			return false;
	}

	/* Get values of arguments. */
	for (i = 0; i < arg_count; i++) {
		memcpy(&arg_index, (const unsigned char *)arg +
		       (size_t)i * sizeof(arg_index), sizeof(arg_index));
		arg_val[i] = env->frame->tmpvar[arg_index];
	}

	/* Do call. */
	if (!rt_call(env, callee, (uint32_t)arg_count, &arg_val[0], &ret)) {
		for (i = arg_count - 1; i >= 0; i--)
			rt_unpin_local(env, &arg_val[i]);
		rt_unpin_local(env, &ret);
		return false;
	}

	/* Store a return value. */
	env->frame->tmpvar[dst] = ret;

	for (i = arg_count - 1; i >= 0; i--)
		rt_unpin_local(env, &arg_val[i]);
	rt_unpin_local(env, &ret);

	return true;
}

/*
 * THISCALL helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_thiscall_helper(
	NoctEnv *env,
	int dst,
	int obj,
	const char *name,
	uint32_t name_len,
	uint32_t name_hash,
	int arg_count,
	int *arg)
{
	struct rt_value arg_val[NOCT_ARG_MAX];
	struct rt_func *callee;
	struct rt_value *obj_val;
	struct rt_value ret;
	bool inject_this;
	int call_arg_count;
	int i;
	int arg_index;

	UNUSED_PARAMETER(name);
	UNUSED_PARAMETER(name_len);

	/* Get a receiver object. */
	obj_val = &env->frame->tmpvar[obj];

	/* name_hash is the pre-resolved callee tmpvar for the new bytecode
	 * layout.  The legacy argument slots are retained in this C ABI so
	 * existing AOT/JIT call sequences need only change their decoder. */
	if (env->frame->tmpvar[name_hash].type != NOCT_VALUE_FUNC) {
		rt_error(env, N_TR("Not a function."));
		return false;
	}
	callee = env->frame->tmpvar[name_hash].val.func;
	inject_this = callee->param_count > 0 &&
		callee->param_name[0] != NULL &&
		strcmp(callee->param_name[0], "this") == 0;
	call_arg_count = arg_count + (inject_this ? 1 : 0);
	if (call_arg_count > NOCT_ARG_MAX) {
		rt_error(env, N_TR("Too many parameters."));
		return false;
	}

	/*
	 * Pin the argument and result slots.
	 *
	 * These live on the C stack, and rt_call() crosses a safepoint,
	 * so without pinning a collection running in another thread
	 * would move the objects and leave these copies dangling.
	 */
	memset(&ret, 0, sizeof(ret));
	for (i = 0; i < call_arg_count; i++)
		memset(&arg_val[i], 0, sizeof(arg_val[i]));
	if (!rt_pin_local(env, &ret))
		return false;
	for (i = 0; i < call_arg_count; i++) {
		if (!rt_pin_local(env, &arg_val[i])) {
			rt_unpin_local(env, &ret);
			return false;
		}
	}

	/* Get values of arguments. */
	if (inject_this)
		arg_val[0] = *obj_val;
	for (i = 0; i < arg_count; i++) {
		memcpy(&arg_index, (const unsigned char *)arg +
		       (size_t)i * sizeof(arg_index), sizeof(arg_index));
		arg_val[i + (inject_this ? 1 : 0)] =
			env->frame->tmpvar[arg_index];
	}

	/* Do call. */
	if (!rt_call(env, callee, (uint32_t)call_arg_count, &arg_val[0], &ret)) {
		for (i = call_arg_count - 1; i >= 0; i--)
			rt_unpin_local(env, &arg_val[i]);
		rt_unpin_local(env, &ret);
		return false;
	}

	/* Store a return value. */
	env->frame->tmpvar[dst] = ret;

	for (i = call_arg_count - 1; i >= 0; i--)
		rt_unpin_local(env, &arg_val[i]);
	rt_unpin_local(env, &ret);

	return true;
}

/*
 * SAFEPOINT helper.
 */
NOCT_DLL
bool
CDECL
noct_ex_safepoint_helper(
	NoctEnv *env)
{
	/* Do call. */
	if (!rt_safepoint(env))
		return false;

	return true;
}

/*
 * PBASE helper. (ABCE: materialize a packed payload address.)
 *
 * The result is a long value holding the raw payload pointer.  It is
 * only valid until the next safepoint source; the ABCE pass
 * guarantees by construction that no such source exists between the
 * PBASE and its last use.  See docs/design/01-abce.md.
 */
NOCT_DLL
bool
CDECL
noct_ex_pbase_helper(
	NoctEnv *env,
	int dst,
	int src)
{
	struct rt_value *src_val;
	struct rt_value *dst_val;

	src_val = &env->frame->tmpvar[src];
	dst_val = &env->frame->tmpvar[dst];

	/* Belt and braces: the guard has already proven this. */
	if (src_val->type != NOCT_VALUE_PACKED) {
		rt_error(env, N_TR("Value is not a packed."));
		return false;
	}
	if (src_val->val.packed->packed_buffer == NULL) {
		rt_error(env, N_TR("Packed is unmapped."));
		return false;
	}

	dst_val->type = NOCT_VALUE_LONG;
	dst_val->val.l = (int64_t)(intptr_t)src_val->val.packed->packed_buffer;

	return true;
}

/*
 * PCHECK helper. (ABCE guard: packed type test, never errors.)
 */
NOCT_DLL
bool
CDECL
noct_ex_pcheck_helper(
	NoctEnv *env,
	int dst,
	int src,
	int packed_type)
{
	struct rt_value *src_val;
	struct rt_value *dst_val;
	int result;

	src_val = &env->frame->tmpvar[src];
	dst_val = &env->frame->tmpvar[dst];

	result = 0;
	if (src_val->type == NOCT_VALUE_PACKED &&
	    src_val->val.packed->packed_buffer != NULL &&
	    src_val->val.packed->type == packed_type)
		result = 1;

	dst_val->type = NOCT_VALUE_INT;
	dst_val->val.i = result;

	return true;
}

/*
 * TYPEIS helper. (ABCE guard: value type test, never errors.)
 */
NOCT_DLL
bool
CDECL
noct_ex_typeis_helper(
	NoctEnv *env,
	int dst,
	int src,
	int value_type)
{
	struct rt_value *src_val;
	struct rt_value *dst_val;
	int result;

	src_val = &env->frame->tmpvar[src];
	dst_val = &env->frame->tmpvar[dst];

	result = (src_val->type == value_type) ? 1 : 0;

	dst_val->type = NOCT_VALUE_INT;
	dst_val->val.i = result;

	return true;
}

/*
 * PLEN helper. (ABCE guard: packed element count, never errors.)
 */
NOCT_DLL
bool
CDECL
noct_ex_plen_helper(
	NoctEnv *env,
	int dst,
	int src)
{
	struct rt_value *src_val;
	struct rt_value *dst_val;

	src_val = &env->frame->tmpvar[src];
	dst_val = &env->frame->tmpvar[dst];

	dst_val->type = NOCT_VALUE_INT;
	if (src_val->type == NOCT_VALUE_PACKED)
		dst_val->val.i = (int)src_val->val.packed->elem_size;
	else
		dst_val->val.i = 0;

	return true;
}

/*
 * PLOAD8U helper. (ABCE fast body: raw uint8 load, no checks.)
 */
NOCT_DLL
bool
CDECL
noct_ex_pload8u_helper(
	NoctEnv *env,
	int dst,
	int base,
	int ofs)
{
	struct rt_value *base_val;
	struct rt_value *ofs_val;
	struct rt_value *dst_val;
	const uint8_t *p;

	base_val = &env->frame->tmpvar[base];
	ofs_val = &env->frame->tmpvar[ofs];
	dst_val = &env->frame->tmpvar[dst];

	p = (const uint8_t *)(intptr_t)base_val->val.l;
	if (ofs_val->type == NOCT_VALUE_LONG)
		dst_val->val.i = (int)p[(intptr_t)ofs_val->val.l];
	else
		dst_val->val.i = (int)p[ofs_val->val.i];
	dst_val->type = NOCT_VALUE_INT;

	return true;
}

/*
 * PSTORE8 helper. (ABCE fast body: raw uint8 store, no checks.)
 */
NOCT_DLL
bool
CDECL
noct_ex_pstore8_helper(
	NoctEnv *env,
	int base,
	int ofs,
	int src)
{
	struct rt_value *base_val;
	struct rt_value *ofs_val;
	struct rt_value *src_val;
	uint8_t *p;

	base_val = &env->frame->tmpvar[base];
	ofs_val = &env->frame->tmpvar[ofs];
	src_val = &env->frame->tmpvar[src];

	p = (uint8_t *)(intptr_t)base_val->val.l;
	if (ofs_val->type == NOCT_VALUE_LONG)
		p[(intptr_t)ofs_val->val.l] = (uint8_t)src_val->val.i;
	else
		p[ofs_val->val.i] = (uint8_t)src_val->val.i;

	return true;
}

/*
 * Width-parameterized ABCE load/store helpers.  Offsets are ELEMENT
 * indices; semantics mirror rt_get_packed_elem / rt_set_packed_elem.
 */

#define ABCE_OFS(v) (((v)->type == NOCT_VALUE_LONG) ? (intptr_t)(v)->val.l : (intptr_t)(v)->val.i)

NOCT_DLL
bool
CDECL
noct_ex_pload8s_helper(
	NoctEnv *env,
	int dst,
	int base,
	int ofs)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	struct rt_value *d = &env->frame->tmpvar[dst];

	d->val.i = (int)*((const int8_t *)(intptr_t)b->val.l + ABCE_OFS(o));
	d->type = NOCT_VALUE_INT;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_pload16u_helper(
	NoctEnv *env,
	int dst,
	int base,
	int ofs)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	struct rt_value *d = &env->frame->tmpvar[dst];

	d->val.i = (int)*((const uint16_t *)(intptr_t)b->val.l + ABCE_OFS(o));
	d->type = NOCT_VALUE_INT;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_pload16s_helper(
	NoctEnv *env,
	int dst,
	int base,
	int ofs)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	struct rt_value *d = &env->frame->tmpvar[dst];

	d->val.i = (int)*((const int16_t *)(intptr_t)b->val.l + ABCE_OFS(o));
	d->type = NOCT_VALUE_INT;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_pload32_helper(
	NoctEnv *env,
	int dst,
	int base,
	int ofs)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	struct rt_value *d = &env->frame->tmpvar[dst];

	d->val.i = (int)*((const int32_t *)(intptr_t)b->val.l + ABCE_OFS(o));
	d->type = NOCT_VALUE_INT;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_pload64_helper(
	NoctEnv *env,
	int dst,
	int base,
	int ofs)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	struct rt_value *d = &env->frame->tmpvar[dst];

	d->val.l = *((const int64_t *)(intptr_t)b->val.l + ABCE_OFS(o));
	d->type = NOCT_VALUE_LONG;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_pstore16_helper(
	NoctEnv *env,
	int base,
	int ofs,
	int src)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	struct rt_value *s = &env->frame->tmpvar[src];
	uint16_t v;

	if (s->type == NOCT_VALUE_LONG)
		v = (uint16_t)s->val.l;
	else
		v = (uint16_t)s->val.i;
	*((uint16_t *)(intptr_t)b->val.l + ABCE_OFS(o)) = v;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_pstore32_helper(
	NoctEnv *env,
	int base,
	int ofs,
	int src)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	struct rt_value *s = &env->frame->tmpvar[src];
	uint32_t v;

	if (s->type == NOCT_VALUE_LONG)
		v = (uint32_t)s->val.l;
	else
		v = (uint32_t)s->val.i;
	*((uint32_t *)(intptr_t)b->val.l + ABCE_OFS(o)) = v;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_pstore64_helper(
	NoctEnv *env,
	int base,
	int ofs,
	int src)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	struct rt_value *s = &env->frame->tmpvar[src];
	int64_t v;

	if (s->type == NOCT_VALUE_LONG)
		v = s->val.l;
	else
		v = (int64_t)s->val.i;
	*((int64_t *)(intptr_t)b->val.l + ABCE_OFS(o)) = v;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_ploadf32_helper(
	NoctEnv *env,
	int dst,
	int base,
	int ofs)
{
	struct rt_value *d = &env->frame->tmpvar[dst];
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	const char *p;

	p = (const char *)(intptr_t)b->val.l + (int64_t)ABCE_OFS(o) * 4;
	memcpy(&d->val.f, p, sizeof(float));
	d->type = NOCT_VALUE_FLOAT;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_pstoref32_helper(
	NoctEnv *env,
	int base,
	int ofs,
	int src)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	struct rt_value *s = &env->frame->tmpvar[src];
	char *p;

	p = (char *)(intptr_t)b->val.l + (int64_t)ABCE_OFS(o) * 4;
	memcpy(p, &s->val.f, sizeof(float));
	return true;
}

/*
 * CHECKTYPE helper. (Type annotation entry check.)
 */
NOCT_DLL
bool
CDECL
noct_ex_checktype_helper(
	NoctEnv *env,
	int slot,
	int value_type)
{
	struct rt_value *val;
	const char *type_name;
	int packed_type;
	bool is_return;
	bool restricted;

	val = &env->frame->tmpvar[slot];
	is_return = (value_type & TYPECHECK_RETURN_FLAG) != 0;
	value_type &= ~TYPECHECK_RETURN_FLAG;
	packed_type = -1;
	restricted = false;
	if (value_type >= TYPECHECK_PACKED_BASE &&
	    value_type < TYPECHECK_PACKED_BASE + NOCT_PACKED_ANY) {
		packed_type = value_type - TYPECHECK_PACKED_BASE;
	} else if (value_type >= TYPECHECK_RPACKED_BASE &&
		   value_type < TYPECHECK_RPACKED_BASE + NOCT_PACKED_ANY) {
		packed_type = value_type - TYPECHECK_RPACKED_BASE;
		restricted = true;
	}
	if (packed_type >= 0) {
		static const char *const packed_name[] = {
			"packedint8", "packeduint8",
			"packedint16", "packeduint16",
			"packedint32", "packeduint32",
			"packedint64", "packeduint64",
			"packedfloat", "packeddouble"
		};
		static const char *const rpacked_name[] = {
			"rpackedint8", "rpackeduint8",
			"rpackedint16", "rpackeduint16",
			"rpackedint32", "rpackeduint32",
			"rpackedint64", "rpackeduint64",
			"rpackedfloat", "rpackeddouble"
		};

		if (val->type == NOCT_VALUE_PACKED &&
		    val->val.packed->type == packed_type)
			return true;
		type_name = restricted ? rpacked_name[packed_type] :
			packed_name[packed_type];
		rt_error(env, is_return ?
			 N_TR("%s(): return type mismatch (expected %s).") :
			 N_TR("%s(): argument type mismatch (expected %s)."),
			 env->frame->func != NULL ? env->frame->func->name : "?",
			 type_name);
		return false;
	}
	if (val->type == value_type)
		return true;

	/* Allow harmless widenings: int literals are int-tagged and
	   floating literals are float-tagged, so a long/double annotation
	   must accept them. */
	if (!is_return && value_type == NOCT_VALUE_LONG &&
	    val->type == NOCT_VALUE_INT)
		return true;
	if (!is_return && value_type == NOCT_VALUE_DOUBLE &&
	    val->type == NOCT_VALUE_FLOAT)
		return true;

	switch (value_type) {
	case NOCT_VALUE_INT:	type_name = "int";	break;
	case NOCT_VALUE_LONG:	type_name = "long";	break;
	case NOCT_VALUE_FLOAT:	type_name = "float";	break;
	case NOCT_VALUE_DOUBLE:	type_name = "double";	break;
	case NOCT_VALUE_STRING:	type_name = "string";	break;
	case NOCT_VALUE_ARRAY:	type_name = "array";	break;
	case NOCT_VALUE_DICT:	type_name = "dict";	break;
	case NOCT_VALUE_PACKED:	type_name = "packed";	break;
	case NOCT_VALUE_FUNC:	type_name = "func";	break;
	default:		type_name = "unknown";	break;
	}

	rt_error(env, is_return ?
		 N_TR("%s(): return type mismatch (expected %s).") :
		 N_TR("%s(): argument type mismatch (expected %s)."),
		 env->frame->func != NULL ? env->frame->func->name : "?",
		 type_name);
	return false;
}

/* Evaluate a condition without depending on the target's word order. */
NOCT_DLL
int
CDECL
noct_ex_condition_helper(
	NoctEnv *env,
	int slot)
{
	struct rt_value *v;

	v = &env->frame->tmpvar[slot];
	switch (v->type) {
	case NOCT_VALUE_INT:    return v->val.i != 0;
	case NOCT_VALUE_LONG:   return v->val.l != 0;
	case NOCT_VALUE_FLOAT:  return v->val.f != 0.0f;
	case NOCT_VALUE_DOUBLE: return v->val.lf != 0.0;
	default:
		rt_error(env, N_TR("Condition is not a number."));
		return -1;
	}
}

/*
 * Typed arithmetic helpers (docs/design/07-typed-ops.md).
 *
 * Dispatch-free: they trust the operand tags (int for i*, float for
 * f*) and are undefined on wrong-typed operands (D-TOP1).  The LIR
 * generator emits the corresponding opcodes only under type proofs.
 * Integer arithmetic is performed in uint32_t (defined wraparound,
 * matching both the generic helpers' shipped behavior and the inline
 * machine code).  Comparisons yield an int-tagged 0/1.
 *
 * These are compiled unconditionally (not gated on the optimizer):
 * precompiled bytecode containing typed ops must run on targets that
 * never optimize, exactly like the PLOAD/PSTORE family above.
 */

#define TYPED_I2(name, expr)						\
NOCT_DLL								\
bool									\
CDECL									\
name(									\
	NoctEnv *env,							\
	int dst,							\
	int src1,							\
	int src2)							\
{									\
	struct rt_value *d = &env->frame->tmpvar[dst];			\
	uint32_t a = (uint32_t)env->frame->tmpvar[src1].val.i;		\
	uint32_t b = (uint32_t)env->frame->tmpvar[src2].val.i;		\
	d->val.i = (int32_t)(expr);					\
	d->type = NOCT_VALUE_INT;					\
	return true;							\
}

TYPED_I2(noct_ex_iadd_helper, a + b)
TYPED_I2(noct_ex_isub_helper, a - b)
TYPED_I2(noct_ex_imul_helper, a * b)
TYPED_I2(noct_ex_iand_helper, a & b)
TYPED_I2(noct_ex_ior_helper,  a | b)
TYPED_I2(noct_ex_ixor_helper, a ^ b)

/* For ISHL/ISHR, src2 is the shift count as an immediate (0..31,
   guaranteed by the emission rule; masked here so a hand-crafted
   bytecode cannot reach C undefined behavior). */
NOCT_DLL
bool
CDECL
noct_ex_ishl_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *d = &env->frame->tmpvar[dst];
	uint32_t a = (uint32_t)env->frame->tmpvar[src1].val.i;

	d->val.i = (int32_t)(a << ((uint32_t)src2 & 31));
	d->type = NOCT_VALUE_INT;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_ishr_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *d = &env->frame->tmpvar[dst];
	uint32_t a = (uint32_t)env->frame->tmpvar[src1].val.i;

	/* LOGICAL shift: matches noct_ex_shr_helper's int semantics. */
	d->val.i = (int32_t)(a >> ((uint32_t)src2 & 31));
	d->type = NOCT_VALUE_INT;
	return true;
}

/*
 * IDIV/IMOD: the emission rule (literal divisor not in {0, -1})
 * makes the checks below unreachable from our compiler, but bytecode
 * files are an external input and a division trap would take down
 * the process, so they stay (defensive; D-TOP5).
 */
NOCT_DLL
bool
CDECL
noct_ex_idiv_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *d = &env->frame->tmpvar[dst];
	int32_t a = (int32_t)env->frame->tmpvar[src1].val.i;
	int32_t b = (int32_t)env->frame->tmpvar[src2].val.i;

	if (b == 0) {
		rt_error(env, N_TR("Division by zero."));
		return false;
	}
	if (b == -1 && a == (-2147483647 - 1)) {
		/* Wraps: -INT_MIN == INT_MIN in two's complement. */
		d->val.i = a;
	} else {
		d->val.i = a / b;
	}
	d->type = NOCT_VALUE_INT;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_imod_helper(
	NoctEnv *env,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *d = &env->frame->tmpvar[dst];
	int32_t a = (int32_t)env->frame->tmpvar[src1].val.i;
	int32_t b = (int32_t)env->frame->tmpvar[src2].val.i;

	if (b == 0) {
		rt_error(env, N_TR("Division by zero."));
		return false;
	}
	if (b == -1 && a == (-2147483647 - 1)) {
		d->val.i = 0;
	} else {
		d->val.i = a % b;
	}
	d->type = NOCT_VALUE_INT;
	return true;
}

#define TYPED_ICMP(name, op)						\
NOCT_DLL								\
bool									\
CDECL									\
name(									\
	NoctEnv *env,							\
	int dst,							\
	int src1,							\
	int src2)							\
{									\
	struct rt_value *d = &env->frame->tmpvar[dst];			\
	int32_t a = (int32_t)env->frame->tmpvar[src1].val.i;		\
	int32_t b = (int32_t)env->frame->tmpvar[src2].val.i;		\
	d->val.i = (a op b) ? 1 : 0;					\
	d->type = NOCT_VALUE_INT;					\
	return true;							\
}

TYPED_ICMP(noct_ex_ilt_helper,  <)
TYPED_ICMP(noct_ex_ilte_helper, <=)
TYPED_ICMP(noct_ex_igt_helper,  >)
TYPED_ICMP(noct_ex_igte_helper, >=)

#define TYPED_F2(name, op)						\
NOCT_DLL								\
bool									\
CDECL									\
name(									\
	NoctEnv *env,							\
	int dst,							\
	int src1,							\
	int src2)							\
{									\
	struct rt_value *d = &env->frame->tmpvar[dst];			\
	float a = env->frame->tmpvar[src1].val.f;			\
	float b = env->frame->tmpvar[src2].val.f;			\
	d->val.f = a op b;						\
	d->type = NOCT_VALUE_FLOAT;					\
	return true;							\
}

TYPED_F2(noct_ex_fadd_helper, +)
TYPED_F2(noct_ex_fsub_helper, -)
TYPED_F2(noct_ex_fmul_helper, *)
/* Division is IEEE-total (07 Part 0): zero divisors yield inf/NaN. */
TYPED_F2(noct_ex_fdiv_helper, /)

/* C comparison semantics: any comparison involving NaN yields 0. */
#define TYPED_FCMP(name, op)						\
NOCT_DLL								\
bool									\
CDECL									\
name(									\
	NoctEnv *env,							\
	int dst,							\
	int src1,							\
	int src2)							\
{									\
	struct rt_value *d = &env->frame->tmpvar[dst];			\
	float a = env->frame->tmpvar[src1].val.f;			\
	float b = env->frame->tmpvar[src2].val.f;			\
	d->val.i = (a op b) ? 1 : 0;					\
	d->type = NOCT_VALUE_INT;					\
	return true;							\
}

TYPED_FCMP(noct_ex_flt_helper,  <)
TYPED_FCMP(noct_ex_flte_helper, <=)
TYPED_FCMP(noct_ex_fgt_helper,  >)
TYPED_FCMP(noct_ex_fgte_helper, >=)

/*
 * 128-bit SIMD helpers (docs/design/06-simd.md).
 *
 * Portable lane-wise emulation over env->vreg[]; the reference
 * semantics for every backend.  The x86_64/arm64 JITs emit inline
 * vector instructions instead; every other backend (and the
 * interpreter) calls these.  Lane order is element memory order, so
 * big-endian ports are self-consistent by construction.  All access
 * goes through memcpy: no alignment requirements anywhere.
 *
 * Operand convention: (env, a, b, c) ints.  For loads/stores a base
 * operand is a TMPVAR INDEX whose slot holds a long payload address
 * (PBASE-derived) and an ofs operand is a tmpvar index whose slot
 * holds an int element index; vreg operands and shift counts arrive
 * as immediate ints.  These helpers trust their inputs exactly like
 * the PLOAD/PSTORE family: the ABCE/SIMD guards proved bounds and
 * types at runtime.
 *
 * Always compiled (precompiled bytecode must run everywhere), C89.
 */

union rt_vlanes {
	uint8_t b[16];
	int32_t i[4];
	uint32_t u[4];
	float f[4];
};

NOCT_DLL
bool
CDECL
noct_ex_vloadi32x4_helper(
	NoctEnv *env,
	int vd,
	int base,
	int ofs)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	const char *p;

	p = (const char *)(intptr_t)b->val.l +
		(int64_t)o->val.i * 4;
	memcpy(env->vreg[vd], p, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vstorei32x4_helper(
	NoctEnv *env,
	int base,
	int ofs,
	int vs)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	char *p;

	p = (char *)(intptr_t)b->val.l +
		(int64_t)o->val.i * 4;
	memcpy(p, env->vreg[vs], 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vsplati32_helper(
	NoctEnv *env,
	int vd,
	int src,
	int unused)
{
	union rt_vlanes x;
	int32_t v = (int32_t)env->frame->tmpvar[src].val.i;

	UNUSED_PARAMETER(unused);
	x.i[0] = v;
	x.i[1] = v;
	x.i[2] = v;
	x.i[3] = v;
	memcpy(env->vreg[vd], &x, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vgetlanei32_helper(
	NoctEnv *env,
	int dst,
	int vs,
	int lane)
{
	struct rt_value *d = &env->frame->tmpvar[dst];
	union rt_vlanes x;

	memcpy(&x, env->vreg[vs], 16);
	d->val.i = x.i[lane & 3];
	d->type = NOCT_VALUE_INT;
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vmov128_helper(
	NoctEnv *env,
	int vd,
	int vs,
	int unused)
{
	UNUSED_PARAMETER(unused);
	memcpy(env->vreg[vd], env->vreg[vs], 16);
	return true;
}

#define RT_VALU(name, expr)						\
NOCT_DLL								\
bool									\
CDECL									\
name(									\
	NoctEnv *env,							\
	int vd,								\
	int va,								\
	int vb)								\
{									\
	union rt_vlanes x;						\
	union rt_vlanes y;						\
	int k;								\
	memcpy(&x, env->vreg[va], 16);					\
	memcpy(&y, env->vreg[vb], 16);					\
	for (k = 0; k < 4; k++)						\
		x.u[k] = (expr);					\
	memcpy(env->vreg[vd], &x, 16);					\
	return true;							\
}

RT_VALU(noct_ex_vaddi32x4_helper, x.u[k] + y.u[k])
RT_VALU(noct_ex_vsubi32x4_helper, x.u[k] - y.u[k])
RT_VALU(noct_ex_vmuli32x4_helper, x.u[k] * y.u[k])
RT_VALU(noct_ex_vand128_helper,   x.u[k] & y.u[k])
RT_VALU(noct_ex_vor128_helper,    x.u[k] | y.u[k])
RT_VALU(noct_ex_vxor128_helper,   x.u[k] ^ y.u[k])

NOCT_DLL
bool
CDECL
noct_ex_vshli32x4_helper(
	NoctEnv *env,
	int vd,
	int va,
	int count)
{
	union rt_vlanes x;
	int k;
	uint32_t c = (uint32_t)count & 31;

	memcpy(&x, env->vreg[va], 16);
	for (k = 0; k < 4; k++)
		x.u[k] = x.u[k] << c;
	memcpy(env->vreg[vd], &x, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vshri32x4_helper(
	NoctEnv *env,
	int vd,
	int va,
	int count)
{
	union rt_vlanes x;
	int k;
	uint32_t c = (uint32_t)count & 31;

	memcpy(&x, env->vreg[va], 16);
	/* LOGICAL: matches the scalar int >> semantics. */
	for (k = 0; k < 4; k++)
		x.u[k] = x.u[k] >> c;
	memcpy(env->vreg[vd], &x, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vloadf32x4_helper(
	NoctEnv *env,
	int vd,
	int base,
	int ofs)
{
	return noct_ex_vloadi32x4_helper(env, vd, base, ofs);
}

NOCT_DLL
bool
CDECL
noct_ex_vstoref32x4_helper(
	NoctEnv *env,
	int base,
	int ofs,
	int vs)
{
	return noct_ex_vstorei32x4_helper(env, base, ofs, vs);
}

NOCT_DLL
bool
CDECL
noct_ex_vsplatf32_helper(
	NoctEnv *env,
	int vd,
	int src,
	int unused)
{
	union rt_vlanes x;
	float v = env->frame->tmpvar[src].val.f;

	UNUSED_PARAMETER(unused);
	x.f[0] = v;
	x.f[1] = v;
	x.f[2] = v;
	x.f[3] = v;
	memcpy(env->vreg[vd], &x, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vgetlanef32_helper(
	NoctEnv *env,
	int dst,
	int vs,
	int lane)
{
	struct rt_value *d = &env->frame->tmpvar[dst];
	union rt_vlanes x;

	memcpy(&x, env->vreg[vs], 16);
	d->val.f = x.f[lane & 3];
	d->type = NOCT_VALUE_FLOAT;
	return true;
}

#define RT_VALF(name, expr) \
NOCT_DLL \
bool \
CDECL \
name( \
	NoctEnv *env, \
	int vd, \
	int va, \
	int vb) \
{ \
	union rt_vlanes x; \
	union rt_vlanes y; \
	int k; \
	memcpy(&x, env->vreg[va], 16); \
	memcpy(&y, env->vreg[vb], 16); \
	for (k = 0; k < 4; k++) \
		x.f[k] = (expr); \
	memcpy(env->vreg[vd], &x, 16); \
	return true; \
}

RT_VALF(noct_ex_vaddf32x4_helper, x.f[k] + y.f[k])
RT_VALF(noct_ex_vsubf32x4_helper, x.f[k] - y.f[k])
RT_VALF(noct_ex_vmulf32x4_helper, x.f[k] * y.f[k])
RT_VALF(noct_ex_vdivf32x4_helper, x.f[k] / y.f[k])

#undef RT_VALF

/*
 * C89 fallback derived from FreeBSD msun s_fmaf.c (also used by musl).
 * Copyright (c) 2005-2011 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * A binary64 value has more than twice binary32's precision.  Computing in
 * double is therefore exact enough except for the binary32 halfway cases;
 * those cases are detected and the low double bit is nudged toward the exact
 * result before the final conversion.  No fenv.h dependency is used because
 * OpenWatcom 1.9 does not provide it; Noct arithmetic uses the default
 * round-to-nearest-even mode.
 */
#if defined(__WATCOMC__) || defined(NOCT_FORCE_SOFT_FMAF)
static float
noct_fmaf32_soft(float x, float y, float z)
{
	volatile double xy;
	volatile double result;
	double err;
	union {
		double f;
		uint64_t i;
	} u;
	int e;
	int neg;

	xy = (double)x * (double)y;
	result = xy + (double)z;
	u.f = result;
	e = (int)((u.i >> 52) & 0x7ffu);
	if ((u.i & (uint64_t)0x1fffffffU) != (uint64_t)0x10000000U ||
	    e == 0x7ff ||
	    (result - xy == (double)z && result - (double)z == xy))
		return (float)result;

	neg = (int)(u.i >> 63);
	if (neg == ((double)z > xy))
		err = xy - result + (double)z;
	else
		err = (double)z - result + xy;
	if (neg == (err < 0.0))
		u.i++;
	else
		u.i--;
	return (float)u.f;
}
#endif

static float
noct_fmaf32(float x, float y, float z)
{
#if defined(__WATCOMC__) || defined(NOCT_FORCE_SOFT_FMAF)
	return noct_fmaf32_soft(x, y, z);
#else
	return fmaf(x, y, z);
#endif
}

NOCT_DLL
bool
CDECL
noct_ex_vfmaf32x4_helper(
	NoctEnv *env,
	int vd,
	int va,
	int packed_vb_vc)
{
	union rt_vlanes a;
	union rt_vlanes b;
	union rt_vlanes c;
	union rt_vlanes d;
	int vb;
	int vc;
	int k;

	vb = (packed_vb_vc >> 8) & 0xff;
	vc = packed_vb_vc & 0xff;
	memcpy(&a, env->vreg[va], 16);
	memcpy(&b, env->vreg[vb], 16);
	memcpy(&c, env->vreg[vc], 16);
	for (k = 0; k < 4; k++)
		d.f[k] = noct_fmaf32(a.f[k], b.f[k], c.f[k]);
	memcpy(env->vreg[vd], &d, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vcvti32f32x4_helper(NoctEnv *env, int vd, int va, int unused)
{
	union rt_vlanes x;
	int k;
	UNUSED_PARAMETER(unused);
	memcpy(&x, env->vreg[va], 16);
	for (k = 0; k < 4; k++)
		x.f[k] = (float)x.i[k];
	memcpy(env->vreg[vd], &x, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vcvtf32i32x4_helper(NoctEnv *env, int vd, int va, int unused)
{
	union rt_vlanes x;
	int k;
	UNUSED_PARAMETER(unused);
	memcpy(&x, env->vreg[va], 16);
	for (k = 0; k < 4; k++)
		x.i[k] = (int32_t)x.f[k];
	memcpy(env->vreg[vd], &x, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vmins32x4_helper(NoctEnv *env, int vd, int va, int vb)
{
	union rt_vlanes a, b, d;
	int k;

	memcpy(&a, env->vreg[va], 16);
	memcpy(&b, env->vreg[vb], 16);
	for (k = 0; k < 4; k++)
		d.i[k] = a.i[k] < b.i[k] ? a.i[k] : b.i[k];
	memcpy(env->vreg[vd], &d, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vmaxs32x4_helper(NoctEnv *env, int vd, int va, int vb)
{
	union rt_vlanes a, b, d;
	int k;

	memcpy(&a, env->vreg[va], 16);
	memcpy(&b, env->vreg[vb], 16);
	for (k = 0; k < 4; k++)
		d.i[k] = a.i[k] > b.i[k] ? a.i[k] : b.i[k];
	memcpy(env->vreg[vd], &d, 16);
	return true;
}

/*
 * OR a replicated byte immediate into each 32-bit lane.  The fourth
 * typed-helper operand packs imm8 in bits 8..15 and shift in bits 0..7;
 * this keeps the portable helper ABI at (env, int, int, int).
 */
NOCT_DLL
bool
CDECL
noct_ex_vori32x4i_helper(
	NoctEnv *env,
	int vd,
	int vs,
	int packed_imm)
{
	union rt_vlanes x;
	uint32_t value;
	int shift;
	int k;

	shift = packed_imm & 0xff;
	value = (uint32_t)((packed_imm >> 8) & 0xff);
	value <<= (uint32_t)shift & 31;
	memcpy(&x, env->vreg[vs], 16);
	for (k = 0; k < 4; k++)
		x.u[k] |= value;
	memcpy(env->vreg[vd], &x, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vcmpi32x4_helper(
	NoctEnv *env,
	int vd,
	int va,
	int packed_vb_pred)
{
	union rt_vlanes a, b, d;
	int vb = (packed_vb_pred >> 8) & 0xff;
	int pred = packed_vb_pred & 0xff;
	int k;

	memcpy(&a, env->vreg[va], 16);
	memcpy(&b, env->vreg[vb], 16);
	for (k = 0; k < 4; k++) {
		bool yes;
		switch (pred) {
		case VCMP_EQ: yes = a.i[k] == b.i[k]; break;
		case VCMP_NE: yes = a.i[k] != b.i[k]; break;
		case VCMP_LT: yes = a.i[k] <  b.i[k]; break;
		case VCMP_LE: yes = a.i[k] <= b.i[k]; break;
		case VCMP_GT: yes = a.i[k] >  b.i[k]; break;
		case VCMP_GE: yes = a.i[k] >= b.i[k]; break;
		default: return false;
		}
		d.u[k] = yes ? UINT32_MAX : 0;
	}
	memcpy(env->vreg[vd], &d, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vcmpf32x4_helper(
	NoctEnv *env,
	int vd,
	int va,
	int packed_vb_pred)
{
	union rt_vlanes a, b, d;
	int vb = (packed_vb_pred >> 8) & 0xff;
	int pred = packed_vb_pred & 0xff;
	int k;

	memcpy(&a, env->vreg[va], 16);
	memcpy(&b, env->vreg[vb], 16);
	for (k = 0; k < 4; k++) {
		bool yes;
		switch (pred) {
		case VCMP_EQ: yes = a.f[k] == b.f[k]; break;
		case VCMP_NE: yes = a.f[k] != b.f[k]; break;
		case VCMP_LT: yes = a.f[k] <  b.f[k]; break;
		case VCMP_LE: yes = a.f[k] <= b.f[k]; break;
		case VCMP_GT: yes = a.f[k] >  b.f[k]; break;
		case VCMP_GE: yes = a.f[k] >= b.f[k]; break;
		default: return false;
		}
		d.u[k] = yes ? UINT32_MAX : 0;
	}
	memcpy(env->vreg[vd], &d, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vselect128_helper(
	NoctEnv *env,
	int vd,
	int vm,
	int packed_vt_vf)
{
	union rt_vlanes m, t, f, d;
	int vt = (packed_vt_vf >> 8) & 0xff;
	int vf = packed_vt_vf & 0xff;
	int k;

	memcpy(&m, env->vreg[vm], 16);
	memcpy(&t, env->vreg[vt], 16);
	memcpy(&f, env->vreg[vf], 16);
	for (k = 0; k < 4; k++)
		d.u[k] = (m.u[k] & t.u[k]) | (~m.u[k] & f.u[k]);
	memcpy(env->vreg[vd], &d, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vmaskstorei32x4_helper(
	NoctEnv *env,
	int base,
	int ofs,
	int packed_vs_vm)
{
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *o = &env->frame->tmpvar[ofs];
	union rt_vlanes value, mask;
	char *p;
	int vs = (packed_vs_vm >> 8) & 0xff;
	int vm = packed_vs_vm & 0xff;
	int k;

	p = (char *)(intptr_t)b->val.l + (int64_t)o->val.i * 4;
	memcpy(&value, env->vreg[vs], 16);
	memcpy(&mask, env->vreg[vm], 16);
	for (k = 0; k < 4; k++) {
		if (mask.u[k] == UINT32_MAX)
			memcpy(p + k * 4, &value.u[k], 4);
	}
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vinductf32x4_helper(
	NoctEnv *env,
	int vd,
	int state,
	int step)
{
	struct rt_value *s = &env->frame->tmpvar[state];
	struct rt_value *d = &env->frame->tmpvar[step];
	union rt_vlanes out;
	float x = s->val.f;
	float delta = d->val.f;
	int k;

	for (k = 0; k < 4; k++) {
		volatile float rounded;
		out.f[k] = x;
		rounded = x + delta;
		x = rounded;
	}
	s->val.f = x;
	memcpy(env->vreg[vd], &out, 16);
	return true;
}

NOCT_DLL
bool
CDECL
noct_ex_vgatheri32x4_checked_helper(
	NoctEnv *env,
	int vd,
	int base,
	int packed_plen_vi)
{
	int plen = (packed_plen_vi >> 8) & 0xffff;
	int vi = packed_plen_vi & 0xff;
	struct rt_value *b = &env->frame->tmpvar[base];
	struct rt_value *n = &env->frame->tmpvar[plen];
	union rt_vlanes index, out;
	char *p = (char *)(intptr_t)b->val.l;
	int k;

	memcpy(&index, env->vreg[vi], 16);
	for (k = 0; k < 4; k++) {
		int32_t j = index.i[k];
		if (j < 0) {
			rt_error(env, N_TR("Subscript is negative."));
			return false;
		}
		if ((uint32_t)j >= (uint32_t)n->val.i) {
			rt_error(env, N_TR("Array index %ld is out-of-range."),
				 (long)j);
			return false;
		}
		memcpy(&out.u[k], p + (size_t)(uint32_t)j * 4, 4);
	}
	memcpy(env->vreg[vd], &out, 16);
	return true;
}
