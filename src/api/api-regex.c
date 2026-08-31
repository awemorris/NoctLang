/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Regex API and its regular expression engine.
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
 * the String.* APIs. Runaway patterns are stopped by a step
 * budget and a recursion depth cap.
 */

#include <noct/noct.h>

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

/* Parse tree node types. */
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

/* An engine match result. Positions are character indices. */
struct rx_match {
	int start;
	int end;
	int ngroups;
	int group_start[10];
	int group_end[10];
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

/* Dynamically growing replacement buffer. */
struct regex_output {
	uint32_t *codepoint;
	int length;
	int capacity;
};

static const char *regex_search_param[] = {"pat", "s", "from"};
static const char *regex_matches_param[] = {"pat", "s"};
static const char *regex_replace_all_param[] = {"pat", "s", "repl"};

/* Forward declarations. */
static int noct_rx_utf8_len(const char *s);
static void noct_rx_utf8_decode(const char *s, uint32_t *out);
static int noct_rx_utf8_encode(uint32_t cp, char *out);
static void rx_error(struct rx_comp *c, const char *msg);
static struct rx_node *rx_new_node(struct rx_comp *c, int type);
static uint32_t rx_peek(struct rx_comp *c);
static uint32_t rx_next(struct rx_comp *c);
static bool rx_is_word_cp(uint32_t cp);
static int rx_class_add(struct rx_class *cl, uint32_t lo, uint32_t hi);
static void rx_class_add_predef(struct rx_class *cl, uint32_t esc);
static uint32_t rx_escape_char(uint32_t esc);
static int rx_alloc_class(struct rx_comp *c);
static struct rx_node *rx_predef_node(struct rx_comp *c, uint32_t esc);
static struct rx_node *rx_parse_class(struct rx_comp *c);
static struct rx_node *rx_parse_alt(struct rx_comp *c);
static struct rx_node *rx_parse_atom(struct rx_comp *c);
static bool rx_parse_braces(struct rx_comp *c, int *min, int *max);
static struct rx_node *rx_parse_rep(struct rx_comp *c);
static struct rx_node *rx_parse_cat(struct rx_comp *c);
static int rx_emit(struct rx_comp *c, int op);
static void rx_gen(struct rx_comp *c, struct rx_node *n);
static int noct_rx_compile(struct rx_prog *prog, const uint32_t *pat, int patlen, bool anchor_end, char *errbuf, size_t errsize);
static bool rx_class_test(const struct rx_class *cl, uint32_t cp);
static int rx_run(struct rx_exec *e, int pc, int pos, int *saves, int depth);
static int noct_rx_search(const struct rx_prog *prog, const uint32_t *str, int len, int from, struct rx_match *m);
static size_t noct_rx_prog_size(void);
static bool cfunc_Regex_search(NoctEnv *env);
static bool cfunc_Regex_matches(NoctEnv *env);
static bool cfunc_Regex_replaceAll(NoctEnv *env);
static bool regex_register_function(NoctEnv *env, NoctValue *dict, NoctValue *func_value, const char *global_name, const char *field_name, size_t param_count, const char *param[], bool (*cfunc)(NoctEnv *env));
static bool regex_prepare(NoctEnv *env, const char *pat_s, const char *str_s, bool anchor_end, struct rx_prog **prog, uint32_t **str, int *str_len);
static bool regex_append(NoctEnv *env, struct regex_output *output, uint32_t codepoint);

/*
 * Register the "Regex.*" APIs.
 */
NOCT_DLL
bool
noct_register_api_regex(
	NoctEnv *env)
{
	NoctValue dict;
	NoctValue func_value;
	bool success;

	memset(&dict, 0, sizeof(dict));
	memset(&func_value, 0, sizeof(func_value));
	success = false;

	if (!noct_pin_local(env, 2, &dict, &func_value))
		return false;

	if (!noct_make_empty_dict(env, &dict))
		goto cleanup;
	if (!noct_set_global(env, "Regex", &dict))
		goto cleanup;
	if (!regex_register_function(
		env,
		&dict,
		&func_value,
		"Regex.search",
		"search",
		3,
		regex_search_param,
		cfunc_Regex_search))
		goto cleanup;
	if (!regex_register_function(
		env,
		&dict,
		&func_value,
		"Regex.matches",
		"matches",
		2,
		regex_matches_param,
		cfunc_Regex_matches))
		goto cleanup;
	if (!regex_register_function(
		env,
		&dict,
		&func_value,
		"Regex.replaceAll",
		"replaceAll",
		3,
		regex_replace_all_param,
		cfunc_Regex_replaceAll))
		goto cleanup;

	success = true;

cleanup:
	(void)noct_unpin_local(env, 2, &dict, &func_value);

	return success;
}

/*
 * UTF-8 decoding
 */

/* Count Unicode codepoints in a valid UTF-8 string. */
static int
noct_rx_utf8_len(
	const char *s)
{
	int n;

	n = 0;
	/* Count Unicode codepoints in the UTF-8 string. */
	while (*s != '\0') {
		unsigned char c;

		c = (unsigned char)*s;
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

/* Decode a valid UTF-8 string into codepoints. */
static void
noct_rx_utf8_decode(
	const char *s,
	uint32_t *out)
{
	/* Decode every UTF-8 sequence into one codepoint. */
	while (*s != '\0') {
		unsigned char c;
		uint32_t cp;
		int n;

		c = (unsigned char)*s;
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
		/* Consume the continuation bytes of this sequence. */
		while (n > 1) {
			cp = (cp << 6) | ((unsigned char)*s & 0x3F);
			s++;
			n--;
		}
		*out++ = cp;
	}
}

/* Encode one Unicode codepoint as UTF-8. */
static int
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

/* Record the first pattern compilation error. */
static void
rx_error(
	struct rx_comp *c,
	const char *msg)
{
	if (!c->failed && c->errbuf != NULL)
		snprintf(c->errbuf, c->errsize, "%s", msg);
	c->failed = true;
}

/* Allocate and initialize one parse-tree node. */
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

/* Peek at the next pattern codepoint. */
static uint32_t
rx_peek(
	struct rx_comp *c)
{
	if (c->pos >= c->patlen)
		return 0;

	return c->pat[c->pos];
}

/* Consume the next pattern codepoint. */
static uint32_t
rx_next(
	struct rx_comp *c)
{
	if (c->pos >= c->patlen)
		return 0;

	return c->pat[c->pos++];
}

/* Test whether a codepoint belongs to the ASCII word class. */
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

/* Append one inclusive range to a character class. */
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
	/* Add the ranges selected by the predefined class escape. */
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
	/* Translate supported control escapes to codepoints. */
	switch (esc) {
	case 'n':
		return '\n';
	case 't':
		return '\t';
	case 'r':
		return '\r';
	case 'f':
		return '\f';
	case '0':
		return '\0';
	default:
		return esc;
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

	if (esc != 'd' &&
	    esc != 'D' &&
	    esc != 'w' &&
	    esc != 'W' &&
	    esc != 's' &&
	    esc != 'S')
		return NULL;

	idx = rx_alloc_class(c);
	cl = &c->prog->cls[idx];
	if (esc == 'D' ||
	    esc == 'W' ||
	    esc == 'S')
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
	/* Parse entries until the closing bracket. */
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

			if (esc == 'd' ||
			    esc == 'w' ||
			    esc == 's') {
				rx_class_add_predef(cl, esc);
				continue;
			}
			lo = rx_escape_char(esc);
		}

		/* Range? */
		if (rx_peek(c) == '-' &&
		    c->pos + 1 < c->patlen &&
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

/* Parse one atom. */
static struct rx_node *
rx_parse_atom(
	struct rx_comp *c)
{
	struct rx_node *n;
	uint32_t cp;

	cp = rx_next(c);
	/* Parse the atom selected by the next pattern codepoint. */
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
		bool capture;
		int gidx;

		capture = true;
		gidx = 0;

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
		uint32_t esc;

		esc = rx_next(c);
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
	int save;
	int m;
	int n;
	bool has_m;
	bool has_n;
	bool comma;

	save = c->pos;
	m = 0;
	n = 0;
	has_m = false;
	has_n = false;
	comma = false;

	rx_next(c);	/* '{' */
	/* Parse the lower repetition bound. */
	while (rx_peek(c) >= '0' && rx_peek(c) <= '9') {
		m = m * 10 + (int)(rx_next(c) - '0');
		has_m = true;
		if (m > RX_MAX_REPEAT)
			break;
	}
	if (rx_peek(c) == ',') {
		rx_next(c);
		comma = true;
		/* Parse the optional upper repetition bound. */
		while (rx_peek(c) >= '0' && rx_peek(c) <= '9') {
			n = n * 10 + (int)(rx_next(c) - '0');
			has_n = true;
			if (n > RX_MAX_REPEAT)
				break;
		}
	}
	if (!has_m ||
	    rx_peek(c) != '}' ||
	    m > RX_MAX_REPEAT ||
	    (has_n &&
	     (n > RX_MAX_REPEAT ||
	      n < m))) {
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
	/* Combine adjacent atoms into concatenation nodes. */
	while (c->pos < c->patlen && !c->failed) {
		uint32_t cp;

		cp = rx_peek(c);
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
	/* Fold every alternative into the parse tree. */
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

/* Append one initialized instruction to the compiled program. */
static int
rx_emit(
	struct rx_comp *c,
	int op)
{
	struct rx_prog *p;

	p = c->prog;

	if (p->ninst >= RX_MAX_INSTS) {
		rx_error(c, "Regex too large.");
		return p->ninst - 1;
	}
	memset(&p->ins[p->ninst], 0, sizeof(struct rx_inst));
	p->ins[p->ninst].op = (uint8_t)op;
	return p->ninst++;
}

/* Emit instructions for one parse-tree node. */
static void
rx_gen(
	struct rx_comp *c,
	struct rx_node *n)
{
	struct rx_prog *p;
	int i;

	p = c->prog;
	if (c->failed)
		return;

	/* Emit the instruction sequence for this parse-tree node. */
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

			/* Emit one optional copy for each remaining repeat. */
			for (k = 0; k < n->max - n->min; k++) {
				splits[nsplits++] = rx_emit(c, RX_I_SPLIT);
				rx_gen(c, n->a);
			}
			/* Patch the optional branches after emission. */
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

/* Compile a pattern, returning zero on success and -1 on error. */
static int
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

	c = noct_malloc(sizeof(struct rx_comp));
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
		noct_free(c);
		return -1;
	}
	noct_free(c);

	return 0;
}

/*
 * Execution
 */

/* Test one codepoint against a compiled character class. */
static bool
rx_class_test(
	const struct rx_class *cl,
	uint32_t cp)
{
	int i;
	bool in;

	in = false;
	/* Test the codepoint against every range in the class. */
	for (i = 0; i < cl->nranges; i++) {
		if (cp >= cl->lo[i] && cp <= cl->hi[i]) {
			in = true;
			break;
		}
	}

	return cl->negate ? !in : in;
}

/* Return 1 on match, zero on failure, or -1 on budget exhaustion. */
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

	/* Execute instructions until this path accepts or fails. */
	for (;;) {
		const struct rx_inst *ins;

		if (--e->budget < 0) {
			e->overflow = true;
			return -1;
		}
		ins = &e->prog->ins[pc];
		/* Execute the current regular-expression instruction. */
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
			int old;
			int r;

			old = saves[ins->save];
			saves[ins->save] = pos;
			r = rx_run(e, pc + 1, pos, saves, depth + 1);
			if (r != 0)
				return r;
			saves[ins->save] = old;
			return 0;
		}
		case RX_I_SPLIT:
		{
			int r;

			r = rx_run(e, ins->x, pos, saves, depth + 1);
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

/* Search for the leftmost match at or after the requested position. */
static int
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

	/* Try every possible starting position from left to right. */
	for (st = from; st <= len; st++) {
		/* Clear all capture slots for this attempt. */
		for (i = 0; i < 20; i++)
			saves[i] = -1;
		r = rx_run(&e, 0, st, saves, 0);
		if (r < 0)
			return -1;
		if (r == 1) {
			m->start = saves[0];
			m->end = saves[1];
			m->ngroups = prog->ngroups;
			/* Copy the capture slots into the match result. */
			for (i = 0; i < 10; i++) {
				m->group_start[i] = saves[i * 2];
				m->group_end[i] = saves[i * 2 + 1];
			}
			return 1;
		}
	}

	return 0;
}

/* Return the opaque compiled-program allocation size. */
static size_t
noct_rx_prog_size(
	void)
{
	return sizeof(struct rx_prog);
}

/* Register one function in the Regex package dictionary. */
static bool
regex_register_function(
	NoctEnv *env,
	NoctValue *dict,
	NoctValue *func_value,
	const char *global_name,
	const char *field_name,
	size_t param_count,
	const char *param[],
	bool (*cfunc)(NoctEnv *env))
{
	if (!noct_register_cfunc(
		env,
		global_name,
		param_count,
		param,
		cfunc,
		NULL))
		return false;
	if (!noct_get_global(env, global_name, func_value))
		return false;
	if (!noct_set_dict_elem_cstr(env, dict, field_name, func_value))
		return false;

	return true;
}

/* Decode the input string and compile the regular expression. */
static bool
regex_prepare(
	NoctEnv *env,
	const char *pat_s,
	const char *str_s,
	bool anchor_end,
	struct rx_prog **prog,
	uint32_t **str,
	int *str_len)
{
	char errbuf[128];
	uint32_t *pat;
	int pat_len;

	*prog = NULL;
	*str = NULL;
	*str_len = 0;
	pat = NULL;

	*prog = noct_malloc(noct_rx_prog_size());
	if (*prog == NULL) {
		noct_out_of_memory(env);
		return false;
	}

	pat_len = noct_rx_utf8_len(pat_s);
	pat = noct_malloc(sizeof(uint32_t) * (size_t)(pat_len + 1));
	if (pat == NULL) {
		noct_free(*prog);
		*prog = NULL;
		noct_out_of_memory(env);
		return false;
	}
	noct_rx_utf8_decode(pat_s, pat);

	if (noct_rx_compile(
		*prog,
		pat,
		pat_len,
		anchor_end,
		errbuf,
		sizeof(errbuf)) < 0) {
		noct_free(pat);
		noct_free(*prog);
		*prog = NULL;
		noct_error(env, N_TR("Regex error: %s"), errbuf);
		return false;
	}
	noct_free(pat);

	*str_len = noct_rx_utf8_len(str_s);
	*str = noct_malloc(sizeof(uint32_t) * (size_t)(*str_len + 1));
	if (*str == NULL) {
		noct_free(*prog);
		*prog = NULL;
		noct_out_of_memory(env);
		return false;
	}
	noct_rx_utf8_decode(str_s, *str);

	return true;
}

/* Append one codepoint to a replacement buffer. */
static bool
regex_append(
	NoctEnv *env,
	struct regex_output *output,
	uint32_t codepoint)
{
	if (output->length >= output->capacity) {
		uint32_t *new_codepoint;
		int new_capacity;

		if (output->capacity > INT_MAX / 2) {
			noct_out_of_memory(env);
			return false;
		}
		new_capacity = output->capacity * 2;
		new_codepoint = noct_realloc(
			output->codepoint,
			sizeof(uint32_t) * (size_t)new_capacity);
		if (new_codepoint == NULL) {
			noct_out_of_memory(env);
			return false;
		}
		output->codepoint = new_codepoint;
		output->capacity = new_capacity;
	}

	output->codepoint[output->length++] = codepoint;
	return true;
}

/* Implement Regex.search(pat, s, from). */
static bool
cfunc_Regex_search(
	NoctEnv *env)
{
	NoctValue pat;
	NoctValue str;
	NoctValue from;
	NoctValue ret;
	NoctValue groups;
	NoctValue group;
	NoctValue tmp;
	const char *pat_s;
	const char *str_s;
	struct rx_prog *prog;
	uint32_t *codepoint;
	struct rx_match match;
	int from_index;
	int length;
	int result;
	int i;
	bool success;

	memset(&pat, 0, sizeof(pat));
	memset(&str, 0, sizeof(str));
	memset(&from, 0, sizeof(from));
	memset(&ret, 0, sizeof(ret));
	memset(&groups, 0, sizeof(groups));
	memset(&group, 0, sizeof(group));
	memset(&tmp, 0, sizeof(tmp));
	prog = NULL;
	codepoint = NULL;
	success = false;

	if (!noct_pin_local(
		env,
		7,
		&pat,
		&str,
		&from,
		&ret,
		&groups,
		&group,
		&tmp))
		return false;
	if (!noct_get_arg_check_string(env, 0, &pat, &pat_s))
		goto cleanup;
	if (!noct_get_arg_check_string(env, 1, &str, &str_s))
		goto cleanup;
	if (!noct_get_arg_check_int(env, 2, &from, &from_index))
		goto cleanup;
	if (!regex_prepare(
		env,
		pat_s,
		str_s,
		false,
		&prog,
		&codepoint,
		&length))
		goto cleanup;

	if (from_index < 0)
		from_index = 0;
	if (from_index > length)
		from_index = length;

	result = noct_rx_search(prog, codepoint, length, from_index, &match);
	noct_free(codepoint);
	noct_free(prog);
	codepoint = NULL;
	prog = NULL;
	if (result < 0) {
		noct_error(env, N_TR("Regex too complex."));
		goto cleanup;
	}
	if (result == 0) {
		success = noct_set_return_make_int(env, &ret, 0);
		goto cleanup;
	}

	if (!noct_make_empty_dict(env, &ret))
		goto cleanup;
	if (!noct_set_dict_elem_make_int(
		env,
		&ret,
		"start",
		&tmp,
		match.start))
		goto cleanup;
	if (!noct_set_dict_elem_make_int(
		env,
		&ret,
		"end",
		&tmp,
		match.end))
		goto cleanup;
	if (!noct_make_empty_array(env, &groups))
		goto cleanup;

	/* Build one dictionary for each capture group. */
	for (i = 1; i <= match.ngroups; i++) {
		if (!noct_make_empty_dict(env, &group))
			goto cleanup;
		if (!noct_set_dict_elem_make_int(
			env,
			&group,
			"start",
			&tmp,
			match.group_start[i]))
			goto cleanup;
		if (!noct_set_dict_elem_make_int(
			env,
			&group,
			"end",
			&tmp,
			match.group_end[i]))
			goto cleanup;
		if (!noct_set_array_elem(
			env,
			&groups,
			(size_t)(i - 1),
			&group))
			goto cleanup;
	}
	if (!noct_set_dict_elem_cstr(env, &ret, "groups", &groups))
		goto cleanup;
	success = noct_set_return(env, &ret);

cleanup:
	noct_free(codepoint);
	noct_free(prog);
	if (!noct_unpin_local(
		env,
		7,
		&pat,
		&str,
		&from,
		&ret,
		&groups,
		&group,
		&tmp))
		success = false;

	return success;
}

/* Implement Regex.matches(pat, s). */
static bool
cfunc_Regex_matches(
	NoctEnv *env)
{
	NoctValue pat;
	NoctValue str;
	NoctValue ret;
	const char *pat_s;
	const char *str_s;
	struct rx_prog *prog;
	uint32_t *codepoint;
	struct rx_match match;
	int length;
	int result;
	bool success;

	memset(&pat, 0, sizeof(pat));
	memset(&str, 0, sizeof(str));
	memset(&ret, 0, sizeof(ret));
	prog = NULL;
	codepoint = NULL;
	success = false;

	if (!noct_pin_local(env, 3, &pat, &str, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &pat, &pat_s))
		goto cleanup;
	if (!noct_get_arg_check_string(env, 1, &str, &str_s))
		goto cleanup;
	if (!regex_prepare(
		env,
		pat_s,
		str_s,
		true,
		&prog,
		&codepoint,
		&length))
		goto cleanup;

	/* Search from zero; the compiled program also anchors the end. */
	result = noct_rx_search(prog, codepoint, length, 0, &match);
	noct_free(codepoint);
	noct_free(prog);
	codepoint = NULL;
	prog = NULL;
	if (result < 0) {
		noct_error(env, N_TR("Regex too complex."));
		goto cleanup;
	}
	if (result == 1 && match.start != 0)
		result = 0;
	success = noct_set_return_make_int(
		env,
		&ret,
		result == 1 ? 1 : 0);

cleanup:
	noct_free(codepoint);
	noct_free(prog);
	if (!noct_unpin_local(env, 3, &pat, &str, &ret))
		success = false;

	return success;
}

/* Implement Regex.replaceAll(pat, s, repl). */
static bool
cfunc_Regex_replaceAll(
	NoctEnv *env)
{
	NoctValue pat;
	NoctValue str;
	NoctValue repl;
	NoctValue ret;
	const char *pat_s;
	const char *str_s;
	const char *repl_s;
	struct rx_prog *prog;
	uint32_t *codepoint;
	uint32_t *replacement;
	struct regex_output output;
	struct rx_match match;
	char *utf8;
	int length;
	int replacement_length;
	int utf8_length;
	int from;
	int i;
	int result;
	bool success;

	memset(&pat, 0, sizeof(pat));
	memset(&str, 0, sizeof(str));
	memset(&repl, 0, sizeof(repl));
	memset(&ret, 0, sizeof(ret));
	prog = NULL;
	codepoint = NULL;
	replacement = NULL;
	output.codepoint = NULL;
	output.length = 0;
	output.capacity = 0;
	utf8 = NULL;
	success = false;

	if (!noct_pin_local(env, 4, &pat, &str, &repl, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &pat, &pat_s))
		goto cleanup;
	if (!noct_get_arg_check_string(env, 1, &str, &str_s))
		goto cleanup;
	if (!noct_get_arg_check_string(env, 2, &repl, &repl_s))
		goto cleanup;
	if (!regex_prepare(
		env,
		pat_s,
		str_s,
		false,
		&prog,
		&codepoint,
		&length))
		goto cleanup;

	replacement_length = noct_rx_utf8_len(repl_s);
	replacement = noct_malloc(
		sizeof(uint32_t) * (size_t)(replacement_length + 1));
	if (replacement == NULL) {
		noct_out_of_memory(env);
		goto cleanup;
	}
	noct_rx_utf8_decode(repl_s, replacement);

	if (length > INT_MAX - 64) {
		noct_out_of_memory(env);
		goto cleanup;
	}
	output.capacity = length + 64;
	output.codepoint = noct_malloc(
		sizeof(uint32_t) * (size_t)output.capacity);
	if (output.codepoint == NULL) {
		noct_out_of_memory(env);
		goto cleanup;
	}

	from = 0;
	/* Find and replace every non-overlapping match. */
	while (from <= length) {
		result = noct_rx_search(prog, codepoint, length, from, &match);
		if (result < 0) {
			noct_error(env, N_TR("Regex too complex."));
			goto cleanup;
		}
		if (result == 0)
			break;

		/* Copy the text preceding this match. */
		for (i = from; i < match.start; i++) {
			if (!regex_append(env, &output, codepoint[i]))
				goto cleanup;
		}

		/* Expand capture references in the replacement text. */
		for (i = 0; i < replacement_length; i++) {
			if (replacement[i] == '$' &&
			    i + 1 < replacement_length) {
				uint32_t next;

				next = replacement[i + 1];
				if (next == '$') {
					if (!regex_append(env, &output, '$'))
						goto cleanup;
					i++;
					continue;
				}
				if (next >= '0' && next <= '9') {
					int group_index;
					int group_start;
					int group_end;
					int k;

					group_index = (int)(next - '0');
					group_start =
					    match.group_start[group_index];
					group_end = match.group_end[group_index];
					if (group_start >= 0) {
						/* Copy the referenced capture. */
						for (k = group_start;
						     k < group_end; k++) {
							if (!regex_append(
								env,
								&output,
								codepoint[k]))
								goto cleanup;
						}
					}
					i++;
					continue;
				}
			}
			if (!regex_append(env, &output, replacement[i]))
				goto cleanup;
		}

		if (match.end > match.start) {
			from = match.end;
		} else {
			/* Preserve one codepoint after an empty match. */
			if (match.end < length) {
				if (!regex_append(
					env,
					&output,
					codepoint[match.end]))
					goto cleanup;
			}
			from = match.end + 1;
		}
	}

	/* Copy the suffix following the last match. */
	for (i = from; i < length; i++) {
		if (!regex_append(env, &output, codepoint[i]))
			goto cleanup;
	}

	if (output.length > (INT_MAX - 1) / 4) {
		noct_out_of_memory(env);
		goto cleanup;
	}
	utf8 = noct_malloc((size_t)(output.length * 4 + 1));
	if (utf8 == NULL) {
		noct_out_of_memory(env);
		goto cleanup;
	}
	utf8_length = 0;
	/* Encode the replacement result as UTF-8. */
	for (i = 0; i < output.length; i++) {
		utf8_length += noct_rx_utf8_encode(
			output.codepoint[i],
			utf8 + utf8_length);
	}
	utf8[utf8_length] = '\0';
	success = noct_set_return_make_string(env, &ret, utf8);

cleanup:
	noct_free(utf8);
	noct_free(output.codepoint);
	noct_free(replacement);
	noct_free(codepoint);
	noct_free(prog);
	if (!noct_unpin_local(env, 4, &pat, &str, &repl, &ret))
		success = false;

	return success;
}
