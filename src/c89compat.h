/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * C89 Compatibility
 */

#ifndef NOCT_C89COMPAT_H
#define NOCT_C89COMPAT_H

#include <noct/noct.h>

/*
 * For GCC/Clang
 */
#if defined(__GNUC__)

/* Define a macro that indicates a target architecture. */
#if defined(__i386__) && !defined(__x86_64__)
#define ARCH_X86
#elif defined(__x86_64__)
#define ARCH_X86_64
#elif defined(__aarch64__)
#define ARCH_ARM64
#elif defined(__arm__)
#define ARCH_ARM32
#elif defined(__mips64) || defined(__mips64__)
#define ARCH_MIPS64
#elif defined(__mips__)
#define ARCH_MIPS32
#elif defined(_ARCH_PPC64)
#define ARCH_PPC64
#elif defined(_ARCH_PPC)
#define ARCH_PPC32
#elif defined(__riscv) && (__riscv_xlen == 64)
#define ARCH_RISCV64
#elif defined(__riscv) && (__riscv_xlen == 32)
#define ARCH_RISCV32
#endif

/* Define a macro that indicates a endian. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ARCH_LE
#else
#define ARCH_BE
#endif

/* No pointer aliasing. */
#define RESTRICT			__restrict

/* Suppress unused warnings. */
#define UNUSED_PARAMETER(x)		(void)(x)

/* UTF-8 string literal. */
#define U8(s)				u8##s

/* UTF-32 character literal. */
#define U32C(literal, unicode)		U##literal

/* Secure strcpy() */
#if defined(_WIN32)
#define strlcpy(d, s, l)		strcpy_s(d, l, s)
#define strlcat(d, s, l)		strcat_s(d, l, s)
#endif
#if defined(__linux) && !(defined(__GLIBC__) && (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 38))
#include <stdio.h>
#include <string.h>
inline void strlcpy(char *d, const char *s, size_t len)
{
	snprintf(d, len, "%s", s);
}
inline void strlcat(char *d, const char *s, size_t len)
{
	size_t l = strlen(d);
	if (len > l)
		snprintf(d + l, len - l, "%s", s);
}
#endif

#endif /* End of GCC/Clang */

/*
 * For MSVC
 */
#ifdef _MSC_VER

#if defined(_M_IX86)
#define ARCH_X86
#elif defined(_M_X64)
#define ARCH_X86_64
#elif defined(_M_ARM64)
#define ARCH_ARM64
#endif

#define ARCH_LE

/* Do not get warnings for usages of string.h functions. */
#define _CRT_SECURE_NO_WARNINGS

/* POSIX libc to MSVCRT mapping */
#define strdup _strdup
#define strcasecmp _stricmp

#define RESTRICT			__restrict
#define UNUSED_PARAMETER(x)		(void)(x)
#define U8(s)				u8##s
#define U32C(literal, unicode)		U##literal

#define strlcpy(s, d, l)		strcpy_s(s, l, d)
#define strlcat(s, d, l)		strcat_s(s, l, d)

#endif /* End of MSVC */

/*
 * For Sun Studio
 */
#if defined(__sun) && !defined(__GNUC__)

#if defined(__sparc)
#define ARCH_SPARC
#else
#define ARCH_X86_64
#endif

#define NOCT_INLINE				__inline
#define RESTRICT			__restrict
#define UNUSED_PARAMETER(x)		(void)(x)
#define U8(s)				u8##s
#define U32_C(literal, unicode)		(unicode)

#endif /* End of Sun Studio */

/*
 * Byteorder
 */

#ifdef ARCH_LE
#define HOSTTOLE64(d)	(d)
#define HOSTTOLE32(d)	(d)
#define HOSTTOLE16(d)	(d)
#define HOSTTOBE64(d)	((((d) & 0xff) << 48) | ((((d) >> 8) & 0xff) << 40) | ((((d) >> 16) & 0xff) << 32) | ((((d) >> 24) & 0xff) << 24) | ((((d) >> 32) & 0xff) << 16) | ((((d) >> 40) & 0xff) << 8) | ((((d) >> 48) & 0xff)))
#define HOSTTOBE32(d)	((((d) & 0xff) << 24) | ((((d) >> 8) & 0xff) << 16) | ((((d) >> 16) & 0xff) << 8) | (((d) >> 24) & 0xff))
#define HOSTTOBE16(d)	((((d) & 0xff) << 8) | (((d) >> 8) & 0xff))
#define LETOHOST64(d)	(d)
#define LETOHOST32(d)	(d)
#define LETOHOST16(d)	(d)
#define BETOHOST64(d)	((((d) & 0xff) << 48) | ((((d) >> 8) & 0xff) << 40) | ((((d) >> 16) & 0xff) << 32) | ((((d) >> 24) & 0xff) << 24) | ((((d) >> 32) & 0xff) << 16) | ((((d) >> 40) & 0xff) << 8) | ((((d) >> 48) & 0xff)))
#define BETOHOST32(d)	((((d) & 0xff) << 24) | ((((d) >> 8) & 0xff) << 16) | ((((d) >> 16) & 0xff) << 8) | (((d) >> 24) & 0xff))
#define BETOHOST16(d)	((((d) & 0xff) << 8) | (((d) >> 8) & 0xff))
#else
#define HOSTTOLE64(d)	((((d) & 0xff) << 48) | ((((d) >> 8) & 0xff) << 40) | ((((d) >> 16) & 0xff) << 32) | ((((d) >> 24) & 0xff) << 24) | ((((d) >> 32) & 0xff) << 16) | ((((d) >> 40) & 0xff) << 8) | ((((d) >> 48) & 0xff)))
#define HOSTTOLE32(d)	((((d) & 0xff) << 24) | ((((d) >> 8) & 0xff) << 16) | ((((d) >> 16) & 0xff) << 8) | (((d) >> 24) & 0xff))
#define HOSTTOLE16(d)	((((d) & 0xff) << 8) | (((d) >> 8) & 0xff))
#define HOSTTOBE64(d)	(d)
#define HOSTTOBE32(d)	(d)
#define HOSTTOBE16(d)	(d)
#define LETOHOST64(d)	((((d) & 0xff) << 48) | ((((d) >> 8) & 0xff) << 40) | ((((d) >> 16) & 0xff) << 32) | ((((d) >> 24) & 0xff) << 24) | ((((d) >> 32) & 0xff) << 16) | ((((d) >> 40) & 0xff) << 8) | ((((d) >> 48) & 0xff)))
#define LETOHOST32(d)	((((d) & 0xff) << 24) | ((((d) >> 8) & 0xff) << 16) | ((((d) >> 16) & 0xff) << 8) | (((d) >> 24) & 0xff))
#define LETOHOST16(d)	((((d) & 0xff) << 8) | (((d) >> 8) & 0xff))
#define BETOHOST64(d)	(d)
#define BETOHOST32(d)	(d)
#define BETOHOST16(d)	(d)
#endif

/*
 * Message Translation
 */
#if defined(USE_GETTEXT_COMPAT)
#define _(s)		noct_gettext(s)
const char *noct_gettext(const char *msg);
#else
#define _(s)		(s)
#endif

#endif
