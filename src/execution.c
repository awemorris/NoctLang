/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * Execution Helpers
 */

#include "c89compat.h"
#include "runtime.h"
#include "execution.h"
#include "intrinsics.h"

#include <string.h>
#include <assert.h>

/*
 * Add helper.
 */
SYSVABI
bool
rt_add_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i + src2_val->val.i;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.i + src2_val->val.f;
			break;
		case NOCT_VALUE_STRING:
			if (!noct_make_string_format(rt, dst_val, "%d%s", src1_val->val.i, src2_val->val.str->s))
				return false;
			break;
		default:
			noct_error(rt, _("Value is not a number or a string."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f + (float)src2_val->val.i;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f + src2_val->val.f;
			break;
		case NOCT_VALUE_STRING:
			if (!noct_make_string_format(rt, dst_val, "%f%s", src1_val->val.f, src2_val->val.str->s))
				return false;
			break;
		default:
			noct_error(rt, _("Value is not a number or a string."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (!noct_make_string_format(rt, dst_val, "%s%d", src1_val->val.str->s, src2_val->val.i))
				return false;
			break;
		case NOCT_VALUE_FLOAT:
			if (!noct_make_string_format(rt, dst_val, "%s%f", src1_val->val.str->s, src2_val->val.f))
				return false;
			break;
		case NOCT_VALUE_STRING:
			if (!noct_make_string_format(rt, dst_val, "%s%s", src1_val->val.str->s, src2_val->val.str->s))
				return false;
			break;
		default:
			noct_error(rt, _("Value is not a number or a string."));
			return false;
		}
		break;
	default:
		noct_error(rt, _("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * Subtract helper.
 */
SYSVABI
bool
rt_sub_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i - src2_val->val.i;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.i - src2_val->val.f;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f - (float)src2_val->val.i;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f - src2_val->val.f;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	default:
		noct_error(rt, _("Value is not a number."));
		return false;
	}

	return true;
}

/*
 * Multiply helper.
 */
SYSVABI
bool
rt_mul_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i * src2_val->val.i;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.i * src2_val->val.f;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f * (float)src2_val->val.i;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f * src2_val->val.f;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	default:
		noct_error(rt, _("Value is not a number."));
		return false;
	}

	return true;
}

/*
 * Multiply helper.
 */
SYSVABI
bool
rt_div_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (src2_val->val.i == 0) {
				noct_error(rt, _("Division by zero."));
				return false;
			}
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i / src2_val->val.i;
			break;
		case NOCT_VALUE_FLOAT:
			if (src2_val->val.f == 0) {
				noct_error(rt, _("Division by zero."));
				return false;
			}
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = (float)src1_val->val.i / src2_val->val.f;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			if (src2_val->val.i == 0) {
				noct_error(rt, _("Division by zero."));
				return false;
			}
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f / (float)src2_val->val.i;
			break;
		case NOCT_VALUE_FLOAT:
			if (src2_val->val.f == 0) {
				noct_error(rt, _("Division by zero."));
				return false;
			}
			dst_val->type = NOCT_VALUE_FLOAT;
			dst_val->val.f = src1_val->val.f / src2_val->val.f;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	default:
		noct_error(rt, _("Value is not a number."));
		return false;
	}

	return true;
}

/*
 * Modulo helper.
 */
SYSVABI
bool
rt_mod_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i % src2_val->val.i;
			break;
		default:
			noct_error(rt, _("Value is not an integer."));
			return false;
		}
		break;
	default:
		noct_error(rt, _("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * AND helper.
 */
SYSVABI
bool
rt_and_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i & src2_val->val.i;
			break;
		default:
			noct_error(rt, _("Value is not an integer."));
			return false;
		}
		break;
	default:
		noct_error(rt, _("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * OR helper.
 */
SYSVABI
bool
rt_or_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i | src2_val->val.i;
			break;
		default:
			noct_error(rt, _("Value is not an integer."));
			return false;
		}
		break;
	default:
		noct_error(rt, _("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * XOR helper.
 */
SYSVABI
bool
rt_xor_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = src1_val->val.i ^ src2_val->val.i;
			break;
		default:
			noct_error(rt, _("Value is not an integer."));
			return false;
		}
		break;
	default:
		noct_error(rt, _("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * NEG helper.
 */
SYSVABI
bool
rt_neg_helper(
	struct rt_env *rt,
	int dst,
	int src)
{
	struct rt_value *dst_val;
	struct rt_value *src_val;

	dst_val = &rt->frame->tmpvar[dst];
	src_val = &rt->frame->tmpvar[src];

	switch (src_val->type) {
	case NOCT_VALUE_INT:
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.i = -src_val->val.i;
		break;
	case NOCT_VALUE_FLOAT:
		dst_val->type = NOCT_VALUE_FLOAT;
		dst_val->val.f = -src_val->val.f;
		break;
	default:
		noct_error(rt, _("Value is not a number."));
		return false;
	}

	return true;
}

/*
 * NEG helper.
 */
SYSVABI
bool
rt_not_helper(
	struct rt_env *rt,
	int dst,
	int src)
{
	struct rt_value *dst_val;
	struct rt_value *src_val;

	dst_val = &rt->frame->tmpvar[dst];
	src_val = &rt->frame->tmpvar[src];

	switch (src_val->type) {
	case NOCT_VALUE_INT:
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.i = src_val->val.i == 0 ? 1 : 0;
		break;
	default:
		noct_error(rt, _("Value is not an integer."));
		return false;
	}

	return true;
}

/*
 * LT helper.
 */
SYSVABI
bool
rt_lt_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i < src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i < src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f < (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f < src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->s, src2_val->val.str->s) < 0 ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a string."));
			break;
		}
		break;
	default:
		noct_error(rt, _("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * LTE helper.
 */
SYSVABI
bool
rt_lte_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i <= src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i <= src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f <= (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f <= src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->s, src2_val->val.str->s) <= 0 ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a string."));
			break;
		}
		break;
	default:
		noct_error(rt, _("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * GT helper.
 */
SYSVABI
bool
rt_gt_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i > src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i > src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f > (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f > src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->s, src2_val->val.str->s) > 0 ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a string."));
			break;
		}
		break;
	default:
		noct_error(rt, _("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * GTE helper.
 */
SYSVABI
bool
rt_gte_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i >= src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i >= src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f >= (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f >= src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->s, src2_val->val.str->s) >= 0 ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a string."));
			break;
		}
		break;
	default:
		noct_error(rt, _("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * EQ helper.
 */
SYSVABI
bool
rt_eq_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i == src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i == src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f == (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f == src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->s, src2_val->val.str->s) == 0 ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a string."));
			break;
		}
		break;
	default:
		noct_error(rt, _("Value is not a number or a string."));
		return false;
	}

	return true;
}

/* NEQ helper. */
SYSVABI
bool
rt_neq_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2)
{
	struct rt_value *dst_val;
	struct rt_value *src1_val;
	struct rt_value *src2_val;

	dst_val = &rt->frame->tmpvar[dst];
	src1_val = &rt->frame->tmpvar[src1];
	src2_val = &rt->frame->tmpvar[src2];

	switch (src1_val->type) {
	case NOCT_VALUE_INT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.i != src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = ((float)src1_val->val.i != src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_FLOAT:
		switch (src2_val->type) {
		case NOCT_VALUE_INT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f != (float)src2_val->val.i) ? 1 : 0;
			break;
		case NOCT_VALUE_FLOAT:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = (src1_val->val.f != src2_val->val.f) ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a number."));
			return false;
		}
		break;
	case NOCT_VALUE_STRING:
		switch (src2_val->type) {
		case NOCT_VALUE_STRING:
			dst_val->type = NOCT_VALUE_INT;
			dst_val->val.i = strcmp(src1_val->val.str->s, src2_val->val.str->s) != 0 ? 1 : 0;
			break;
		default:
			noct_error(rt, _("Value is not a string."));
			break;
		}
		break;
	default:
		noct_error(rt, _("Value is not a number or a string."));
		return false;
	}

	return true;
}

/*
 * STOREARRAY helper.
 */
SYSVABI
bool
rt_storearray_helper(
	struct rt_env *rt,
	int arr,
	int subscr,
	int val)
{
	struct rt_value *arr_val;
	struct rt_value *subscr_val;
	struct rt_value *val_val;
	int subscript;
	const char *key;
	bool is_dict;

	arr_val = &rt->frame->tmpvar[arr];
	if (arr_val->type == NOCT_VALUE_ARRAY) {
		is_dict = false;
	} else if (arr_val->type == NOCT_VALUE_DICT) {
		is_dict = true;
	} else {
		noct_error(rt, _("Not an array or a dictionary."));
		return false;
	}

	subscr_val = &rt->frame->tmpvar[subscr];
	if (!is_dict) {
		if (subscr_val->type != NOCT_VALUE_INT) {
			noct_error(rt, _("Subscript not an integer."));
			return false;
		}
		subscript = subscr_val->val.i;
		key = NULL;
	} else {
		if (subscr_val->type != NOCT_VALUE_STRING) {
			noct_error(rt, _("Subscript not a string."));
			return false;
		}
		subscript = -1;
		key = subscr_val->val.str->s;
	}

	val_val = &rt->frame->tmpvar[val];

	/* Store the element. */
	if (!is_dict) {
		if (!noct_set_array_elem(rt, arr_val, subscript, val_val))
			return false;
	} else {
		if (!noct_set_dict_elem(rt, arr_val, key, val_val))
			return false;
	}

	rt_make_deep_reference(rt, val_val);

	return true;
}

/*
 * loadarray helper.
 */
SYSVABI
bool
rt_loadarray_helper(
	struct rt_env *rt,
	int dst,
	int arr,
	int subscr)
{
	struct rt_value *dst_val;
	struct rt_value *arr_val;
	struct rt_value *subscr_val;
	int subscript;
	const char *key;
	bool is_dict;

	dst_val = &rt->frame->tmpvar[dst];
	arr_val = &rt->frame->tmpvar[arr];
	subscr_val = &rt->frame->tmpvar[subscr];

	/* Check the array type. */
	if (arr_val->type == NOCT_VALUE_ARRAY) {
		is_dict = false;
	} else if (arr_val->type == NOCT_VALUE_DICT) {
		is_dict = true;
	} else {
		noct_error(rt, _("Not an array or a dictionary."));
		return false;
	}

	/* Check the subscript type. */
	if (!is_dict) {
		if (subscr_val->type != NOCT_VALUE_INT) {
			noct_error(rt, _("Subscript not an integer."));
			return false;
		}
		subscript = subscr_val->val.i;
		key = NULL;
	} else {
		if (subscr_val->type != NOCT_VALUE_STRING) {
			noct_error(rt, _("Subscript not a string."));
			return false;
		}
		subscript = -1;;
		key = subscr_val->val.str->s;
	}

	/* Load the element. */
	if (!is_dict) {
		if (!noct_get_array_elem(rt, arr_val, subscript, dst_val))
			return false;
	} else {
		if (!noct_get_dict_elem(rt, arr_val, key, dst_val))
			return false;
	}

	return true;
}

/*
 * LEN helper.
 */
SYSVABI
bool
rt_len_helper(
	struct rt_env *rt,
	int dst,
	int src)
{
	struct rt_value *dst_val;
	struct rt_value *src_val;

	dst_val = &rt->frame->tmpvar[dst];
	src_val = &rt->frame->tmpvar[src];

	switch (src_val->type) {
	case NOCT_VALUE_STRING:
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.i = (int)strlen(src_val->val.str->s);
		break;
	case NOCT_VALUE_ARRAY:
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.i = src_val->val.arr->size;
		break;
	case NOCT_VALUE_DICT:
		dst_val->type = NOCT_VALUE_INT;
		dst_val->val.i = src_val->val.dict->size;
		break;
	default:
		noct_error(rt, _("Value is not a string, an array, or a dictionary."));
		return false;
	}

	return true;
}

/* getdictkeybyindex helper. */
SYSVABI
bool
rt_getdictkeybyindex_helper(
	struct rt_env *rt,
	int dst,
	int dict,
	int subscr)
{
	struct rt_value *dst_val;
	struct rt_value *dict_val;
	struct rt_value *subscr_val;

	dst_val = &rt->frame->tmpvar[dst];
	dict_val = &rt->frame->tmpvar[dict];
	subscr_val = &rt->frame->tmpvar[subscr];

	if (dict_val->type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Not a dictionary."));
		return false;
	}
	if (subscr_val->type != NOCT_VALUE_INT) {
		noct_error(rt, _("Subscript not an integer."));
		return false;
	}
	if (subscr_val->val.i >= dict_val->val.dict->size) {
		noct_error(rt, _("Dictionary index out-of-range."));
		return false;
	}

	/* Load the element. */
	if (!noct_make_string(rt, dst_val, dict_val->val.dict->key[subscr_val->val.i]))
		return false;

	return true;
}

/*
 * getdictvalbyindex helper.
 */
SYSVABI
bool
rt_getdictvalbyindex_helper(
	struct rt_env *rt,
	int dst,
	int dict,
	int subscr)
{
	struct rt_value *dst_val;
	struct rt_value *dict_val;
	struct rt_value *subscr_val;

	dst_val = &rt->frame->tmpvar[dst];
	dict_val = &rt->frame->tmpvar[dict];
	subscr_val = &rt->frame->tmpvar[subscr];

	if (dict_val->type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Not a dictionary."));
		return false;
	}
	if (subscr_val->type != NOCT_VALUE_INT) {
		noct_error(rt, _("Subscript not an integer."));
		return false;
	}
	if (subscr_val->val.i >= dict_val->val.dict->size) {
		noct_error(rt, _("Dictionary index out-of-range."));
		return false;
	}

	/* Load the element. */
	*dst_val = dict_val->val.dict->value[subscr_val->val.i];

	return true;
}

/*
 * loadsymbol helper.
 */
SYSVABI
bool
rt_loadsymbol_helper(
	struct rt_env *rt,
	int dst,
	const char *symbol)
{
	struct rt_bindglobal *global;

	/* Search a global variable. */
	if (rt_find_global(rt, symbol, &global)) {
		rt->frame->tmpvar[dst] = global->val;
	} else {
		noct_error(rt, _("Symbol \"%s\" not found."), symbol);
		return false;
	}

	return true;
}

/*
 * storesymbol helper.
 */
SYSVABI
bool
rt_storesymbol_helper(
	struct rt_env *rt,
	const char *symbol,
	int src)
{
	struct rt_bindlocal *local;
	struct rt_bindglobal *global;

	/* Search a global variable. */
	if (rt_find_global(rt, symbol, &global)) {
		/* Found. */
		global->val = rt->frame->tmpvar[src];
		rt_make_deep_reference(rt, &global->val);
	} else {
		/* Not found. Bind a global variable. */
		assert(local == NULL);
		assert(global == NULL);
		if (!rt_add_global(rt, symbol, &global))
			return false;
		global->val = rt->frame->tmpvar[src];
	}

	return true;
}

/*
 * loaddot helper.
 */
SYSVABI
bool
rt_loaddot_helper(
	struct rt_env *rt,
	int dst,
	int dict,
	const char *field)
{
	/* Special field "length". */
	if (strcmp(field, "length") == 0) {
		if (rt->frame->tmpvar[dict].type == NOCT_VALUE_DICT) {
			int size;
			if (!noct_get_dict_size(rt, &rt->frame->tmpvar[dict], &size))
				return false;

			noct_make_int(&rt->frame->tmpvar[dst], size);
			return true;
		} else if (rt->frame->tmpvar[dict].type == NOCT_VALUE_ARRAY) {
			int size;
			if (!noct_get_array_size(rt, &rt->frame->tmpvar[dict], &size))
				return false;

			noct_make_int(&rt->frame->tmpvar[dst], size);
			return true;
		} else if (rt->frame->tmpvar[dict].type == NOCT_VALUE_STRING) {
			noct_make_int(&rt->frame->tmpvar[dst],
				    strlen(rt->frame->tmpvar[dict].val.str->s));
			return true;
		}
	}

	if (rt->frame->tmpvar[dict].type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Not a dictionary."));
		return false;
	}

	if (!noct_get_dict_elem(rt, &rt->frame->tmpvar[dict], field, &rt->frame->tmpvar[dst]))
		return false;

	return true;
}

/*
 * STOREDOT helper.
 */
SYSVABI
bool
rt_storedot_helper(
	struct rt_env *rt,
	int dict,
	const char *field,
	int src)
{
	if (rt->frame->tmpvar[dict].type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Not a dictionary."));
		return false;
	}

	/* Store. */
	if (!noct_set_dict_elem(rt, &rt->frame->tmpvar[dict], field, &rt->frame->tmpvar[src]))
		return false;

	rt_make_deep_reference(rt, &rt->frame->tmpvar[src]);

	return true;
}

/* CALL helper. */
SYSVABI
bool
rt_call_helper(
	struct rt_env *rt,
	int dst,
	int func,
	int arg_count,
	int *arg)
{
	struct rt_value arg_val[NOCT_ARG_MAX];
	struct rt_func *callee;
	struct rt_value ret;
	int i;

	/* Get a function. */
	if (rt->frame->tmpvar[func].type != NOCT_VALUE_FUNC) {
		noct_error(rt, _("Not a function."));
		return false;
	}
	callee = rt->frame->tmpvar[func].val.func;

	/* Get values of arguments. */
	for (i = 0; i < arg_count; i++)
		arg_val[i] = rt->frame->tmpvar[arg[i]];

	/* Make a callframe. */
	if (!rt_enter_frame(rt, callee))
		return false;

	/* Do call. */
	if (!noct_call(rt, callee, arg_count, &arg_val[0], &ret))
		return false;

	/* Destroy a callframe. */
	rt_leave_frame(rt);

	/* Store a return value. */
	rt->frame->tmpvar[dst] = ret;

	return true;
}

/*
 * THISCALL helper.
 */
SYSVABI
bool
rt_thiscall_helper(
	struct rt_env *rt,
	int dst,
	int obj,
	const char *name,
	int arg_count,
	int *arg)
{
	struct rt_value arg_val[NOCT_ARG_MAX];
	struct rt_value callee_value;
	struct rt_func *callee;
	struct rt_value *obj_val;
	struct rt_value ret;
	int i;

	/* Get a receiver object. */
	obj_val = &rt->frame->tmpvar[obj];

	/* Check for an intrinsic. (callee == NULL is not found.)*/
	if (!rt_get_intrin_thiscall_func(rt, name, &callee))
		return false;
	if (callee == NULL) {
		/* If not an intrinsic call, object must be a dictionary. */
		if (rt->frame->tmpvar[obj].type != NOCT_VALUE_DICT) {
			noct_error(rt, _("Not a dictionary."));
			return false;
		}

		/* Get a function from a receiver object. */
		if (!noct_get_dict_elem(rt, &rt->frame->tmpvar[obj], name, &callee_value))
			return false;
		if (callee_value.type != NOCT_VALUE_FUNC) {
			noct_error(rt, _("Not a function."));
			return false;
		}
		callee = callee_value.val.func;
	}

	/* Get values of arguments. */
	arg_count++;
	arg_val[0] = *obj_val;
	for (i = 1; i < arg_count; i++)
		arg_val[i] = rt->frame->tmpvar[arg[i - 1]];

	/* Make a callframe. */
	if (!rt_enter_frame(rt, callee))
		return false;

	/* Do call. */
	if (!noct_call(rt, callee, arg_count, &arg_val[0], &ret))
		return false;

	/* Destroy a callframe. */
	rt_leave_frame(rt);

	/* Store a return value. */
	rt->frame->tmpvar[dst] = ret;

	return true;
}
