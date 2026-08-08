/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Regex: a small regular expression engine for the Regex.* intrinsics.
 *
 * The dialect is the commonly used POSIX/Java subset:
 *
 *   literals  a ...           (any non-special character)
 *   .                         (any character except newline)
 *   [abc] [a-z] [^a-z]        (character classes; ] literal if first)
 *   \d \D \w \W \s \S         (predefined classes, ASCII + '_' for \w)
 *   \b \B                     (word boundary / non-boundary)
 *   ^ $                       (line anchors; multiline behavior)
 *   ( ... )  (?: ... )        (capturing / non-capturing groups)
 *   a|b                       (alternation)
 *   * + ? {m} {m,} {m,n}      (greedy repetition)
 *   *? +? ?? {m,n}?           (lazy repetition)
 *   \n \t \r \f \\ \. etc.    (escapes; unknown escapes are literal)
 *
 * Matching is leftmost, backtracking, with capture groups 1-9.
 * Positions are in characters (Unicode codepoints), consistent with
 * the String.* intrinsics. Runaway patterns are stopped by a step
 * budget and a recursion depth cap.
 */

#include "regex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compiled program limits. */
#define RX_MAX_INSTS	8192
#define RX_MAX_CLASSES	64
#define RX_MAX_RANGES	48
#define RX_MAX_NODES	4096
#define RX_MAX_REPEAT	100
#define RX_STEP_BUDGET	2000000
#define RX_DEPTH_MAX	8000

/* Instructions. */
enum {
	RX_I_CHAR,
	RX_I_ANY,
	RX_I_CLASS,
	RX_I_BOL,
	RX_I_EOL,
	RX_I_EOS,
	RX_I_WB,
	RX_I_NWB,
	RX_I_SPLIT,
	RX_I_JMP,
	RX_I_SAVE,
	RX_I_MATCH
};

struct rx_inst {
	uint8_t op;
	uint32_t cp;		/* RX_I_CHAR */
	int16_t cls;		/* RX_I_CLASS */
	int16_t save;		/* RX_I_SAVE */
	int32_t x, y;		/* RX_I_SPLIT / RX_I_JMP */
};

struct rx_class {
	bool negate;
	int nranges;
	uint32_t lo[RX_MAX_RANGES];
	uint32_t hi[RX_MAX_RANGES];
};

/* Parse tree. */
enum {
	RX_N_EMPTY,
	RX_N_CHAR,
	RX_N_ANY,
	RX_N_CLASS,
	RX_N_CAT,
	RX_N_ALT,
	RX_N_REP,
	RX_N_GROUP,
	RX_N_BOL,
	RX_N_EOL,
	RX_N_WB,
	RX_N_NWB
};

struct rx_node {
	int type;
	uint32_t cp;
	int cls;
	struct rx_node *a, *b;
	int min, max;		/* RX_N_REP; max < 0 means unbounded */
	bool lazy;
	int gidx;		/* RX_N_GROUP */
};

struct rx_prog {
	struct rx_inst ins[RX_MAX_INSTS];
	int ninst;
	struct rx_class cls[RX_MAX_CLASSES];
	int ncls;
	int ngroups;		/* highest capture index used */
};

/* Compiler state. */
struct rx_comp {
	const uint32_t *pat;
	int patlen;
	int pos;
	struct rx_prog *prog;
	struct rx_node nodes[RX_MAX_NODES];
	int nnodes;
	int ngroups;
	char *errbuf;
	size_t errsize;
	bool failed;
};

/* Execution state. */
struct rx_exec {
	const struct rx_prog *prog;
	const uint32_t *str;
	int len;
	int budget;
	bool overflow;
};

/*
 * UTF-8 decoding
 */

int
noct_rx_utf8_len(
	const char *s)
{
	int n;

	n = 0;
	while (*s != '\0') {
		unsigned char c = (unsigned char)*s;
		if (c < 0x80)
			s += 1;
		else if (c < 0xE0)
			s += 2;
		else if (c < 0xF0)
			s += 3;
		else
			s += 4;
		n++;
	}
	return n;
}

void
noct_rx_utf8_decode(
	const char *s,
	uint32_t *out)
{
	while (*s != '\0') {
		unsigned char c = (unsigned char)*s;
		uint32_t cp;
		int n;

		if (c < 0x80) {
			cp = c;
			n = 1;
		} else if (c < 0xE0) {
			cp = c & 0x1F;
			n = 2;
		} else if (c < 0xF0) {
			cp = c & 0x0F;
			n = 3;
		} else {
			cp = c & 0x07;
			n = 4;
		}
		s++;
		while (n > 1) {
			cp = (cp << 6) | ((unsigned char)*s & 0x3F);
			s++;
			n--;
		}
		*out++ = cp;
	}
}

int
noct_rx_utf8_encode(
	uint32_t cp,
	char *out)
{
	if (cp < 0x80) {
		out[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800) {
		out[0] = (char)(0xC0 | (cp >> 6));
		out[1] = (char)(0x80 | (cp & 0x3F));
		return 2;
	}
	if (cp < 0x10000) {
		out[0] = (char)(0xE0 | (cp >> 12));
		out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
		out[2] = (char)(0x80 | (cp & 0x3F));
		return 3;
	}
	out[0] = (char)(0xF0 | (cp >> 18));
	out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
	out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
	out[3] = (char)(0x80 | (cp & 0x3F));
	return 4;
}

/*
 * Parser
 */

static void
rx_error(
	struct rx_comp *c,
	const char *msg)
{
	if (!c->failed && c->errbuf != NULL)
		snprintf(c->errbuf, c->errsize, "%s", msg);
	c->failed = true;
}

static struct rx_node *
rx_new_node(
	struct rx_comp *c,
	int type)
{
	struct rx_node *n;

	if (c->nnodes >= RX_MAX_NODES) {
		rx_error(c, "Regex too large.");
		return &c->nodes[0];
	}
	n = &c->nodes[c->nnodes++];
	memset(n, 0, sizeof(*n));
	n->type = type;
	return n;
}

static uint32_t
rx_peek(
	struct rx_comp *c)
{
	if (c->pos >= c->patlen)
		return 0;
	return c->pat[c->pos];
}

static uint32_t
rx_next(
	struct rx_comp *c)
{
	if (c->pos >= c->patlen)
		return 0;
	return c->pat[c->pos++];
}

static bool
rx_is_word_cp(
	uint32_t cp)
{
	if (cp >= '0' && cp <= '9')
		return true;
	if (cp >= 'A' && cp <= 'Z')
		return true;
	if (cp >= 'a' && cp <= 'z')
		return true;
	if (cp == '_')
		return true;
	return false;
}

static int
rx_class_add(
	struct rx_class *cl,
	uint32_t lo,
	uint32_t hi)
{
	if (cl->nranges >= RX_MAX_RANGES)
		return -1;
	cl->lo[cl->nranges] = lo;
	cl->hi[cl->nranges] = hi;
	cl->nranges++;
	return 0;
}

/* Add the ranges of a predefined class (\d, \w, \s). */
static void
rx_class_add_predef(
	struct rx_class *cl,
	uint32_t esc)
{
	switch (esc) {
	case 'd':
		rx_class_add(cl, '0', '9');
		break;
	case 'w':
		rx_class_add(cl, '0', '9');
		rx_class_add(cl, 'A', 'Z');
		rx_class_add(cl, 'a', 'z');
		rx_class_add(cl, '_', '_');
		break;
	case 's':
		rx_class_add(cl, ' ', ' ');
		rx_class_add(cl, '\t', '\t');
		rx_class_add(cl, '\n', '\n');
		rx_class_add(cl, '\r', '\r');
		rx_class_add(cl, '\f', '\f');
		rx_class_add(cl, 0x0B, 0x0B);
		break;
	default:
		break;
	}
}

/* Map a control escape to its character, or return the char itself. */
static uint32_t
rx_escape_char(
	uint32_t esc)
{
	switch (esc) {
	case 'n': return '\n';
	case 't': return '\t';
	case 'r': return '\r';
	case 'f': return '\f';
	case '0': return '\0';
	default: return esc;
	}
}

/* Allocate a class slot in the program. */
static int
rx_alloc_class(
	struct rx_comp *c)
{
	if (c->prog->ncls >= RX_MAX_CLASSES) {
		rx_error(c, "Too many character classes.");
		return 0;
	}
	memset(&c->prog->cls[c->prog->ncls], 0, sizeof(struct rx_class));
	return c->prog->ncls++;
}

/* Build a node for a predefined class escape, or NULL if not one. */
static struct rx_node *
rx_predef_node(
	struct rx_comp *c,
	uint32_t esc)
{
	struct rx_node *n;
	struct rx_class *cl;
	int idx;

	if (esc != 'd' && esc != 'D' && esc != 'w' && esc != 'W' &&
	    esc != 's' && esc != 'S')
		return NULL;

	idx = rx_alloc_class(c);
	cl = &c->prog->cls[idx];
	if (esc == 'D' || esc == 'W' || esc == 'S')
		cl->negate = true;
	rx_class_add_predef(cl, (uint32_t)(esc | 0x20));

	n = rx_new_node(c, RX_N_CLASS);
	n->cls = idx;
	return n;
}

/* Parse a [...] character class. The '[' is already consumed. */
static struct rx_node *
rx_parse_class(
	struct rx_comp *c)
{
	struct rx_node *n;
	struct rx_class *cl;
	int idx;
	bool first;

	idx = rx_alloc_class(c);
	cl = &c->prog->cls[idx];

	if (rx_peek(c) == '^') {
		rx_next(c);
		cl->negate = true;
	}

	first = true;
	while (true) {
		uint32_t lo;

		if (c->pos >= c->patlen) {
			rx_error(c, "Unterminated character class.");
			break;
		}
		lo = rx_next(c);
		if (lo == ']' && !first)
			break;
		first = false;

		if (lo == '\\') {
			uint32_t esc = rx_next(c);
			if (esc == 'd' || esc == 'w' || esc == 's') {
				rx_class_add_predef(cl, esc);
				continue;
			}
			lo = rx_escape_char(esc);
		}

		/* Range? */
		if (rx_peek(c) == '-' && c->pos + 1 < c->patlen &&
		    c->pat[c->pos + 1] != ']') {
			uint32_t hi;
			rx_next(c);	/* '-' */
			hi = rx_next(c);
			if (hi == '\\')
				hi = rx_escape_char(rx_next(c));
			if (hi < lo) {
				rx_error(c, "Invalid character range.");
				hi = lo;
			}
			if (rx_class_add(cl, lo, hi) < 0)
				rx_error(c, "Character class too large.");
		} else {
			if (rx_class_add(cl, lo, lo) < 0)
				rx_error(c, "Character class too large.");
		}
	}

	n = rx_new_node(c, RX_N_CLASS);
	n->cls = idx;
	return n;
}

static struct rx_node *rx_parse_alt(struct rx_comp *c);

/* Parse one atom. */
static struct rx_node *
rx_parse_atom(
	struct rx_comp *c)
{
	struct rx_node *n;
	uint32_t cp;

	cp = rx_next(c);
	switch (cp) {
	case '.':
		return rx_new_node(c, RX_N_ANY);
	case '^':
		return rx_new_node(c, RX_N_BOL);
	case '$':
		return rx_new_node(c, RX_N_EOL);
	case '[':
		return rx_parse_class(c);
	case '(':
	{
		bool capture = true;
		int gidx = 0;

		if (rx_peek(c) == '?') {
			/* Only (?: ... ) is supported. */
			rx_next(c);
			if (rx_next(c) != ':') {
				rx_error(c, "Unsupported (?...) construct.");
				return rx_new_node(c, RX_N_EMPTY);
			}
			capture = false;
		}
		if (capture) {
			if (c->ngroups >= 9) {
				rx_error(c, "Too many capture groups.");
				return rx_new_node(c, RX_N_EMPTY);
			}
			gidx = ++c->ngroups;
		}
		n = rx_new_node(c, RX_N_GROUP);
		n->gidx = capture ? gidx : 0;
		n->a = rx_parse_alt(c);
		if (rx_next(c) != ')')
			rx_error(c, "Unmatched parenthesis.");
		return n;
	}
	case ')':
		rx_error(c, "Unmatched parenthesis.");
		return rx_new_node(c, RX_N_EMPTY);
	case '\\':
	{
		uint32_t esc = rx_next(c);
		if (esc == 'b')
			return rx_new_node(c, RX_N_WB);
		if (esc == 'B')
			return rx_new_node(c, RX_N_NWB);
		n = rx_predef_node(c, esc);
		if (n != NULL)
			return n;
		n = rx_new_node(c, RX_N_CHAR);
		n->cp = rx_escape_char(esc);
		return n;
	}
	default:
		n = rx_new_node(c, RX_N_CHAR);
		n->cp = cp;
		return n;
	}
}

/* Parse {m}, {m,}, {m,n}. Returns false if not a valid repeat form. */
static bool
rx_parse_braces(
	struct rx_comp *c,
	int *pmin,
	int *pmax)
{
	int save = c->pos;
	int m = 0, n = 0;
	bool has_m = false, has_n = false, comma = false;

	rx_next(c);	/* '{' */
	while (rx_peek(c) >= '0' && rx_peek(c) <= '9') {
		m = m * 10 + (int)(rx_next(c) - '0');
		has_m = true;
		if (m > RX_MAX_REPEAT)
			break;
	}
	if (rx_peek(c) == ',') {
		rx_next(c);
		comma = true;
		while (rx_peek(c) >= '0' && rx_peek(c) <= '9') {
			n = n * 10 + (int)(rx_next(c) - '0');
			has_n = true;
			if (n > RX_MAX_REPEAT)
				break;
		}
	}
	if (!has_m || rx_peek(c) != '}' || m > RX_MAX_REPEAT ||
	    (has_n && (n > RX_MAX_REPEAT || n < m))) {
		/* Not a repeat: treat '{' as a literal. */
		c->pos = save;
		return false;
	}
	rx_next(c);	/* '}' */
	*pmin = m;
	if (!comma)
		*pmax = m;
	else if (has_n)
		*pmax = n;
	else
		*pmax = -1;
	return true;
}

/* Parse an atom with a possible repetition suffix. */
static struct rx_node *
rx_parse_rep(
	struct rx_comp *c)
{
	struct rx_node *atom, *n;
	int min, max;
	uint32_t cp;

	atom = rx_parse_atom(c);
	cp = rx_peek(c);
	if (cp == '*') {
		min = 0;
		max = -1;
		rx_next(c);
	} else if (cp == '+') {
		min = 1;
		max = -1;
		rx_next(c);
	} else if (cp == '?') {
		min = 0;
		max = 1;
		rx_next(c);
	} else if (cp == '{') {
		if (!rx_parse_braces(c, &min, &max))
			return atom;
	} else {
		return atom;
	}

	n = rx_new_node(c, RX_N_REP);
	n->a = atom;
	n->min = min;
	n->max = max;
	if (rx_peek(c) == '?') {
		rx_next(c);
		n->lazy = true;
	}
	return n;
}

/* Parse a concatenation. */
static struct rx_node *
rx_parse_cat(
	struct rx_comp *c)
{
	struct rx_node *left, *n;

	left = NULL;
	while (c->pos < c->patlen && !c->failed) {
		uint32_t cp = rx_peek(c);
		if (cp == '|' || cp == ')')
			break;
		n = rx_parse_rep(c);
		if (left == NULL) {
			left = n;
		} else {
			struct rx_node *cat = rx_new_node(c, RX_N_CAT);
			cat->a = left;
			cat->b = n;
			left = cat;
		}
	}
	if (left == NULL)
		left = rx_new_node(c, RX_N_EMPTY);
	return left;
}

/* Parse an alternation. */
static struct rx_node *
rx_parse_alt(
	struct rx_comp *c)
{
	struct rx_node *left;

	left = rx_parse_cat(c);
	while (rx_peek(c) == '|' && !c->failed) {
		struct rx_node *alt, *right;
		rx_next(c);
		right = rx_parse_cat(c);
		alt = rx_new_node(c, RX_N_ALT);
		alt->a = left;
		alt->b = right;
		left = alt;
	}
	return left;
}

/*
 * Code generation
 */

static int
rx_emit(
	struct rx_comp *c,
	int op)
{
	struct rx_prog *p = c->prog;

	if (p->ninst >= RX_MAX_INSTS) {
		rx_error(c, "Regex too large.");
		return p->ninst - 1;
	}
	memset(&p->ins[p->ninst], 0, sizeof(struct rx_inst));
	p->ins[p->ninst].op = (uint8_t)op;
	return p->ninst++;
}

static void
rx_gen(
	struct rx_comp *c,
	struct rx_node *n)
{
	struct rx_prog *p = c->prog;
	int i;

	if (c->failed)
		return;

	switch (n->type) {
	case RX_N_EMPTY:
		break;
	case RX_N_CHAR:
		i = rx_emit(c, RX_I_CHAR);
		p->ins[i].cp = n->cp;
		break;
	case RX_N_ANY:
		rx_emit(c, RX_I_ANY);
		break;
	case RX_N_CLASS:
		i = rx_emit(c, RX_I_CLASS);
		p->ins[i].cls = (int16_t)n->cls;
		break;
	case RX_N_BOL:
		rx_emit(c, RX_I_BOL);
		break;
	case RX_N_EOL:
		rx_emit(c, RX_I_EOL);
		break;
	case RX_N_WB:
		rx_emit(c, RX_I_WB);
		break;
	case RX_N_NWB:
		rx_emit(c, RX_I_NWB);
		break;
	case RX_N_CAT:
		rx_gen(c, n->a);
		rx_gen(c, n->b);
		break;
	case RX_N_ALT:
	{
		int sp, jmp;
		sp = rx_emit(c, RX_I_SPLIT);
		rx_gen(c, n->a);
		jmp = rx_emit(c, RX_I_JMP);
		p->ins[sp].x = sp + 1;
		p->ins[sp].y = p->ninst;
		rx_gen(c, n->b);
		p->ins[jmp].x = p->ninst;
		break;
	}
	case RX_N_GROUP:
		if (n->gidx > 0) {
			i = rx_emit(c, RX_I_SAVE);
			p->ins[i].save = (int16_t)(n->gidx * 2);
		}
		rx_gen(c, n->a);
		if (n->gidx > 0) {
			i = rx_emit(c, RX_I_SAVE);
			p->ins[i].save = (int16_t)(n->gidx * 2 + 1);
		}
		break;
	case RX_N_REP:
	{
		int k;

		/* The mandatory part. */
		for (k = 0; k < n->min; k++)
			rx_gen(c, n->a);

		if (n->max < 0) {
			/* Unbounded tail: L: SPLIT body, end; body; JMP L */
			int sp = rx_emit(c, RX_I_SPLIT);
			rx_gen(c, n->a);
			i = rx_emit(c, RX_I_JMP);
			p->ins[i].x = sp;
			if (n->lazy) {
				p->ins[sp].x = p->ninst;
				p->ins[sp].y = sp + 1;
			} else {
				p->ins[sp].x = sp + 1;
				p->ins[sp].y = p->ninst;
			}
		} else {
			/* Bounded tail: a chain of optional copies. */
			int splits[RX_MAX_REPEAT];
			int nsplits = 0;
			for (k = 0; k < n->max - n->min; k++) {
				splits[nsplits++] = rx_emit(c, RX_I_SPLIT);
				rx_gen(c, n->a);
			}
			for (k = 0; k < nsplits; k++) {
				if (n->lazy) {
					p->ins[splits[k]].x = p->ninst;
					p->ins[splits[k]].y = splits[k] + 1;
				} else {
					p->ins[splits[k]].x = splits[k] + 1;
					p->ins[splits[k]].y = p->ninst;
				}
			}
		}
		break;
	}
	default:
		break;
	}
}

/*
 * Compile a pattern. Returns 0 on success, -1 on error.
 */
int
noct_rx_compile(
	struct rx_prog *prog,
	const uint32_t *pat,
	int patlen,
	bool anchor_end,
	char *errbuf,
	size_t errsize)
{
	struct rx_comp *c;
	struct rx_node *root;
	int i;

	c = malloc(sizeof(struct rx_comp));
	if (c == NULL) {
		snprintf(errbuf, errsize, "Out of memory.");
		return -1;
	}
	memset(c, 0, sizeof(*c));
	memset(prog, 0, sizeof(*prog));
	c->pat = pat;
	c->patlen = patlen;
	c->prog = prog;
	c->errbuf = errbuf;
	c->errsize = errsize;

	i = rx_emit(c, RX_I_SAVE);
	prog->ins[i].save = 0;
	root = rx_parse_alt(c);
	if (c->pos < c->patlen && !c->failed)
		rx_error(c, "Unmatched parenthesis.");
	rx_gen(c, root);
	if (anchor_end)
		rx_emit(c, RX_I_EOS);
	i = rx_emit(c, RX_I_SAVE);
	prog->ins[i].save = 1;
	rx_emit(c, RX_I_MATCH);
	prog->ngroups = c->ngroups;

	if (c->failed) {
		free(c);
		return -1;
	}
	free(c);
	return 0;
}

/*
 * Execution
 */

static bool
rx_class_test(
	const struct rx_class *cl,
	uint32_t cp)
{
	int i;
	bool in;

	in = false;
	for (i = 0; i < cl->nranges; i++) {
		if (cp >= cl->lo[i] && cp <= cl->hi[i]) {
			in = true;
			break;
		}
	}
	return cl->negate ? !in : in;
}

/* Returns 1 on match, 0 on failure, -1 on budget/depth overflow. */
static int
rx_run(
	struct rx_exec *e,
	int pc,
	int pos,
	int *saves,
	int depth)
{
	if (depth > RX_DEPTH_MAX) {
		e->overflow = true;
		return -1;
	}

	for (;;) {
		const struct rx_inst *ins;

		if (--e->budget < 0) {
			e->overflow = true;
			return -1;
		}
		ins = &e->prog->ins[pc];
		switch (ins->op) {
		case RX_I_CHAR:
			if (pos < e->len && e->str[pos] == ins->cp) {
				pc++;
				pos++;
				continue;
			}
			return 0;
		case RX_I_ANY:
			if (pos < e->len && e->str[pos] != '\n') {
				pc++;
				pos++;
				continue;
			}
			return 0;
		case RX_I_CLASS:
			if (pos < e->len &&
			    rx_class_test(&e->prog->cls[ins->cls], e->str[pos])) {
				pc++;
				pos++;
				continue;
			}
			return 0;
		case RX_I_BOL:
			if (pos == 0 || e->str[pos - 1] == '\n') {
				pc++;
				continue;
			}
			return 0;
		case RX_I_EOL:
			if (pos == e->len || e->str[pos] == '\n') {
				pc++;
				continue;
			}
			return 0;
		case RX_I_EOS:
			if (pos == e->len) {
				pc++;
				continue;
			}
			return 0;
		case RX_I_WB:
		case RX_I_NWB:
		{
			bool w1, w2, b;
			w1 = pos > 0 && rx_is_word_cp(e->str[pos - 1]);
			w2 = pos < e->len && rx_is_word_cp(e->str[pos]);
			b = (w1 != w2);
			if (b == (ins->op == RX_I_WB)) {
				pc++;
				continue;
			}
			return 0;
		}
		case RX_I_SAVE:
		{
			int old = saves[ins->save];
			int r;
			saves[ins->save] = pos;
			r = rx_run(e, pc + 1, pos, saves, depth + 1);
			if (r != 0)
				return r;
			saves[ins->save] = old;
			return 0;
		}
		case RX_I_SPLIT:
		{
			int r = rx_run(e, ins->x, pos, saves, depth + 1);
			if (r != 0)
				return r;
			pc = ins->y;
			continue;
		}
		case RX_I_JMP:
			pc = ins->x;
			continue;
		case RX_I_MATCH:
			return 1;
		default:
			return 0;
		}
	}
}

/*
 * Search for the leftmost match at or after "from".
 * Returns 1 found, 0 not found, -1 overflow.
 */
int
noct_rx_search(
	const struct rx_prog *prog,
	const uint32_t *str,
	int len,
	int from,
	struct rx_match *m)
{
	struct rx_exec e;
	int saves[20];
	int st, i, r;

	e.prog = prog;
	e.str = str;
	e.len = len;
	e.budget = RX_STEP_BUDGET;
	e.overflow = false;

	for (st = from; st <= len; st++) {
		for (i = 0; i < 20; i++)
			saves[i] = -1;
		r = rx_run(&e, 0, st, saves, 0);
		if (r < 0)
			return -1;
		if (r == 1) {
			m->start = saves[0];
			m->end = saves[1];
			m->ngroups = prog->ngroups;
			for (i = 0; i < 10; i++) {
				m->group_start[i] = saves[i * 2];
				m->group_end[i] = saves[i * 2 + 1];
			}
			return 1;
		}
	}
	return 0;
}

/*
 * The compiled program size, for opaque allocation by callers.
 */
size_t
noct_rx_prog_size(void)
{
	return sizeof(struct rx_prog);
}
