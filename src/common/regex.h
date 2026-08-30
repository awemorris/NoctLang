/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Regex: a small regular expression engine for the Regex.* intrinsics.
 */

#ifndef NOCT_REGEX_H
#define NOCT_REGEX_H

#include <noct/c89compat.h>

struct rx_prog;

/* An engine match result. Positions are character indices. */
struct rx_match {
	int start;
	int end;
	int ngroups;
	int group_start[10];
	int group_end[10];
};

/* The compiled program is opaque to callers; allocate via malloc. */
size_t noct_rx_prog_size(void);

int noct_rx_compile(struct rx_prog *prog, const uint32_t *pat, int patlen,
		    bool anchor_end, char *errbuf, size_t errsize);

int noct_rx_search(const struct rx_prog *prog, const uint32_t *str, int len,
		   int from, struct rx_match *m);

/* UTF-8 helpers shared with the intrinsics. */
int noct_rx_utf8_len(const char *s);
void noct_rx_utf8_decode(const char *s, uint32_t *out);
int noct_rx_utf8_encode(uint32_t cp, char *out);

#endif
