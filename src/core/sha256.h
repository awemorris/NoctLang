/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#ifndef NOCT_SHA256_H
#define NOCT_SHA256_H

#include <noct/noct.h>

struct noct_sha256 {
	uint32_t state[8];
	uint64_t bit_count;
	uint8_t block[64];
	size_t block_size;
};

void noct_sha256_init(struct noct_sha256 *ctx);
void noct_sha256_update(struct noct_sha256 *ctx, const void *data, size_t size);
void noct_sha256_final(struct noct_sha256 *ctx, uint8_t digest[32]);
void noct_sha256_bytes(const void *data, size_t size, uint8_t digest[32]);
void noct_sha256_hex(const uint8_t digest[32], char hex[65]);

#endif
