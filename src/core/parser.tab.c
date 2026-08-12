/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         ast_yyparse
#define yylex           ast_yylex
#define yyerror         ast_yyerror
#define yydebug         ast_yydebug
#define yynerrs         ast_yynerrs
#define yylval          ast_yylval
#define yychar          ast_yychar
#define yylloc          ast_yylloc

/* First part of user prologue.  */
#line 1 "parser.y"

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#include <noct/c89compat.h>
#include "ast.h"

#include <stdio.h>
#include <string.h>

#undef DEBUG
#ifdef DEBUG
static void print_debug(const char *s);
#define debug(s) print_debug(s)
#else
#define debug(s)
#endif

#define YYMALLOC ast_malloc
#define YYFREE ast_free

extern int ast_error_line;
extern int ast_error_column;

int ast_yylex(void *);
void ast_yyerror(void *, char *s);
void *ast_malloc(size_t size); 
void ast_free(void *p);

/* Internal: called back from the parser. */
struct ast_func_list *ast_accept_func_list(struct ast_func_list *impl_list, struct ast_func *func);
struct ast_func *ast_accept_func(int flags, char *name, struct ast_param_list *param_list, char *return_type_name, struct ast_stmt_list *stmt_list);
bool ast_accept_toplevel_var(int line, char *name, struct ast_expr *rhs, bool is_let, bool is_static);
bool ast_accept_toplevel_accel_var(int line, char *name, struct ast_expr *rhs);
bool ast_accept_toplevel_class(int line, char *name, struct ast_kv_list *kv_list);
bool ast_accept_require(char *name);
struct ast_param_list *ast_accept_param_list(struct ast_param_list *param_list, char *name);
struct ast_param_list *ast_accept_param_list_typed(struct ast_param_list *param_list, char *name, char *type_name);
struct ast_stmt_list *ast_accept_stmt_list(struct ast_stmt_list *stmt_list, struct ast_stmt *stmt);
void ast_accept_stmt(struct ast_stmt *stmt, int line);
struct ast_stmt *ast_accept_expr_stmt(int line, struct ast_expr *expr);
struct ast_stmt *ast_accept_assign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs, bool is_var, bool is_let);
struct ast_stmt *ast_accept_assign_stmt_typed(int line, struct ast_expr *lhs, struct ast_expr *rhs, bool is_var, bool is_let, char *type_name);
struct ast_expr *ast_accept_term_expr(struct ast_term *term);
struct ast_term *ast_accept_int_term(int i);
char *ast_strdup(const char *s);
struct ast_stmt *ast_accept_plusassign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs);
struct ast_stmt *ast_accept_minusassign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs);
struct ast_stmt *ast_accept_mulassign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs);
struct ast_stmt *ast_accept_divassign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs);
struct ast_stmt *ast_accept_modassign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs);
struct ast_stmt *ast_accept_andassign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs);
struct ast_stmt *ast_accept_orassign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs);
struct ast_stmt *ast_accept_shlassign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs);
struct ast_stmt *ast_accept_shrassign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs);
struct ast_stmt *ast_accept_plusplus_stmt(int line, struct ast_expr *expr);
struct ast_stmt *ast_accept_minusminus_stmt(int line, struct ast_expr *expr);
struct ast_stmt *ast_accept_if_stmt(int line, struct ast_expr *cond, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_if_stmt_single(int line, struct ast_expr *cond, struct ast_stmt *stmt);
struct ast_stmt *ast_accept_elif_stmt(int line, struct ast_expr *cond, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_elif_stmt_single(int line, struct ast_expr *cond, struct ast_stmt *stmt);
struct ast_stmt *ast_accept_else_stmt(int line, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_else_stmt_single(int line, struct ast_stmt *stmt);
struct ast_stmt *ast_accept_while_stmt(int line, struct ast_expr *cond, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_while_stmt_single(int line, struct ast_expr *cond, struct ast_stmt *stmt);
struct ast_stmt *ast_accept_for_kv_stmt(int line, char *key_sym, char *val_sym, struct ast_expr *array, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_for_kv_stmt_single(int line, char *key_sym, char *val_sym, struct ast_expr *array, struct ast_stmt *stmt);
struct ast_stmt *ast_accept_for_v_stmt(int line, char *iter_sym, struct ast_expr *array, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_for_v_stmt_single(int line, char *iter_sym, struct ast_expr *array, struct ast_stmt *stmt);
struct ast_stmt *ast_accept_for_range_stmt(int line, char *counter_sym, struct ast_expr *start, struct ast_expr *stop, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_for_range_stmt_single(int line, char *counter_sym, struct ast_expr *start, struct ast_expr *stop, struct ast_stmt *stmt);
struct ast_stmt *ast_accept_return_stmt(int line, struct ast_expr *expr);
struct ast_stmt *ast_accept_break_stmt(int line);
struct ast_stmt *ast_accept_continue_stmt(int line);
struct ast_expr *ast_accept_term_expr(struct ast_term *term);
struct ast_expr *ast_accept_lt_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_lte_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_gt_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_gte_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_eq_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_neq_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_plus_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_minus_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_mul_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_div_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_mod_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_and_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_land_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_lor_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_or_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_xor_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_shl_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_shr_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_neg_expr(struct ast_expr *expr);
struct ast_expr *ast_accept_not_expr(struct ast_expr *expr);
struct ast_expr *ast_accept_par_expr(struct ast_expr *expr);
struct ast_expr *ast_accept_subscr_expr(struct ast_expr *expr1, struct ast_expr *expr2);
char *ast_accept_type_extent_int(int value);
char *ast_accept_type_extent_list(char *list, char *extent);
char *ast_accept_shaped_type(char *name, char *extents);
struct ast_expr *ast_accept_dot_expr(struct ast_expr *obj, char *symbol);
struct ast_expr *ast_accept_call_expr(struct ast_expr *func, struct ast_arg_list *arg_list);
struct ast_expr *ast_accept_array_expr(struct ast_arg_list *arg_list);
struct ast_expr *ast_accept_dict_expr(struct ast_kv_list *kv_list);
struct ast_expr *ast_accept_class_expr(struct ast_kv_list *kv_list);
struct ast_expr *ast_accept_func_expr(struct ast_param_list *param_list, struct ast_stmt_list *stmt_list);
struct ast_expr *ast_accept_new_expr(char *cls, struct ast_kv_list *kv_list);
struct ast_expr *ast_accept_extend_expr(char *cls, struct ast_kv_list *kv_list);
struct ast_kv_list *ast_accept_kv_list(struct ast_kv_list *kv_list, struct ast_kv *kv);
struct ast_kv *ast_accept_kv(char *key, struct ast_expr *value);
struct ast_term *ast_accept_int_term(int i);
struct ast_term *ast_accept_long_term(int64_t i);
struct ast_term *ast_accept_float_term(float f);
struct ast_term *ast_accept_double_term(double lf);
struct ast_term *ast_accept_str_term(char *s);
struct ast_term *ast_accept_symbol_term(char *symbol);
struct ast_term *ast_accept_empty_array_term(void);
struct ast_term *ast_accept_empty_dict_term(void);
struct ast_arg_list *ast_accept_arg_list(struct ast_arg_list *arg_list, struct ast_expr *expr);
struct ast_stmt *ast_accept_gpu_launch_stmt(int line, char *name,
	struct ast_expr *grid, struct ast_expr *block, struct ast_arg_list *args);
struct ast_stmt *ast_accept_shared_stmt(int line, struct ast_expr *lhs,
	struct ast_expr *rhs, bool is_var);

#line 129 "parser.y"

extern void ast_yyerror(void *scanner, char *s);

#line 210 "parser.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_TOKEN_SYMBOL = 3,               /* TOKEN_SYMBOL  */
  YYSYMBOL_TOKEN_STR = 4,                  /* TOKEN_STR  */
  YYSYMBOL_TOKEN_INT = 5,                  /* TOKEN_INT  */
  YYSYMBOL_TOKEN_LONG = 6,                 /* TOKEN_LONG  */
  YYSYMBOL_TOKEN_FLOAT = 7,                /* TOKEN_FLOAT  */
  YYSYMBOL_TOKEN_DOUBLE = 8,               /* TOKEN_DOUBLE  */
  YYSYMBOL_TOKEN_FUNC = 9,                 /* TOKEN_FUNC  */
  YYSYMBOL_TOKEN_CLASS = 10,               /* TOKEN_CLASS  */
  YYSYMBOL_TOKEN_NEW = 11,                 /* TOKEN_NEW  */
  YYSYMBOL_TOKEN_LAMBDA = 12,              /* TOKEN_LAMBDA  */
  YYSYMBOL_TOKEN_LARR = 13,                /* TOKEN_LARR  */
  YYSYMBOL_TOKEN_RARR = 14,                /* TOKEN_RARR  */
  YYSYMBOL_TOKEN_PLUS = 15,                /* TOKEN_PLUS  */
  YYSYMBOL_TOKEN_MINUS = 16,               /* TOKEN_MINUS  */
  YYSYMBOL_TOKEN_MUL = 17,                 /* TOKEN_MUL  */
  YYSYMBOL_TOKEN_DIV = 18,                 /* TOKEN_DIV  */
  YYSYMBOL_TOKEN_MOD = 19,                 /* TOKEN_MOD  */
  YYSYMBOL_TOKEN_SHL = 20,                 /* TOKEN_SHL  */
  YYSYMBOL_TOKEN_SHR = 21,                 /* TOKEN_SHR  */
  YYSYMBOL_TOKEN_ASSIGN = 22,              /* TOKEN_ASSIGN  */
  YYSYMBOL_TOKEN_PLUSASSIGN = 23,          /* TOKEN_PLUSASSIGN  */
  YYSYMBOL_TOKEN_MINUSASSIGN = 24,         /* TOKEN_MINUSASSIGN  */
  YYSYMBOL_TOKEN_MULASSIGN = 25,           /* TOKEN_MULASSIGN  */
  YYSYMBOL_TOKEN_DIVASSIGN = 26,           /* TOKEN_DIVASSIGN  */
  YYSYMBOL_TOKEN_MODASSIGN = 27,           /* TOKEN_MODASSIGN  */
  YYSYMBOL_TOKEN_ANDASSIGN = 28,           /* TOKEN_ANDASSIGN  */
  YYSYMBOL_TOKEN_ORASSIGN = 29,            /* TOKEN_ORASSIGN  */
  YYSYMBOL_TOKEN_SHLASSIGN = 30,           /* TOKEN_SHLASSIGN  */
  YYSYMBOL_TOKEN_SHRASSIGN = 31,           /* TOKEN_SHRASSIGN  */
  YYSYMBOL_TOKEN_PLUSPLUS = 32,            /* TOKEN_PLUSPLUS  */
  YYSYMBOL_TOKEN_MINUSMINUS = 33,          /* TOKEN_MINUSMINUS  */
  YYSYMBOL_TOKEN_ANDAND = 34,              /* TOKEN_ANDAND  */
  YYSYMBOL_TOKEN_OROR = 35,                /* TOKEN_OROR  */
  YYSYMBOL_TOKEN_LPAR = 36,                /* TOKEN_LPAR  */
  YYSYMBOL_TOKEN_RPAR = 37,                /* TOKEN_RPAR  */
  YYSYMBOL_TOKEN_RPAR_LBLK = 38,           /* TOKEN_RPAR_LBLK  */
  YYSYMBOL_TOKEN_LBLK = 39,                /* TOKEN_LBLK  */
  YYSYMBOL_TOKEN_LBLK_BLK = 40,            /* TOKEN_LBLK_BLK  */
  YYSYMBOL_TOKEN_RBLK = 41,                /* TOKEN_RBLK  */
  YYSYMBOL_TOKEN_SEMICOLON = 42,           /* TOKEN_SEMICOLON  */
  YYSYMBOL_TOKEN_COLON = 43,               /* TOKEN_COLON  */
  YYSYMBOL_TOKEN_DOT = 44,                 /* TOKEN_DOT  */
  YYSYMBOL_TOKEN_COMMA = 45,               /* TOKEN_COMMA  */
  YYSYMBOL_TOKEN_IF = 46,                  /* TOKEN_IF  */
  YYSYMBOL_TOKEN_ELSE = 47,                /* TOKEN_ELSE  */
  YYSYMBOL_TOKEN_ELSE_LBLK = 48,           /* TOKEN_ELSE_LBLK  */
  YYSYMBOL_TOKEN_ELSEIF = 49,              /* TOKEN_ELSEIF  */
  YYSYMBOL_TOKEN_WHILE = 50,               /* TOKEN_WHILE  */
  YYSYMBOL_TOKEN_FOR = 51,                 /* TOKEN_FOR  */
  YYSYMBOL_TOKEN_IN = 52,                  /* TOKEN_IN  */
  YYSYMBOL_TOKEN_DOTDOT = 53,              /* TOKEN_DOTDOT  */
  YYSYMBOL_TOKEN_GT = 54,                  /* TOKEN_GT  */
  YYSYMBOL_TOKEN_GTE = 55,                 /* TOKEN_GTE  */
  YYSYMBOL_TOKEN_LT = 56,                  /* TOKEN_LT  */
  YYSYMBOL_TOKEN_LTE = 57,                 /* TOKEN_LTE  */
  YYSYMBOL_TOKEN_EQ = 58,                  /* TOKEN_EQ  */
  YYSYMBOL_TOKEN_NEQ = 59,                 /* TOKEN_NEQ  */
  YYSYMBOL_TOKEN_RETURN = 60,              /* TOKEN_RETURN  */
  YYSYMBOL_TOKEN_BREAK = 61,               /* TOKEN_BREAK  */
  YYSYMBOL_TOKEN_CONTINUE = 62,            /* TOKEN_CONTINUE  */
  YYSYMBOL_TOKEN_RPAR_DARROW_LBLK = 63,    /* TOKEN_RPAR_DARROW_LBLK  */
  YYSYMBOL_TOKEN_AND = 64,                 /* TOKEN_AND  */
  YYSYMBOL_TOKEN_OR = 65,                  /* TOKEN_OR  */
  YYSYMBOL_TOKEN_XOR = 66,                 /* TOKEN_XOR  */
  YYSYMBOL_TOKEN_VAR = 67,                 /* TOKEN_VAR  */
  YYSYMBOL_TOKEN_LET = 68,                 /* TOKEN_LET  */
  YYSYMBOL_TOKEN_EXTEND = 69,              /* TOKEN_EXTEND  */
  YYSYMBOL_TOKEN_STATIC = 70,              /* TOKEN_STATIC  */
  YYSYMBOL_TOKEN_INLINE = 71,              /* TOKEN_INLINE  */
  YYSYMBOL_TOKEN_REQUIRE = 72,             /* TOKEN_REQUIRE  */
  YYSYMBOL_TOKEN_ACCEL = 73,               /* TOKEN_ACCEL  */
  YYSYMBOL_TOKEN_GPU = 74,                 /* TOKEN_GPU  */
  YYSYMBOL_TOKEN_DUNDER_ACCEL = 75,        /* TOKEN_DUNDER_ACCEL  */
  YYSYMBOL_TOKEN_DUNDER_GPU = 76,          /* TOKEN_DUNDER_GPU  */
  YYSYMBOL_TOKEN_DUNDER_FAST = 77,         /* TOKEN_DUNDER_FAST  */
  YYSYMBOL_TOKEN_GPU_LAUNCH_L = 78,        /* TOKEN_GPU_LAUNCH_L  */
  YYSYMBOL_TOKEN_GPU_LAUNCH_R = 79,        /* TOKEN_GPU_LAUNCH_R  */
  YYSYMBOL_TOKEN_SHARED = 80,              /* TOKEN_SHARED  */
  YYSYMBOL_TOKEN_NOT = 81,                 /* TOKEN_NOT  */
  YYSYMBOL_UNARYMINUS = 82,                /* UNARYMINUS  */
  YYSYMBOL_CALL = 83,                      /* CALL  */
  YYSYMBOL_YYACCEPT = 84,                  /* $accept  */
  YYSYMBOL_func_list = 85,                 /* func_list  */
  YYSYMBOL_toplevel_decl = 86,             /* toplevel_decl  */
  YYSYMBOL_func_prefix = 87,               /* func_prefix  */
  YYSYMBOL_func = 88,                      /* func  */
  YYSYMBOL_param_list = 89,                /* param_list  */
  YYSYMBOL_type_name = 90,                 /* type_name  */
  YYSYMBOL_type_extent = 91,               /* type_extent  */
  YYSYMBOL_type_extent_list = 92,          /* type_extent_list  */
  YYSYMBOL_stmt_list = 93,                 /* stmt_list  */
  YYSYMBOL_stmt = 94,                      /* stmt  */
  YYSYMBOL_gpu_launch_stmt = 95,           /* gpu_launch_stmt  */
  YYSYMBOL_expr_stmt = 96,                 /* expr_stmt  */
  YYSYMBOL_assign_stmt = 97,               /* assign_stmt  */
  YYSYMBOL_plusassign_stmt = 98,           /* plusassign_stmt  */
  YYSYMBOL_minusassign_stmt = 99,          /* minusassign_stmt  */
  YYSYMBOL_mulassign_stmt = 100,           /* mulassign_stmt  */
  YYSYMBOL_divassign_stmt = 101,           /* divassign_stmt  */
  YYSYMBOL_modassign_stmt = 102,           /* modassign_stmt  */
  YYSYMBOL_andassign_stmt = 103,           /* andassign_stmt  */
  YYSYMBOL_orassign_stmt = 104,            /* orassign_stmt  */
  YYSYMBOL_shlassign_stmt = 105,           /* shlassign_stmt  */
  YYSYMBOL_shrassign_stmt = 106,           /* shrassign_stmt  */
  YYSYMBOL_plusplus_stmt = 107,            /* plusplus_stmt  */
  YYSYMBOL_minusminus_stmt = 108,          /* minusminus_stmt  */
  YYSYMBOL_if_stmt = 109,                  /* if_stmt  */
  YYSYMBOL_elif_stmt = 110,                /* elif_stmt  */
  YYSYMBOL_else_stmt = 111,                /* else_stmt  */
  YYSYMBOL_while_stmt = 112,               /* while_stmt  */
  YYSYMBOL_for_stmt = 113,                 /* for_stmt  */
  YYSYMBOL_return_stmt = 114,              /* return_stmt  */
  YYSYMBOL_break_stmt = 115,               /* break_stmt  */
  YYSYMBOL_continue_stmt = 116,            /* continue_stmt  */
  YYSYMBOL_expr = 117,                     /* expr  */
  YYSYMBOL_call_expr = 118,                /* call_expr  */
  YYSYMBOL_lambda_expr = 119,              /* lambda_expr  */
  YYSYMBOL_arg_list = 120,                 /* arg_list  */
  YYSYMBOL_multi_index_list = 121,         /* multi_index_list  */
  YYSYMBOL_kv_list = 122,                  /* kv_list  */
  YYSYMBOL_kv = 123,                       /* kv  */
  YYSYMBOL_property_name = 124,            /* property_name  */
  YYSYMBOL_term = 125                      /* term  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  34
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   3609

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  84
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  42
/* YYNRULES -- Number of rules.  */
#define YYNRULES  166
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  389

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   338


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   249,   249,   254,   259,   264,   270,   276,   282,   288,
     294,   300,   306,   312,   319,   323,   327,   331,   335,   339,
     343,   347,   351,   355,   361,   367,   373,   380,   386,   392,
     398,   405,   410,   415,   420,   426,   430,   435,   440,   445,
     450,   454,   461,   465,   471,   475,   479,   483,   487,   491,
     495,   499,   503,   507,   511,   515,   519,   523,   527,   531,
     535,   539,   543,   547,   551,   555,   560,   565,   571,   577,
     582,   587,   592,   596,   600,   605,   610,   616,   622,   628,
     634,   640,   646,   652,   658,   664,   670,   676,   682,   687,
     693,   698,   704,   709,   715,   720,   726,   731,   736,   741,
     746,   751,   757,   762,   767,   773,   779,   784,   789,   794,
     800,   805,   810,   815,   820,   825,   830,   835,   840,   845,
     850,   855,   860,   865,   870,   875,   880,   885,   890,   895,
     900,   905,   909,   914,   919,   925,   931,   935,   940,   945,
     950,   956,   961,   967,   972,   978,   983,   989,   994,   999,
    1004,  1010,  1015,  1021,  1025,  1029,  1033,  1037,  1041,  1046,
    1051,  1056,  1061,  1066,  1071,  1076,  1081
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "TOKEN_SYMBOL",
  "TOKEN_STR", "TOKEN_INT", "TOKEN_LONG", "TOKEN_FLOAT", "TOKEN_DOUBLE",
  "TOKEN_FUNC", "TOKEN_CLASS", "TOKEN_NEW", "TOKEN_LAMBDA", "TOKEN_LARR",
  "TOKEN_RARR", "TOKEN_PLUS", "TOKEN_MINUS", "TOKEN_MUL", "TOKEN_DIV",
  "TOKEN_MOD", "TOKEN_SHL", "TOKEN_SHR", "TOKEN_ASSIGN",
  "TOKEN_PLUSASSIGN", "TOKEN_MINUSASSIGN", "TOKEN_MULASSIGN",
  "TOKEN_DIVASSIGN", "TOKEN_MODASSIGN", "TOKEN_ANDASSIGN",
  "TOKEN_ORASSIGN", "TOKEN_SHLASSIGN", "TOKEN_SHRASSIGN", "TOKEN_PLUSPLUS",
  "TOKEN_MINUSMINUS", "TOKEN_ANDAND", "TOKEN_OROR", "TOKEN_LPAR",
  "TOKEN_RPAR", "TOKEN_RPAR_LBLK", "TOKEN_LBLK", "TOKEN_LBLK_BLK",
  "TOKEN_RBLK", "TOKEN_SEMICOLON", "TOKEN_COLON", "TOKEN_DOT",
  "TOKEN_COMMA", "TOKEN_IF", "TOKEN_ELSE", "TOKEN_ELSE_LBLK",
  "TOKEN_ELSEIF", "TOKEN_WHILE", "TOKEN_FOR", "TOKEN_IN", "TOKEN_DOTDOT",
  "TOKEN_GT", "TOKEN_GTE", "TOKEN_LT", "TOKEN_LTE", "TOKEN_EQ",
  "TOKEN_NEQ", "TOKEN_RETURN", "TOKEN_BREAK", "TOKEN_CONTINUE",
  "TOKEN_RPAR_DARROW_LBLK", "TOKEN_AND", "TOKEN_OR", "TOKEN_XOR",
  "TOKEN_VAR", "TOKEN_LET", "TOKEN_EXTEND", "TOKEN_STATIC", "TOKEN_INLINE",
  "TOKEN_REQUIRE", "TOKEN_ACCEL", "TOKEN_GPU", "TOKEN_DUNDER_ACCEL",
  "TOKEN_DUNDER_GPU", "TOKEN_DUNDER_FAST", "TOKEN_GPU_LAUNCH_L",
  "TOKEN_GPU_LAUNCH_R", "TOKEN_SHARED", "TOKEN_NOT", "UNARYMINUS", "CALL",
  "$accept", "func_list", "toplevel_decl", "func_prefix", "func",
  "param_list", "type_name", "type_extent", "type_extent_list",
  "stmt_list", "stmt", "gpu_launch_stmt", "expr_stmt", "assign_stmt",
  "plusassign_stmt", "minusassign_stmt", "mulassign_stmt",
  "divassign_stmt", "modassign_stmt", "andassign_stmt", "orassign_stmt",
  "shlassign_stmt", "shrassign_stmt", "plusplus_stmt", "minusminus_stmt",
  "if_stmt", "elif_stmt", "else_stmt", "while_stmt", "for_stmt",
  "return_stmt", "break_stmt", "continue_stmt", "expr", "call_expr",
  "lambda_expr", "arg_list", "multi_index_list", "kv_list", "kv",
  "property_name", "term", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-181)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-32)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     680,  -181,    16,    32,    44,   135,    94,    -7,    89,    90,
      92,    93,    95,  -181,   103,  -181,    68,    99,   107,  -181,
     125,   127,    19,   123,   128,   129,   131,   101,  -181,   133,
    -181,  -181,  -181,  -181,  -181,  -181,  -181,    98,    53,   508,
     508,   124,   132,  -181,   144,  -181,  -181,  -181,  -181,  -181,
     137,     1,  -181,   114,  -181,  -181,  -181,  -181,  -181,  -181,
       8,  -181,   118,  -181,  -181,  -181,  -181,  -181,  -181,   134,
     161,   106,   508,  1306,    77,   163,   508,  1966,  -181,  -181,
    -181,  2020,   508,   508,  -181,   508,   138,   145,  -181,   -25,
     508,  -181,   173,   508,   523,   139,  -181,  3370,    -4,     4,
     -36,  -181,   -37,  2074,  -181,    17,   141,     4,   508,   508,
     508,   508,   508,   508,   508,   508,   508,   508,   350,  -181,
     112,   508,   508,   508,   508,   508,   508,   508,   508,   508,
    -181,  2128,  2182,  2236,    27,    27,   226,   146,  -181,   171,
    3370,  -181,  3370,  -181,    18,   550,  -181,   508,   331,  -181,
    -181,  -181,   597,  1534,     0,   332,   332,     4,     4,     4,
     628,   628,  3456,  3424,  -181,   -22,  -181,   284,   284,   284,
     284,   783,   783,  3550,  3488,  3520,  -181,  -181,  -181,   154,
    -181,  -181,   152,   115,  -181,   176,  1227,  -181,   189,   190,
     191,  1320,   155,   157,   508,   508,    -1,  -181,  -181,  -181,
    -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,
    -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,
    1426,    27,   410,   164,  -181,  -181,    20,  3370,  -181,   489,
    -181,    23,  -181,   508,  -181,   508,  -181,    47,  -181,   508,
     508,  -181,   568,   508,   508,   225,  -181,  2290,  -181,  -181,
    1588,  1642,   508,   508,   508,   508,   508,   508,   508,   508,
     508,   508,   508,   508,   193,   196,  -181,   201,  -181,    27,
    -181,  -181,  -181,  3370,  3370,  -181,  -181,  -181,   -21,   647,
    2344,  1696,  -181,  1750,  1804,   -27,  -181,   508,    27,   508,
      27,  2398,  2452,  2506,  2560,  2614,  2668,  2722,  2776,  2830,
    2884,  2938,  2992,  -181,  -181,  -181,  -181,  -181,    47,  -181,
     508,  1227,  -181,  1227,  -181,  1227,  -181,   238,   508,  3046,
     -11,  3100,   227,   508,   508,  -181,  -181,  -181,  -181,  -181,
    -181,  -181,  -181,  -181,  -181,   726,  -181,  1361,  -181,   805,
    -181,   884,  -181,   963,   200,  1480,  -181,   508,  -181,  -181,
     508,  3154,  3208,  -181,   217,  -181,  -181,  -181,   508,  1227,
    -181,   508,  3262,  3316,  -181,  -181,   429,  1858,  -181,  1042,
    1912,  -181,  -181,   212,   -16,  1227,  -181,  -181,  1227,  -181,
    -181,   213,  -181,  1121,  -181,  1200,  -181,  -181,  -181
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,    14,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     3,     0,     2,     0,     0,     0,    15,
       0,     0,     0,     0,     0,     0,     0,     0,    23,     0,
      25,    17,    19,    21,     1,     5,     4,     0,     0,     0,
       0,     0,     0,    16,     0,    24,    26,    18,    20,    13,
       0,     0,   153,     0,    12,   154,   155,   156,   157,   158,
       0,   149,     0,   164,   163,   159,   160,   161,   162,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   131,   136,
     106,     0,     0,     0,    22,     0,    31,     0,    42,     0,
       0,    11,     0,     0,     0,     0,   165,   145,     0,   128,
     164,    42,     0,     0,   166,     0,     0,   129,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     6,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       8,     0,     0,     0,     0,     0,     0,     0,    42,     0,
     151,   150,   152,   135,     0,     0,   132,     0,     0,    42,
     107,   133,     0,     0,     0,   121,   122,   123,   124,   125,
     126,   127,   114,   113,   142,     0,   130,   117,   118,   115,
     116,   119,   120,   111,   110,   112,     9,    10,     7,    35,
      37,    32,     0,   164,    28,     0,     0,    42,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    43,    45,    44,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
       0,     0,     0,    33,   134,   140,     0,   146,   144,     0,
     139,     0,   108,     0,   109,     0,   141,     0,    42,     0,
       0,    93,     0,     0,     0,     0,   103,     0,   104,   105,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    68,     0,    27,     0,
     137,   143,   138,   147,   148,    39,    38,    40,     0,     0,
       0,     0,    92,     0,     0,     0,   102,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    86,    87,    42,    34,    36,     0,    30,
       0,     0,    42,     0,    42,     0,    42,     0,     0,     0,
       0,     0,     0,     0,     0,    69,    77,    78,    79,    80,
      81,    82,    83,    84,    85,     0,    41,     0,    89,     0,
      91,     0,    95,     0,     0,     0,    70,     0,    75,    71,
       0,     0,     0,    29,     0,    88,    90,    94,     0,     0,
      42,     0,     0,     0,    72,    73,     0,     0,    99,     0,
       0,    74,    76,     0,     0,     0,    42,    98,     0,    42,
      67,     0,    97,     0,   101,     0,    66,    96,   100
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -181,  -181,   244,  -181,   245,   185,  -130,   -48,  -181,   -46,
    -180,  -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,
    -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,  -181,
    -181,  -181,  -181,   -39,  -181,  -181,  -115,  -181,   -52,   169,
     143,  -181
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    12,    13,    14,    15,    89,   181,   277,   278,   136,
     197,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,    78,    79,    98,   154,    60,    61,
      62,    80
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      77,    81,    28,   165,    86,   182,   241,   134,   139,   -31,
     146,   347,   137,   138,   234,   236,   307,   108,   317,    16,
     139,   381,   105,   147,   308,   318,   149,   -31,    43,   147,
     179,   348,    97,    99,   103,    17,   180,   107,    87,    88,
     118,   147,   144,   131,   132,   235,   133,    18,   120,    91,
     275,   140,   276,    92,   142,   148,    52,    53,   151,   224,
      29,   270,    92,    92,   272,    92,   252,   253,    92,   153,
     155,   156,   157,   158,   159,   160,   161,   162,   163,    97,
      52,    53,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   267,   222,   226,    54,    34,    44,    27,    30,    31,
     231,    32,    33,   229,     1,     2,    37,    38,   227,    63,
      64,    65,    66,    67,    68,    52,    69,    70,   104,    71,
      96,    39,    72,    55,    56,    57,    58,    59,    41,    40,
      42,   338,    45,   340,    51,   342,    50,    46,    47,   306,
      48,   242,    73,    49,    19,    74,    82,    55,    56,    57,
      58,    59,   247,    84,    83,   250,   251,    90,   320,    85,
     322,    93,     3,     4,    95,     5,   106,     6,     7,     8,
       9,    10,    11,    94,   223,    75,    52,    53,   145,   368,
     152,   134,    55,    56,    57,    58,    59,    76,   135,   221,
     237,   238,   279,   239,   273,   382,   274,   248,   384,   249,
     280,   281,    20,    21,   283,   284,    22,   269,    23,    24,
      25,    26,   240,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   302,   243,   244,   245,   285,   183,
      64,    65,    66,    67,    68,   303,    69,    70,   304,    71,
     305,   344,    72,    55,    56,    57,    58,    59,   319,   350,
     321,   374,   358,   366,   380,   386,    35,    36,   102,   335,
     336,   141,    73,   166,     0,    74,   339,   184,   341,     0,
     343,   337,   185,   186,   187,   188,   189,   190,     0,   345,
       0,     0,     0,     0,   351,   352,   191,   192,   193,     0,
       0,     0,     0,   194,   195,    75,     0,   108,     0,   109,
     110,   111,   112,   113,   114,   115,   196,    76,   362,     0,
       0,   363,     0,     0,   369,     0,     0,     0,     0,   367,
     118,     0,   370,     0,     0,     0,     0,    97,   120,     0,
     383,     0,     0,   385,   183,    64,    65,    66,    67,    68,
       0,    69,    70,     0,    71,   108,     0,    72,     0,   111,
     112,   113,     0,    63,    64,    65,    66,    67,    68,     0,
      69,    70,     0,    71,     0,     0,    72,    73,   118,     0,
      74,     0,   228,     0,     0,     0,   120,   185,   186,   187,
     188,   189,   190,     0,     0,     0,    73,   164,     0,    74,
       0,   191,   192,   193,     0,     0,     0,     0,   194,   195,
      75,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   196,    76,   183,    64,    65,    66,    67,    68,    75,
      69,    70,     0,    71,     0,     0,    72,     0,     0,     0,
       0,    76,    63,    64,    65,    66,    67,    68,     0,    69,
      70,     0,    71,     0,     0,    72,    73,     0,     0,    74,
       0,   268,     0,     0,     0,     0,   185,   186,   187,   188,
     189,   190,     0,     0,     0,    73,   373,     0,    74,     0,
     191,   192,   193,     0,     0,     0,     0,   194,   195,    75,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     196,    76,   183,    64,    65,    66,    67,    68,    75,    69,
      70,     0,    71,     0,     0,    72,     0,     0,     0,     0,
      76,    63,    64,    65,    66,    67,    68,     0,    69,    70,
       0,    71,     0,     0,    72,    73,    52,    53,    74,     0,
     271,     0,     0,     0,     0,   185,   186,   187,   188,   189,
     190,     0,     0,     0,    73,     0,     0,    74,     0,   191,
     192,   193,     0,    52,    53,     0,   194,   195,    75,     0,
       0,     0,     0,     0,   143,     0,     0,     0,     0,   196,
      76,   183,    64,    65,    66,    67,    68,    75,    69,    70,
       0,    71,     0,     0,    72,     0,     0,     0,     0,    76,
       0,   225,     0,    55,    56,    57,    58,    59,     0,     0,
      52,    53,     0,     0,    73,     0,     0,    74,     0,   282,
       0,     0,     0,     0,   185,   186,   187,   188,   189,   190,
      55,    56,    57,    58,    59,     0,     0,     0,   191,   192,
     193,     0,     0,     0,     0,   194,   195,    75,   230,     0,
       0,   108,     0,   109,   110,   111,   112,   113,   196,    76,
     183,    64,    65,    66,    67,    68,     0,    69,    70,     0,
      71,     0,     0,    72,   118,     0,     0,    55,    56,    57,
      58,    59,   120,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    73,     0,     0,    74,     0,   309,     1,
       2,     0,     0,   185,   186,   187,   188,   189,   190,     0,
       0,     0,     0,     0,     0,     0,     0,   191,   192,   193,
       0,     0,     0,     0,   194,   195,    75,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   196,    76,   183,
      64,    65,    66,    67,    68,     0,    69,    70,     0,    71,
       0,     0,    72,     0,     0,     0,     0,     3,     4,     0,
       5,     0,     6,     7,     8,     9,    10,    11,     0,     0,
       0,     0,    73,     0,     0,    74,     0,   353,     0,     0,
       0,     0,   185,   186,   187,   188,   189,   190,     0,     0,
       0,     0,     0,     0,     0,     0,   191,   192,   193,     0,
       0,     0,     0,   194,   195,    75,   108,     0,   109,   110,
     111,   112,   113,   114,   115,     0,   196,    76,   183,    64,
      65,    66,    67,    68,     0,    69,    70,     0,    71,   118,
       0,    72,     0,     0,     0,     0,     0,   120,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   121,   122,   123,
     124,    73,     0,     0,    74,     0,   355,     0,     0,     0,
       0,   185,   186,   187,   188,   189,   190,     0,     0,     0,
       0,     0,     0,     0,     0,   191,   192,   193,     0,     0,
       0,     0,   194,   195,    75,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   196,    76,   183,    64,    65,
      66,    67,    68,     0,    69,    70,     0,    71,     0,     0,
      72,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      73,     0,     0,    74,     0,   356,     0,     0,     0,     0,
     185,   186,   187,   188,   189,   190,     0,     0,     0,     0,
       0,     0,     0,     0,   191,   192,   193,     0,     0,     0,
       0,   194,   195,    75,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   196,    76,   183,    64,    65,    66,
      67,    68,     0,    69,    70,     0,    71,     0,     0,    72,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    73,
       0,     0,    74,     0,   357,     0,     0,     0,     0,   185,
     186,   187,   188,   189,   190,     0,     0,     0,     0,     0,
       0,     0,     0,   191,   192,   193,     0,     0,     0,     0,
     194,   195,    75,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   196,    76,   183,    64,    65,    66,    67,
      68,     0,    69,    70,     0,    71,     0,     0,    72,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    73,     0,
       0,    74,     0,   377,     0,     0,     0,     0,   185,   186,
     187,   188,   189,   190,     0,     0,     0,     0,     0,     0,
       0,     0,   191,   192,   193,     0,     0,     0,     0,   194,
     195,    75,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   196,    76,   183,    64,    65,    66,    67,    68,
       0,    69,    70,     0,    71,     0,     0,    72,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    73,     0,     0,
      74,     0,   387,     0,     0,     0,     0,   185,   186,   187,
     188,   189,   190,     0,     0,     0,     0,     0,     0,     0,
       0,   191,   192,   193,     0,     0,     0,     0,   194,   195,
      75,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   196,    76,   183,    64,    65,    66,    67,    68,     0,
      69,    70,     0,    71,     0,     0,    72,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     183,    64,    65,    66,    67,    68,    73,    69,    70,    74,
      71,   388,     0,    72,     0,     0,   185,   186,   187,   188,
     189,   190,     0,     0,     0,     0,     0,     0,     0,     0,
     191,   192,   193,    73,     0,     0,    74,   194,   195,    75,
       0,     0,     0,   185,   186,   187,   188,   189,   190,     0,
     196,    76,     0,     0,     0,     0,     0,   191,   192,   193,
       0,     0,     0,     0,   194,   195,    75,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   196,    76,   100,
      64,    65,    66,    67,    68,     0,    69,    70,     0,    71,
       0,     0,    72,    63,    64,    65,    66,    67,    68,     0,
      69,    70,     0,    71,     0,     0,    72,     0,     0,     0,
       0,     0,    73,     0,     0,    74,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    73,     0,     0,    74,
       0,     0,   246,     0,     0,     0,     0,     0,     0,   101,
       0,     0,     0,     0,   108,    75,   109,   110,   111,   112,
     113,   114,   115,     0,     0,     0,     0,    76,     0,    75,
       0,     0,     0,     0,     0,   116,   117,   118,     0,     0,
       0,    76,     0,     0,     0,   120,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   121,   122,   123,   124,   125,
     126,     0,     0,     0,     0,   127,   128,   129,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   108,
     354,   109,   110,   111,   112,   113,   114,   115,   254,   255,
     256,   257,   258,   259,   260,   261,   262,   263,   264,   265,
     116,   117,   118,     0,     0,     0,     0,     0,   266,     0,
     120,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     121,   122,   123,   124,   125,   126,     0,     0,     0,     0,
     127,   128,   129,   108,     0,   109,   110,   111,   112,   113,
     114,   115,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   116,   117,   118,   359,   360,     0,
       0,     0,     0,     0,   120,     0,     0,     0,     0,     0,
       0,     0,     0,   361,   121,   122,   123,   124,   125,   126,
       0,     0,     0,     0,   127,   128,   129,   108,   232,   109,
     110,   111,   112,   113,   114,   115,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   116,   117,
     118,     0,     0,     0,     0,     0,     0,     0,   120,   233,
       0,     0,     0,     0,     0,     0,     0,     0,   121,   122,
     123,   124,   125,   126,     0,     0,     0,     0,   127,   128,
     129,   108,     0,   109,   110,   111,   112,   113,   114,   115,
     287,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   116,   117,   118,     0,     0,     0,     0,     0,
       0,   288,   120,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   121,   122,   123,   124,   125,   126,     0,     0,
       0,     0,   127,   128,   129,   108,     0,   109,   110,   111,
     112,   113,   114,   115,   289,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   116,   117,   118,     0,
       0,     0,     0,     0,     0,   290,   120,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   121,   122,   123,   124,
     125,   126,     0,     0,     0,     0,   127,   128,   129,   108,
       0,   109,   110,   111,   112,   113,   114,   115,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     116,   117,   118,   311,   312,     0,     0,     0,     0,     0,
     120,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     121,   122,   123,   124,   125,   126,     0,     0,     0,     0,
     127,   128,   129,   108,     0,   109,   110,   111,   112,   113,
     114,   115,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   116,   117,   118,   313,   314,     0,
       0,     0,     0,     0,   120,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   121,   122,   123,   124,   125,   126,
       0,     0,     0,     0,   127,   128,   129,   108,     0,   109,
     110,   111,   112,   113,   114,   115,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   116,   117,
     118,   315,   316,     0,     0,     0,     0,     0,   120,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   121,   122,
     123,   124,   125,   126,     0,     0,     0,     0,   127,   128,
     129,   108,     0,   109,   110,   111,   112,   113,   114,   115,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   116,   117,   118,   375,   376,     0,     0,     0,
       0,     0,   120,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   121,   122,   123,   124,   125,   126,     0,     0,
       0,     0,   127,   128,   129,   108,     0,   109,   110,   111,
     112,   113,   114,   115,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   116,   117,   118,   378,
     379,     0,     0,     0,     0,     0,   120,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   121,   122,   123,   124,
     125,   126,     0,     0,     0,     0,   127,   128,   129,   108,
       0,   109,   110,   111,   112,   113,   114,   115,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     116,   117,   118,     0,     0,     0,     0,     0,   119,     0,
     120,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     121,   122,   123,   124,   125,   126,     0,     0,     0,     0,
     127,   128,   129,   108,     0,   109,   110,   111,   112,   113,
     114,   115,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   116,   117,   118,     0,     0,     0,
       0,     0,   130,     0,   120,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   121,   122,   123,   124,   125,   126,
       0,     0,     0,     0,   127,   128,   129,   108,     0,   109,
     110,   111,   112,   113,   114,   115,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   116,   117,
     118,   150,     0,     0,     0,     0,     0,     0,   120,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   121,   122,
     123,   124,   125,   126,     0,     0,     0,     0,   127,   128,
     129,   108,     0,   109,   110,   111,   112,   113,   114,   115,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   116,   117,   118,     0,     0,     0,     0,     0,
     176,     0,   120,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   121,   122,   123,   124,   125,   126,     0,     0,
       0,     0,   127,   128,   129,   108,     0,   109,   110,   111,
     112,   113,   114,   115,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   116,   117,   118,     0,
       0,     0,     0,     0,   177,     0,   120,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   121,   122,   123,   124,
     125,   126,     0,     0,     0,     0,   127,   128,   129,   108,
       0,   109,   110,   111,   112,   113,   114,   115,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     116,   117,   118,     0,     0,     0,     0,     0,   178,     0,
     120,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     121,   122,   123,   124,   125,   126,     0,     0,     0,     0,
     127,   128,   129,   108,     0,   109,   110,   111,   112,   113,
     114,   115,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   116,   117,   118,     0,     0,     0,
       0,     0,   286,     0,   120,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   121,   122,   123,   124,   125,   126,
       0,     0,     0,     0,   127,   128,   129,   108,     0,   109,
     110,   111,   112,   113,   114,   115,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   116,   117,
     118,     0,     0,     0,     0,     0,     0,     0,   120,   310,
       0,     0,     0,     0,     0,     0,     0,     0,   121,   122,
     123,   124,   125,   126,     0,     0,     0,     0,   127,   128,
     129,   108,     0,   109,   110,   111,   112,   113,   114,   115,
     323,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   116,   117,   118,     0,     0,     0,     0,     0,
       0,     0,   120,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   121,   122,   123,   124,   125,   126,     0,     0,
       0,     0,   127,   128,   129,   108,     0,   109,   110,   111,
     112,   113,   114,   115,   324,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   116,   117,   118,     0,
       0,     0,     0,     0,     0,     0,   120,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   121,   122,   123,   124,
     125,   126,     0,     0,     0,     0,   127,   128,   129,   108,
       0,   109,   110,   111,   112,   113,   114,   115,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     116,   117,   118,     0,     0,     0,     0,     0,   325,     0,
     120,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     121,   122,   123,   124,   125,   126,     0,     0,     0,     0,
     127,   128,   129,   108,     0,   109,   110,   111,   112,   113,
     114,   115,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   116,   117,   118,     0,     0,     0,
       0,     0,   326,     0,   120,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   121,   122,   123,   124,   125,   126,
       0,     0,     0,     0,   127,   128,   129,   108,     0,   109,
     110,   111,   112,   113,   114,   115,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   116,   117,
     118,     0,     0,     0,     0,     0,   327,     0,   120,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   121,   122,
     123,   124,   125,   126,     0,     0,     0,     0,   127,   128,
     129,   108,     0,   109,   110,   111,   112,   113,   114,   115,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   116,   117,   118,     0,     0,     0,     0,     0,
     328,     0,   120,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   121,   122,   123,   124,   125,   126,     0,     0,
       0,     0,   127,   128,   129,   108,     0,   109,   110,   111,
     112,   113,   114,   115,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   116,   117,   118,     0,
       0,     0,     0,     0,   329,     0,   120,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   121,   122,   123,   124,
     125,   126,     0,     0,     0,     0,   127,   128,   129,   108,
       0,   109,   110,   111,   112,   113,   114,   115,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     116,   117,   118,     0,     0,     0,     0,     0,   330,     0,
     120,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     121,   122,   123,   124,   125,   126,     0,     0,     0,     0,
     127,   128,   129,   108,     0,   109,   110,   111,   112,   113,
     114,   115,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   116,   117,   118,     0,     0,     0,
       0,     0,   331,     0,   120,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   121,   122,   123,   124,   125,   126,
       0,     0,     0,     0,   127,   128,   129,   108,     0,   109,
     110,   111,   112,   113,   114,   115,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   116,   117,
     118,     0,     0,     0,     0,     0,   332,     0,   120,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   121,   122,
     123,   124,   125,   126,     0,     0,     0,     0,   127,   128,
     129,   108,     0,   109,   110,   111,   112,   113,   114,   115,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   116,   117,   118,     0,     0,     0,     0,     0,
     333,     0,   120,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   121,   122,   123,   124,   125,   126,     0,     0,
       0,     0,   127,   128,   129,   108,     0,   109,   110,   111,
     112,   113,   114,   115,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   116,   117,   118,     0,
       0,     0,     0,     0,   334,     0,   120,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   121,   122,   123,   124,
     125,   126,     0,     0,     0,     0,   127,   128,   129,   108,
       0,   109,   110,   111,   112,   113,   114,   115,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     116,   117,   118,     0,     0,     0,     0,     0,   346,     0,
     120,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     121,   122,   123,   124,   125,   126,     0,     0,     0,     0,
     127,   128,   129,   108,     0,   109,   110,   111,   112,   113,
     114,   115,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   116,   117,   118,     0,     0,     0,
       0,     0,   349,     0,   120,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   121,   122,   123,   124,   125,   126,
       0,     0,     0,     0,   127,   128,   129,   108,     0,   109,
     110,   111,   112,   113,   114,   115,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   116,   117,
     118,     0,     0,     0,     0,     0,   364,     0,   120,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   121,   122,
     123,   124,   125,   126,     0,     0,     0,     0,   127,   128,
     129,   108,     0,   109,   110,   111,   112,   113,   114,   115,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   116,   117,   118,     0,     0,     0,     0,     0,
     365,     0,   120,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   121,   122,   123,   124,   125,   126,     0,     0,
       0,     0,   127,   128,   129,   108,     0,   109,   110,   111,
     112,   113,   114,   115,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   116,   117,   118,     0,
       0,     0,     0,     0,   371,     0,   120,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   121,   122,   123,   124,
     125,   126,     0,     0,     0,     0,   127,   128,   129,   108,
       0,   109,   110,   111,   112,   113,   114,   115,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     116,   117,   118,     0,     0,     0,     0,     0,   372,     0,
     120,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     121,   122,   123,   124,   125,   126,     0,     0,     0,     0,
     127,   128,   129,   108,     0,   109,   110,   111,   112,   113,
     114,   115,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   116,   117,   118,     0,     0,     0,
       0,     0,     0,     0,   120,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   121,   122,   123,   124,   125,   126,
       0,     0,     0,     0,   127,   128,   129,   108,     0,   109,
     110,   111,   112,   113,   114,   115,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   116,     0,
     118,     0,     0,     0,     0,     0,     0,     0,   120,   108,
       0,   109,   110,   111,   112,   113,   114,   115,   121,   122,
     123,   124,   125,   126,     0,     0,     0,     0,   127,   128,
     129,     0,   118,     0,     0,     0,     0,     0,     0,     0,
     120,   108,     0,   109,   110,   111,   112,   113,   114,   115,
     121,   122,   123,   124,   125,   126,     0,     0,     0,     0,
     127,   128,   129,     0,   118,     0,     0,     0,     0,     0,
       0,     0,   120,   108,     0,   109,   110,   111,   112,   113,
     114,   115,   121,   122,   123,   124,   125,   126,     0,     0,
       0,     0,   127,     0,   129,     0,   118,     0,     0,     0,
       0,     0,     0,   108,   120,   109,   110,   111,   112,   113,
     114,   115,     0,     0,   121,   122,   123,   124,   125,   126,
       0,     0,     0,     0,   127,     0,   118,     0,     0,     0,
       0,     0,     0,     0,   120,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   121,   122,   123,   124,   125,   126
};

static const yytype_int16 yycheck[] =
{
      39,    40,     9,   118,     3,   135,   186,    43,    45,    45,
      14,    22,    37,    38,    14,    37,    37,    13,    45,     3,
      45,    37,    74,    45,    45,    52,    63,    63,     9,    45,
       3,    42,    71,    72,    73,     3,     9,    76,    37,    38,
      36,    45,    94,    82,    83,    45,    85,     3,    44,    41,
       3,    90,     5,    45,    93,   101,     3,     4,    41,    41,
      67,    41,    45,    45,    41,    45,    67,    68,    45,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
       3,     4,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   221,   138,   145,    41,     0,    77,     3,     9,     9,
     152,     9,     9,   149,     9,    10,     3,    39,   147,     3,
       4,     5,     6,     7,     8,     3,    10,    11,    41,    13,
      14,    22,    16,    70,    71,    72,    73,    74,     3,    22,
       3,   311,     9,   313,    36,   315,     3,     9,     9,   269,
       9,   187,    36,    42,     9,    39,    22,    70,    71,    72,
      73,    74,   191,     9,    22,   194,   195,    43,   288,    22,
     290,    43,    67,    68,     3,    70,     3,    72,    73,    74,
      75,    76,    77,    39,     3,    69,     3,     4,    39,   359,
      39,    43,    70,    71,    72,    73,    74,    81,    43,    43,
      36,    39,   238,    78,   233,   375,   235,    42,   378,    42,
     239,   240,    67,    68,   243,   244,    71,    43,    73,    74,
      75,    76,    36,   252,   253,   254,   255,   256,   257,   258,
     259,   260,   261,   262,   263,    36,    36,    36,     3,     3,
       4,     5,     6,     7,     8,    42,    10,    11,    42,    13,
      39,     3,    16,    70,    71,    72,    73,    74,   287,    22,
     289,   366,    52,    36,    42,    42,    12,    12,    73,   305,
     308,    92,    36,   120,    -1,    39,   312,    41,   314,    -1,
     316,   310,    46,    47,    48,    49,    50,    51,    -1,   318,
      -1,    -1,    -1,    -1,   323,   324,    60,    61,    62,    -1,
      -1,    -1,    -1,    67,    68,    69,    -1,    13,    -1,    15,
      16,    17,    18,    19,    20,    21,    80,    81,   347,    -1,
      -1,   350,    -1,    -1,   360,    -1,    -1,    -1,    -1,   358,
      36,    -1,   361,    -1,    -1,    -1,    -1,   366,    44,    -1,
     376,    -1,    -1,   379,     3,     4,     5,     6,     7,     8,
      -1,    10,    11,    -1,    13,    13,    -1,    16,    -1,    17,
      18,    19,    -1,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    36,    36,    -1,
      39,    -1,    41,    -1,    -1,    -1,    44,    46,    47,    48,
      49,    50,    51,    -1,    -1,    -1,    36,    37,    -1,    39,
      -1,    60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,
      69,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    80,    81,     3,     4,     5,     6,     7,     8,    69,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    81,     3,     4,     5,     6,     7,     8,    -1,    10,
      11,    -1,    13,    -1,    -1,    16,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    -1,    -1,    -1,    36,    37,    -1,    39,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      80,    81,     3,     4,     5,     6,     7,     8,    69,    10,
      11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,    -1,
      81,     3,     4,     5,     6,     7,     8,    -1,    10,    11,
      -1,    13,    -1,    -1,    16,    36,     3,     4,    39,    -1,
      41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,    50,
      51,    -1,    -1,    -1,    36,    -1,    -1,    39,    -1,    60,
      61,    62,    -1,     3,     4,    -1,    67,    68,    69,    -1,
      -1,    -1,    -1,    -1,    41,    -1,    -1,    -1,    -1,    80,
      81,     3,     4,     5,     6,     7,     8,    69,    10,    11,
      -1,    13,    -1,    -1,    16,    -1,    -1,    -1,    -1,    81,
      -1,    41,    -1,    70,    71,    72,    73,    74,    -1,    -1,
       3,     4,    -1,    -1,    36,    -1,    -1,    39,    -1,    41,
      -1,    -1,    -1,    -1,    46,    47,    48,    49,    50,    51,
      70,    71,    72,    73,    74,    -1,    -1,    -1,    60,    61,
      62,    -1,    -1,    -1,    -1,    67,    68,    69,    41,    -1,
      -1,    13,    -1,    15,    16,    17,    18,    19,    80,    81,
       3,     4,     5,     6,     7,     8,    -1,    10,    11,    -1,
      13,    -1,    -1,    16,    36,    -1,    -1,    70,    71,    72,
      73,    74,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    36,    -1,    -1,    39,    -1,    41,     9,
      10,    -1,    -1,    46,    47,    48,    49,    50,    51,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    60,    61,    62,
      -1,    -1,    -1,    -1,    67,    68,    69,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    80,    81,     3,
       4,     5,     6,     7,     8,    -1,    10,    11,    -1,    13,
      -1,    -1,    16,    -1,    -1,    -1,    -1,    67,    68,    -1,
      70,    -1,    72,    73,    74,    75,    76,    77,    -1,    -1,
      -1,    -1,    36,    -1,    -1,    39,    -1,    41,    -1,    -1,
      -1,    -1,    46,    47,    48,    49,    50,    51,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    60,    61,    62,    -1,
      -1,    -1,    -1,    67,    68,    69,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    -1,    80,    81,     3,     4,
       5,     6,     7,     8,    -1,    10,    11,    -1,    13,    36,
      -1,    16,    -1,    -1,    -1,    -1,    -1,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,
      57,    36,    -1,    -1,    39,    -1,    41,    -1,    -1,    -1,
      -1,    46,    47,    48,    49,    50,    51,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    60,    61,    62,    -1,    -1,
      -1,    -1,    67,    68,    69,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    80,    81,     3,     4,     5,
       6,     7,     8,    -1,    10,    11,    -1,    13,    -1,    -1,
      16,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      36,    -1,    -1,    39,    -1,    41,    -1,    -1,    -1,    -1,
      46,    47,    48,    49,    50,    51,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    60,    61,    62,    -1,    -1,    -1,
      -1,    67,    68,    69,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    80,    81,     3,     4,     5,     6,
       7,     8,    -1,    10,    11,    -1,    13,    -1,    -1,    16,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,
      -1,    -1,    39,    -1,    41,    -1,    -1,    -1,    -1,    46,
      47,    48,    49,    50,    51,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    60,    61,    62,    -1,    -1,    -1,    -1,
      67,    68,    69,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    80,    81,     3,     4,     5,     6,     7,
       8,    -1,    10,    11,    -1,    13,    -1,    -1,    16,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,    -1,
      -1,    39,    -1,    41,    -1,    -1,    -1,    -1,    46,    47,
      48,    49,    50,    51,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    60,    61,    62,    -1,    -1,    -1,    -1,    67,
      68,    69,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    80,    81,     3,     4,     5,     6,     7,     8,
      -1,    10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,    -1,    -1,
      39,    -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,
      49,    50,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,
      69,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    80,    81,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,     8,    36,    10,    11,    39,
      13,    41,    -1,    16,    -1,    -1,    46,    47,    48,    49,
      50,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      60,    61,    62,    36,    -1,    -1,    39,    67,    68,    69,
      -1,    -1,    -1,    46,    47,    48,    49,    50,    51,    -1,
      80,    81,    -1,    -1,    -1,    -1,    -1,    60,    61,    62,
      -1,    -1,    -1,    -1,    67,    68,    69,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    80,    81,     3,
       4,     5,     6,     7,     8,    -1,    10,    11,    -1,    13,
      -1,    -1,    16,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    -1,    42,    -1,    -1,    -1,    -1,    -1,    -1,    63,
      -1,    -1,    -1,    -1,    13,    69,    15,    16,    17,    18,
      19,    20,    21,    -1,    -1,    -1,    -1,    81,    -1,    69,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,
      -1,    81,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,
      59,    -1,    -1,    -1,    -1,    64,    65,    66,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    13,
      79,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,
      44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,
      64,    65,    66,    13,    -1,    15,    16,    17,    18,    19,
      20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,    -1,
      -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    53,    54,    55,    56,    57,    58,    59,
      -1,    -1,    -1,    -1,    64,    65,    66,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,
      36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,    45,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,
      56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,
      66,    13,    -1,    15,    16,    17,    18,    19,    20,    21,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,
      -1,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    54,    55,    56,    57,    58,    59,    -1,    -1,
      -1,    -1,    64,    65,    66,    13,    -1,    15,    16,    17,
      18,    19,    20,    21,    22,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,
      -1,    -1,    -1,    -1,    -1,    43,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,
      58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,
      -1,    15,    16,    17,    18,    19,    20,    21,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,    -1,
      44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,
      64,    65,    66,    13,    -1,    15,    16,    17,    18,    19,
      20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,    -1,
      -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,    59,
      -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,
      16,    17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,
      36,    37,    38,    -1,    -1,    -1,    -1,    -1,    44,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,
      56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,
      66,    13,    -1,    15,    16,    17,    18,    19,    20,    21,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    34,    35,    36,    37,    38,    -1,    -1,    -1,
      -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    54,    55,    56,    57,    58,    59,    -1,    -1,
      -1,    -1,    64,    65,    66,    13,    -1,    15,    16,    17,
      18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,    37,
      38,    -1,    -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,
      58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,
      -1,    15,    16,    17,    18,    19,    20,    21,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,
      44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,
      64,    65,    66,    13,    -1,    15,    16,    17,    18,    19,
      20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,    -1,
      -1,    -1,    42,    -1,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,    59,
      -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,
      16,    17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,
      36,    37,    -1,    -1,    -1,    -1,    -1,    -1,    44,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,
      56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,
      66,    13,    -1,    15,    16,    17,    18,    19,    20,    21,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,
      42,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    54,    55,    56,    57,    58,    59,    -1,    -1,
      -1,    -1,    64,    65,    66,    13,    -1,    15,    16,    17,
      18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,
      -1,    -1,    -1,    -1,    42,    -1,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,
      58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,
      -1,    15,    16,    17,    18,    19,    20,    21,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,
      44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,
      64,    65,    66,    13,    -1,    15,    16,    17,    18,    19,
      20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,    -1,
      -1,    -1,    42,    -1,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,    59,
      -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,
      16,    17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,
      36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,    45,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,
      56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,
      66,    13,    -1,    15,    16,    17,    18,    19,    20,    21,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    54,    55,    56,    57,    58,    59,    -1,    -1,
      -1,    -1,    64,    65,    66,    13,    -1,    15,    16,    17,
      18,    19,    20,    21,    22,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,
      58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,
      -1,    15,    16,    17,    18,    19,    20,    21,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,
      44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,
      64,    65,    66,    13,    -1,    15,    16,    17,    18,    19,
      20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,    -1,
      -1,    -1,    42,    -1,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,    59,
      -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,
      16,    17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,
      36,    -1,    -1,    -1,    -1,    -1,    42,    -1,    44,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,
      56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,
      66,    13,    -1,    15,    16,    17,    18,    19,    20,    21,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,
      42,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    54,    55,    56,    57,    58,    59,    -1,    -1,
      -1,    -1,    64,    65,    66,    13,    -1,    15,    16,    17,
      18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,
      -1,    -1,    -1,    -1,    42,    -1,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,
      58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,
      -1,    15,    16,    17,    18,    19,    20,    21,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,
      44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,
      64,    65,    66,    13,    -1,    15,    16,    17,    18,    19,
      20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,    -1,
      -1,    -1,    42,    -1,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,    59,
      -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,
      16,    17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,
      36,    -1,    -1,    -1,    -1,    -1,    42,    -1,    44,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,
      56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,
      66,    13,    -1,    15,    16,    17,    18,    19,    20,    21,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,
      42,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    54,    55,    56,    57,    58,    59,    -1,    -1,
      -1,    -1,    64,    65,    66,    13,    -1,    15,    16,    17,
      18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,
      -1,    -1,    -1,    -1,    42,    -1,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,
      58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,
      -1,    15,    16,    17,    18,    19,    20,    21,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,
      44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,
      64,    65,    66,    13,    -1,    15,    16,    17,    18,    19,
      20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,    -1,
      -1,    -1,    42,    -1,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,    59,
      -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,
      16,    17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,
      36,    -1,    -1,    -1,    -1,    -1,    42,    -1,    44,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,
      56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,
      66,    13,    -1,    15,    16,    17,    18,    19,    20,    21,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,
      42,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    54,    55,    56,    57,    58,    59,    -1,    -1,
      -1,    -1,    64,    65,    66,    13,    -1,    15,    16,    17,
      18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,
      -1,    -1,    -1,    -1,    42,    -1,    44,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,
      58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,
      -1,    15,    16,    17,    18,    19,    20,    21,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,
      44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      54,    55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,
      64,    65,    66,    13,    -1,    15,    16,    17,    18,    19,
      20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,    59,
      -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,
      16,    17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    -1,
      36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,    13,
      -1,    15,    16,    17,    18,    19,    20,    21,    54,    55,
      56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,
      66,    -1,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      44,    13,    -1,    15,    16,    17,    18,    19,    20,    21,
      54,    55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,
      64,    65,    66,    -1,    36,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    44,    13,    -1,    15,    16,    17,    18,    19,
      20,    21,    54,    55,    56,    57,    58,    59,    -1,    -1,
      -1,    -1,    64,    -1,    66,    -1,    36,    -1,    -1,    -1,
      -1,    -1,    -1,    13,    44,    15,    16,    17,    18,    19,
      20,    21,    -1,    -1,    54,    55,    56,    57,    58,    59,
      -1,    -1,    -1,    -1,    64,    -1,    36,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,    59
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     9,    10,    67,    68,    70,    72,    73,    74,    75,
      76,    77,    85,    86,    87,    88,     3,     3,     3,     9,
      67,    68,    71,    73,    74,    75,    76,     3,     9,    67,
       9,     9,     9,     9,     0,    86,    88,     3,    39,    22,
      22,     3,     3,     9,    77,     9,     9,     9,     9,    42,
       3,    36,     3,     4,    41,    70,    71,    72,    73,    74,
     122,   123,   124,     3,     4,     5,     6,     7,     8,    10,
      11,    13,    16,    36,    39,    69,    81,   117,   118,   119,
     125,   117,    22,    22,     9,    22,     3,    37,    38,    89,
      43,    41,    45,    43,    39,     3,    14,   117,   120,   117,
       3,    63,    89,   117,    41,   122,     3,   117,    13,    15,
      16,    17,    18,    19,    20,    21,    34,    35,    36,    42,
      44,    54,    55,    56,    57,    58,    59,    64,    65,    66,
      42,   117,   117,   117,    43,    43,    93,    37,    38,    45,
     117,   123,   117,    41,   122,    39,    14,    45,    93,    63,
      37,    41,    39,   117,   121,   117,   117,   117,   117,   117,
     117,   117,   117,   117,    37,   120,   124,   117,   117,   117,
     117,   117,   117,   117,   117,   117,    42,    42,    42,     3,
       9,    90,    90,     3,    41,    46,    47,    48,    49,    50,
      51,    60,    61,    62,    67,    68,    80,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,    43,    93,     3,    41,    41,   122,   117,    41,    93,
      41,   122,    14,    45,    14,    45,    37,    36,    39,    78,
      36,    94,    93,    36,    36,    36,    42,   117,    42,    42,
     117,   117,    67,    68,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    42,    90,    41,    43,
      41,    41,    41,   117,   117,     3,     5,    91,    92,    93,
     117,   117,    41,   117,   117,     3,    42,    22,    43,    22,
      43,   117,   117,   117,   117,   117,   117,   117,   117,   117,
     117,   117,   117,    42,    42,    39,    90,    37,    45,    41,
      45,    37,    38,    37,    38,    37,    38,    45,    52,   117,
      90,   117,    90,    22,    22,    42,    42,    42,    42,    42,
      42,    42,    42,    42,    42,    93,    91,   117,    94,    93,
      94,    93,    94,    93,     3,   117,    42,    22,    42,    42,
      22,   117,   117,    41,    79,    41,    41,    41,    52,    37,
      38,    53,   117,   117,    42,    42,    36,   117,    94,    93,
     117,    42,    42,    37,   120,    37,    38,    41,    37,    38,
      42,    37,    94,    93,    94,    93,    42,    41,    41
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    84,    85,    85,    85,    85,    86,    86,    86,    86,
      86,    86,    86,    86,    87,    87,    87,    87,    87,    87,
      87,    87,    87,    87,    87,    87,    87,    88,    88,    88,
      88,    89,    89,    89,    89,    90,    90,    90,    91,    91,
      92,    92,    93,    93,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    95,    95,    96,    97,
      97,    97,    97,    97,    97,    97,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   109,
     110,   110,   111,   111,   112,   112,   113,   113,   113,   113,
     113,   113,   114,   114,   115,   116,   117,   117,   117,   117,
     117,   117,   117,   117,   117,   117,   117,   117,   117,   117,
     117,   117,   117,   117,   117,   117,   117,   117,   117,   117,
     117,   117,   117,   117,   117,   117,   117,   117,   117,   117,
     117,   118,   118,   119,   119,   120,   120,   121,   121,   122,
     122,   123,   123,   124,   124,   124,   124,   124,   124,   125,
     125,   125,   125,   125,   125,   125,   125
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     1,     2,     2,     5,     6,     5,     6,
       6,     5,     4,     3,     1,     2,     3,     2,     3,     2,
       3,     2,     4,     2,     3,     2,     3,     7,     6,    10,
       9,     1,     3,     3,     5,     1,     4,     1,     1,     1,
       1,     3,     0,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,    10,     9,     2,     4,
       5,     5,     6,     6,     7,     5,     7,     4,     4,     4,
       4,     4,     4,     4,     4,     4,     3,     3,     6,     5,
       6,     5,     3,     2,     6,     5,    10,     9,     8,     7,
      10,     9,     3,     2,     2,     2,     1,     3,     4,     4,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     2,     2,
       3,     1,     3,     3,     4,     3,     1,     5,     5,     4,
       4,     4,     3,     5,     4,     1,     3,     3,     3,     1,
       3,     3,     3,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (scanner, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, scanner); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, void *scanner)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (scanner);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, void *scanner)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, scanner);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule, void *scanner)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]), scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, scanner); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, void *scanner)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (scanner);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Location data for the lookahead symbol.  */
YYLTYPE yylloc
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void *scanner)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */


/* User initialization code.  */
#line 243 "parser.y"
{
	ast_yylloc.last_line = yylloc.first_line = 0;
	ast_yylloc.last_column = yylloc.first_column = 0;
}

#line 2149 "parser.tab.c"

  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (scanner);
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* func_list: func  */
#line 250 "parser.y"
                {
			(yyval.func_list) = ast_accept_func_list(NULL, (yyvsp[0].func));
			debug("func_list: class");
		}
#line 2365 "parser.tab.c"
    break;

  case 3: /* func_list: toplevel_decl  */
#line 255 "parser.y"
                {
			(yyval.func_list) = NULL;
			debug("func_list: toplevel_decl");
		}
#line 2374 "parser.tab.c"
    break;

  case 4: /* func_list: func_list func  */
#line 260 "parser.y"
                {
			(yyval.func_list) = ast_accept_func_list((yyvsp[-1].func_list), (yyvsp[0].func));
			debug("func_list: func_list func");
		}
#line 2383 "parser.tab.c"
    break;

  case 5: /* func_list: func_list toplevel_decl  */
#line 265 "parser.y"
                {
			(yyval.func_list) = (yyvsp[-1].func_list);
			debug("func_list: func_list toplevel_decl");
		}
#line 2392 "parser.tab.c"
    break;

  case 6: /* toplevel_decl: TOKEN_VAR TOKEN_SYMBOL TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 271 "parser.y"
                {
			if (!ast_accept_toplevel_var((yylsp[-4]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].expr), false, false))
				YYABORT;
			debug("toplevel_decl: var");
		}
#line 2402 "parser.tab.c"
    break;

  case 7: /* toplevel_decl: TOKEN_ACCEL TOKEN_VAR TOKEN_SYMBOL TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 277 "parser.y"
                {
			if (!ast_accept_toplevel_accel_var((yylsp[-5]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].expr)))
				YYABORT;
			debug("toplevel_decl: accel var");
		}
#line 2412 "parser.tab.c"
    break;

  case 8: /* toplevel_decl: TOKEN_LET TOKEN_SYMBOL TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 283 "parser.y"
                {
			if (!ast_accept_toplevel_var((yylsp[-4]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].expr), true, false))
				YYABORT;
			debug("toplevel_decl: let");
		}
#line 2422 "parser.tab.c"
    break;

  case 9: /* toplevel_decl: TOKEN_STATIC TOKEN_VAR TOKEN_SYMBOL TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 289 "parser.y"
                {
			if (!ast_accept_toplevel_var((yylsp[-5]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].expr), false, true))
				YYABORT;
			debug("toplevel_decl: static var");
		}
#line 2432 "parser.tab.c"
    break;

  case 10: /* toplevel_decl: TOKEN_STATIC TOKEN_LET TOKEN_SYMBOL TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 295 "parser.y"
                {
			if (!ast_accept_toplevel_var((yylsp[-5]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].expr), true, true))
				YYABORT;
			debug("toplevel_decl: static let");
		}
#line 2442 "parser.tab.c"
    break;

  case 11: /* toplevel_decl: TOKEN_CLASS TOKEN_SYMBOL TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 301 "parser.y"
                {
			if (!ast_accept_toplevel_class((yylsp[-4]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].kv_list)))
				YYABORT;
			debug("toplevel_decl: class");
		}
#line 2452 "parser.tab.c"
    break;

  case 12: /* toplevel_decl: TOKEN_CLASS TOKEN_SYMBOL TOKEN_LBLK TOKEN_RBLK  */
#line 307 "parser.y"
                {
			if (!ast_accept_toplevel_class((yylsp[-3]).first_line + 1, (yyvsp[-2].sval), NULL))
				YYABORT;
			debug("toplevel_decl: class empty");
		}
#line 2462 "parser.tab.c"
    break;

  case 13: /* toplevel_decl: TOKEN_REQUIRE TOKEN_SYMBOL TOKEN_SEMICOLON  */
#line 313 "parser.y"
                {
			if (!ast_accept_require((yyvsp[-1].sval)))
				YYABORT;
			debug("toplevel_decl: require");
		}
#line 2472 "parser.tab.c"
    break;

  case 14: /* func_prefix: TOKEN_FUNC  */
#line 320 "parser.y"
                {
			(yyval.ival) = 0;
		}
#line 2480 "parser.tab.c"
    break;

  case 15: /* func_prefix: TOKEN_STATIC TOKEN_FUNC  */
#line 324 "parser.y"
                {
			(yyval.ival) = 1;
		}
#line 2488 "parser.tab.c"
    break;

  case 16: /* func_prefix: TOKEN_STATIC TOKEN_INLINE TOKEN_FUNC  */
#line 328 "parser.y"
                {
			(yyval.ival) = 3;
		}
#line 2496 "parser.tab.c"
    break;

  case 17: /* func_prefix: TOKEN_DUNDER_ACCEL TOKEN_FUNC  */
#line 332 "parser.y"
                {
			(yyval.ival) = 4;
		}
#line 2504 "parser.tab.c"
    break;

  case 18: /* func_prefix: TOKEN_STATIC TOKEN_DUNDER_ACCEL TOKEN_FUNC  */
#line 336 "parser.y"
                {
			(yyval.ival) = 5;
		}
#line 2512 "parser.tab.c"
    break;

  case 19: /* func_prefix: TOKEN_DUNDER_GPU TOKEN_FUNC  */
#line 340 "parser.y"
                {
			(yyval.ival) = 8;
		}
#line 2520 "parser.tab.c"
    break;

  case 20: /* func_prefix: TOKEN_STATIC TOKEN_DUNDER_GPU TOKEN_FUNC  */
#line 344 "parser.y"
                {
			(yyval.ival) = 9;
		}
#line 2528 "parser.tab.c"
    break;

  case 21: /* func_prefix: TOKEN_DUNDER_FAST TOKEN_FUNC  */
#line 348 "parser.y"
                {
			(yyval.ival) = 16;
		}
#line 2536 "parser.tab.c"
    break;

  case 22: /* func_prefix: TOKEN_STATIC TOKEN_INLINE TOKEN_DUNDER_FAST TOKEN_FUNC  */
#line 352 "parser.y"
                {
			(yyval.ival) = 19;
		}
#line 2544 "parser.tab.c"
    break;

  case 23: /* func_prefix: TOKEN_ACCEL TOKEN_FUNC  */
#line 356 "parser.y"
                {
			ast_yyerror(scanner,
				"accel func is no longer accepted; use __accel func.");
			YYABORT;
		}
#line 2554 "parser.tab.c"
    break;

  case 24: /* func_prefix: TOKEN_STATIC TOKEN_ACCEL TOKEN_FUNC  */
#line 362 "parser.y"
                {
			ast_yyerror(scanner,
				"accel func is no longer accepted; use static __accel func.");
			YYABORT;
		}
#line 2564 "parser.tab.c"
    break;

  case 25: /* func_prefix: TOKEN_GPU TOKEN_FUNC  */
#line 368 "parser.y"
                {
			ast_yyerror(scanner,
				"gpu func is no longer accepted; use __gpu func.");
			YYABORT;
		}
#line 2574 "parser.tab.c"
    break;

  case 26: /* func_prefix: TOKEN_STATIC TOKEN_GPU TOKEN_FUNC  */
#line 374 "parser.y"
                {
			ast_yyerror(scanner,
				"gpu func is no longer accepted; use static __gpu func.");
			YYABORT;
		}
#line 2584 "parser.tab.c"
    break;

  case 27: /* func: func_prefix TOKEN_SYMBOL TOKEN_LPAR param_list TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 381 "parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-6].ival), (yyvsp[-5].sval), (yyvsp[-3].param_list), NULL, (yyvsp[-1].stmt_list));
			if ((yyval.func) == NULL) YYABORT;
			debug("func: func name(param_list) { stmt_list }");
		}
#line 2594 "parser.tab.c"
    break;

  case 28: /* func: func_prefix TOKEN_SYMBOL TOKEN_LPAR TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 387 "parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-5].ival), (yyvsp[-4].sval), NULL, NULL, (yyvsp[-1].stmt_list));
			if ((yyval.func) == NULL) YYABORT;
			debug("func: func name() { stmt_list }");
		}
#line 2604 "parser.tab.c"
    break;

  case 29: /* func: func_prefix TOKEN_SYMBOL TOKEN_LPAR param_list TOKEN_RPAR TOKEN_COLON type_name TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 393 "parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-9].ival), (yyvsp[-8].sval), (yyvsp[-6].param_list), (yyvsp[-3].sval), (yyvsp[-1].stmt_list));
			if ((yyval.func) == NULL) YYABORT;
			debug("func: func name(param_list): type { stmt_list }");
		}
#line 2614 "parser.tab.c"
    break;

  case 30: /* func: func_prefix TOKEN_SYMBOL TOKEN_LPAR TOKEN_RPAR TOKEN_COLON type_name TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 399 "parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-8].ival), (yyvsp[-7].sval), NULL, (yyvsp[-3].sval), (yyvsp[-1].stmt_list));
			if ((yyval.func) == NULL) YYABORT;
			debug("func: func name(): type { stmt_list }");
		}
#line 2624 "parser.tab.c"
    break;

  case 31: /* param_list: TOKEN_SYMBOL  */
#line 406 "parser.y"
                {
			(yyval.param_list) = ast_accept_param_list(NULL, (yyvsp[0].sval));
			debug("param_list: symbol");
		}
#line 2633 "parser.tab.c"
    break;

  case 32: /* param_list: TOKEN_SYMBOL TOKEN_COLON type_name  */
#line 411 "parser.y"
                {
			(yyval.param_list) = ast_accept_param_list_typed(NULL, (yyvsp[-2].sval), (yyvsp[0].sval));
			debug("param_list: symbol: type");
		}
#line 2642 "parser.tab.c"
    break;

  case 33: /* param_list: param_list TOKEN_COMMA TOKEN_SYMBOL  */
#line 416 "parser.y"
                {
			(yyval.param_list) = ast_accept_param_list((yyvsp[-2].param_list), (yyvsp[0].sval));
			debug("param_list: param_list symbol");
		}
#line 2651 "parser.tab.c"
    break;

  case 34: /* param_list: param_list TOKEN_COMMA TOKEN_SYMBOL TOKEN_COLON type_name  */
#line 421 "parser.y"
                {
			(yyval.param_list) = ast_accept_param_list_typed((yyvsp[-4].param_list), (yyvsp[-2].sval), (yyvsp[0].sval));
			debug("param_list: param_list symbol: type");
		}
#line 2660 "parser.tab.c"
    break;

  case 35: /* type_name: TOKEN_SYMBOL  */
#line 427 "parser.y"
                {
			(yyval.sval) = (yyvsp[0].sval);
		}
#line 2668 "parser.tab.c"
    break;

  case 36: /* type_name: TOKEN_SYMBOL TOKEN_LPAR type_extent_list TOKEN_RPAR  */
#line 431 "parser.y"
                {
			(yyval.sval) = ast_accept_shaped_type((yyvsp[-3].sval), (yyvsp[-1].sval));
			if ((yyval.sval) == NULL) YYABORT;
		}
#line 2677 "parser.tab.c"
    break;

  case 37: /* type_name: TOKEN_FUNC  */
#line 436 "parser.y"
                {
			(yyval.sval) = ast_strdup("func");
		}
#line 2685 "parser.tab.c"
    break;

  case 38: /* type_extent: TOKEN_INT  */
#line 441 "parser.y"
                {
			(yyval.sval) = ast_accept_type_extent_int((yyvsp[0].ival));
			if ((yyval.sval) == NULL) YYABORT;
		}
#line 2694 "parser.tab.c"
    break;

  case 39: /* type_extent: TOKEN_SYMBOL  */
#line 446 "parser.y"
                {
			(yyval.sval) = (yyvsp[0].sval);
		}
#line 2702 "parser.tab.c"
    break;

  case 40: /* type_extent_list: type_extent  */
#line 451 "parser.y"
                {
			(yyval.sval) = (yyvsp[0].sval);
		}
#line 2710 "parser.tab.c"
    break;

  case 41: /* type_extent_list: type_extent_list TOKEN_COMMA type_extent  */
#line 455 "parser.y"
                {
			(yyval.sval) = ast_accept_type_extent_list((yyvsp[-2].sval), (yyvsp[0].sval));
			if ((yyval.sval) == NULL) YYABORT;
		}
#line 2719 "parser.tab.c"
    break;

  case 42: /* stmt_list: %empty  */
#line 461 "parser.y"
                {
			(yyval.stmt_list) = ast_accept_stmt_list(NULL, NULL);
			debug("stmt_list: empty");
		}
#line 2728 "parser.tab.c"
    break;

  case 43: /* stmt_list: stmt_list stmt  */
#line 466 "parser.y"
                {
			(yyval.stmt_list) = ast_accept_stmt_list((yyvsp[-1].stmt_list), (yyvsp[0].stmt));
			debug("stmt_list: stmt_list stmt");
		}
#line 2737 "parser.tab.c"
    break;

  case 44: /* stmt: expr_stmt  */
#line 472 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2745 "parser.tab.c"
    break;

  case 45: /* stmt: gpu_launch_stmt  */
#line 476 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2753 "parser.tab.c"
    break;

  case 46: /* stmt: assign_stmt  */
#line 480 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2761 "parser.tab.c"
    break;

  case 47: /* stmt: plusassign_stmt  */
#line 484 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2769 "parser.tab.c"
    break;

  case 48: /* stmt: minusassign_stmt  */
#line 488 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2777 "parser.tab.c"
    break;

  case 49: /* stmt: mulassign_stmt  */
#line 492 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2785 "parser.tab.c"
    break;

  case 50: /* stmt: divassign_stmt  */
#line 496 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2793 "parser.tab.c"
    break;

  case 51: /* stmt: modassign_stmt  */
#line 500 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2801 "parser.tab.c"
    break;

  case 52: /* stmt: andassign_stmt  */
#line 504 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2809 "parser.tab.c"
    break;

  case 53: /* stmt: orassign_stmt  */
#line 508 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2817 "parser.tab.c"
    break;

  case 54: /* stmt: shlassign_stmt  */
#line 512 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2825 "parser.tab.c"
    break;

  case 55: /* stmt: shrassign_stmt  */
#line 516 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2833 "parser.tab.c"
    break;

  case 56: /* stmt: plusplus_stmt  */
#line 520 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2841 "parser.tab.c"
    break;

  case 57: /* stmt: minusminus_stmt  */
#line 524 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2849 "parser.tab.c"
    break;

  case 58: /* stmt: if_stmt  */
#line 528 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2857 "parser.tab.c"
    break;

  case 59: /* stmt: elif_stmt  */
#line 532 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2865 "parser.tab.c"
    break;

  case 60: /* stmt: else_stmt  */
#line 536 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2873 "parser.tab.c"
    break;

  case 61: /* stmt: while_stmt  */
#line 540 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2881 "parser.tab.c"
    break;

  case 62: /* stmt: for_stmt  */
#line 544 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2889 "parser.tab.c"
    break;

  case 63: /* stmt: return_stmt  */
#line 548 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2897 "parser.tab.c"
    break;

  case 64: /* stmt: break_stmt  */
#line 552 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2905 "parser.tab.c"
    break;

  case 65: /* stmt: continue_stmt  */
#line 556 "parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2913 "parser.tab.c"
    break;

  case 66: /* gpu_launch_stmt: TOKEN_SYMBOL TOKEN_GPU_LAUNCH_L expr TOKEN_COMMA expr TOKEN_GPU_LAUNCH_R TOKEN_LPAR arg_list TOKEN_RPAR TOKEN_SEMICOLON  */
#line 561 "parser.y"
                {
			(yyval.stmt) = ast_accept_gpu_launch_stmt((yylsp[-9]).first_line + 1, (yyvsp[-9].sval), (yyvsp[-7].expr), (yyvsp[-5].expr), (yyvsp[-2].arg_list));
			if ((yyval.stmt) == NULL) YYABORT;
		}
#line 2922 "parser.tab.c"
    break;

  case 67: /* gpu_launch_stmt: TOKEN_SYMBOL TOKEN_GPU_LAUNCH_L expr TOKEN_COMMA expr TOKEN_GPU_LAUNCH_R TOKEN_LPAR TOKEN_RPAR TOKEN_SEMICOLON  */
#line 566 "parser.y"
                {
			(yyval.stmt) = ast_accept_gpu_launch_stmt((yylsp[-8]).first_line + 1, (yyvsp[-8].sval), (yyvsp[-6].expr), (yyvsp[-4].expr), NULL);
			if ((yyval.stmt) == NULL) YYABORT;
		}
#line 2931 "parser.tab.c"
    break;

  case 68: /* expr_stmt: expr TOKEN_SEMICOLON  */
#line 572 "parser.y"
                {
			(yyval.stmt) = ast_accept_expr_stmt((yylsp[-1]).first_line + 1, (yyvsp[-1].expr));
			debug("expr_stmt");
		}
#line 2940 "parser.tab.c"
    break;

  case 69: /* assign_stmt: expr TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 578 "parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr), false, false);
			debug("assign_stmt");
		}
#line 2949 "parser.tab.c"
    break;

  case 70: /* assign_stmt: TOKEN_VAR expr TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 583 "parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt((yylsp[-4]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr), true, false);
			debug("var assign_stmt");
		}
#line 2958 "parser.tab.c"
    break;

  case 71: /* assign_stmt: TOKEN_LET expr TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 588 "parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt((yylsp[-4]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr), false, true);
			debug("let assign_stmt");
		}
#line 2967 "parser.tab.c"
    break;

  case 72: /* assign_stmt: TOKEN_SHARED TOKEN_VAR expr TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 593 "parser.y"
                {
			(yyval.stmt) = ast_accept_shared_stmt((yylsp[-5]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr), true);
		}
#line 2975 "parser.tab.c"
    break;

  case 73: /* assign_stmt: TOKEN_SHARED TOKEN_LET expr TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 597 "parser.y"
                {
			(yyval.stmt) = ast_accept_shared_stmt((yylsp[-5]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr), false);
		}
#line 2983 "parser.tab.c"
    break;

  case 74: /* assign_stmt: TOKEN_VAR expr TOKEN_COLON type_name TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 601 "parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt_typed((yylsp[-6]).first_line + 1, (yyvsp[-5].expr), (yyvsp[-1].expr), true, false, (yyvsp[-3].sval));
			debug("var typed assign_stmt");
		}
#line 2992 "parser.tab.c"
    break;

  case 75: /* assign_stmt: TOKEN_VAR expr TOKEN_COLON type_name TOKEN_SEMICOLON  */
#line 606 "parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt_typed((yylsp[-4]).first_line + 1, (yyvsp[-3].expr), ast_accept_term_expr(ast_accept_int_term(0)), true, false, (yyvsp[-1].sval));
			debug("var typed decl_stmt");
		}
#line 3001 "parser.tab.c"
    break;

  case 76: /* assign_stmt: TOKEN_LET expr TOKEN_COLON type_name TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 611 "parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt_typed((yylsp[-6]).first_line + 1, (yyvsp[-5].expr), (yyvsp[-1].expr), false, true, (yyvsp[-3].sval));
			debug("let typed assign_stmt");
		}
#line 3010 "parser.tab.c"
    break;

  case 77: /* plusassign_stmt: expr TOKEN_PLUSASSIGN expr TOKEN_SEMICOLON  */
#line 617 "parser.y"
                {
			(yyval.stmt) = ast_accept_plusassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("plusassign_stmt");
		}
#line 3019 "parser.tab.c"
    break;

  case 78: /* minusassign_stmt: expr TOKEN_MINUSASSIGN expr TOKEN_SEMICOLON  */
#line 623 "parser.y"
                {
			(yyval.stmt) = ast_accept_minusassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("minusassign_stmt");
		}
#line 3028 "parser.tab.c"
    break;

  case 79: /* mulassign_stmt: expr TOKEN_MULASSIGN expr TOKEN_SEMICOLON  */
#line 629 "parser.y"
                {
			(yyval.stmt) = ast_accept_mulassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("mulassign_stmt");
		}
#line 3037 "parser.tab.c"
    break;

  case 80: /* divassign_stmt: expr TOKEN_DIVASSIGN expr TOKEN_SEMICOLON  */
#line 635 "parser.y"
                {
			(yyval.stmt) = ast_accept_divassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("divassign_stmt");
		}
#line 3046 "parser.tab.c"
    break;

  case 81: /* modassign_stmt: expr TOKEN_MODASSIGN expr TOKEN_SEMICOLON  */
#line 641 "parser.y"
                {
			(yyval.stmt) = ast_accept_modassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("modassign_stmt");
		}
#line 3055 "parser.tab.c"
    break;

  case 82: /* andassign_stmt: expr TOKEN_ANDASSIGN expr TOKEN_SEMICOLON  */
#line 647 "parser.y"
                {
			(yyval.stmt) = ast_accept_andassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("andassign_stmt");
		}
#line 3064 "parser.tab.c"
    break;

  case 83: /* orassign_stmt: expr TOKEN_ORASSIGN expr TOKEN_SEMICOLON  */
#line 653 "parser.y"
                {
			(yyval.stmt) = ast_accept_orassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("orassign_stmt");
		}
#line 3073 "parser.tab.c"
    break;

  case 84: /* shlassign_stmt: expr TOKEN_SHLASSIGN expr TOKEN_SEMICOLON  */
#line 659 "parser.y"
                {
			(yyval.stmt) = ast_accept_shlassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("shlassign_stmt");
		}
#line 3082 "parser.tab.c"
    break;

  case 85: /* shrassign_stmt: expr TOKEN_SHRASSIGN expr TOKEN_SEMICOLON  */
#line 665 "parser.y"
                {
			(yyval.stmt) = ast_accept_shrassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("shrassign_stmt");
		}
#line 3091 "parser.tab.c"
    break;

  case 86: /* plusplus_stmt: expr TOKEN_PLUSPLUS TOKEN_SEMICOLON  */
#line 671 "parser.y"
                {
			(yyval.stmt) = ast_accept_plusplus_stmt((yylsp[-2]).first_line + 1, (yyvsp[-2].expr));
			debug("plusplus_stmt");
		}
#line 3100 "parser.tab.c"
    break;

  case 87: /* minusminus_stmt: expr TOKEN_MINUSMINUS TOKEN_SEMICOLON  */
#line 677 "parser.y"
                {
			(yyval.stmt) = ast_accept_minusminus_stmt((yylsp[-2]).first_line + 1, (yyvsp[-2].expr));
			debug("plusplus_stmt");
		}
#line 3109 "parser.tab.c"
    break;

  case 88: /* if_stmt: TOKEN_IF TOKEN_LPAR expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 683 "parser.y"
                {
			(yyval.stmt) = ast_accept_if_stmt((yylsp[-5]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("if_stmt: stmt_list");
		}
#line 3118 "parser.tab.c"
    break;

  case 89: /* if_stmt: TOKEN_IF TOKEN_LPAR expr TOKEN_RPAR stmt  */
#line 688 "parser.y"
                {
			(yyval.stmt) = ast_accept_if_stmt_single((yylsp[-4]).first_line + 1, (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("if_stmt: stmt_list");
		}
#line 3127 "parser.tab.c"
    break;

  case 90: /* elif_stmt: TOKEN_ELSEIF TOKEN_LPAR expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 694 "parser.y"
                {
			(yyval.stmt) = ast_accept_elif_stmt((yylsp[-5]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("elif_stmt: stmt_list");
		}
#line 3136 "parser.tab.c"
    break;

  case 91: /* elif_stmt: TOKEN_ELSEIF TOKEN_LPAR expr TOKEN_RPAR stmt  */
#line 699 "parser.y"
                {
			(yyval.stmt) = ast_accept_elif_stmt_single((yylsp[-4]).first_line + 1, (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("elif_stmt: stmt_list");
		}
#line 3145 "parser.tab.c"
    break;

  case 92: /* else_stmt: TOKEN_ELSE_LBLK stmt_list TOKEN_RBLK  */
#line 705 "parser.y"
                {
			(yyval.stmt) = ast_accept_else_stmt((yylsp[-2]).first_line + 1, (yyvsp[-1].stmt_list));
			debug("else_stmt: stmt_list");
		}
#line 3154 "parser.tab.c"
    break;

  case 93: /* else_stmt: TOKEN_ELSE stmt  */
#line 710 "parser.y"
                {
			(yyval.stmt) = ast_accept_else_stmt_single((yylsp[-1]).first_line + 1, (yyvsp[0].stmt));
			debug("else_stmt: stmt_list");
		}
#line 3163 "parser.tab.c"
    break;

  case 94: /* while_stmt: TOKEN_WHILE TOKEN_LPAR expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 716 "parser.y"
                {
			(yyval.stmt) = ast_accept_while_stmt((yylsp[-5]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("while_stmt: stmt_list");
		}
#line 3172 "parser.tab.c"
    break;

  case 95: /* while_stmt: TOKEN_WHILE TOKEN_LPAR expr TOKEN_RPAR stmt  */
#line 721 "parser.y"
                {
			(yyval.stmt) = ast_accept_while_stmt_single((yylsp[-4]).first_line + 1, (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("while_stmt: stmt_list");
		}
#line 3181 "parser.tab.c"
    break;

  case 96: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_COMMA TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 727 "parser.y"
                {
			(yyval.stmt) = ast_accept_for_kv_stmt((yylsp[-9]).first_line + 1, (yyvsp[-7].sval), (yyvsp[-5].sval), (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("for_stmt: for(k, v in array) { stmt_list }");
		}
#line 3190 "parser.tab.c"
    break;

  case 97: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_COMMA TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR stmt  */
#line 732 "parser.y"
                {
			(yyval.stmt) = ast_accept_for_kv_stmt_single((yylsp[-8]).first_line + 1, (yyvsp[-6].sval), (yyvsp[-4].sval), (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("for_stmt: for(k, v in array) stmt");
		}
#line 3199 "parser.tab.c"
    break;

  case 98: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 737 "parser.y"
                {
			(yyval.stmt) = ast_accept_for_v_stmt((yylsp[-7]).first_line + 1, (yyvsp[-5].sval), (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("for_stmt: for(v in array) { stmt_list }");
		}
#line 3208 "parser.tab.c"
    break;

  case 99: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR stmt  */
#line 742 "parser.y"
                {
			(yyval.stmt) = ast_accept_for_v_stmt_single((yylsp[-6]).first_line + 1, (yyvsp[-4].sval), (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("for_stmt: for(v in array) stmt_list");
		}
#line 3217 "parser.tab.c"
    break;

  case 100: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_DOTDOT expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 747 "parser.y"
                {
			(yyval.stmt) = ast_accept_for_range_stmt((yylsp[-9]).first_line + 1, (yyvsp[-7].sval), (yyvsp[-5].expr), (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("for_stmt: for(i in x..y) { stmt_list }");
		}
#line 3226 "parser.tab.c"
    break;

  case 101: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_DOTDOT expr TOKEN_RPAR stmt  */
#line 752 "parser.y"
                {
			(yyval.stmt) = ast_accept_for_range_stmt_single((yylsp[-8]).first_line + 1, (yyvsp[-6].sval), (yyvsp[-4].expr), (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("for_stmt: for(i in x..y) stmt");
		}
#line 3235 "parser.tab.c"
    break;

  case 102: /* return_stmt: TOKEN_RETURN expr TOKEN_SEMICOLON  */
#line 758 "parser.y"
                {
			(yyval.stmt) = ast_accept_return_stmt((yylsp[-2]).first_line + 1, (yyvsp[-1].expr));
			debug("rerurn_stmt:");
		}
#line 3244 "parser.tab.c"
    break;

  case 103: /* return_stmt: TOKEN_RETURN TOKEN_SEMICOLON  */
#line 763 "parser.y"
                {
			(yyval.stmt) = ast_accept_return_stmt((yylsp[-1]).first_line + 1, NULL);
			debug("rerurn_stmt NULL:");
		}
#line 3253 "parser.tab.c"
    break;

  case 104: /* break_stmt: TOKEN_BREAK TOKEN_SEMICOLON  */
#line 768 "parser.y"
                {
			(yyval.stmt) = ast_accept_break_stmt((yylsp[-1]).first_line + 1);
			debug("break_stmt:");
		}
#line 3262 "parser.tab.c"
    break;

  case 105: /* continue_stmt: TOKEN_CONTINUE TOKEN_SEMICOLON  */
#line 774 "parser.y"
                {
			(yyval.stmt) = ast_accept_continue_stmt((yylsp[-1]).first_line + 1);
			debug("continue_stmt");
		}
#line 3271 "parser.tab.c"
    break;

  case 106: /* expr: term  */
#line 780 "parser.y"
                {
			(yyval.expr) = ast_accept_term_expr((yyvsp[0].term));
			debug("expr: term");
		}
#line 3280 "parser.tab.c"
    break;

  case 107: /* expr: TOKEN_LPAR expr TOKEN_RPAR  */
#line 785 "parser.y"
                {
			(yyval.expr) = ast_accept_par_expr((yyvsp[-1].expr));
			debug("expr: (expr)");
		}
#line 3289 "parser.tab.c"
    break;

  case 108: /* expr: expr TOKEN_LARR expr TOKEN_RARR  */
#line 790 "parser.y"
                {
			(yyval.expr) = ast_accept_subscr_expr((yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("expr: array[subscript]");
		}
#line 3298 "parser.tab.c"
    break;

  case 109: /* expr: expr TOKEN_LARR multi_index_list TOKEN_RARR  */
#line 795 "parser.y"
                {
			(yyval.expr) = ast_accept_subscr_expr((yyvsp[-3].expr),
				ast_accept_array_expr((yyvsp[-1].arg_list)));
			debug("expr: array[index, ...]");
		}
#line 3308 "parser.tab.c"
    break;

  case 110: /* expr: expr TOKEN_OR expr  */
#line 801 "parser.y"
                {
			(yyval.expr) = ast_accept_or_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr or expr");
		}
#line 3317 "parser.tab.c"
    break;

  case 111: /* expr: expr TOKEN_AND expr  */
#line 806 "parser.y"
                {
			(yyval.expr) = ast_accept_and_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr and expr");
		}
#line 3326 "parser.tab.c"
    break;

  case 112: /* expr: expr TOKEN_XOR expr  */
#line 811 "parser.y"
                {
			(yyval.expr) = ast_accept_xor_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr xor expr");
		}
#line 3335 "parser.tab.c"
    break;

  case 113: /* expr: expr TOKEN_OROR expr  */
#line 816 "parser.y"
                {
			(yyval.expr) = ast_accept_lor_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr || expr");
		}
#line 3344 "parser.tab.c"
    break;

  case 114: /* expr: expr TOKEN_ANDAND expr  */
#line 821 "parser.y"
                {
			(yyval.expr) = ast_accept_land_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr && expr");
		}
#line 3353 "parser.tab.c"
    break;

  case 115: /* expr: expr TOKEN_LT expr  */
#line 826 "parser.y"
                {
			(yyval.expr) = ast_accept_lt_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr lt expr");
		}
#line 3362 "parser.tab.c"
    break;

  case 116: /* expr: expr TOKEN_LTE expr  */
#line 831 "parser.y"
                {
			(yyval.expr) = ast_accept_lte_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr lte expr");
		}
#line 3371 "parser.tab.c"
    break;

  case 117: /* expr: expr TOKEN_GT expr  */
#line 836 "parser.y"
                {
			(yyval.expr) = ast_accept_gt_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr gt expr");
		}
#line 3380 "parser.tab.c"
    break;

  case 118: /* expr: expr TOKEN_GTE expr  */
#line 841 "parser.y"
                {
			(yyval.expr) = ast_accept_gte_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr gte expr");
		}
#line 3389 "parser.tab.c"
    break;

  case 119: /* expr: expr TOKEN_EQ expr  */
#line 846 "parser.y"
                {
			(yyval.expr) = ast_accept_eq_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr eq expr");
		}
#line 3398 "parser.tab.c"
    break;

  case 120: /* expr: expr TOKEN_NEQ expr  */
#line 851 "parser.y"
                {
			(yyval.expr) = ast_accept_neq_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr neq expr");
		}
#line 3407 "parser.tab.c"
    break;

  case 121: /* expr: expr TOKEN_PLUS expr  */
#line 856 "parser.y"
                {
			(yyval.expr) = ast_accept_plus_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr plus expr");
		}
#line 3416 "parser.tab.c"
    break;

  case 122: /* expr: expr TOKEN_MINUS expr  */
#line 861 "parser.y"
                {
			(yyval.expr) = ast_accept_minus_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr sub expr");
		}
#line 3425 "parser.tab.c"
    break;

  case 123: /* expr: expr TOKEN_MUL expr  */
#line 866 "parser.y"
                {
			(yyval.expr) = ast_accept_mul_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr mul expr");
		}
#line 3434 "parser.tab.c"
    break;

  case 124: /* expr: expr TOKEN_DIV expr  */
#line 871 "parser.y"
                {
			(yyval.expr) = ast_accept_div_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr div expr");
		}
#line 3443 "parser.tab.c"
    break;

  case 125: /* expr: expr TOKEN_MOD expr  */
#line 876 "parser.y"
                {
			(yyval.expr) = ast_accept_mod_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr div expr");
		}
#line 3452 "parser.tab.c"
    break;

  case 126: /* expr: expr TOKEN_SHL expr  */
#line 881 "parser.y"
                {
			(yyval.expr) = ast_accept_shl_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr shl expr");
		}
#line 3461 "parser.tab.c"
    break;

  case 127: /* expr: expr TOKEN_SHR expr  */
#line 886 "parser.y"
                {
			(yyval.expr) = ast_accept_shr_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr shr expr");
		}
#line 3470 "parser.tab.c"
    break;

  case 128: /* expr: TOKEN_MINUS expr  */
#line 891 "parser.y"
                {
			(yyval.expr) = ast_accept_neg_expr((yyvsp[0].expr));
			debug("expr: neg expr");
		}
#line 3479 "parser.tab.c"
    break;

  case 129: /* expr: TOKEN_NOT expr  */
#line 896 "parser.y"
                {
			(yyval.expr) = ast_accept_not_expr((yyvsp[0].expr));
			debug("expr: not expr");
		}
#line 3488 "parser.tab.c"
    break;

  case 130: /* expr: expr TOKEN_DOT property_name  */
#line 901 "parser.y"
                {
			(yyval.expr) = ast_accept_dot_expr((yyvsp[-2].expr), (yyvsp[0].sval));
			debug("expr: expr.symbol");
		}
#line 3497 "parser.tab.c"
    break;

  case 131: /* expr: call_expr  */
#line 906 "parser.y"
                {
			(yyval.expr) = (yyvsp[0].expr);
		}
#line 3505 "parser.tab.c"
    break;

  case 132: /* expr: TOKEN_LARR arg_list TOKEN_RARR  */
#line 910 "parser.y"
                {
			(yyval.expr) = ast_accept_array_expr((yyvsp[-1].arg_list));
			debug("expr: array");
		}
#line 3514 "parser.tab.c"
    break;

  case 133: /* expr: TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 915 "parser.y"
                {
			(yyval.expr) = ast_accept_dict_expr((yyvsp[-1].kv_list));
			debug("expr: dict");
		}
#line 3523 "parser.tab.c"
    break;

  case 134: /* expr: TOKEN_CLASS TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 920 "parser.y"
                {
			/* class is a frozen dict. */
			(yyval.expr) = ast_accept_class_expr((yyvsp[-1].kv_list));
			debug("expr: class");
		}
#line 3533 "parser.tab.c"
    break;

  case 135: /* expr: TOKEN_CLASS TOKEN_LBLK TOKEN_RBLK  */
#line 926 "parser.y"
                {
			/* class is a frozen dict. */
			(yyval.expr) = ast_accept_class_expr(NULL);
			debug("expr: class");
		}
#line 3543 "parser.tab.c"
    break;

  case 136: /* expr: lambda_expr  */
#line 932 "parser.y"
                {
			(yyval.expr) = (yyvsp[0].expr);
		}
#line 3551 "parser.tab.c"
    break;

  case 137: /* expr: TOKEN_NEW TOKEN_SYMBOL TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 936 "parser.y"
                {
			(yyval.expr) = ast_accept_new_expr((yyvsp[-3].sval), (yyvsp[-1].kv_list));
			debug("expr: new");
		}
#line 3560 "parser.tab.c"
    break;

  case 138: /* expr: TOKEN_EXTEND TOKEN_SYMBOL TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 941 "parser.y"
                {
			(yyval.expr) = ast_accept_extend_expr((yyvsp[-3].sval), (yyvsp[-1].kv_list));
			debug("expr: extend");
		}
#line 3569 "parser.tab.c"
    break;

  case 139: /* expr: TOKEN_EXTEND TOKEN_SYMBOL TOKEN_LBLK TOKEN_RBLK  */
#line 946 "parser.y"
                {
			(yyval.expr) = ast_accept_extend_expr((yyvsp[-2].sval), NULL);
			debug("expr: extend");
		}
#line 3578 "parser.tab.c"
    break;

  case 140: /* expr: TOKEN_NEW TOKEN_SYMBOL TOKEN_LBLK TOKEN_RBLK  */
#line 951 "parser.y"
                {
			(yyval.expr) = ast_accept_new_expr((yyvsp[-2].sval), NULL);
			debug("expr: new");
		}
#line 3587 "parser.tab.c"
    break;

  case 141: /* call_expr: expr TOKEN_LPAR arg_list TOKEN_RPAR  */
#line 957 "parser.y"
                {
			(yyval.expr) = ast_accept_call_expr((yyvsp[-3].expr), (yyvsp[-1].arg_list));
			debug("expr: call(param_list)");
		}
#line 3596 "parser.tab.c"
    break;

  case 142: /* call_expr: expr TOKEN_LPAR TOKEN_RPAR  */
#line 962 "parser.y"
                {
			(yyval.expr) = ast_accept_call_expr((yyvsp[-2].expr), NULL);
			debug("expr: call()");
		}
#line 3605 "parser.tab.c"
    break;

  case 143: /* lambda_expr: TOKEN_LPAR param_list TOKEN_RPAR_DARROW_LBLK stmt_list TOKEN_RBLK  */
#line 968 "parser.y"
                {
			(yyval.expr) = ast_accept_func_expr((yyvsp[-3].param_list), (yyvsp[-1].stmt_list));
			debug("expr: func param_list stmt_list");
		}
#line 3614 "parser.tab.c"
    break;

  case 144: /* lambda_expr: TOKEN_LPAR TOKEN_RPAR_DARROW_LBLK stmt_list TOKEN_RBLK  */
#line 973 "parser.y"
                {
			(yyval.expr) = ast_accept_func_expr(NULL, (yyvsp[-1].stmt_list));
			debug("expr: func stmt_list");
		}
#line 3623 "parser.tab.c"
    break;

  case 145: /* arg_list: expr  */
#line 979 "parser.y"
                {
			(yyval.arg_list) = ast_accept_arg_list(NULL, (yyvsp[0].expr));
			debug("arg_list: expr");
		}
#line 3632 "parser.tab.c"
    break;

  case 146: /* arg_list: arg_list TOKEN_COMMA expr  */
#line 984 "parser.y"
                {
			(yyval.arg_list) = ast_accept_arg_list((yyvsp[-2].arg_list), (yyvsp[0].expr));
			debug("arg_list: arg_list arg");
		}
#line 3641 "parser.tab.c"
    break;

  case 147: /* multi_index_list: expr TOKEN_COMMA expr  */
#line 990 "parser.y"
                {
			(yyval.arg_list) = ast_accept_arg_list(NULL, (yyvsp[-2].expr));
			(yyval.arg_list) = ast_accept_arg_list((yyval.arg_list), (yyvsp[0].expr));
		}
#line 3650 "parser.tab.c"
    break;

  case 148: /* multi_index_list: multi_index_list TOKEN_COMMA expr  */
#line 995 "parser.y"
                {
			(yyval.arg_list) = ast_accept_arg_list((yyvsp[-2].arg_list), (yyvsp[0].expr));
		}
#line 3658 "parser.tab.c"
    break;

  case 149: /* kv_list: kv  */
#line 1000 "parser.y"
                {
			(yyval.kv_list) = ast_accept_kv_list(NULL, (yyvsp[0].kv));
			debug("kv_list: kv");
		}
#line 3667 "parser.tab.c"
    break;

  case 150: /* kv_list: kv_list TOKEN_COMMA kv  */
#line 1005 "parser.y"
                {
			(yyval.kv_list) = ast_accept_kv_list((yyvsp[-2].kv_list), (yyvsp[0].kv));
			debug("kv_list: kv_list kv");
		}
#line 3676 "parser.tab.c"
    break;

  case 151: /* kv: TOKEN_STR TOKEN_COLON expr  */
#line 1011 "parser.y"
                {
			(yyval.kv) = ast_accept_kv((yyvsp[-2].sval), (yyvsp[0].expr));
			debug("kv");
		}
#line 3685 "parser.tab.c"
    break;

  case 152: /* kv: property_name TOKEN_COLON expr  */
#line 1016 "parser.y"
                {
			(yyval.kv) = ast_accept_kv((yyvsp[-2].sval), (yyvsp[0].expr));
			debug("kv");
		}
#line 3694 "parser.tab.c"
    break;

  case 153: /* property_name: TOKEN_SYMBOL  */
#line 1022 "parser.y"
                {
			(yyval.sval) = (yyvsp[0].sval);
		}
#line 3702 "parser.tab.c"
    break;

  case 154: /* property_name: TOKEN_STATIC  */
#line 1026 "parser.y"
                {
			(yyval.sval) = ast_strdup("static");
		}
#line 3710 "parser.tab.c"
    break;

  case 155: /* property_name: TOKEN_INLINE  */
#line 1030 "parser.y"
                {
			(yyval.sval) = ast_strdup("inline");
		}
#line 3718 "parser.tab.c"
    break;

  case 156: /* property_name: TOKEN_REQUIRE  */
#line 1034 "parser.y"
                {
			(yyval.sval) = ast_strdup("require");
		}
#line 3726 "parser.tab.c"
    break;

  case 157: /* property_name: TOKEN_ACCEL  */
#line 1038 "parser.y"
                {
			(yyval.sval) = ast_strdup("accel");
		}
#line 3734 "parser.tab.c"
    break;

  case 158: /* property_name: TOKEN_GPU  */
#line 1042 "parser.y"
                {
			(yyval.sval) = ast_strdup("gpu");
		}
#line 3742 "parser.tab.c"
    break;

  case 159: /* term: TOKEN_INT  */
#line 1047 "parser.y"
                {
			(yyval.term) = ast_accept_int_term((yyvsp[0].ival));
			debug("term: int");
		}
#line 3751 "parser.tab.c"
    break;

  case 160: /* term: TOKEN_LONG  */
#line 1052 "parser.y"
                {
			(yyval.term) = ast_accept_long_term((yyvsp[0].lval));
			debug("term: long");
		}
#line 3760 "parser.tab.c"
    break;

  case 161: /* term: TOKEN_FLOAT  */
#line 1057 "parser.y"
                {
			(yyval.term) = ast_accept_float_term((yyvsp[0].fval));
			debug("term: float");
		}
#line 3769 "parser.tab.c"
    break;

  case 162: /* term: TOKEN_DOUBLE  */
#line 1062 "parser.y"
                {
			(yyval.term) = ast_accept_double_term((yyvsp[0].lfval));
			debug("term: double");
		}
#line 3778 "parser.tab.c"
    break;

  case 163: /* term: TOKEN_STR  */
#line 1067 "parser.y"
                {
			(yyval.term) = ast_accept_str_term((yyvsp[0].sval));
			debug("term: string");
		}
#line 3787 "parser.tab.c"
    break;

  case 164: /* term: TOKEN_SYMBOL  */
#line 1072 "parser.y"
                {
			(yyval.term) = ast_accept_symbol_term((yyvsp[0].sval));
			debug("term: symbol");
		}
#line 3796 "parser.tab.c"
    break;

  case 165: /* term: TOKEN_LARR TOKEN_RARR  */
#line 1077 "parser.y"
                {
			(yyval.term) = ast_accept_empty_array_term();
			debug("term: empty array symbol");
		}
#line 3805 "parser.tab.c"
    break;

  case 166: /* term: TOKEN_LBLK TOKEN_RBLK  */
#line 1082 "parser.y"
                {
			(yyval.term) = ast_accept_empty_dict_term();
			debug("term: empty dict symbol");
		}
#line 3814 "parser.tab.c"
    break;


#line 3818 "parser.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (scanner, YY_("syntax error"));
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, scanner);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (scanner, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, scanner);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 1087 "parser.y"


#ifdef DEBUG
static void print_debug(const char *s)
{
	fprintf(stderr, "%s\n", s);
}
#endif

void ast_yyerror(void *scanner, char *s)
{
	extern int ast_error_line;
	extern int ast_error_column;
	extern char ast_error_message[65536];
	extern const char *noct_gettext(const char *msg);

	(void)scanner;
	(void)s;

	ast_error_line = ast_yylloc.first_line + 1;
	ast_error_column = ast_yylloc.first_column + 1;
	
	if (s != NULL)
		snprintf(ast_error_message, sizeof(ast_error_message), "%s", N_TR(s));
	else
		ast_error_message[0] = '\0';
}
