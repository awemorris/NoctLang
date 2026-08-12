/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* API: File.* */

#include <noct/noct.h>
#include "runtime.h"
#include "objectmodel.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(NOCT_TARGET_POSIX)
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#if defined(NOCT_TARGET_PC98BE)
#include <unistd.h>
#endif

#if defined(NOCT_TARGET_WINDOWS)
#include <io.h>
#include <windows.h>
#include <direct.h>
#define access	_access
#define F_OK	0
#endif

#if defined(NOCT_TARGET_DOS4G)
/* OpenWatcom DOS: getcwd/chdir live in <direct.h>. */
#include <direct.h>
#endif

static bool cfunc_File_open(NoctEnv *env);
static bool cfunc_File_close(NoctEnv *env);
static bool cfunc_File_tell(NoctEnv *env);
static bool cfunc_File_seek(NoctEnv *env);
static bool cfunc_File_read(NoctEnv *env);
static bool cfunc_File_write(NoctEnv *env);
static bool cfunc_File_readExact(NoctEnv *env);
static bool cfunc_File_writeAll(NoctEnv *env);
static void file_finalizer(void *native_pointer);
static bool cfunc_FileUtil_checkFileExists(NoctEnv *env);
static bool cfunc_FileUtil_listDirectory(NoctEnv *env);
static bool cfunc_FileUtil_readTextEucJp(NoctEnv *env);
static bool cfunc_FileUtil_getCurrentDirectory(NoctEnv *env);
static bool cfunc_FileUtil_setCurrentDirectory(NoctEnv *env);
static bool cfunc_FileUtil_getHomeDirectory(NoctEnv *env);
static bool cfunc_FileUtil_getFileSize(NoctEnv *env);
static bool cfunc_FileUtil_readText(NoctEnv *env);
static bool cfunc_FileUtil_writeText(NoctEnv *env);
static bool cfunc_FileUtil_tryWriteText(NoctEnv *env);
static bool cfunc_FileUtil_tryReadText(NoctEnv *env);
static bool cfunc_FileUtil_readForEachLine(NoctEnv *env);
static bool cfunc_FileUtil_writeForEachLine(NoctEnv *env);
static bool cfunc_FileUtil_makeDirectoryExclusive(NoctEnv *env);

bool noct_register_api_binary(NoctEnv *env);
#if defined(NOCT_USE_MODEL_WEIGHTS)
bool noct_register_api_weights(NoctEnv *env);
#endif

#define FILE_HANDLE_MAGIC 0x4e46494cU

struct file_handle {
	uint32_t magic;
	struct rt_vm *owner;
	FILE *file;
	bool closed;
};

static bool cfunc_FileUtil_mmap8(NoctEnv *env);
static bool cfunc_FileUtil_mmap16(NoctEnv *env);
static bool cfunc_FileUtil_mmap32(NoctEnv *env);
static bool cfunc_FileUtil_mmap64(NoctEnv *env);
static bool cfunc_FileUtil_mflush(NoctEnv *env);
static bool cfunc_FileUtil_munmap(NoctEnv *env);

#define FILE_MAPPING_MAGIC 0x4e4d4150U /* "NMAP" */

struct file_mapping {
	uint32_t magic;
	void *mapped_base;
	size_t mapped_length;
	size_t logical_length;
	size_t delta;
	int allow_read;
	int allow_write;
#if defined(NOCT_TARGET_WINDOWS)
	HANDLE file_handle;
	HANDLE mapping_handle;
#endif
};

static void file_mapping_finalizer(void *native_pointer);
static bool cfunc_FileUtil_mmap_impl(NoctEnv *env, int packed_type,
				     size_t element_width);

static struct {
	const struct NoctDirectoryBackend *operations;
	void *context;
} directory_backend;

NOCT_DLL void
noct_set_directory_backend(const struct NoctDirectoryBackend *backend,
			   void *context)
{
	directory_backend.operations = backend;
	directory_backend.context = context;
}

/*
 * FileUtil.tryReadText(path)
 *
 * Like readText, but returns the integer 0 instead of raising an
 * error when the path cannot be read as a regular file (missing,
 * unreadable, or a directory).
 */
static bool
cfunc_FileUtil_tryReadText(NoctEnv *env)
{
	NoctValue path, ret;
	const char *path_s;
	FILE *file = NULL;
	char *buf = NULL;
	long size;
	bool ok = false;

	if (!noct_pin_local(env, 2, &path, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s))
		goto cleanup;

	file = fopen(path_s, "rb");
	if (file == NULL)
		goto fail_soft;
	if (fseek(file, 0, SEEK_END) != 0)
		goto fail_soft;
	size = ftell(file);
	if (size < 0)
		goto fail_soft;
	if (fseek(file, 0, SEEK_SET) != 0)
		goto fail_soft;
	buf = noct_malloc((size_t)size + 1);
	if (buf == NULL)
		goto fail_soft;
	if (size > 0 && fread(buf, 1, (size_t)size, file) != (size_t)size)
		goto fail_soft;
	buf[size] = '\0';
	if (!noct_set_return_make_string(env, &ret, buf))
		goto cleanup;
	ok = true;
	goto cleanup;

fail_soft:
	if (!noct_set_return_make_int(env, &ret, 0))
		goto cleanup;
	ok = true;

cleanup:
	if (buf != NULL)
		noct_free(buf);
	if (file != NULL)
		fclose(file);
	(void)noct_unpin_local(env, 2, &path, &ret);
	return ok;
}

/*
 * FileUtil.tryWriteText(path, text)
 *
 * Like writeText, but returns 0 instead of raising an error when the
 * file cannot be created or written (e.g. a filesystem that rejects
 * the name, like a leading dot on FAT16).  Returns 1 on success.
 */
static bool
cfunc_FileUtil_tryWriteText(NoctEnv *env)
{
	NoctValue path, text, ret;
	const char *path_s, *text_s;
	FILE *file = NULL;
	size_t length;
	int result = 0;

	if (!noct_pin_local(env, 3, &path, &text, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s) ||
	    !noct_get_arg_check_string(env, 1, &text, &text_s)) {
		(void)noct_unpin_local(env, 3, &path, &text, &ret);
		return false;
	}
	file = fopen(path_s, "wb");
	if (file != NULL) {
		length = strlen(text_s);
		if (fwrite(text_s, 1, length, file) == length &&
		    fflush(file) == 0)
			result = 1;
		if (fclose(file) != 0)
			result = 0;
	}
	if (!noct_set_return_make_int(env, &ret, result)) {
		(void)noct_unpin_local(env, 3, &path, &text, &ret);
		return false;
	}
	(void)noct_unpin_local(env, 3, &path, &text, &ret);
	return true;
}

struct ffi_item {
	const char *global_name;
	const char *package_name;
	const char *field_name;
	size_t param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(NoctEnv *env);
};

static struct ffi_item ffi_items[] = {
	{"File.open",	"File",	"open",		2, {"path", "mode"},	cfunc_File_open},
	{"File.close",	"File",	"close",	1, {"file"},		cfunc_File_close},
	{"File.tell",	"File", "tell",		1, {"file"},		cfunc_File_tell},
	{"File.seek",	"File", "seek",		2, {"file", "offset"},	cfunc_File_seek},
	{"File.read",	"File", "read",		2, {"file", "len"},	cfunc_File_read},
	{"File.write",	"File", "write",	4, {"file", "data", "offset", "size"},	cfunc_File_write},
	{"File.readExact", "File", "readExact", 2, {"file", "byteCount"}, cfunc_File_readExact},
	{"File.writeAll", "File", "writeAll", 4, {"file", "bytes", "byteOffset", "byteCount"}, cfunc_File_writeAll},

	{"FileUtil.checkFileExists",	"FileUtil", "checkFileExists",	1, {"path"},		cfunc_FileUtil_checkFileExists},
	{"FileUtil.listDirectory",	"FileUtil", "listDirectory",	1, {"path"},		cfunc_FileUtil_listDirectory},
	{"FileUtil.readTextEucJp",	"FileUtil", "readTextEucJp",	1, {"path"},		cfunc_FileUtil_readTextEucJp},
	{"FileUtil.getCurrentDirectory", "FileUtil", "getCurrentDirectory", 0, {NULL},	cfunc_FileUtil_getCurrentDirectory},
	{"FileUtil.setCurrentDirectory", "FileUtil", "setCurrentDirectory", 1, {"path"},	cfunc_FileUtil_setCurrentDirectory},
	{"FileUtil.getHomeDirectory",	"FileUtil", "getHomeDirectory",	0, {NULL},	cfunc_FileUtil_getHomeDirectory},
	{"FileUtil.getFileSize",	"FileUtil", "getFileSize",	1, {"path"},		cfunc_FileUtil_getFileSize},
	{"FileUtil.readText",		"FileUtil", "readText",		1, {"path"},		cfunc_FileUtil_readText},
	{"FileUtil.writeText",		"FileUtil", "writeText",	2, {"path", "text"},	cfunc_FileUtil_writeText},
	{"FileUtil.tryWriteText",	"FileUtil", "tryWriteText",	2, {"path", "text"},	cfunc_FileUtil_tryWriteText},
	{"FileUtil.tryReadText",	"FileUtil", "tryReadText",	1, {"path"},		cfunc_FileUtil_tryReadText},
	{"FileUtil.readForEachLine",	"FileUtil", "readForEachLine",	2, {"path", "func"},	cfunc_FileUtil_readForEachLine},
	{"FileUtil.writeForEachLine",	"FileUtil", "writeForEachLine",	2, {"path", "lines"},	cfunc_FileUtil_writeForEachLine},
	{"FileUtil.makeDirectoryExclusive", "FileUtil", "makeDirectoryExclusive", 1, {"path"}, cfunc_FileUtil_makeDirectoryExclusive},
	{"FileUtil.mmap8",		"FileUtil", "mmap8",		5, {"path", "offset", "size", "allow_read", "allow_write"}, cfunc_FileUtil_mmap8},
	{"FileUtil.mmap16",		"FileUtil", "mmap16",		5, {"path", "offset", "size", "allow_read", "allow_write"}, cfunc_FileUtil_mmap16},
	{"FileUtil.mmap32",		"FileUtil", "mmap32",		5, {"path", "offset", "size", "allow_read", "allow_write"}, cfunc_FileUtil_mmap32},
	{"FileUtil.mmap64",		"FileUtil", "mmap64",		5, {"path", "offset", "size", "allow_read", "allow_write"}, cfunc_FileUtil_mmap64},
	{"FileUtil.mflush",		"FileUtil", "mflush",		1, {"mapped"},	cfunc_FileUtil_mflush},
	{"FileUtil.munmap",		"FileUtil", "munmap",		1, {"mapped"},	cfunc_FileUtil_munmap},
};

NOCT_DLL bool
noct_register_api_file(NoctEnv *env)
{
	NoctValue file_dict;
	NoctValue fileutil_dict;
	size_t i;

	if (!noct_make_empty_dict(env, &file_dict) ||
	    !noct_make_empty_dict(env, &fileutil_dict) ||
	    !noct_set_global(env, "File", &file_dict) ||
	    !noct_set_global(env, "FileUtil", &fileutil_dict))
		return false;
	for (i = 0; i < sizeof(ffi_items) / sizeof(ffi_items[0]); i++) {
		NoctValue funcval;
		NoctValue *package = !strcmp(ffi_items[i].package_name, "File") ?
			&file_dict : &fileutil_dict;

		if (!noct_register_cfunc(env, ffi_items[i].global_name,
					 ffi_items[i].param_count,
					 ffi_items[i].param,
					 ffi_items[i].cfunc, NULL) ||
		    !noct_get_global(env, ffi_items[i].global_name, &funcval) ||
		    !noct_set_dict_elem_cstr(env, package,
					     ffi_items[i].field_name, &funcval))
			return false;
	}
	if (!noct_register_api_binary(env))
		return false;
#if defined(NOCT_USE_MODEL_WEIGHTS)
	if (!noct_register_api_weights(env))
		return false;
#endif
	return true;
}

static bool
fileutil_get_nonnegative_u64(NoctEnv *env, uint32_t index, NoctValue *value,
			     uint64_t *result)
{
	if (!noct_get_arg(env, index, value))
		return false;
	if (value->type == NOCT_VALUE_INT) {
		if (value->val.i < 0) {
			noct_error(env, N_TR("File mapping offset and size must not be negative."));
			return false;
		}
		*result = (uint64_t)(uint32_t)value->val.i;
		return true;
	}
	if (value->type == NOCT_VALUE_LONG) {
		if (value->val.l < 0) {
			noct_error(env, N_TR("File mapping offset and size must not be negative."));
			return false;
		}
		*result = (uint64_t)value->val.l;
		return true;
	}
	noct_error(env, N_TR("File mapping offset and size must be integers."));
	return false;
}

static bool
fileutil_get_mapping(NoctEnv *env, NoctValue *value,
			 struct file_mapping **mapping)
{
	void *native_pointer;
	void *packed_pointer;
	void (*native_finalizer)(void *native_pointer);

	if (!noct_get_arg_check_packed(env, 0, value, NOCT_PACKED_ANY))
		return false;
	if (!noct_get_packed_native_pointer(env, value, &native_pointer,
					    &native_finalizer))
		return false;
	if (native_pointer == NULL || native_finalizer == NULL) {
		if (!noct_get_packed_pointer(env, value, &packed_pointer))
			return false;
		UNUSED_PARAMETER(packed_pointer);
		noct_error(env, N_TR("Packed is not a file mapping."));
		return false;
	}
	if (native_finalizer != file_mapping_finalizer ||
	    ((struct file_mapping *)native_pointer)->magic != FILE_MAPPING_MAGIC) {
		noct_error(env, N_TR("Packed is not a file mapping."));
		return false;
	}
	*mapping = (struct file_mapping *)native_pointer;
	return true;
}

static void
file_mapping_finalizer(void *native_pointer)
{
	struct file_mapping *mapping = (struct file_mapping *)native_pointer;

	if (mapping == NULL || mapping->magic != FILE_MAPPING_MAGIC)
		return;
#if defined(NOCT_TARGET_POSIX)
	if (mapping->mapped_base != NULL)
		(void)munmap(mapping->mapped_base, mapping->mapped_length);
#elif defined(NOCT_TARGET_WINDOWS)
	if (mapping->mapped_base != NULL)
		(void)UnmapViewOfFile(mapping->mapped_base);
	if (mapping->mapping_handle != NULL)
		(void)CloseHandle(mapping->mapping_handle);
	if (mapping->file_handle != NULL &&
	    mapping->file_handle != INVALID_HANDLE_VALUE)
		(void)CloseHandle(mapping->file_handle);
#endif
	mapping->magic = 0;
	noct_free(mapping);
}

static bool
cfunc_FileUtil_mmap_impl(NoctEnv *env, int packed_type, size_t element_width)
{
#if !defined(NOCT_TARGET_POSIX) && !defined(NOCT_TARGET_WINDOWS)
	UNUSED_PARAMETER(packed_type);
	UNUSED_PARAMETER(element_width);
	noct_error(env, N_TR("File mapping is not supported on this platform."));
	return false;
#else
	NoctValue path, offset_value, size_value, read_value, write_value, ret;
	const char *path_s;
	uint64_t offset, requested_size, end, map_offset, delta;
	size_t map_length, elem_count;
	int allow_read, allow_write;
	struct file_mapping *mapping = NULL;
	bool transferred = false;
	bool ok = false;
#if defined(NOCT_TARGET_POSIX)
	int fd = -1;
	long page_size;
	struct stat st;
	int prot;
	off_t os_offset;
#elif defined(NOCT_TARGET_WINDOWS)
	SYSTEM_INFO system_info;
	LARGE_INTEGER file_size;
	DWORD file_access, protect, view_access;
	ULARGE_INTEGER os_offset;
#endif

	if (!noct_pin_local(env, 6, &path, &offset_value, &size_value,
			    &read_value, &write_value, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s) ||
	    !fileutil_get_nonnegative_u64(env, 1, &offset_value, &offset) ||
	    !fileutil_get_nonnegative_u64(env, 2, &size_value, &requested_size) ||
	    !noct_get_arg_check_int(env, 3, &read_value, &allow_read) ||
	    !noct_get_arg_check_int(env, 4, &write_value, &allow_write))
		goto cleanup;
	if ((allow_read != 0 && allow_read != 1) ||
	    (allow_write != 0 && allow_write != 1)) {
		noct_error(env, N_TR("File mapping permissions must be 0 or 1."));
		goto cleanup;
	}
	if (!allow_read && !allow_write) {
		noct_error(env, N_TR("File mapping must allow reading or writing."));
		goto cleanup;
	}
	if (!allow_read && allow_write) {
		noct_error(env, N_TR("Write-only file mappings are not supported."));
		goto cleanup;
	}
	if (requested_size == 0) {
		noct_error(env, N_TR("File mapping size must be greater than zero."));
		goto cleanup;
	}
	if (offset > UINT64_MAX - requested_size) {
		noct_error(env, N_TR("File mapping range is too large."));
		goto cleanup;
	}
	end = offset + requested_size;
	if ((offset % element_width) != 0 ||
	    (requested_size % element_width) != 0) {
		noct_error(env, N_TR("File mapping range is not aligned to the element size."));
		goto cleanup;
	}
	if (requested_size > (uint64_t)SIZE_MAX) {
		noct_error(env, N_TR("File mapping size is too large."));
		goto cleanup;
	}
	elem_count = (size_t)(requested_size / element_width);

#if defined(NOCT_TARGET_POSIX)
	page_size = sysconf(_SC_PAGE_SIZE);
	if (page_size <= 0) {
		noct_error(env, N_TR("Cannot determine the file mapping page size."));
		goto cleanup;
	}
	map_offset = offset - offset % (uint64_t)page_size;
	delta = offset - map_offset;
	if (requested_size > (uint64_t)SIZE_MAX - delta) {
		noct_error(env, N_TR("File mapping size is too large."));
		goto cleanup;
	}
	map_length = (size_t)(requested_size + delta);
	os_offset = (off_t)map_offset;
	if (os_offset < 0 || (uint64_t)os_offset != map_offset) {
		noct_error(env, N_TR("File mapping offset is too large."));
		goto cleanup;
	}
	fd = open(path_s, allow_write ? O_RDWR : O_RDONLY);
	if (fd < 0) {
		noct_error(env, N_TR("Cannot open file %s for mapping: %s"),
			   path_s, strerror(errno));
		goto cleanup;
	}
	if (fstat(fd, &st) != 0) {
		noct_error(env, N_TR("Cannot determine mapped file size: %s"),
			   strerror(errno));
		goto cleanup;
	}
	if (S_ISREG(st.st_mode) &&
	    (st.st_size < 0 || end > (uint64_t)st.st_size)) {
		noct_error(env, N_TR("File mapping range exceeds the file size."));
		goto cleanup;
	}
	mapping = noct_calloc(1, sizeof(*mapping));
	if (mapping == NULL) {
		noct_out_of_memory(env);
		goto cleanup;
	}
	prot = PROT_READ | (allow_write ? PROT_WRITE : 0);
	mapping->mapped_base = mmap(NULL, map_length, prot, MAP_SHARED,
				    fd, os_offset);
	if (mapping->mapped_base == MAP_FAILED) {
		mapping->mapped_base = NULL;
		noct_error(env, N_TR("Cannot map file %s: %s"),
			   path_s, strerror(errno));
		goto cleanup;
	}
#elif defined(NOCT_TARGET_WINDOWS)
	GetSystemInfo(&system_info);
	if (system_info.dwAllocationGranularity == 0) {
		noct_error(env, N_TR("Cannot determine the file mapping allocation granularity."));
		goto cleanup;
	}
	map_offset = offset - offset %
		(uint64_t)system_info.dwAllocationGranularity;
	delta = offset - map_offset;
	if (requested_size > (uint64_t)SIZE_MAX - delta) {
		noct_error(env, N_TR("File mapping size is too large."));
		goto cleanup;
	}
	map_length = (size_t)(requested_size + delta);
	mapping = noct_calloc(1, sizeof(*mapping));
	if (mapping == NULL) {
		noct_out_of_memory(env);
		goto cleanup;
	}
	mapping->file_handle = INVALID_HANDLE_VALUE;
	file_access = GENERIC_READ | (allow_write ? GENERIC_WRITE : 0);
	mapping->file_handle = CreateFileA(path_s, file_access,
					   FILE_SHARE_READ | FILE_SHARE_WRITE,
					   NULL, OPEN_EXISTING,
					   FILE_ATTRIBUTE_NORMAL, NULL);
	if (mapping->file_handle == INVALID_HANDLE_VALUE) {
		noct_error(env, N_TR("Cannot open file for mapping (Windows error %lu)."),
			   (unsigned long)GetLastError());
		goto cleanup;
	}
	if (!GetFileSizeEx(mapping->file_handle, &file_size)) {
		noct_error(env, N_TR("Cannot determine mapped file size (Windows error %lu)."),
			   (unsigned long)GetLastError());
		goto cleanup;
	}
	if (file_size.QuadPart < 0 || end > (uint64_t)file_size.QuadPart) {
		noct_error(env, N_TR("File mapping range exceeds the file size."));
		goto cleanup;
	}
	protect = allow_write ? PAGE_READWRITE : PAGE_READONLY;
	mapping->mapping_handle = CreateFileMappingA(mapping->file_handle, NULL,
						      protect, 0, 0, NULL);
	if (mapping->mapping_handle == NULL) {
		noct_error(env, N_TR("Cannot create file mapping (Windows error %lu)."),
			   (unsigned long)GetLastError());
		goto cleanup;
	}
	os_offset.QuadPart = map_offset;
	view_access = allow_write ? FILE_MAP_WRITE : FILE_MAP_READ;
	mapping->mapped_base = MapViewOfFile(mapping->mapping_handle,
					     view_access,
					     os_offset.HighPart,
					     os_offset.LowPart,
					     map_length);
	if (mapping->mapped_base == NULL) {
		noct_error(env, N_TR("Cannot map file view (Windows error %lu)."),
			   (unsigned long)GetLastError());
		goto cleanup;
	}
#endif

	mapping->magic = FILE_MAPPING_MAGIC;
	mapping->mapped_length = map_length;
	mapping->logical_length = (size_t)requested_size;
	mapping->delta = (size_t)delta;
	mapping->allow_read = allow_read;
	mapping->allow_write = allow_write;
	if (!noct_make_packed(env, &ret, packed_type,
			      (size_t)requested_size, elem_count,
			      (char *)mapping->mapped_base + mapping->delta,
			      mapping, file_mapping_finalizer))
		goto cleanup;
	transferred = true;
	if (!noct_set_return(env, &ret))
		goto cleanup;
	ok = true;

cleanup:
#if defined(NOCT_TARGET_POSIX)
	if (fd >= 0)
		(void)close(fd);
#endif
	if (!transferred && mapping != NULL) {
		if (mapping->magic == 0)
			mapping->magic = FILE_MAPPING_MAGIC;
		file_mapping_finalizer(mapping);
	}
	(void)noct_unpin_local(env, 6, &path, &offset_value, &size_value,
			      &read_value, &write_value, &ret);
	return ok;
#endif
}

static bool cfunc_FileUtil_mmap8(NoctEnv *env)
{
	return cfunc_FileUtil_mmap_impl(env, NOCT_PACKED_UINT8, 1);
}

static bool cfunc_FileUtil_mmap16(NoctEnv *env)
{
	return cfunc_FileUtil_mmap_impl(env, NOCT_PACKED_UINT16, 2);
}

static bool cfunc_FileUtil_mmap32(NoctEnv *env)
{
	return cfunc_FileUtil_mmap_impl(env, NOCT_PACKED_UINT32, 4);
}

static bool cfunc_FileUtil_mmap64(NoctEnv *env)
{
	return cfunc_FileUtil_mmap_impl(env, NOCT_PACKED_UINT64, 8);
}

static bool
cfunc_FileUtil_mflush(NoctEnv *env)
{
#if !defined(NOCT_TARGET_POSIX) && !defined(NOCT_TARGET_WINDOWS)
	noct_error(env, N_TR("File mapping is not supported on this platform."));
	return false;
#else
	NoctValue value, ret;
	struct file_mapping *mapping;
	bool ok = false;

	if (!noct_pin_local(env, 2, &value, &ret))
		return false;
	if (!fileutil_get_mapping(env, &value, &mapping))
		goto cleanup;
#if defined(NOCT_TARGET_POSIX)
	if (msync(mapping->mapped_base, mapping->mapped_length, MS_SYNC) != 0) {
		noct_error(env, N_TR("Cannot flush file mapping: %s"),
			   strerror(errno));
		goto cleanup;
	}
#elif defined(NOCT_TARGET_WINDOWS)
	if (!FlushViewOfFile(mapping->mapped_base, mapping->mapped_length)) {
		noct_error(env, N_TR("Cannot flush file mapping (Windows error %lu)."),
			   (unsigned long)GetLastError());
		goto cleanup;
	}
	if (mapping->allow_write && !FlushFileBuffers(mapping->file_handle)) {
		noct_error(env, N_TR("Cannot flush mapped file (Windows error %lu)."),
			   (unsigned long)GetLastError());
		goto cleanup;
	}
#endif
	if (!noct_set_return_make_int(env, &ret, 0))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &value, &ret);
	return ok;
#endif
}

static bool
cfunc_FileUtil_munmap(NoctEnv *env)
{
#if !defined(NOCT_TARGET_POSIX) && !defined(NOCT_TARGET_WINDOWS)
	noct_error(env, N_TR("File mapping is not supported on this platform."));
	return false;
#else
	NoctValue value, ret;
	struct file_mapping *mapping;
	bool ok = false;

	if (!noct_pin_local(env, 2, &value, &ret))
		return false;
	if (!fileutil_get_mapping(env, &value, &mapping))
		goto cleanup;
#if defined(NOCT_TARGET_POSIX)
	if (munmap(mapping->mapped_base, mapping->mapped_length) != 0) {
		noct_error(env, N_TR("Cannot unmap file mapping: %s"),
			   strerror(errno));
		goto cleanup;
	}
	mapping->mapped_base = NULL;
#elif defined(NOCT_TARGET_WINDOWS)
	if (!UnmapViewOfFile(mapping->mapped_base)) {
		noct_error(env, N_TR("Cannot unmap file view (Windows error %lu)."),
			   (unsigned long)GetLastError());
		goto cleanup;
	}
	mapping->mapped_base = NULL;
	if (mapping->mapping_handle != NULL) {
		(void)CloseHandle(mapping->mapping_handle);
		mapping->mapping_handle = NULL;
	}
	if (mapping->file_handle != NULL &&
	    mapping->file_handle != INVALID_HANDLE_VALUE) {
		(void)CloseHandle(mapping->file_handle);
		mapping->file_handle = INVALID_HANDLE_VALUE;
	}
#endif
	if (!noct_finalize_packed(env, &value))
		goto cleanup;
	if (!noct_set_return_make_int(env, &ret, 0))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &value, &ret);
	return ok;
#endif
}

static bool
get_file(NoctEnv *env, NoctValue *value, FILE **file)
{
	struct file_handle *handle;
	void *native_pointer;
	void (*finalizer)(void *);

	if (!noct_get_dict_native_pointer(env, value, &native_pointer,
					  &finalizer))
		return false;
	if (finalizer != file_finalizer || native_pointer == NULL) {
		noct_error(env, N_TR("File handle kind mismatch."));
		return false;
	}
	handle = (struct file_handle *)native_pointer;
	if (handle->magic != FILE_HANDLE_MAGIC) {
		noct_error(env, N_TR("File handle is invalid."));
		return false;
	}
	if (handle->owner != env->vm) {
		noct_error(env, N_TR("File handle belongs to a different VM."));
		return false;
	}
	if (handle->closed || handle->file == NULL) {
		noct_error(env, N_TR("File is closed."));
		return false;
	}
	*file = handle->file;
	return true;
}

static void
file_finalizer(void *native_pointer)
{
	struct file_handle *handle;
	handle = (struct file_handle *)native_pointer;
	if (handle == NULL) return;
	if (handle->magic == FILE_HANDLE_MAGIC) {
		if (!handle->closed && handle->file != NULL)
			(void)fclose(handle->file);
		handle->file = NULL;
		handle->closed = true;
		handle->magic = 0;
	}
	noct_free(handle);
}

static bool
cfunc_File_open(NoctEnv *env)
{
	NoctValue path, mode, ret;
	const char *path_s, *mode_s;
	struct file_handle *handle = NULL;
	FILE *file = NULL;
	bool installed = false;
	bool ok = false;

	if (!noct_pin_local(env, 3, &path, &mode, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s) ||
	    !noct_get_arg_check_string(env, 1, &mode, &mode_s))
		goto cleanup;
	if (strcmp(mode_s, "r") && strcmp(mode_s, "rb") &&
	    strcmp(mode_s, "w") && strcmp(mode_s, "wb")) {
		noct_error(env, N_TR("Unsupported file mode."));
		goto cleanup;
	}
	file = fopen(path_s, mode_s);
	if (file == NULL) {
		noct_error(env, N_TR("Cannot open file %s."), path_s);
		goto cleanup;
	}
	handle = noct_malloc(sizeof(*handle));
	if (handle == NULL) {
		noct_error(env, N_TR("Out of memory."));
		goto cleanup;
	}
	handle->magic = FILE_HANDLE_MAGIC;
	handle->owner = env->vm;
	handle->file = file;
	handle->closed = false;
	if (!noct_make_empty_dict(env, &ret) ||
	    !noct_set_dict_native_pointer(env, &ret, handle, file_finalizer))
		goto cleanup;
	installed = true;
	if (!noct_set_return(env, &ret))
		goto cleanup;
	ok = true;
cleanup:
	if (!ok && file != NULL) {
		/*
		 * Once installed, the dictionary finalizer owns the stream.  Clear
		 * that ownership before closing it ourselves.  If clearing fails,
		 * leave the stream to the finalizer instead of risking a double
		 * fclose on a native pointer that is still reachable by the VM.
		 */
		if (!installed ||
		    noct_set_dict_native_pointer(env, &ret, NULL, NULL)) {
			(void)fclose(file);
			if (handle != NULL) noct_free(handle);
		}
	}
	(void)noct_unpin_local(env, 3, &path, &mode, &ret);
	return ok;
}

static bool
cfunc_File_close(NoctEnv *env)
{
	NoctValue file_value, ret;
	struct file_handle *handle;
	void *native_pointer;
	void (*finalizer)(void *);
	FILE *file;
	bool ok = false;

	if (!noct_pin_local(env, 2, &file_value, &ret))
		return false;
	if (!noct_get_arg_check_dict(env, 0, &file_value) ||
	    !get_file(env, &file_value, &file))
		goto cleanup;
	if (!noct_get_dict_native_pointer(env, &file_value, &native_pointer,
					  &finalizer))
		goto cleanup;
	handle = (struct file_handle *)native_pointer;
	if (finalizer != file_finalizer || handle == NULL ||
	    handle->magic != FILE_HANDLE_MAGIC) {
		noct_error(env, N_TR("File handle kind mismatch."));
		goto cleanup;
	}
	if (fclose(file) != 0) {
		handle->file = NULL;
		handle->closed = true;
		noct_error(env, N_TR("File close error."));
		goto cleanup;
	}
	handle->file = NULL;
	handle->closed = true;
	if (!noct_set_return_make_int(env, &ret, 0))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &file_value, &ret);
	return ok;
}

static bool
cfunc_File_readExact(NoctEnv *env)
{
	NoctValue file_value, count_value, ret;
	FILE *file;
	size_t count, actual;
	void *buffer;
	bool ok;
	buffer = NULL; ok = false;
	if (!noct_pin_local(env, 3, &file_value, &count_value, &ret)) return false;
	if (!noct_get_arg_check_dict(env, 0, &file_value) ||
	    !noct_get_arg_check_int_long(env, 1, &count_value, &count) ||
	    !get_file(env, &file_value, &file)) goto cleanup;
	if ((count_value.type == NOCT_VALUE_INT && count_value.val.i < 0) ||
	    (count_value.type == NOCT_VALUE_LONG && count_value.val.l < 0) ||
	    count == 0) {
		noct_error(env, N_TR("Exact read byte count must be positive."));
		goto cleanup;
	}
	if (!noct_make_packed(env, &ret, NOCT_PACKED_UINT8,
			      count, count, NULL, NULL, NULL) ||
	    !noct_get_packed_pointer(env, &ret, &buffer)) goto cleanup;
	actual = fread(buffer, 1, count, file);
	if (actual != count) {
		if (ferror(file)) noct_error(env, N_TR("File read error."));
		else noct_error(env, N_TR("Unexpected end of file."));
		goto cleanup;
	}
	if (!noct_set_return(env, &ret)) goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 3, &file_value, &count_value, &ret);
	return ok;
}

static bool
cfunc_File_writeAll(NoctEnv *env)
{
	NoctValue file_value, bytes_value, offset_value, count_value, ret;
	FILE *file;
	void *pointer;
	size_t size, offset, count, written, actual;
	bool ok;
	ok = false;
	if (!noct_pin_local(env, 5, &file_value, &bytes_value, &offset_value,
			    &count_value, &ret)) return false;
	if (!noct_get_arg_check_dict(env, 0, &file_value) ||
	    !noct_get_arg_check_packed(env, 1, &bytes_value, NOCT_PACKED_UINT8) ||
	    !noct_get_arg_check_int_long(env, 2, &offset_value, &offset) ||
	    !noct_get_arg_check_int_long(env, 3, &count_value, &count) ||
	    !get_file(env, &file_value, &file) ||
	    !noct_get_packed_size(env, &bytes_value, &size) ||
	    !noct_get_packed_pointer(env, &bytes_value, &pointer)) goto cleanup;
	if ((offset_value.type == NOCT_VALUE_INT && offset_value.val.i < 0) ||
	    (offset_value.type == NOCT_VALUE_LONG && offset_value.val.l < 0) ||
	    (count_value.type == NOCT_VALUE_INT && count_value.val.i < 0) ||
	    (count_value.type == NOCT_VALUE_LONG && count_value.val.l < 0) ||
	    offset > size || count > size - offset) {
		noct_error(env, N_TR("File.writeAll range is out-of-bounds."));
		goto cleanup;
	}
	written = 0;
	while (written < count) {
		actual = fwrite((const uint8_t *)pointer + offset + written,
				1, count - written, file);
		if (actual == 0) {
			noct_error(env, N_TR("File write error."));
			goto cleanup;
		}
		written += actual;
	}
	if (!noct_set_return_make_int(env, &ret, 0)) goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 5, &file_value, &bytes_value, &offset_value,
			       &count_value, &ret);
	return ok;
}

static bool
cfunc_FileUtil_makeDirectoryExclusive(NoctEnv *env)
{
	NoctValue path_value, ret;
	const char *path;
	int result;
	bool ok;
	ok = false;
	if (!noct_pin_local(env, 2, &path_value, &ret)) return false;
	if (!noct_get_arg_check_string(env, 0, &path_value, &path)) goto cleanup;
	if (path[0] == '\0') {
		noct_error(env, N_TR("Directory path must not be empty."));
		goto cleanup;
	}
#if defined(NOCT_TARGET_WINDOWS) || defined(NOCT_TARGET_DOS4G) || defined(NOCT_TARGET_PC98DOS)
	result = _mkdir(path);
#elif defined(NOCT_TARGET_POSIX)
	result = mkdir(path, 0777);
#else
	result = -1;
	errno = ENOSYS;
#endif
	if (result != 0) {
		if (errno == EEXIST)
			noct_error(env, N_TR("Output directory already exists."));
		else
			noct_error(env, N_TR("Cannot create output directory %s."), path);
		goto cleanup;
	}
	if (!noct_set_return_make_int(env, &ret, 0)) goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &path_value, &ret);
	return ok;
}

static bool
cfunc_File_tell(NoctEnv *env)
{
	NoctValue file_value, ret;
	FILE *file;
	long offset;
	bool ok = false;

	if (!noct_pin_local(env, 2, &file_value, &ret))
		return false;
	if (!noct_get_arg_check_dict(env, 0, &file_value) ||
	    !get_file(env, &file_value, &file))
		goto cleanup;
	offset = ftell(file);
	if (offset < 0) {
		noct_error(env, N_TR("File tell error."));
		goto cleanup;
	}
	if (!noct_set_return_make_int_long(env, &ret, (size_t)offset))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &file_value, &ret);
	return ok;
}

static bool
cfunc_File_seek(NoctEnv *env)
{
	NoctValue file_value, offset_value, ret;
	FILE *file;
	size_t offset;
	bool ok = false;

	if (!noct_pin_local(env, 3, &file_value, &offset_value, &ret))
		return false;
	if (!noct_get_arg_check_dict(env, 0, &file_value) ||
	    !noct_get_arg_check_int_long(env, 1, &offset_value, &offset) ||
	    !get_file(env, &file_value, &file))
		goto cleanup;
	if (offset > 0x7fffffffU || fseek(file, (long)offset, SEEK_SET) != 0) {
		noct_error(env, N_TR("File seek error."));
		goto cleanup;
	}
	if (!noct_set_return_make_int(env, &ret, 1))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 3, &file_value, &offset_value, &ret);
	return ok;
}

static bool
cfunc_File_read(NoctEnv *env)
{
	NoctValue file_value, length_value, ret;
	FILE *file;
	size_t length, actual;
	void *buffer = NULL;
	bool transferred = false;
	bool ok = false;

	if (!noct_pin_local(env, 3, &file_value, &length_value, &ret))
		return false;
	if (!noct_get_arg_check_dict(env, 0, &file_value) ||
	    !noct_get_arg_check_int_long(env, 1, &length_value, &length) ||
	    !get_file(env, &file_value, &file))
		goto cleanup;
	/* Noct's packed representation currently requires at least one byte. */
	if (length == 0) {
		noct_error(env, N_TR("Read length must be greater than zero."));
		goto cleanup;
	}
	buffer = noct_malloc(length);
	if (buffer == NULL) {
		noct_error(env, N_TR("Out of memory."));
		goto cleanup;
	}
	actual = fread(buffer, 1, length, file);
	if (actual == 0 || (actual < length && ferror(file))) {
		noct_error(env, N_TR("File read error."));
		goto cleanup;
	}
	if (!noct_make_packed(env, &ret, NOCT_PACKED_UINT8, actual, actual,
			      buffer, buffer, noct_free))
		goto cleanup;
	transferred = true;
	if (!noct_set_return(env, &ret))
		goto cleanup;
	ok = true;
cleanup:
	if (!transferred && buffer != NULL)
		noct_free(buffer);
	(void)noct_unpin_local(env, 3, &file_value, &length_value, &ret);
	return ok;
}

static bool
cfunc_File_write(NoctEnv *env)
{
	NoctValue file_value, data, offset_value, length_value, ret;
	FILE *file;
	size_t offset, length, packed_size;
	void *buffer;
	bool ok = false;

	if (!noct_pin_local(env, 5, &file_value, &data, &offset_value,
			    &length_value, &ret))
		return false;
	if (!noct_get_arg_check_dict(env, 0, &file_value) ||
	    !noct_get_arg_check_packed(env, 1, &data, NOCT_PACKED_UINT8) ||
	    !noct_get_arg_check_int_long(env, 2, &offset_value, &offset) ||
	    !noct_get_arg_check_int_long(env, 3, &length_value, &length) ||
	    !get_file(env, &file_value, &file) ||
	    !noct_get_packed_size(env, &data, &packed_size))
		goto cleanup;
	if (offset > packed_size || length > packed_size - offset) {
		noct_error(env, N_TR("Offset is out-of-range."));
		goto cleanup;
	}
	if (!noct_get_packed_pointer(env, &data, &buffer))
		goto cleanup;
	if (length != 0 && fwrite((char *)buffer + offset, 1, length, file) !=
			   length) {
		noct_error(env, N_TR("File write error."));
		goto cleanup;
	}
	if (!noct_set_return_make_int(env, &ret, 1))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 5, &file_value, &data, &offset_value,
			     &length_value, &ret);
	return ok;
}

static bool
cfunc_FileUtil_checkFileExists(NoctEnv *env)
{
	NoctValue path, ret;
	const char *path_s;
	FILE *fp;
	int exists;
	bool ok = false;

	if (!noct_pin_local(env, 2, &path, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s))
		goto cleanup;
	fp = fopen(path_s, "rb");
	exists = fp != NULL;
	if (fp != NULL)
		(void)fclose(fp);
	if (!noct_set_return_make_int(env, &ret, exists))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &path, &ret);
	return ok;
}

/*
 * FileUtil.listDirectory(path)
 *
 * Returns an array of entry names, sorted; directories carry a
 * trailing "/". "." and ".." are omitted. A missing or unreadable
 * directory yields an empty array.
 */
static bool
cfunc_FileUtil_listDirectory(NoctEnv *env)
{
	NoctValue path, ret, elem;
	const char *path_s;
	char **names = NULL;
	size_t nnames = 0, alloc = 0, i, j;
	bool ok = false;
#if defined(NOCT_TARGET_POSIX)
	DIR *dir = NULL;
#endif

	if (!noct_pin_local(env, 3, &path, &ret, &elem))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s))
		goto cleanup;
	if (!noct_make_empty_array(env, &ret))
		goto cleanup;
	if (directory_backend.operations != NULL &&
	    directory_backend.operations->read != NULL) {
		for (i = 0; i < 65536; i++) {
			char buffer[256];
			char *name;
			size_t length;
			int is_directory = 0;
			int result = directory_backend.operations->read(
				directory_backend.context, path_s, i, buffer,
				sizeof(buffer), &is_directory);

			if (result <= 0)
				break;
			buffer[sizeof(buffer) - 1] = '\0';
			length = strlen(buffer);
			if (length == 0 || !strcmp(buffer, ".") ||
			    !strcmp(buffer, ".."))
				continue;
			name = noct_malloc(length + (is_directory ? 2 : 1));
			if (name == NULL)
				goto cleanup;
			memcpy(name, buffer, length);
			if (is_directory)
				name[length++] = '/';
			name[length] = '\0';
			if (nnames == alloc) {
				char **new_names;
				size_t new_alloc = alloc == 0 ? 16 : alloc * 2;

				new_names = noct_realloc(names,
						 sizeof(*names) * new_alloc);
				if (new_names == NULL) {
					noct_free(name);
					goto cleanup;
				}
				names = new_names;
				alloc = new_alloc;
			}
			names[nnames++] = name;
		}
	} else {
#if defined(NOCT_TARGET_POSIX)
		struct dirent *ent;

		dir = opendir(path_s);
		if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			char full[2048];
			struct stat st;
			size_t len;
			char *name;

			if (strcmp(ent->d_name, ".") == 0 ||
			    strcmp(ent->d_name, "..") == 0)
				continue;

			snprintf(full, sizeof(full), "%s/%s", path_s, ent->d_name);
			len = strlen(ent->d_name);
			name = noct_malloc(len + 2);
			if (name == NULL)
				continue;
			strcpy(name, ent->d_name);
			if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
				strcat(name, "/");

			if (nnames >= alloc) {
				char **nn;
				alloc = alloc == 0 ? 64 : alloc * 2;
				nn = noct_realloc(names, sizeof(char *) * alloc);
				if (nn == NULL) {
					noct_free(name);
					continue;
				}
				names = nn;
			}
			names[nnames++] = name;
		}
		closedir(dir);
		dir = NULL;
		}
#endif
	}

	/* Sort for deterministic completion. */
	for (i = 0; i + 1 < nnames; i++) {
		for (j = i + 1; j < nnames; j++) {
			if (strcmp(names[j], names[i]) < 0) {
				char *t = names[i];
				names[i] = names[j];
				names[j] = t;
			}
		}
	}

	for (i = 0; i < nnames; i++) {
		if (!noct_make_string(env, &elem, names[i]))
			goto cleanup;
		if (!noct_set_array_elem(env, &ret, i, &elem))
			goto cleanup;
	}
	if (!noct_set_return(env, &ret))
		goto cleanup;
	ok = true;
cleanup:
#if defined(NOCT_TARGET_POSIX)
	if (dir != NULL)
		closedir(dir);
#endif
	for (i = 0; i < nnames; i++)
		noct_free(names[i]);
	noct_free(names);
	(void)noct_unpin_local(env, 3, &path, &ret, &elem);
	return ok;
}

/* JIS X 0208 to Unicode, generated in jisx0208.c. */
extern const uint16_t noct_jisx0208_to_ucs[7896];

/* Encode a codepoint as UTF-8; returns the byte count. */
static int
fileutil_utf8_encode(uint32_t cp, char *out)
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
	out[0] = (char)(0xE0 | (cp >> 12));
	out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
	out[2] = (char)(0x80 | (cp & 0x3F));
	return 3;
}

/*
 * FileUtil.readTextEucJp(path)
 *
 * Reads an EUC-JP encoded file and returns its content as a (UTF-8)
 * string. Undecodable bytes become U+FFFD. Returns an empty string
 * for a missing file.
 */
static bool
cfunc_FileUtil_readTextEucJp(NoctEnv *env)
{
	NoctValue path, ret;
	const char *path_s;
	FILE *fp = NULL;
	unsigned char *raw = NULL;
	char *out = NULL;
	long size;
	size_t i, o;
	bool ok = false;

	if (!noct_pin_local(env, 2, &path, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s))
		goto cleanup;

	fp = fopen(path_s, "rb");
	if (fp == NULL) {
		ok = noct_set_return_make_string(env, &ret, "");
		goto cleanup;
	}
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	raw = malloc((size_t)size + 1);
	if (raw == NULL)
		goto cleanup;
	if (fread(raw, 1, (size_t)size, fp) != (size_t)size)
		goto cleanup;
	fclose(fp);
	fp = NULL;

	/* Worst case each EUC byte pair becomes 3 UTF-8 bytes. */
	out = malloc((size_t)size * 3 + 4);
	if (out == NULL)
		goto cleanup;

	o = 0;
	i = 0;
	while (i < (size_t)size) {
		unsigned char c = raw[i];
		uint32_t cp;

		if (c < 0x80) {
			out[o++] = (char)c;
			i++;
			continue;
		}
		if (c == 0x8E && i + 1 < (size_t)size) {
			/* Half-width katakana: 0x8E 0xA1-0xDF. */
			unsigned char c2 = raw[i + 1];
			if (c2 >= 0xA1 && c2 <= 0xDF) {
				cp = 0xFF61 + (uint32_t)(c2 - 0xA1);
				o += (size_t)fileutil_utf8_encode(cp, out + o);
				i += 2;
				continue;
			}
			i++;
			continue;
		}
		if (c >= 0xA1 && c <= 0xF4 && i + 1 < (size_t)size) {
			unsigned char c2 = raw[i + 1];
			if (c2 >= 0xA1 && c2 <= 0xFE) {
				size_t idx = (size_t)(c - 0xA1) * 94 + (size_t)(c2 - 0xA1);
				cp = noct_jisx0208_to_ucs[idx];
				if (cp == 0)
					cp = 0xFFFD;
				o += (size_t)fileutil_utf8_encode(cp, out + o);
				i += 2;
				continue;
			}
		}
		/* Undecodable byte. */
		o += (size_t)fileutil_utf8_encode(0xFFFD, out + o);
		i++;
	}
	out[o] = '\0';

	ok = noct_set_return_make_string(env, &ret, out);

cleanup:
	if (fp != NULL)
		fclose(fp);
	free(raw);
	free(out);
	(void)noct_unpin_local(env, 2, &path, &ret);
	return ok;
}

/*
 * FileUtil.getCurrentDirectory()
 */
static bool
cfunc_FileUtil_getCurrentDirectory(NoctEnv *env)
{
	NoctValue ret;
	char buf[2048];
	bool ok = false;

	if (!noct_pin_local(env, 1, &ret))
		return false;
#if defined(NOCT_TARGET_WINDOWS)
	if (GetCurrentDirectoryA((DWORD)sizeof(buf), buf) == 0)
		buf[0] = '\0';
#else
	if (getcwd(buf, sizeof(buf)) == NULL)
		buf[0] = '\0';
#endif
	ok = noct_set_return_make_string(env, &ret, buf);
	(void)noct_unpin_local(env, 1, &ret);
	return ok;
}

/*
 * FileUtil.setCurrentDirectory(path)
 */
static bool
cfunc_FileUtil_setCurrentDirectory(NoctEnv *env)
{
	NoctValue path, ret;
	const char *path_s;
	int r;
	bool ok = false;

	if (!noct_pin_local(env, 2, &path, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s))
		goto cleanup;
#if defined(NOCT_TARGET_WINDOWS)
	r = SetCurrentDirectoryA(path_s) ? 0 : -1;
#else
	r = chdir(path_s);
#endif
	ok = noct_set_return_make_int(env, &ret, r == 0 ? 1 : 0);
cleanup:
	(void)noct_unpin_local(env, 2, &path, &ret);
	return ok;
}

/*
 * FileUtil.getHomeDirectory()
 *
 * HOME on POSIX; USERPROFILE on Windows.
 */
static bool
cfunc_FileUtil_getHomeDirectory(NoctEnv *env)
{
	NoctValue ret;
	const char *home;
	bool ok = false;

	if (!noct_pin_local(env, 1, &ret))
		return false;
	home = getenv("HOME");
#if defined(NOCT_TARGET_WINDOWS)
	if (home == NULL || home[0] == '\0')
		home = getenv("USERPROFILE");
#endif
	if (home == NULL)
		home = "";
	ok = noct_set_return_make_string(env, &ret, home);
	(void)noct_unpin_local(env, 1, &ret);
	return ok;
}

static bool
cfunc_FileUtil_getFileSize(NoctEnv *env)
{
	NoctValue path, ret;
	const char *path_s;
	FILE *file = NULL;
	long size;
	bool ok = false;

	if (!noct_pin_local(env, 2, &path, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s))
		goto cleanup;
	file = fopen(path_s, "rb");
	if (file == NULL) {
		ok = noct_set_return_make_int(env, &ret, 0);
		goto cleanup;
	}
	if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0) {
		noct_error(env, N_TR("Cannot determine file size."));
		goto cleanup;
	}
	if (!noct_set_return_make_int_long(env, &ret, (size_t)size))
		goto cleanup;
	ok = true;
cleanup:
	if (file != NULL)
		(void)fclose(file);
	(void)noct_unpin_local(env, 2, &path, &ret);
	return ok;
}

static bool
cfunc_FileUtil_readText(NoctEnv *env)
{
	NoctValue path, ret;
	const char *path_s;
	FILE *file = NULL;
	char *data = NULL;
	long length;
	bool ok = false;

	if (!noct_pin_local(env, 2, &path, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s))
		goto cleanup;
	file = fopen(path_s, "rb");
	if (file == NULL) {
		noct_error(env, N_TR("Cannot open file %s."), path_s);
		goto cleanup;
	}
	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET) != 0) {
		noct_error(env, N_TR("Cannot determine file size."));
		goto cleanup;
	}
	data = noct_malloc((size_t)length + 1U);
	if (data == NULL) {
		noct_error(env, N_TR("Out of memory."));
		goto cleanup;
	}
	if (fread(data, 1, (size_t)length, file) != (size_t)length) {
		noct_error(env, N_TR("Cannot read file %s."), path_s);
		goto cleanup;
	}
	data[length] = '\0';
	if (!noct_set_return_make_string(env, &ret, data))
		goto cleanup;
	ok = true;
cleanup:
	if (file != NULL)
		(void)fclose(file);
	if (data != NULL)
		noct_free(data);
	(void)noct_unpin_local(env, 2, &path, &ret);
	return ok;
}

static bool
cfunc_FileUtil_writeText(NoctEnv *env)
{
	NoctValue path, text, ret;
	const char *path_s, *text_s;
	FILE *file = NULL;
	size_t length;
	bool ok = false;

	if (!noct_pin_local(env, 3, &path, &text, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s) ||
	    !noct_get_arg_check_string(env, 1, &text, &text_s))
		goto cleanup;
	file = fopen(path_s, "wb");
	if (file == NULL) {
		noct_error(env, N_TR("Cannot open file %s."), path_s);
		goto cleanup;
	}
	length = strlen(text_s);
	if (fwrite(text_s, 1, length, file) != length || fflush(file) != 0) {
		noct_error(env, N_TR("Cannot write file %s."), path_s);
		goto cleanup;
	}
	if (!noct_set_return_make_int(env, &ret, 1))
		goto cleanup;
	ok = true;
cleanup:
	if (file != NULL && fclose(file) != 0 && ok) {
		noct_error(env, N_TR("Cannot close file %s."), path_s);
		ok = false;
	}
	(void)noct_unpin_local(env, 3, &path, &text, &ret);
	return ok;
}

static bool
cfunc_FileUtil_readForEachLine(NoctEnv *env)
{
	char buffer[8192];
	NoctValue path, function_value, line, ret;
	NoctFunc *function;
	const char *path_s;
	FILE *file = NULL;
	bool ok = false;

	if (!noct_pin_local(env, 4, &path, &function_value, &line, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s) ||
	    !noct_get_arg_check_func(env, 1, &function_value, &function))
		goto cleanup;
	file = fopen(path_s, "rb");
	if (file == NULL) {
		noct_error(env, N_TR("Cannot open file %s."), path_s);
		goto cleanup;
	}
	while (fgets(buffer, sizeof(buffer), file) != NULL) {
		size_t length = strlen(buffer);

		if (length != 0 && buffer[length - 1] == '\n')
			buffer[length - 1] = '\0';
		if (!noct_make_string(env, &line, buffer) ||
		    !noct_call(env, function, 1, &line, &ret))
			goto cleanup;
	}
	if (ferror(file)) {
		noct_error(env, N_TR("Cannot read file %s."), path_s);
		goto cleanup;
	}
	if (!noct_set_return_make_int(env, &ret, 1))
		goto cleanup;
	ok = true;
cleanup:
	if (file != NULL)
		(void)fclose(file);
	(void)noct_unpin_local(env, 4, &path, &function_value, &line, &ret);
	return ok;
}

static bool
cfunc_FileUtil_writeForEachLine(NoctEnv *env)
{
	NoctValue path, lines, line, ret;
	const char *path_s, *text;
	FILE *file = NULL;
	size_t count, index;
	bool ok = false;

	if (!noct_pin_local(env, 4, &path, &lines, &line, &ret))
		return false;
	if (!noct_get_arg_check_string(env, 0, &path, &path_s) ||
	    !noct_get_arg_check_array(env, 1, &lines) ||
	    !noct_get_array_size(env, &lines, &count))
		goto cleanup;
	file = fopen(path_s, "wb");
	if (file == NULL) {
		noct_error(env, N_TR("Cannot open file %s."), path_s);
		goto cleanup;
	}
	for (index = 0; index < count; index++) {
		if (!noct_get_array_elem(env, &lines, index, &line) ||
		    !noct_get_string(env, &line, &text) ||
		    fprintf(file, "%s\n", text) < 0)
			goto cleanup;
	}
	if (fflush(file) != 0) {
		noct_error(env, N_TR("Cannot write file %s."), path_s);
		goto cleanup;
	}
	if (!noct_set_return_make_int(env, &ret, 1))
		goto cleanup;
	ok = true;
cleanup:
	if (file != NULL && fclose(file) != 0 && ok) {
		noct_error(env, N_TR("Cannot close file %s."), path_s);
		ok = false;
	}
	(void)noct_unpin_local(env, 4, &path, &lines, &line, &ret);
	return ok;
}
