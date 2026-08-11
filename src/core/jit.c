/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * JIT (common): Just-In-Time native code generation
 */

#include <noct/c89compat.h>

#include "jit-x86.c"
#include "jit-x86_64.c"
#include "jit-arm32.c"
#include "jit-arm64.c"
#include "jit-mips32.c"
#include "jit-mips64.c"
#include "jit-ppc32.c"
#include "jit-ppc64.c"
#include "jit-riscv32.c"
#include "jit-riscv64.c"

/* Disable JIT on targets without an architecture implementation. */
#if defined(NOCT_USE_JIT)
#if !defined(NOCT_ARCH_X86) && !defined(NOCT_ARCH_X86_64) && \
    !defined(NOCT_ARCH_ARM32) && !defined(NOCT_ARCH_ARM64) && \
    !defined(NOCT_ARCH_MIPS32) && !defined(NOCT_ARCH_MIPS64) && \
    !defined(NOCT_ARCH_PPC32) && !defined(NOCT_ARCH_PPC64) && \
    !defined(NOCT_ARCH_RISCV32) && !defined(NOCT_ARCH_RISCV64)
#undef NOCT_USE_JIT
#endif
#endif

/*
 * Architecture Independent
 */
#if defined(NOCT_USE_JIT)

#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32)
#include <windows.h>		/* VirtualAlloc(), VirtualProtect(), VirtualFree() */
#elif defined(NOCT_TARGET_DOS4G)
#include <dos.h>
#include <i86.h>
#elif defined(NOCT_TARGET_PC98BE)
/* Freestanding: allocation and cache/protection policy are supplied below. */
#else
#include <sys/mman.h>		/* mmap(), mprotect(), munmap() */
#include <unistd.h>		/* sysconf() */
#endif

static size_t
jit_page_size(void)
{
#if defined(_WIN32)
	SYSTEM_INFO info;

	GetSystemInfo(&info);
	return (size_t)info.dwPageSize;
#elif defined(NOCT_TARGET_DOS4G) || defined(NOCT_TARGET_PC98BE)
	return 16;
#else
	long size = sysconf(_SC_PAGESIZE);

	return size > 0 ? (size_t)size : 4096;
#endif
}

static size_t
jit_align_up(size_t value, size_t alignment)
{
	return (value + alignment - 1) / alignment * alignment;
}

static bool
jit_slab_allocate(struct rt_env *env, size_t requested_size,
		  struct jit_slab **result)
{
	struct jit_slab *slab;
	size_t size;
	size_t page_size;

	page_size = jit_page_size();
	if (requested_size == 0 || requested_size > jit_get_code_size(env))
		requested_size = jit_get_code_size(env);
	if (requested_size < page_size)
		requested_size = page_size;
	size = jit_align_up(requested_size, page_size);
	slab = noct_malloc(sizeof(*slab));
	if (slab == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	memset(slab, 0, sizeof(*slab));
	if (!jit_map_memory_region((void **)&slab->base, size)) {
		noct_free(slab);
		rt_error(env, "Memory mapping failed.");
		return false;
	}
	slab->current = slab->base;
	slab->committed = slab->base;
	slab->end = slab->base + size;
	slab->size = size;

	if (env->vm->jit_slab_tail != NULL)
		env->vm->jit_slab_tail->next = slab;
	else
		env->vm->jit_slab_head = slab;
	env->vm->jit_slab_tail = slab;
	env->vm->jit_slab_current = slab;
	*result = slab;
	return true;
}

bool
jit_slab_acquire(struct rt_env *env, struct jit_slab **slab,
		 void **code_top, void **code_end)
{
	struct jit_slab *current = env->vm->jit_slab_current;

	if (current == NULL || current->current >= current->end) {
		if (!jit_slab_allocate(env, 0, &current))
			return false;
	}
	*slab = current;
	*code_top = current->current;
	*code_end = current->end;
	return true;
}

bool
jit_slab_reserve(struct rt_env *env, size_t estimated_size)
{
	struct jit_slab *slab;

	if (!env->vm->config.jit_enable ||
	    env->vm->jit_slab_current != NULL)
		return true;
	return jit_slab_allocate(env, estimated_size, &slab);
}

void
jit_slab_finish(struct rt_env *env, struct jit_slab *slab, void *code_end)
{
	assert(slab == env->vm->jit_slab_current);
	assert((uint8_t *)code_end >= slab->current);
	assert((uint8_t *)code_end <= slab->end);
	slab->current = code_end;
}

void
jit_slab_abandon(struct rt_env *env, struct jit_slab *slab)
{
	if (env->vm->jit_slab_current == slab)
		env->vm->jit_slab_current = NULL;
}

void
jit_slab_clear_overflow(struct rt_env *env)
{
	env->error_message[0] = '\0';
	env->line = 0;
}

void
jit_slab_commit_all(struct rt_env *env)
{
	struct jit_slab *slab;
	size_t page_size = jit_page_size();

	for (slab = env->vm->jit_slab_head; slab != NULL; slab = slab->next) {
		uint8_t *end;

		if (slab->committed >= slab->current)
			continue;
		end = slab->base + jit_align_up(
			(size_t)(slab->current - slab->base), page_size);
		assert(end <= slab->end);
		jit_map_executable(slab->committed,
				   (size_t)(end - slab->committed));
		slab->committed = end;
		slab->current = end;
	}
}

void
jit_slab_free_all(struct rt_env *env)
{
	struct jit_slab *slab = env->vm->jit_slab_head;

	while (slab != NULL) {
		struct jit_slab *next = slab->next;

		jit_unmap_memory_region(slab->base, slab->size);
		noct_free(slab);
		slab = next;
	}
	env->vm->jit_slab_head = NULL;
	env->vm->jit_slab_tail = NULL;
	env->vm->jit_slab_current = NULL;
}

/*
 * Map the memory region for the generated code.
 */
bool
jit_map_memory_region(
	void **region,
	size_t size)
{
#if defined(_WIN32)
	*region = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT,
			       PAGE_READWRITE);
#elif defined(__APPLE__)
	/* Use MAP_JIT flag to avoid W^X. */
	*region = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_JIT, -1, 0);
#elif defined(__FreeBSD__) && defined(PROT_MAX)
	/* Use PROT_MAX() to avoid W^X. */
	*region = mmap(NULL, size, PROT_READ | PROT_WRITE |
		       PROT_MAX(PROT_READ | PROT_WRITE | PROT_EXEC),
		       MAP_ANON | MAP_PRIVATE, -1, 0);
#elif defined(__NetBSD__) && defined(PROT_MPROTECT)
	/* Use PROT_MPROTECT() to avoid W^X. */
	*region = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_MPROTECT(PROT_READ | PROT_EXEC), MAP_ANON | MAP_PRIVATE, -1, 0);
#elif defined(NOCT_TARGET_DOS4G)
	*region = noct_malloc(size);
	{
		union REGS regs;
		unsigned short current_cs = 0;
		_asm { mov current_cs, cs }
		regs.w.ax = 0x0008;
		regs.w.bx = current_cs;
		regs.w.cx = 0xFFFF;
		regs.w.dx = 0x000F;
		int386(0x31, &regs, &regs);
		if (regs.w.cflag != 0) {
			printf("Failed to expand the CS segment limit.\n");
			return false;
		}
	}
#elif defined(NOCT_TARGET_PC98BE)
	/* The i386 bootstrap environment has one flat executable address space. */
	*region = noct_malloc(size);
#else
	/* Assume no W^X. */
	*region = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
#endif
#if !defined(_WIN32) && !defined(NOCT_TARGET_DOS4G) && \
    !defined(NOCT_TARGET_PC98BE)
	if (*region == MAP_FAILED) {
		*region = NULL;
		return false;
	}
#else
	if (*region == NULL)
		return false;
#endif

	/* Anonymous mappings and VirtualAlloc are already zero-filled.  Only
	 * malloc-backed freestanding targets require eager initialization. */
#if defined(NOCT_TARGET_DOS4G) || defined(NOCT_TARGET_PC98BE)
	memset(*region, 0, size);
#endif

	return true;
}

/*
 * Unmap the memory region for the generated code.
 */
void
jit_unmap_memory_region(
	void *region,
	size_t size)
{
#if defined(_WIN32)
	UNUSED_PARAMETER(size);
	VirtualFree(region, 0, MEM_RELEASE);
#elif defined(NOCT_TARGET_DOS4G)
	/* Do nothing. */
#elif defined(NOCT_TARGET_PC98BE)
	UNUSED_PARAMETER(size);
	noct_free(region);
#else
	munmap(region, size);
#endif
}

/*
 * Make a region executable and non-writable.
 */
void
jit_map_executable(
	void *region,
	size_t size)
{
#if defined(_WIN32)
	DWORD dwOldProt;
	VirtualProtect(region, size, PAGE_EXECUTE_READ, &dwOldProt);
	FlushInstructionCache(GetCurrentProcess(), region, size);
#elif defined(NOCT_TARGET_DOS4G) || defined(NOCT_TARGET_PC98BE)
	UNUSED_PARAMETER(region);
	UNUSED_PARAMETER(size);
#else
	mprotect(region, size, PROT_EXEC | PROT_READ);
	__builtin___clear_cache((char *)region, (char *)region + size);
#endif
}

#else /* defined(NOCT_USE_JIT) */

/*
 * Stub for non-JIT build.
 */

#include "runtime.h"

/*
 * Generate a JIT-compiled code for a function.
 */
bool
jit_build(
	struct rt_env *env,
	struct rt_func *func)
{
	UNUSED_PARAMETER(env);
	UNUSED_PARAMETER(func);

	/* stub */
	return false;
}

/*
 * Commit written code.
 */
void
jit_commit(
	struct rt_env *env)
{
	UNUSED_PARAMETER(env);

	/* stub */
}

/*
 * Free a JIT-compiled code for a function.
 */
void
jit_free(
	struct rt_env *env)
{
	UNUSED_PARAMETER(env);

	/* stub */
}

#endif /* defined(NOCT_USE_JIT) */
