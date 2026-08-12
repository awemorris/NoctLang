/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* C89-compatible streaming SHA-256. */

#include "sha256.h"

#include <string.h>

#define ROR32(v, n) (((v) >> (n)) | ((v) << (32 - (n))))

static const uint32_t sha256_k[64] = {
	0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
	0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
	0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
	0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
	0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
	0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
	0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
	0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
	0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
	0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
	0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
	0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
	0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
	0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
	0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static uint32_t
load_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void
store_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static void
sha256_transform(struct noct_sha256 *ctx, const uint8_t block[64])
{
	uint32_t w[64];
	uint32_t a, b, c, d, e, f, g, h;
	uint32_t s0, s1, ch, maj, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = load_be32(block + (size_t)i * 4);
	for (i = 16; i < 64; i++) {
		s0 = ROR32(w[i - 15], 7) ^ ROR32(w[i - 15], 18) ^
		     (w[i - 15] >> 3);
		s1 = ROR32(w[i - 2], 17) ^ ROR32(w[i - 2], 19) ^
		     (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2];
	d = ctx->state[3]; e = ctx->state[4]; f = ctx->state[5];
	g = ctx->state[6]; h = ctx->state[7];
	for (i = 0; i < 64; i++) {
		s1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
		ch = (e & f) ^ ((~e) & g);
		t1 = h + s1 + ch + sha256_k[i] + w[i];
		s0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
		maj = (a & b) ^ (a & c) ^ (b & c);
		t2 = s0 + maj;
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}
	ctx->state[0] += a; ctx->state[1] += b;
	ctx->state[2] += c; ctx->state[3] += d;
	ctx->state[4] += e; ctx->state[5] += f;
	ctx->state[6] += g; ctx->state[7] += h;
}

void
noct_sha256_init(struct noct_sha256 *ctx)
{
	ctx->state[0] = 0x6a09e667U; ctx->state[1] = 0xbb67ae85U;
	ctx->state[2] = 0x3c6ef372U; ctx->state[3] = 0xa54ff53aU;
	ctx->state[4] = 0x510e527fU; ctx->state[5] = 0x9b05688cU;
	ctx->state[6] = 0x1f83d9abU; ctx->state[7] = 0x5be0cd19U;
	ctx->bit_count = 0;
	ctx->block_size = 0;
}

void
noct_sha256_update(struct noct_sha256 *ctx, const void *data, size_t size)
{
	const uint8_t *p;
	size_t take;

	p = (const uint8_t *)data;
	while (size != 0) {
		take = 64 - ctx->block_size;
		if (take > size) take = size;
		memcpy(ctx->block + ctx->block_size, p, take);
		ctx->block_size += take;
		p += take;
		size -= take;
		ctx->bit_count += (uint64_t)take * 8U;
		if (ctx->block_size == 64) {
			sha256_transform(ctx, ctx->block);
			ctx->block_size = 0;
		}
	}
}

void
noct_sha256_final(struct noct_sha256 *ctx, uint8_t digest[32])
{
	uint64_t bits;
	int i;

	bits = ctx->bit_count;
	ctx->block[ctx->block_size++] = 0x80;
	if (ctx->block_size > 56) {
		memset(ctx->block + ctx->block_size, 0, 64 - ctx->block_size);
		sha256_transform(ctx, ctx->block);
		ctx->block_size = 0;
	}
	memset(ctx->block + ctx->block_size, 0, 56 - ctx->block_size);
	for (i = 0; i < 8; i++)
		ctx->block[63 - i] = (uint8_t)(bits >> (i * 8));
	sha256_transform(ctx, ctx->block);
	for (i = 0; i < 8; i++)
		store_be32(digest + (size_t)i * 4, ctx->state[i]);
	memset(ctx, 0, sizeof(*ctx));
}

void
noct_sha256_bytes(const void *data, size_t size, uint8_t digest[32])
{
	struct noct_sha256 ctx;
	noct_sha256_init(&ctx);
	noct_sha256_update(&ctx, data, size);
	noct_sha256_final(&ctx, digest);
}

void
noct_sha256_hex(const uint8_t digest[32], char hex[65])
{
	static const char digits[] = "0123456789abcdef";
	int i;
	for (i = 0; i < 32; i++) {
		hex[i * 2] = digits[digest[i] >> 4];
		hex[i * 2 + 1] = digits[digest[i] & 15];
	}
	hex[64] = '\0';
}
