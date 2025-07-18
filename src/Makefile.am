lib_LIBRARIES = libnoctlang.a
libnoctlang_a_SOURCES = \
  lexer.yy.c \
  parser.tab.c \
  ast.c \
  hir.c \
  lir.c \
  runtime.c \
  interpreter.c \
  execution.c \
  intrinsics.c \
  jit-common.c \
  jit-x86_64.c \
  jit-x86.c \
  jit-arm64.c \
  jit-arm32.c \
  jit-ppc64.c \
  jit-ppc32.c \
  jit-mips64.c \
  jit-mips32.c
libnoctlang_a_CPPFLAGS = -I$(top_srcdir)/include -include config.h

bin_PROGRAMS = noct
noct_SOURCES = \
  lexer.yy.c \
  parser.tab.c \
  ast.c \
  hir.c \
  lir.c \
  runtime.c \
  interpreter.c \
  execution.c \
  intrinsics.c \
  jit-common.c \
  jit-x86_64.c \
  jit-x86.c \
  jit-arm64.c \
  jit-arm32.c \
  jit-ppc64.c \
  jit-ppc32.c \
  jit-mips64.c \
  jit-mips32.c \
  cback.c \
  elback.c \
  cli.c
if USE_NLS
noct_CPPFLAGS = -DUSE_GETTEXT -I$(top_srcdir)/include -include config.h
else
noct_CPPFLAGS = -I$(top_srcdir)/include -include config.h
endif
