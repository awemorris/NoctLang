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
#line 1 "src/core/parser.y"

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
bool ast_accept_toplevel_class(int line, char *name, struct ast_kv_list *kv_list);
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

#line 120 "src/core/parser.y"

extern void ast_yyerror(void *scanner, char *s);

#line 201 "src/core/parser.tab.c"

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
  YYSYMBOL_TOKEN_NOT = 72,                 /* TOKEN_NOT  */
  YYSYMBOL_UNARYMINUS = 73,                /* UNARYMINUS  */
  YYSYMBOL_CALL = 74,                      /* CALL  */
  YYSYMBOL_YYACCEPT = 75,                  /* $accept  */
  YYSYMBOL_func_list = 76,                 /* func_list  */
  YYSYMBOL_toplevel_decl = 77,             /* toplevel_decl  */
  YYSYMBOL_func_prefix = 78,               /* func_prefix  */
  YYSYMBOL_func = 79,                      /* func  */
  YYSYMBOL_param_list = 80,                /* param_list  */
  YYSYMBOL_type_name = 81,                 /* type_name  */
  YYSYMBOL_stmt_list = 82,                 /* stmt_list  */
  YYSYMBOL_stmt = 83,                      /* stmt  */
  YYSYMBOL_expr_stmt = 84,                 /* expr_stmt  */
  YYSYMBOL_assign_stmt = 85,               /* assign_stmt  */
  YYSYMBOL_plusassign_stmt = 86,           /* plusassign_stmt  */
  YYSYMBOL_minusassign_stmt = 87,          /* minusassign_stmt  */
  YYSYMBOL_mulassign_stmt = 88,            /* mulassign_stmt  */
  YYSYMBOL_divassign_stmt = 89,            /* divassign_stmt  */
  YYSYMBOL_modassign_stmt = 90,            /* modassign_stmt  */
  YYSYMBOL_andassign_stmt = 91,            /* andassign_stmt  */
  YYSYMBOL_orassign_stmt = 92,             /* orassign_stmt  */
  YYSYMBOL_shlassign_stmt = 93,            /* shlassign_stmt  */
  YYSYMBOL_shrassign_stmt = 94,            /* shrassign_stmt  */
  YYSYMBOL_plusplus_stmt = 95,             /* plusplus_stmt  */
  YYSYMBOL_minusminus_stmt = 96,           /* minusminus_stmt  */
  YYSYMBOL_if_stmt = 97,                   /* if_stmt  */
  YYSYMBOL_elif_stmt = 98,                 /* elif_stmt  */
  YYSYMBOL_else_stmt = 99,                 /* else_stmt  */
  YYSYMBOL_while_stmt = 100,               /* while_stmt  */
  YYSYMBOL_for_stmt = 101,                 /* for_stmt  */
  YYSYMBOL_return_stmt = 102,              /* return_stmt  */
  YYSYMBOL_break_stmt = 103,               /* break_stmt  */
  YYSYMBOL_continue_stmt = 104,            /* continue_stmt  */
  YYSYMBOL_expr = 105,                     /* expr  */
  YYSYMBOL_call_expr = 106,                /* call_expr  */
  YYSYMBOL_lambda_expr = 107,              /* lambda_expr  */
  YYSYMBOL_arg_list = 108,                 /* arg_list  */
  YYSYMBOL_kv_list = 109,                  /* kv_list  */
  YYSYMBOL_kv = 110,                       /* kv  */
  YYSYMBOL_term = 111                      /* term  */
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
#define YYFINAL  17
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   2962

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  37
/* YYNRULES -- Number of rules.  */
#define YYNRULES  135
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  317

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   329


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
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   233,   233,   238,   243,   248,   254,   260,   266,   272,
     278,   284,   291,   295,   299,   304,   310,   316,   322,   329,
     334,   339,   344,   350,   354,   360,   364,   370,   374,   378,
     382,   386,   390,   394,   398,   402,   406,   410,   414,   418,
     422,   426,   430,   434,   438,   442,   446,   450,   455,   461,
     466,   471,   476,   481,   486,   492,   498,   504,   510,   516,
     522,   528,   534,   540,   546,   552,   558,   563,   569,   574,
     580,   585,   591,   596,   602,   607,   612,   617,   622,   627,
     633,   638,   643,   649,   655,   660,   665,   670,   675,   680,
     685,   690,   695,   700,   705,   710,   715,   720,   725,   730,
     735,   740,   745,   750,   755,   760,   765,   770,   775,   779,
     784,   789,   795,   801,   805,   810,   815,   820,   826,   831,
     837,   842,   848,   853,   859,   864,   870,   875,   881,   886,
     891,   896,   901,   906,   911,   916
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
  "TOKEN_NOT", "UNARYMINUS", "CALL", "$accept", "func_list",
  "toplevel_decl", "func_prefix", "func", "param_list", "type_name",
  "stmt_list", "stmt", "expr_stmt", "assign_stmt", "plusassign_stmt",
  "minusassign_stmt", "mulassign_stmt", "divassign_stmt", "modassign_stmt",
  "andassign_stmt", "orassign_stmt", "shlassign_stmt", "shrassign_stmt",
  "plusplus_stmt", "minusminus_stmt", "if_stmt", "elif_stmt", "else_stmt",
  "while_stmt", "for_stmt", "return_stmt", "break_stmt", "continue_stmt",
  "expr", "call_expr", "lambda_expr", "arg_list", "kv_list", "kv", "term", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-149)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-20)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      19,  -149,    32,    49,   109,    13,    23,  -149,   119,  -149,
     -32,   102,   110,  -149,   136,   138,   134,  -149,  -149,  -149,
     115,     2,   506,   506,   137,   143,  -149,    41,   123,   124,
    -149,    84,  -149,  -149,  -149,  -149,  -149,  -149,  -149,   130,
     168,    91,   506,   296,     6,   169,   506,  1621,  -149,  -149,
    -149,  1675,   506,   506,   127,   131,  -149,    83,   506,   506,
    -149,    62,     8,   142,  -149,  2755,   -11,     4,   -18,  -149,
      37,  1729,  -149,    90,   170,     4,   506,   506,   506,   506,
     506,   506,   506,   506,   506,   506,   366,  -149,   173,   506,
     506,   506,   506,   506,   506,   506,   506,   506,  -149,  1783,
    1837,   100,   100,   194,   163,  -149,   174,  2755,  2755,  -149,
    -149,   103,    10,  -149,   506,   280,  -149,  -149,  -149,    12,
    1891,   233,   233,     4,     4,     4,   167,   167,  2841,  2809,
    -149,     5,  -149,   638,   638,   638,   638,   569,   569,  2903,
      98,  2873,  -149,  -149,  -149,  -149,  -149,   175,  -149,   172,
    1077,  -149,   176,   180,   181,   436,   177,   178,   506,   506,
    -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,
    -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,
    -149,  -149,  1135,   100,   350,   179,  -149,  -149,   104,  2755,
    -149,   420,  -149,   105,  -149,  -149,  -149,   506,  -149,   490,
     506,   506,   215,  -149,  1945,  -149,  -149,  1243,  1297,   506,
     506,   506,   506,   506,   506,   506,   506,   506,   506,   182,
     183,  -149,   187,  -149,   100,  -149,  -149,  -149,   560,  1351,
    -149,  1405,  1459,    -6,  -149,   506,   100,   506,   100,  1999,
    2053,  2107,  2161,  2215,  2269,  2323,  2377,  2431,  2485,  -149,
    -149,  -149,  -149,  -149,  1077,  -149,  1077,  -149,  1077,  -149,
     218,   506,  2539,    -4,  2593,   151,  -149,  -149,  -149,  -149,
    -149,  -149,  -149,  -149,  -149,  -149,   630,  -149,   700,  -149,
     770,  -149,   840,   184,  1189,  -149,   506,  -149,  -149,   506,
    -149,  -149,  -149,  -149,   506,  1077,  -149,   506,  2647,  2701,
    1513,  -149,   910,  1567,  -149,  -149,  1077,  -149,  -149,  1077,
    -149,  -149,   980,  -149,  1050,  -149,  -149
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,    12,     0,     0,     0,     0,     0,     3,     0,     2,
       0,     0,     0,    13,     0,     0,     0,     1,     5,     4,
       0,     0,     0,     0,     0,     0,    14,     0,     0,     0,
      11,     0,   124,   133,   132,   128,   129,   130,   131,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   108,   113,
      84,     0,     0,     0,    19,     0,    25,     0,     0,     0,
      10,     0,     0,     0,   134,   122,     0,   105,   133,    25,
       0,     0,   135,     0,     0,   106,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     6,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     7,     0,
       0,     0,     0,     0,     0,    25,     0,   127,   126,   125,
     112,     0,     0,   109,     0,     0,    25,    85,   110,     0,
       0,    98,    99,   100,   101,   102,   103,   104,    91,    90,
     119,     0,   107,    94,    95,    92,    93,    96,    97,    88,
      87,    89,     8,     9,    23,    24,    20,     0,    16,     0,
       0,    25,     0,     0,     0,     0,     0,     0,     0,     0,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,     0,     0,     0,    21,   111,   117,     0,   123,
     121,     0,   116,     0,    86,   118,    25,     0,    71,     0,
       0,     0,     0,    81,     0,    82,    83,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    48,     0,    15,     0,   114,   120,   115,     0,     0,
      70,     0,     0,     0,    80,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    64,
      65,    25,    22,    18,     0,    25,     0,    25,     0,    25,
       0,     0,     0,     0,     0,     0,    49,    55,    56,    57,
      58,    59,    60,    61,    62,    63,     0,    67,     0,    69,
       0,    73,     0,     0,     0,    50,     0,    53,    51,     0,
      17,    66,    68,    72,     0,     0,    25,     0,     0,     0,
       0,    77,     0,     0,    52,    54,     0,    25,    76,     0,
      25,    75,     0,    79,     0,    74,    78
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -149,  -149,   222,  -149,   226,   191,   -98,   -28,  -148,  -149,
    -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,
    -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,  -149,
     -22,  -149,  -149,   152,   -36,   186,  -149
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,     6,     7,     8,     9,    57,   146,   103,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
     182,    48,    49,    66,    31,    32,    50
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      47,    51,   198,   113,   147,    28,    29,    21,    73,    28,
      29,    28,    29,    28,    29,    28,    29,    76,   286,    65,
      67,    71,    13,    17,    75,   101,   111,   -19,     1,     2,
      99,   100,     1,     2,   114,    10,   107,   108,   287,   260,
      86,   115,   195,    30,    54,   -19,   261,    72,    88,   110,
     114,   187,    11,   192,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,    65,    28,    29,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   188,   184,    55,    56,
      14,    15,   106,   193,    16,   222,     3,     4,   191,     5,
       3,     4,   189,     5,    33,    34,    35,    36,    37,    38,
     116,    39,    40,   144,    41,    64,   277,    42,   279,   145,
     281,    76,    12,    77,    78,    79,    80,    81,    82,    83,
     104,   105,    20,   199,    22,    60,   252,    43,   106,    61,
      44,   118,    23,   204,    86,    61,   207,   208,   263,    24,
     265,    25,    88,    26,   186,   225,   227,   301,    61,    61,
      61,    27,    89,    90,    91,    92,    93,    94,   311,    52,
      45,   313,    95,    46,    97,    53,    58,    59,   228,    62,
     101,    63,    74,   289,   102,   229,   132,   185,   231,   232,
      76,   112,    77,    78,    79,    80,    81,   239,   240,   241,
     242,   243,   244,   245,   246,   247,   248,    33,    34,    35,
      36,    37,    38,    86,    39,    40,   183,    41,   197,   119,
      42,    88,   200,   262,   196,   264,   201,   202,   233,   205,
     206,   283,   224,   276,   249,   250,   251,   278,    18,   280,
      43,   282,    19,    44,    70,   148,   294,     0,   131,   284,
     149,   150,   151,   152,   153,   154,    76,   109,     0,     0,
      79,    80,    81,     0,   155,   156,   157,     0,     0,     0,
       0,   158,   159,    45,   298,     0,    46,   299,   302,    86,
       0,     0,   300,     0,     0,   303,     0,    88,     0,   312,
       0,     0,   314,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,     0,    42,     0,     0,    68,
      34,    35,    36,    37,    38,     0,    39,    40,     0,    41,
       0,     0,    42,     0,     0,     0,    43,     0,     0,    44,
       0,   190,     0,     0,     0,     0,   149,   150,   151,   152,
     153,   154,    43,     0,     0,    44,     0,     0,     0,     0,
     155,   156,   157,     0,     0,     0,     0,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,    69,
      39,    40,     0,    41,     0,    45,    42,     0,    46,    33,
      34,    35,    36,    37,    38,     0,    39,    40,     0,    41,
       0,     0,    42,     0,     0,     0,    43,     0,     0,    44,
       0,   223,     0,     0,     0,     0,   149,   150,   151,   152,
     153,   154,    43,   130,     0,    44,     0,     0,     0,     0,
     155,   156,   157,     0,     0,     0,     0,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,    45,    42,     0,    46,    33,
      34,    35,    36,    37,    38,     0,    39,    40,     0,    41,
       0,     0,    42,     0,     0,     0,    43,     0,     0,    44,
       0,   226,     0,     0,     0,     0,   149,   150,   151,   152,
     153,   154,    43,     0,     0,    44,     0,     0,   203,     0,
     155,   156,   157,     0,     0,     0,     0,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,    45,    42,     0,    46,    33,
      34,    35,    36,    37,    38,     0,    39,    40,     0,    41,
       0,     0,    42,     0,     0,     0,    43,     0,     0,    44,
       0,   230,     0,     0,     0,     0,   149,   150,   151,   152,
     153,   154,    43,     0,     0,    44,     0,     0,     0,     0,
     155,   156,   157,     0,     0,     0,     0,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,    45,    42,     0,    46,     0,
       0,     0,    76,     0,    77,    78,    79,    80,    81,    82,
      83,     0,     0,     0,     0,     0,    43,     0,     0,    44,
       0,   253,     0,     0,     0,    86,   149,   150,   151,   152,
     153,   154,     0,    88,     0,     0,     0,     0,     0,     0,
     155,   156,   157,    89,    90,    91,    92,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,     0,    42,     0,     0,     0,
       0,    76,     0,    77,    78,    79,    80,    81,    82,    83,
       0,     0,     0,     0,     0,     0,    43,     0,     0,    44,
       0,   290,     0,     0,    86,     0,   149,   150,   151,   152,
     153,   154,    88,     0,     0,     0,     0,     0,     0,     0,
     155,   156,   157,     0,     0,     0,     0,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,     0,    42,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    43,     0,     0,    44,
       0,   291,     0,     0,     0,     0,   149,   150,   151,   152,
     153,   154,     0,     0,     0,     0,     0,     0,     0,     0,
     155,   156,   157,     0,     0,     0,     0,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,     0,    42,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    43,     0,     0,    44,
       0,   292,     0,     0,     0,     0,   149,   150,   151,   152,
     153,   154,     0,     0,     0,     0,     0,     0,     0,     0,
     155,   156,   157,     0,     0,     0,     0,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,     0,    42,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    43,     0,     0,    44,
       0,   293,     0,     0,     0,     0,   149,   150,   151,   152,
     153,   154,     0,     0,     0,     0,     0,     0,     0,     0,
     155,   156,   157,     0,     0,     0,     0,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,     0,    42,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    43,     0,     0,    44,
       0,   308,     0,     0,     0,     0,   149,   150,   151,   152,
     153,   154,     0,     0,     0,     0,     0,     0,     0,     0,
     155,   156,   157,     0,     0,     0,     0,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,     0,    42,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    43,     0,     0,    44,
       0,   315,     0,     0,     0,     0,   149,   150,   151,   152,
     153,   154,     0,     0,     0,     0,     0,     0,     0,     0,
     155,   156,   157,     0,     0,     0,     0,   158,   159,    45,
       0,     0,    46,    33,    34,    35,    36,    37,    38,     0,
      39,    40,     0,    41,     0,     0,    42,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      33,    34,    35,    36,    37,    38,    43,    39,    40,    44,
      41,   316,     0,    42,     0,     0,   149,   150,   151,   152,
     153,   154,     0,     0,     0,     0,     0,     0,     0,     0,
     155,   156,   157,    43,     0,     0,    44,   158,   159,    45,
       0,     0,    46,   149,   150,   151,   152,   153,   154,     0,
       0,     0,     0,     0,     0,     0,     0,   155,   156,   157,
       0,     0,     0,     0,   158,   159,    45,     0,    76,    46,
      77,    78,    79,    80,    81,    82,    83,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,   219,   220,    84,
      85,    86,     0,     0,     0,     0,     0,   221,     0,    88,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
      90,    91,    92,    93,    94,     0,     0,     0,     0,    95,
      96,    97,    76,     0,    77,    78,    79,    80,    81,    82,
      83,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    84,    85,    86,   295,   296,     0,     0,
       0,     0,     0,    88,     0,     0,     0,     0,     0,     0,
       0,     0,   297,    89,    90,    91,    92,    93,    94,     0,
       0,     0,     0,    95,    96,    97,    76,     0,    77,    78,
      79,    80,    81,    82,    83,   235,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    84,    85,    86,
       0,     0,     0,     0,     0,     0,   236,    88,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    89,    90,    91,
      92,    93,    94,     0,     0,     0,     0,    95,    96,    97,
      76,     0,    77,    78,    79,    80,    81,    82,    83,   237,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    84,    85,    86,     0,     0,     0,     0,     0,     0,
     238,    88,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    89,    90,    91,    92,    93,    94,     0,     0,     0,
       0,    95,    96,    97,    76,     0,    77,    78,    79,    80,
      81,    82,    83,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,   254,   255,
       0,     0,     0,     0,     0,    88,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    89,    90,    91,    92,    93,
      94,     0,     0,     0,     0,    95,    96,    97,    76,     0,
      77,    78,    79,    80,    81,    82,    83,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    84,
      85,    86,   256,   257,     0,     0,     0,     0,     0,    88,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
      90,    91,    92,    93,    94,     0,     0,     0,     0,    95,
      96,    97,    76,     0,    77,    78,    79,    80,    81,    82,
      83,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    84,    85,    86,   258,   259,     0,     0,
       0,     0,     0,    88,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    89,    90,    91,    92,    93,    94,     0,
       0,     0,     0,    95,    96,    97,    76,     0,    77,    78,
      79,    80,    81,    82,    83,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    84,    85,    86,
     306,   307,     0,     0,     0,     0,     0,    88,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    89,    90,    91,
      92,    93,    94,     0,     0,     0,     0,    95,    96,    97,
      76,     0,    77,    78,    79,    80,    81,    82,    83,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    84,    85,    86,   309,   310,     0,     0,     0,     0,
       0,    88,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    89,    90,    91,    92,    93,    94,     0,     0,     0,
       0,    95,    96,    97,    76,     0,    77,    78,    79,    80,
      81,    82,    83,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,     0,     0,
       0,     0,     0,    87,     0,    88,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    89,    90,    91,    92,    93,
      94,     0,     0,     0,     0,    95,    96,    97,    76,     0,
      77,    78,    79,    80,    81,    82,    83,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    84,
      85,    86,     0,     0,     0,     0,     0,    98,     0,    88,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
      90,    91,    92,    93,    94,     0,     0,     0,     0,    95,
      96,    97,    76,     0,    77,    78,    79,    80,    81,    82,
      83,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    84,    85,    86,   117,     0,     0,     0,
       0,     0,     0,    88,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    89,    90,    91,    92,    93,    94,     0,
       0,     0,     0,    95,    96,    97,    76,     0,    77,    78,
      79,    80,    81,    82,    83,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    84,    85,    86,
       0,     0,     0,     0,     0,   142,     0,    88,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    89,    90,    91,
      92,    93,    94,     0,     0,     0,     0,    95,    96,    97,
      76,     0,    77,    78,    79,    80,    81,    82,    83,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    84,    85,    86,     0,     0,     0,     0,     0,   143,
       0,    88,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    89,    90,    91,    92,    93,    94,     0,     0,     0,
       0,    95,    96,    97,    76,   194,    77,    78,    79,    80,
      81,    82,    83,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,     0,     0,
       0,     0,     0,     0,     0,    88,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    89,    90,    91,    92,    93,
      94,     0,     0,     0,     0,    95,    96,    97,    76,     0,
      77,    78,    79,    80,    81,    82,    83,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    84,
      85,    86,     0,     0,     0,     0,     0,   234,     0,    88,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
      90,    91,    92,    93,    94,     0,     0,     0,     0,    95,
      96,    97,    76,     0,    77,    78,    79,    80,    81,    82,
      83,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    84,    85,    86,     0,     0,     0,     0,
       0,   266,     0,    88,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    89,    90,    91,    92,    93,    94,     0,
       0,     0,     0,    95,    96,    97,    76,     0,    77,    78,
      79,    80,    81,    82,    83,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    84,    85,    86,
       0,     0,     0,     0,     0,   267,     0,    88,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    89,    90,    91,
      92,    93,    94,     0,     0,     0,     0,    95,    96,    97,
      76,     0,    77,    78,    79,    80,    81,    82,    83,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    84,    85,    86,     0,     0,     0,     0,     0,   268,
       0,    88,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    89,    90,    91,    92,    93,    94,     0,     0,     0,
       0,    95,    96,    97,    76,     0,    77,    78,    79,    80,
      81,    82,    83,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,     0,     0,
       0,     0,     0,   269,     0,    88,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    89,    90,    91,    92,    93,
      94,     0,     0,     0,     0,    95,    96,    97,    76,     0,
      77,    78,    79,    80,    81,    82,    83,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    84,
      85,    86,     0,     0,     0,     0,     0,   270,     0,    88,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
      90,    91,    92,    93,    94,     0,     0,     0,     0,    95,
      96,    97,    76,     0,    77,    78,    79,    80,    81,    82,
      83,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    84,    85,    86,     0,     0,     0,     0,
       0,   271,     0,    88,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    89,    90,    91,    92,    93,    94,     0,
       0,     0,     0,    95,    96,    97,    76,     0,    77,    78,
      79,    80,    81,    82,    83,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    84,    85,    86,
       0,     0,     0,     0,     0,   272,     0,    88,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    89,    90,    91,
      92,    93,    94,     0,     0,     0,     0,    95,    96,    97,
      76,     0,    77,    78,    79,    80,    81,    82,    83,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    84,    85,    86,     0,     0,     0,     0,     0,   273,
       0,    88,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    89,    90,    91,    92,    93,    94,     0,     0,     0,
       0,    95,    96,    97,    76,     0,    77,    78,    79,    80,
      81,    82,    83,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,     0,     0,
       0,     0,     0,   274,     0,    88,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    89,    90,    91,    92,    93,
      94,     0,     0,     0,     0,    95,    96,    97,    76,     0,
      77,    78,    79,    80,    81,    82,    83,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    84,
      85,    86,     0,     0,     0,     0,     0,   275,     0,    88,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
      90,    91,    92,    93,    94,     0,     0,     0,     0,    95,
      96,    97,    76,     0,    77,    78,    79,    80,    81,    82,
      83,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    84,    85,    86,     0,     0,     0,     0,
       0,   285,     0,    88,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    89,    90,    91,    92,    93,    94,     0,
       0,     0,     0,    95,    96,    97,    76,     0,    77,    78,
      79,    80,    81,    82,    83,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    84,    85,    86,
       0,     0,     0,     0,     0,   288,     0,    88,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    89,    90,    91,
      92,    93,    94,     0,     0,     0,     0,    95,    96,    97,
      76,     0,    77,    78,    79,    80,    81,    82,    83,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    84,    85,    86,     0,     0,     0,     0,     0,   304,
       0,    88,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    89,    90,    91,    92,    93,    94,     0,     0,     0,
       0,    95,    96,    97,    76,     0,    77,    78,    79,    80,
      81,    82,    83,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,     0,     0,
       0,     0,     0,   305,     0,    88,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    89,    90,    91,    92,    93,
      94,     0,     0,     0,     0,    95,    96,    97,    76,     0,
      77,    78,    79,    80,    81,    82,    83,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    84,
      85,    86,     0,     0,     0,     0,     0,     0,     0,    88,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
      90,    91,    92,    93,    94,     0,     0,     0,     0,    95,
      96,    97,    76,     0,    77,    78,    79,    80,    81,    82,
      83,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    84,     0,    86,     0,     0,     0,     0,
       0,     0,     0,    88,    76,     0,    77,    78,    79,    80,
      81,    82,    83,    89,    90,    91,    92,    93,    94,     0,
       0,     0,     0,    95,    96,    97,     0,    86,     0,     0,
       0,     0,     0,     0,     0,    88,    76,     0,    77,    78,
      79,    80,    81,    82,    83,    89,    90,    91,    92,    93,
      94,     0,     0,     0,     0,    95,    96,    97,     0,    86,
       0,     0,     0,     0,     0,     0,    76,    88,    77,    78,
      79,    80,    81,    82,    83,     0,     0,    89,    90,    91,
      92,    93,    94,     0,     0,     0,     0,    95,     0,    86,
       0,     0,     0,     0,     0,     0,     0,    88,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    89,    90,    91,
      92,    93,    94
};

static const yytype_int16 yycheck[] =
{
      22,    23,   150,    14,   102,     3,     4,    39,    44,     3,
       4,     3,     4,     3,     4,     3,     4,    13,    22,    41,
      42,    43,     9,     0,    46,    43,    62,    45,     9,    10,
      52,    53,     9,    10,    45,     3,    58,    59,    42,    45,
      36,    69,    37,    41,     3,    63,    52,    41,    44,    41,
      45,    41,     3,    41,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,     3,     4,    89,    90,    91,
      92,    93,    94,    95,    96,    97,   112,   105,    37,    38,
      67,    68,    45,   119,    71,   183,    67,    68,   116,    70,
      67,    68,   114,    70,     3,     4,     5,     6,     7,     8,
      63,    10,    11,     3,    13,    14,   254,    16,   256,     9,
     258,    13,     3,    15,    16,    17,    18,    19,    20,    21,
      37,    38,     3,   151,    22,    41,   224,    36,    45,    45,
      39,    41,    22,   155,    36,    45,   158,   159,   236,     3,
     238,     3,    44,     9,    41,    41,    41,   295,    45,    45,
      45,    36,    54,    55,    56,    57,    58,    59,   306,    22,
      69,   309,    64,    72,    66,    22,    43,    43,   196,    39,
      43,     3,     3,    22,    43,   197,     3,     3,   200,   201,
      13,    39,    15,    16,    17,    18,    19,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,     3,     4,     5,
       6,     7,     8,    36,    10,    11,    43,    13,    36,    39,
      16,    44,    36,   235,    39,   237,    36,    36,     3,    42,
      42,     3,    43,   251,    42,    42,    39,   255,     6,   257,
      36,   259,     6,    39,    43,    41,    52,    -1,    86,   261,
      46,    47,    48,    49,    50,    51,    13,    61,    -1,    -1,
      17,    18,    19,    -1,    60,    61,    62,    -1,    -1,    -1,
      -1,    67,    68,    69,   286,    -1,    72,   289,   296,    36,
      -1,    -1,   294,    -1,    -1,   297,    -1,    44,    -1,   307,
      -1,    -1,   310,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,     3,
       4,     5,     6,     7,     8,    -1,    10,    11,    -1,    13,
      -1,    -1,    16,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    63,
      10,    11,    -1,    13,    -1,    69,    16,    -1,    72,     3,
       4,     5,     6,     7,     8,    -1,    10,    11,    -1,    13,
      -1,    -1,    16,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    36,    37,    -1,    39,    -1,    -1,    -1,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    69,    16,    -1,    72,     3,
       4,     5,     6,     7,     8,    -1,    10,    11,    -1,    13,
      -1,    -1,    16,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    36,    -1,    -1,    39,    -1,    -1,    42,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    69,    16,    -1,    72,     3,
       4,     5,     6,     7,     8,    -1,    10,    11,    -1,    13,
      -1,    -1,    16,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    69,    16,    -1,    72,    -1,
      -1,    -1,    13,    -1,    15,    16,    17,    18,    19,    20,
      21,    -1,    -1,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    36,    46,    47,    48,    49,
      50,    51,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,
      60,    61,    62,    54,    55,    56,    57,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    13,    -1,    15,    16,    17,    18,    19,    20,    21,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    36,    -1,    46,    47,    48,    49,
      50,    51,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      60,    61,    62,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    72,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    -1,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,     8,    36,    10,    11,    39,
      13,    41,    -1,    16,    -1,    -1,    46,    47,    48,    49,
      50,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      60,    61,    62,    36,    -1,    -1,    39,    67,    68,    69,
      -1,    -1,    72,    46,    47,    48,    49,    50,    51,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    60,    61,    62,
      -1,    -1,    -1,    -1,    67,    68,    69,    -1,    13,    72,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,    44,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,
      55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,
      65,    66,    13,    -1,    15,    16,    17,    18,    19,    20,
      21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    34,    35,    36,    37,    38,    -1,    -1,
      -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    53,    54,    55,    56,    57,    58,    59,    -1,
      -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    22,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,
      -1,    -1,    -1,    -1,    -1,    -1,    43,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,
      57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,
      13,    -1,    15,    16,    17,    18,    19,    20,    21,    22,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,    -1,
      43,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    54,    55,    56,    57,    58,    59,    -1,    -1,    -1,
      -1,    64,    65,    66,    13,    -1,    15,    16,    17,    18,
      19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    37,    38,
      -1,    -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,
      59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,
      15,    16,    17,    18,    19,    20,    21,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,
      35,    36,    37,    38,    -1,    -1,    -1,    -1,    -1,    44,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,
      55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,
      65,    66,    13,    -1,    15,    16,    17,    18,    19,    20,
      21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    34,    35,    36,    37,    38,    -1,    -1,
      -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    54,    55,    56,    57,    58,    59,    -1,
      -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,
      37,    38,    -1,    -1,    -1,    -1,    -1,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,
      57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,
      13,    -1,    15,    16,    17,    18,    19,    20,    21,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,
      -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    54,    55,    56,    57,    58,    59,    -1,    -1,    -1,
      -1,    64,    65,    66,    13,    -1,    15,    16,    17,    18,
      19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,
      -1,    -1,    -1,    42,    -1,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,
      59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,
      15,    16,    17,    18,    19,    20,    21,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,    44,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,
      55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,
      65,    66,    13,    -1,    15,    16,    17,    18,    19,    20,
      21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    34,    35,    36,    37,    -1,    -1,    -1,
      -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    54,    55,    56,    57,    58,    59,    -1,
      -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,
      -1,    -1,    -1,    -1,    -1,    42,    -1,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,
      57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,
      13,    -1,    15,    16,    17,    18,    19,    20,    21,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,
      -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    54,    55,    56,    57,    58,    59,    -1,    -1,    -1,
      -1,    64,    65,    66,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,
      59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,
      15,    16,    17,    18,    19,    20,    21,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,    44,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,
      55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,
      65,    66,    13,    -1,    15,    16,    17,    18,    19,    20,
      21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    34,    35,    36,    -1,    -1,    -1,    -1,
      -1,    42,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    54,    55,    56,    57,    58,    59,    -1,
      -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,
      -1,    -1,    -1,    -1,    -1,    42,    -1,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,
      57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,
      13,    -1,    15,    16,    17,    18,    19,    20,    21,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,
      -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    54,    55,    56,    57,    58,    59,    -1,    -1,    -1,
      -1,    64,    65,    66,    13,    -1,    15,    16,    17,    18,
      19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,
      -1,    -1,    -1,    42,    -1,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,
      59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,
      15,    16,    17,    18,    19,    20,    21,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,    44,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,
      55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,
      65,    66,    13,    -1,    15,    16,    17,    18,    19,    20,
      21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    34,    35,    36,    -1,    -1,    -1,    -1,
      -1,    42,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    54,    55,    56,    57,    58,    59,    -1,
      -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,
      -1,    -1,    -1,    -1,    -1,    42,    -1,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,
      57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,
      13,    -1,    15,    16,    17,    18,    19,    20,    21,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,
      -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    54,    55,    56,    57,    58,    59,    -1,    -1,    -1,
      -1,    64,    65,    66,    13,    -1,    15,    16,    17,    18,
      19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,
      -1,    -1,    -1,    42,    -1,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,
      59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,
      15,    16,    17,    18,    19,    20,    21,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    42,    -1,    44,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,
      55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,
      65,    66,    13,    -1,    15,    16,    17,    18,    19,    20,
      21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    34,    35,    36,    -1,    -1,    -1,    -1,
      -1,    42,    -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    54,    55,    56,    57,    58,    59,    -1,
      -1,    -1,    -1,    64,    65,    66,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    36,
      -1,    -1,    -1,    -1,    -1,    42,    -1,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,
      57,    58,    59,    -1,    -1,    -1,    -1,    64,    65,    66,
      13,    -1,    15,    16,    17,    18,    19,    20,    21,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,    42,
      -1,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    54,    55,    56,    57,    58,    59,    -1,    -1,    -1,
      -1,    64,    65,    66,    13,    -1,    15,    16,    17,    18,
      19,    20,    21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,
      -1,    -1,    -1,    42,    -1,    44,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    54,    55,    56,    57,    58,
      59,    -1,    -1,    -1,    -1,    64,    65,    66,    13,    -1,
      15,    16,    17,    18,    19,    20,    21,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,
      55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,    64,
      65,    66,    13,    -1,    15,    16,    17,    18,    19,    20,
      21,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    34,    -1,    36,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    44,    13,    -1,    15,    16,    17,    18,
      19,    20,    21,    54,    55,    56,    57,    58,    59,    -1,
      -1,    -1,    -1,    64,    65,    66,    -1,    36,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    44,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    54,    55,    56,    57,    58,
      59,    -1,    -1,    -1,    -1,    64,    65,    66,    -1,    36,
      -1,    -1,    -1,    -1,    -1,    -1,    13,    44,    15,    16,
      17,    18,    19,    20,    21,    -1,    -1,    54,    55,    56,
      57,    58,    59,    -1,    -1,    -1,    -1,    64,    -1,    36,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    54,    55,    56,
      57,    58,    59
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     9,    10,    67,    68,    70,    76,    77,    78,    79,
       3,     3,     3,     9,    67,    68,    71,     0,    77,    79,
       3,    39,    22,    22,     3,     3,     9,    36,     3,     4,
      41,   109,   110,     3,     4,     5,     6,     7,     8,    10,
      11,    13,    16,    36,    39,    69,    72,   105,   106,   107,
     111,   105,    22,    22,     3,    37,    38,    80,    43,    43,
      41,    45,    39,     3,    14,   105,   108,   105,     3,    63,
      80,   105,    41,   109,     3,   105,    13,    15,    16,    17,
      18,    19,    20,    21,    34,    35,    36,    42,    44,    54,
      55,    56,    57,    58,    59,    64,    65,    66,    42,   105,
     105,    43,    43,    82,    37,    38,    45,   105,   105,   110,
      41,   109,    39,    14,    45,    82,    63,    37,    41,    39,
     105,   105,   105,   105,   105,   105,   105,   105,   105,   105,
      37,   108,     3,   105,   105,   105,   105,   105,   105,   105,
     105,   105,    42,    42,     3,     9,    81,    81,    41,    46,
      47,    48,    49,    50,    51,    60,    61,    62,    67,    68,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,    43,    82,     3,    41,    41,   109,   105,
      41,    82,    41,   109,    14,    37,    39,    36,    83,    82,
      36,    36,    36,    42,   105,    42,    42,   105,   105,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    42,    81,    41,    43,    41,    41,    41,    82,   105,
      41,   105,   105,     3,    42,    22,    43,    22,    43,   105,
     105,   105,   105,   105,   105,   105,   105,   105,   105,    42,
      42,    39,    81,    41,    37,    38,    37,    38,    37,    38,
      45,    52,   105,    81,   105,    81,    42,    42,    42,    42,
      42,    42,    42,    42,    42,    42,    82,    83,    82,    83,
      82,    83,    82,     3,   105,    42,    22,    42,    42,    22,
      41,    41,    41,    41,    52,    37,    38,    53,   105,   105,
     105,    83,    82,   105,    42,    42,    37,    38,    41,    37,
      38,    83,    82,    83,    82,    41,    41
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    75,    76,    76,    76,    76,    77,    77,    77,    77,
      77,    77,    78,    78,    78,    79,    79,    79,    79,    80,
      80,    80,    80,    81,    81,    82,    82,    83,    83,    83,
      83,    83,    83,    83,    83,    83,    83,    83,    83,    83,
      83,    83,    83,    83,    83,    83,    83,    83,    84,    85,
      85,    85,    85,    85,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    97,    98,    98,
      99,    99,   100,   100,   101,   101,   101,   101,   101,   101,
     102,   102,   103,   104,   105,   105,   105,   105,   105,   105,
     105,   105,   105,   105,   105,   105,   105,   105,   105,   105,
     105,   105,   105,   105,   105,   105,   105,   105,   105,   105,
     105,   105,   105,   105,   105,   105,   105,   105,   106,   106,
     107,   107,   108,   108,   109,   109,   110,   110,   111,   111,
     111,   111,   111,   111,   111,   111
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     1,     2,     2,     5,     5,     6,     6,
       5,     4,     1,     2,     3,     7,     6,    10,     9,     1,
       3,     3,     5,     1,     1,     0,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     4,
       5,     5,     7,     5,     7,     4,     4,     4,     4,     4,
       4,     4,     4,     4,     3,     3,     6,     5,     6,     5,
       3,     2,     6,     5,    10,     9,     8,     7,    10,     9,
       3,     2,     2,     2,     1,     3,     4,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     2,     2,     3,     1,     3,
       3,     4,     3,     1,     5,     5,     4,     4,     4,     3,
       5,     4,     1,     3,     1,     3,     3,     3,     1,     1,
       1,     1,     1,     1,     2,     2
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
#line 227 "src/core/parser.y"
{
	ast_yylloc.last_line = yylloc.first_line = 0;
	ast_yylloc.last_column = yylloc.first_column = 0;
}

#line 1960 "src/core/parser.tab.c"

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
#line 234 "src/core/parser.y"
                {
			(yyval.func_list) = ast_accept_func_list(NULL, (yyvsp[0].func));
			debug("func_list: class");
		}
#line 2176 "src/core/parser.tab.c"
    break;

  case 3: /* func_list: toplevel_decl  */
#line 239 "src/core/parser.y"
                {
			(yyval.func_list) = NULL;
			debug("func_list: toplevel_decl");
		}
#line 2185 "src/core/parser.tab.c"
    break;

  case 4: /* func_list: func_list func  */
#line 244 "src/core/parser.y"
                {
			(yyval.func_list) = ast_accept_func_list((yyvsp[-1].func_list), (yyvsp[0].func));
			debug("func_list: func_list func");
		}
#line 2194 "src/core/parser.tab.c"
    break;

  case 5: /* func_list: func_list toplevel_decl  */
#line 249 "src/core/parser.y"
                {
			(yyval.func_list) = (yyvsp[-1].func_list);
			debug("func_list: func_list toplevel_decl");
		}
#line 2203 "src/core/parser.tab.c"
    break;

  case 6: /* toplevel_decl: TOKEN_VAR TOKEN_SYMBOL TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 255 "src/core/parser.y"
                {
			if (!ast_accept_toplevel_var((yylsp[-4]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].expr), false, false))
				YYABORT;
			debug("toplevel_decl: var");
		}
#line 2213 "src/core/parser.tab.c"
    break;

  case 7: /* toplevel_decl: TOKEN_LET TOKEN_SYMBOL TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 261 "src/core/parser.y"
                {
			if (!ast_accept_toplevel_var((yylsp[-4]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].expr), true, false))
				YYABORT;
			debug("toplevel_decl: let");
		}
#line 2223 "src/core/parser.tab.c"
    break;

  case 8: /* toplevel_decl: TOKEN_STATIC TOKEN_VAR TOKEN_SYMBOL TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 267 "src/core/parser.y"
                {
			if (!ast_accept_toplevel_var((yylsp[-5]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].expr), false, true))
				YYABORT;
			debug("toplevel_decl: static var");
		}
#line 2233 "src/core/parser.tab.c"
    break;

  case 9: /* toplevel_decl: TOKEN_STATIC TOKEN_LET TOKEN_SYMBOL TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 273 "src/core/parser.y"
                {
			if (!ast_accept_toplevel_var((yylsp[-5]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].expr), true, true))
				YYABORT;
			debug("toplevel_decl: static let");
		}
#line 2243 "src/core/parser.tab.c"
    break;

  case 10: /* toplevel_decl: TOKEN_CLASS TOKEN_SYMBOL TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 279 "src/core/parser.y"
                {
			if (!ast_accept_toplevel_class((yylsp[-4]).first_line + 1, (yyvsp[-3].sval), (yyvsp[-1].kv_list)))
				YYABORT;
			debug("toplevel_decl: class");
		}
#line 2253 "src/core/parser.tab.c"
    break;

  case 11: /* toplevel_decl: TOKEN_CLASS TOKEN_SYMBOL TOKEN_LBLK TOKEN_RBLK  */
#line 285 "src/core/parser.y"
                {
			if (!ast_accept_toplevel_class((yylsp[-3]).first_line + 1, (yyvsp[-2].sval), NULL))
				YYABORT;
			debug("toplevel_decl: class empty");
		}
#line 2263 "src/core/parser.tab.c"
    break;

  case 12: /* func_prefix: TOKEN_FUNC  */
#line 292 "src/core/parser.y"
                {
			(yyval.ival) = 0;
		}
#line 2271 "src/core/parser.tab.c"
    break;

  case 13: /* func_prefix: TOKEN_STATIC TOKEN_FUNC  */
#line 296 "src/core/parser.y"
                {
			(yyval.ival) = 1;
		}
#line 2279 "src/core/parser.tab.c"
    break;

  case 14: /* func_prefix: TOKEN_STATIC TOKEN_INLINE TOKEN_FUNC  */
#line 300 "src/core/parser.y"
                {
			(yyval.ival) = 3;
		}
#line 2287 "src/core/parser.tab.c"
    break;

  case 15: /* func: func_prefix TOKEN_SYMBOL TOKEN_LPAR param_list TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 305 "src/core/parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-6].ival), (yyvsp[-5].sval), (yyvsp[-3].param_list), NULL, (yyvsp[-1].stmt_list));
			if ((yyval.func) == NULL) YYABORT;
			debug("func: func name(param_list) { stmt_list }");
		}
#line 2297 "src/core/parser.tab.c"
    break;

  case 16: /* func: func_prefix TOKEN_SYMBOL TOKEN_LPAR TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 311 "src/core/parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-5].ival), (yyvsp[-4].sval), NULL, NULL, (yyvsp[-1].stmt_list));
			if ((yyval.func) == NULL) YYABORT;
			debug("func: func name() { stmt_list }");
		}
#line 2307 "src/core/parser.tab.c"
    break;

  case 17: /* func: func_prefix TOKEN_SYMBOL TOKEN_LPAR param_list TOKEN_RPAR TOKEN_COLON type_name TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 317 "src/core/parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-9].ival), (yyvsp[-8].sval), (yyvsp[-6].param_list), (yyvsp[-3].sval), (yyvsp[-1].stmt_list));
			if ((yyval.func) == NULL) YYABORT;
			debug("func: func name(param_list): type { stmt_list }");
		}
#line 2317 "src/core/parser.tab.c"
    break;

  case 18: /* func: func_prefix TOKEN_SYMBOL TOKEN_LPAR TOKEN_RPAR TOKEN_COLON type_name TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 323 "src/core/parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-8].ival), (yyvsp[-7].sval), NULL, (yyvsp[-3].sval), (yyvsp[-1].stmt_list));
			if ((yyval.func) == NULL) YYABORT;
			debug("func: func name(): type { stmt_list }");
		}
#line 2327 "src/core/parser.tab.c"
    break;

  case 19: /* param_list: TOKEN_SYMBOL  */
#line 330 "src/core/parser.y"
                {
			(yyval.param_list) = ast_accept_param_list(NULL, (yyvsp[0].sval));
			debug("param_list: symbol");
		}
#line 2336 "src/core/parser.tab.c"
    break;

  case 20: /* param_list: TOKEN_SYMBOL TOKEN_COLON type_name  */
#line 335 "src/core/parser.y"
                {
			(yyval.param_list) = ast_accept_param_list_typed(NULL, (yyvsp[-2].sval), (yyvsp[0].sval));
			debug("param_list: symbol: type");
		}
#line 2345 "src/core/parser.tab.c"
    break;

  case 21: /* param_list: param_list TOKEN_COMMA TOKEN_SYMBOL  */
#line 340 "src/core/parser.y"
                {
			(yyval.param_list) = ast_accept_param_list((yyvsp[-2].param_list), (yyvsp[0].sval));
			debug("param_list: param_list symbol");
		}
#line 2354 "src/core/parser.tab.c"
    break;

  case 22: /* param_list: param_list TOKEN_COMMA TOKEN_SYMBOL TOKEN_COLON type_name  */
#line 345 "src/core/parser.y"
                {
			(yyval.param_list) = ast_accept_param_list_typed((yyvsp[-4].param_list), (yyvsp[-2].sval), (yyvsp[0].sval));
			debug("param_list: param_list symbol: type");
		}
#line 2363 "src/core/parser.tab.c"
    break;

  case 23: /* type_name: TOKEN_SYMBOL  */
#line 351 "src/core/parser.y"
                {
			(yyval.sval) = (yyvsp[0].sval);
		}
#line 2371 "src/core/parser.tab.c"
    break;

  case 24: /* type_name: TOKEN_FUNC  */
#line 355 "src/core/parser.y"
                {
			(yyval.sval) = ast_strdup("func");
		}
#line 2379 "src/core/parser.tab.c"
    break;

  case 25: /* stmt_list: %empty  */
#line 360 "src/core/parser.y"
                {
			(yyval.stmt_list) = ast_accept_stmt_list(NULL, NULL);
			debug("stmt_list: empty");
		}
#line 2388 "src/core/parser.tab.c"
    break;

  case 26: /* stmt_list: stmt_list stmt  */
#line 365 "src/core/parser.y"
                {
			(yyval.stmt_list) = ast_accept_stmt_list((yyvsp[-1].stmt_list), (yyvsp[0].stmt));
			debug("stmt_list: stmt_list stmt");
		}
#line 2397 "src/core/parser.tab.c"
    break;

  case 27: /* stmt: expr_stmt  */
#line 371 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2405 "src/core/parser.tab.c"
    break;

  case 28: /* stmt: assign_stmt  */
#line 375 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2413 "src/core/parser.tab.c"
    break;

  case 29: /* stmt: plusassign_stmt  */
#line 379 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2421 "src/core/parser.tab.c"
    break;

  case 30: /* stmt: minusassign_stmt  */
#line 383 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2429 "src/core/parser.tab.c"
    break;

  case 31: /* stmt: mulassign_stmt  */
#line 387 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2437 "src/core/parser.tab.c"
    break;

  case 32: /* stmt: divassign_stmt  */
#line 391 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2445 "src/core/parser.tab.c"
    break;

  case 33: /* stmt: modassign_stmt  */
#line 395 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2453 "src/core/parser.tab.c"
    break;

  case 34: /* stmt: andassign_stmt  */
#line 399 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2461 "src/core/parser.tab.c"
    break;

  case 35: /* stmt: orassign_stmt  */
#line 403 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2469 "src/core/parser.tab.c"
    break;

  case 36: /* stmt: shlassign_stmt  */
#line 407 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2477 "src/core/parser.tab.c"
    break;

  case 37: /* stmt: shrassign_stmt  */
#line 411 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2485 "src/core/parser.tab.c"
    break;

  case 38: /* stmt: plusplus_stmt  */
#line 415 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2493 "src/core/parser.tab.c"
    break;

  case 39: /* stmt: minusminus_stmt  */
#line 419 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2501 "src/core/parser.tab.c"
    break;

  case 40: /* stmt: if_stmt  */
#line 423 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2509 "src/core/parser.tab.c"
    break;

  case 41: /* stmt: elif_stmt  */
#line 427 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2517 "src/core/parser.tab.c"
    break;

  case 42: /* stmt: else_stmt  */
#line 431 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2525 "src/core/parser.tab.c"
    break;

  case 43: /* stmt: while_stmt  */
#line 435 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2533 "src/core/parser.tab.c"
    break;

  case 44: /* stmt: for_stmt  */
#line 439 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2541 "src/core/parser.tab.c"
    break;

  case 45: /* stmt: return_stmt  */
#line 443 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2549 "src/core/parser.tab.c"
    break;

  case 46: /* stmt: break_stmt  */
#line 447 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2557 "src/core/parser.tab.c"
    break;

  case 47: /* stmt: continue_stmt  */
#line 451 "src/core/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 2565 "src/core/parser.tab.c"
    break;

  case 48: /* expr_stmt: expr TOKEN_SEMICOLON  */
#line 456 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_expr_stmt((yylsp[-1]).first_line + 1, (yyvsp[-1].expr));
			debug("expr_stmt");
		}
#line 2574 "src/core/parser.tab.c"
    break;

  case 49: /* assign_stmt: expr TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 462 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr), false, false);
			debug("assign_stmt");
		}
#line 2583 "src/core/parser.tab.c"
    break;

  case 50: /* assign_stmt: TOKEN_VAR expr TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 467 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt((yylsp[-4]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr), true, false);
			debug("var assign_stmt");
		}
#line 2592 "src/core/parser.tab.c"
    break;

  case 51: /* assign_stmt: TOKEN_LET expr TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 472 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt((yylsp[-4]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr), false, true);
			debug("let assign_stmt");
		}
#line 2601 "src/core/parser.tab.c"
    break;

  case 52: /* assign_stmt: TOKEN_VAR expr TOKEN_COLON type_name TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 477 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt_typed((yylsp[-6]).first_line + 1, (yyvsp[-5].expr), (yyvsp[-1].expr), true, false, (yyvsp[-3].sval));
			debug("var typed assign_stmt");
		}
#line 2610 "src/core/parser.tab.c"
    break;

  case 53: /* assign_stmt: TOKEN_VAR expr TOKEN_COLON type_name TOKEN_SEMICOLON  */
#line 482 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt_typed((yylsp[-4]).first_line + 1, (yyvsp[-3].expr), ast_accept_term_expr(ast_accept_int_term(0)), true, false, (yyvsp[-1].sval));
			debug("var typed decl_stmt");
		}
#line 2619 "src/core/parser.tab.c"
    break;

  case 54: /* assign_stmt: TOKEN_LET expr TOKEN_COLON type_name TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 487 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt_typed((yylsp[-6]).first_line + 1, (yyvsp[-5].expr), (yyvsp[-1].expr), false, true, (yyvsp[-3].sval));
			debug("let typed assign_stmt");
		}
#line 2628 "src/core/parser.tab.c"
    break;

  case 55: /* plusassign_stmt: expr TOKEN_PLUSASSIGN expr TOKEN_SEMICOLON  */
#line 493 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_plusassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("plusassign_stmt");
		}
#line 2637 "src/core/parser.tab.c"
    break;

  case 56: /* minusassign_stmt: expr TOKEN_MINUSASSIGN expr TOKEN_SEMICOLON  */
#line 499 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_minusassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("minusassign_stmt");
		}
#line 2646 "src/core/parser.tab.c"
    break;

  case 57: /* mulassign_stmt: expr TOKEN_MULASSIGN expr TOKEN_SEMICOLON  */
#line 505 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_mulassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("mulassign_stmt");
		}
#line 2655 "src/core/parser.tab.c"
    break;

  case 58: /* divassign_stmt: expr TOKEN_DIVASSIGN expr TOKEN_SEMICOLON  */
#line 511 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_divassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("divassign_stmt");
		}
#line 2664 "src/core/parser.tab.c"
    break;

  case 59: /* modassign_stmt: expr TOKEN_MODASSIGN expr TOKEN_SEMICOLON  */
#line 517 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_modassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("modassign_stmt");
		}
#line 2673 "src/core/parser.tab.c"
    break;

  case 60: /* andassign_stmt: expr TOKEN_ANDASSIGN expr TOKEN_SEMICOLON  */
#line 523 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_andassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("andassign_stmt");
		}
#line 2682 "src/core/parser.tab.c"
    break;

  case 61: /* orassign_stmt: expr TOKEN_ORASSIGN expr TOKEN_SEMICOLON  */
#line 529 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_orassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("orassign_stmt");
		}
#line 2691 "src/core/parser.tab.c"
    break;

  case 62: /* shlassign_stmt: expr TOKEN_SHLASSIGN expr TOKEN_SEMICOLON  */
#line 535 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_shlassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("shlassign_stmt");
		}
#line 2700 "src/core/parser.tab.c"
    break;

  case 63: /* shrassign_stmt: expr TOKEN_SHRASSIGN expr TOKEN_SEMICOLON  */
#line 541 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_shrassign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("shrassign_stmt");
		}
#line 2709 "src/core/parser.tab.c"
    break;

  case 64: /* plusplus_stmt: expr TOKEN_PLUSPLUS TOKEN_SEMICOLON  */
#line 547 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_plusplus_stmt((yylsp[-2]).first_line + 1, (yyvsp[-2].expr));
			debug("plusplus_stmt");
		}
#line 2718 "src/core/parser.tab.c"
    break;

  case 65: /* minusminus_stmt: expr TOKEN_MINUSMINUS TOKEN_SEMICOLON  */
#line 553 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_minusminus_stmt((yylsp[-2]).first_line + 1, (yyvsp[-2].expr));
			debug("plusplus_stmt");
		}
#line 2727 "src/core/parser.tab.c"
    break;

  case 66: /* if_stmt: TOKEN_IF TOKEN_LPAR expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 559 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_if_stmt((yylsp[-5]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("if_stmt: stmt_list");
		}
#line 2736 "src/core/parser.tab.c"
    break;

  case 67: /* if_stmt: TOKEN_IF TOKEN_LPAR expr TOKEN_RPAR stmt  */
#line 564 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_if_stmt_single((yylsp[-4]).first_line + 1, (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("if_stmt: stmt_list");
		}
#line 2745 "src/core/parser.tab.c"
    break;

  case 68: /* elif_stmt: TOKEN_ELSEIF TOKEN_LPAR expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 570 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_elif_stmt((yylsp[-5]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("elif_stmt: stmt_list");
		}
#line 2754 "src/core/parser.tab.c"
    break;

  case 69: /* elif_stmt: TOKEN_ELSEIF TOKEN_LPAR expr TOKEN_RPAR stmt  */
#line 575 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_elif_stmt_single((yylsp[-4]).first_line + 1, (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("elif_stmt: stmt_list");
		}
#line 2763 "src/core/parser.tab.c"
    break;

  case 70: /* else_stmt: TOKEN_ELSE_LBLK stmt_list TOKEN_RBLK  */
#line 581 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_else_stmt((yylsp[-2]).first_line + 1, (yyvsp[-1].stmt_list));
			debug("else_stmt: stmt_list");
		}
#line 2772 "src/core/parser.tab.c"
    break;

  case 71: /* else_stmt: TOKEN_ELSE stmt  */
#line 586 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_else_stmt_single((yylsp[-1]).first_line + 1, (yyvsp[0].stmt));
			debug("else_stmt: stmt_list");
		}
#line 2781 "src/core/parser.tab.c"
    break;

  case 72: /* while_stmt: TOKEN_WHILE TOKEN_LPAR expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 592 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_while_stmt((yylsp[-5]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("while_stmt: stmt_list");
		}
#line 2790 "src/core/parser.tab.c"
    break;

  case 73: /* while_stmt: TOKEN_WHILE TOKEN_LPAR expr TOKEN_RPAR stmt  */
#line 597 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_while_stmt_single((yylsp[-4]).first_line + 1, (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("while_stmt: stmt_list");
		}
#line 2799 "src/core/parser.tab.c"
    break;

  case 74: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_COMMA TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 603 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_for_kv_stmt((yylsp[-9]).first_line + 1, (yyvsp[-7].sval), (yyvsp[-5].sval), (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("for_stmt: for(k, v in array) { stmt_list }");
		}
#line 2808 "src/core/parser.tab.c"
    break;

  case 75: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_COMMA TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR stmt  */
#line 608 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_for_kv_stmt_single((yylsp[-8]).first_line + 1, (yyvsp[-6].sval), (yyvsp[-4].sval), (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("for_stmt: for(k, v in array) stmt");
		}
#line 2817 "src/core/parser.tab.c"
    break;

  case 76: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 613 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_for_v_stmt((yylsp[-7]).first_line + 1, (yyvsp[-5].sval), (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("for_stmt: for(v in array) { stmt_list }");
		}
#line 2826 "src/core/parser.tab.c"
    break;

  case 77: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR stmt  */
#line 618 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_for_v_stmt_single((yylsp[-6]).first_line + 1, (yyvsp[-4].sval), (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("for_stmt: for(v in array) stmt_list");
		}
#line 2835 "src/core/parser.tab.c"
    break;

  case 78: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_DOTDOT expr TOKEN_RPAR_LBLK stmt_list TOKEN_RBLK  */
#line 623 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_for_range_stmt((yylsp[-9]).first_line + 1, (yyvsp[-7].sval), (yyvsp[-5].expr), (yyvsp[-3].expr), (yyvsp[-1].stmt_list));
			debug("for_stmt: for(i in x..y) { stmt_list }");
		}
#line 2844 "src/core/parser.tab.c"
    break;

  case 79: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_DOTDOT expr TOKEN_RPAR stmt  */
#line 628 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_for_range_stmt_single((yylsp[-8]).first_line + 1, (yyvsp[-6].sval), (yyvsp[-4].expr), (yyvsp[-2].expr), (yyvsp[0].stmt));
			debug("for_stmt: for(i in x..y) stmt");
		}
#line 2853 "src/core/parser.tab.c"
    break;

  case 80: /* return_stmt: TOKEN_RETURN expr TOKEN_SEMICOLON  */
#line 634 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_return_stmt((yylsp[-2]).first_line + 1, (yyvsp[-1].expr));
			debug("rerurn_stmt:");
		}
#line 2862 "src/core/parser.tab.c"
    break;

  case 81: /* return_stmt: TOKEN_RETURN TOKEN_SEMICOLON  */
#line 639 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_return_stmt((yylsp[-1]).first_line + 1, NULL);
			debug("rerurn_stmt NULL:");
		}
#line 2871 "src/core/parser.tab.c"
    break;

  case 82: /* break_stmt: TOKEN_BREAK TOKEN_SEMICOLON  */
#line 644 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_break_stmt((yylsp[-1]).first_line + 1);
			debug("break_stmt:");
		}
#line 2880 "src/core/parser.tab.c"
    break;

  case 83: /* continue_stmt: TOKEN_CONTINUE TOKEN_SEMICOLON  */
#line 650 "src/core/parser.y"
                {
			(yyval.stmt) = ast_accept_continue_stmt((yylsp[-1]).first_line + 1);
			debug("continue_stmt");
		}
#line 2889 "src/core/parser.tab.c"
    break;

  case 84: /* expr: term  */
#line 656 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_term_expr((yyvsp[0].term));
			debug("expr: term");
		}
#line 2898 "src/core/parser.tab.c"
    break;

  case 85: /* expr: TOKEN_LPAR expr TOKEN_RPAR  */
#line 661 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_par_expr((yyvsp[-1].expr));
			debug("expr: (expr)");
		}
#line 2907 "src/core/parser.tab.c"
    break;

  case 86: /* expr: expr TOKEN_LARR expr TOKEN_RARR  */
#line 666 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_subscr_expr((yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("expr: array[subscript]");
		}
#line 2916 "src/core/parser.tab.c"
    break;

  case 87: /* expr: expr TOKEN_OR expr  */
#line 671 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_or_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr or expr");
		}
#line 2925 "src/core/parser.tab.c"
    break;

  case 88: /* expr: expr TOKEN_AND expr  */
#line 676 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_and_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr and expr");
		}
#line 2934 "src/core/parser.tab.c"
    break;

  case 89: /* expr: expr TOKEN_XOR expr  */
#line 681 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_xor_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr xor expr");
		}
#line 2943 "src/core/parser.tab.c"
    break;

  case 90: /* expr: expr TOKEN_OROR expr  */
#line 686 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_lor_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr || expr");
		}
#line 2952 "src/core/parser.tab.c"
    break;

  case 91: /* expr: expr TOKEN_ANDAND expr  */
#line 691 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_land_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr && expr");
		}
#line 2961 "src/core/parser.tab.c"
    break;

  case 92: /* expr: expr TOKEN_LT expr  */
#line 696 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_lt_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr lt expr");
		}
#line 2970 "src/core/parser.tab.c"
    break;

  case 93: /* expr: expr TOKEN_LTE expr  */
#line 701 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_lte_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr lte expr");
		}
#line 2979 "src/core/parser.tab.c"
    break;

  case 94: /* expr: expr TOKEN_GT expr  */
#line 706 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_gt_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr gt expr");
		}
#line 2988 "src/core/parser.tab.c"
    break;

  case 95: /* expr: expr TOKEN_GTE expr  */
#line 711 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_gte_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr gte expr");
		}
#line 2997 "src/core/parser.tab.c"
    break;

  case 96: /* expr: expr TOKEN_EQ expr  */
#line 716 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_eq_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr eq expr");
		}
#line 3006 "src/core/parser.tab.c"
    break;

  case 97: /* expr: expr TOKEN_NEQ expr  */
#line 721 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_neq_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr neq expr");
		}
#line 3015 "src/core/parser.tab.c"
    break;

  case 98: /* expr: expr TOKEN_PLUS expr  */
#line 726 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_plus_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr plus expr");
		}
#line 3024 "src/core/parser.tab.c"
    break;

  case 99: /* expr: expr TOKEN_MINUS expr  */
#line 731 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_minus_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr sub expr");
		}
#line 3033 "src/core/parser.tab.c"
    break;

  case 100: /* expr: expr TOKEN_MUL expr  */
#line 736 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_mul_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr mul expr");
		}
#line 3042 "src/core/parser.tab.c"
    break;

  case 101: /* expr: expr TOKEN_DIV expr  */
#line 741 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_div_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr div expr");
		}
#line 3051 "src/core/parser.tab.c"
    break;

  case 102: /* expr: expr TOKEN_MOD expr  */
#line 746 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_mod_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr div expr");
		}
#line 3060 "src/core/parser.tab.c"
    break;

  case 103: /* expr: expr TOKEN_SHL expr  */
#line 751 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_shl_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr shl expr");
		}
#line 3069 "src/core/parser.tab.c"
    break;

  case 104: /* expr: expr TOKEN_SHR expr  */
#line 756 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_shr_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr shr expr");
		}
#line 3078 "src/core/parser.tab.c"
    break;

  case 105: /* expr: TOKEN_MINUS expr  */
#line 761 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_neg_expr((yyvsp[0].expr));
			debug("expr: neg expr");
		}
#line 3087 "src/core/parser.tab.c"
    break;

  case 106: /* expr: TOKEN_NOT expr  */
#line 766 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_not_expr((yyvsp[0].expr));
			debug("expr: not expr");
		}
#line 3096 "src/core/parser.tab.c"
    break;

  case 107: /* expr: expr TOKEN_DOT TOKEN_SYMBOL  */
#line 771 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_dot_expr((yyvsp[-2].expr), (yyvsp[0].sval));
			debug("expr: expr.symbol");
		}
#line 3105 "src/core/parser.tab.c"
    break;

  case 108: /* expr: call_expr  */
#line 776 "src/core/parser.y"
                {
			(yyval.expr) = (yyvsp[0].expr);
		}
#line 3113 "src/core/parser.tab.c"
    break;

  case 109: /* expr: TOKEN_LARR arg_list TOKEN_RARR  */
#line 780 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_array_expr((yyvsp[-1].arg_list));
			debug("expr: array");
		}
#line 3122 "src/core/parser.tab.c"
    break;

  case 110: /* expr: TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 785 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_dict_expr((yyvsp[-1].kv_list));
			debug("expr: dict");
		}
#line 3131 "src/core/parser.tab.c"
    break;

  case 111: /* expr: TOKEN_CLASS TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 790 "src/core/parser.y"
                {
			/* class is a frozen dict. */
			(yyval.expr) = ast_accept_class_expr((yyvsp[-1].kv_list));
			debug("expr: class");
		}
#line 3141 "src/core/parser.tab.c"
    break;

  case 112: /* expr: TOKEN_CLASS TOKEN_LBLK TOKEN_RBLK  */
#line 796 "src/core/parser.y"
                {
			/* class is a frozen dict. */
			(yyval.expr) = ast_accept_class_expr(NULL);
			debug("expr: class");
		}
#line 3151 "src/core/parser.tab.c"
    break;

  case 113: /* expr: lambda_expr  */
#line 802 "src/core/parser.y"
                {
			(yyval.expr) = (yyvsp[0].expr);
		}
#line 3159 "src/core/parser.tab.c"
    break;

  case 114: /* expr: TOKEN_NEW TOKEN_SYMBOL TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 806 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_new_expr((yyvsp[-3].sval), (yyvsp[-1].kv_list));
			debug("expr: new");
		}
#line 3168 "src/core/parser.tab.c"
    break;

  case 115: /* expr: TOKEN_EXTEND TOKEN_SYMBOL TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 811 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_extend_expr((yyvsp[-3].sval), (yyvsp[-1].kv_list));
			debug("expr: extend");
		}
#line 3177 "src/core/parser.tab.c"
    break;

  case 116: /* expr: TOKEN_EXTEND TOKEN_SYMBOL TOKEN_LBLK TOKEN_RBLK  */
#line 816 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_extend_expr((yyvsp[-2].sval), NULL);
			debug("expr: extend");
		}
#line 3186 "src/core/parser.tab.c"
    break;

  case 117: /* expr: TOKEN_NEW TOKEN_SYMBOL TOKEN_LBLK TOKEN_RBLK  */
#line 821 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_new_expr((yyvsp[-2].sval), NULL);
			debug("expr: new");
		}
#line 3195 "src/core/parser.tab.c"
    break;

  case 118: /* call_expr: expr TOKEN_LPAR arg_list TOKEN_RPAR  */
#line 827 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_call_expr((yyvsp[-3].expr), (yyvsp[-1].arg_list));
			debug("expr: call(param_list)");
		}
#line 3204 "src/core/parser.tab.c"
    break;

  case 119: /* call_expr: expr TOKEN_LPAR TOKEN_RPAR  */
#line 832 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_call_expr((yyvsp[-2].expr), NULL);
			debug("expr: call()");
		}
#line 3213 "src/core/parser.tab.c"
    break;

  case 120: /* lambda_expr: TOKEN_LPAR param_list TOKEN_RPAR_DARROW_LBLK stmt_list TOKEN_RBLK  */
#line 838 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_func_expr((yyvsp[-3].param_list), (yyvsp[-1].stmt_list));
			debug("expr: func param_list stmt_list");
		}
#line 3222 "src/core/parser.tab.c"
    break;

  case 121: /* lambda_expr: TOKEN_LPAR TOKEN_RPAR_DARROW_LBLK stmt_list TOKEN_RBLK  */
#line 843 "src/core/parser.y"
                {
			(yyval.expr) = ast_accept_func_expr(NULL, (yyvsp[-1].stmt_list));
			debug("expr: func stmt_list");
		}
#line 3231 "src/core/parser.tab.c"
    break;

  case 122: /* arg_list: expr  */
#line 849 "src/core/parser.y"
                {
			(yyval.arg_list) = ast_accept_arg_list(NULL, (yyvsp[0].expr));
			debug("arg_list: expr");
		}
#line 3240 "src/core/parser.tab.c"
    break;

  case 123: /* arg_list: arg_list TOKEN_COMMA expr  */
#line 854 "src/core/parser.y"
                {
			(yyval.arg_list) = ast_accept_arg_list((yyvsp[-2].arg_list), (yyvsp[0].expr));
			debug("arg_list: arg_list arg");
		}
#line 3249 "src/core/parser.tab.c"
    break;

  case 124: /* kv_list: kv  */
#line 860 "src/core/parser.y"
                {
			(yyval.kv_list) = ast_accept_kv_list(NULL, (yyvsp[0].kv));
			debug("kv_list: kv");
		}
#line 3258 "src/core/parser.tab.c"
    break;

  case 125: /* kv_list: kv_list TOKEN_COMMA kv  */
#line 865 "src/core/parser.y"
                {
			(yyval.kv_list) = ast_accept_kv_list((yyvsp[-2].kv_list), (yyvsp[0].kv));
			debug("kv_list: kv_list kv");
		}
#line 3267 "src/core/parser.tab.c"
    break;

  case 126: /* kv: TOKEN_STR TOKEN_COLON expr  */
#line 871 "src/core/parser.y"
                {
			(yyval.kv) = ast_accept_kv((yyvsp[-2].sval), (yyvsp[0].expr));
			debug("kv");
		}
#line 3276 "src/core/parser.tab.c"
    break;

  case 127: /* kv: TOKEN_SYMBOL TOKEN_COLON expr  */
#line 876 "src/core/parser.y"
                {
			(yyval.kv) = ast_accept_kv((yyvsp[-2].sval), (yyvsp[0].expr));
			debug("kv");
		}
#line 3285 "src/core/parser.tab.c"
    break;

  case 128: /* term: TOKEN_INT  */
#line 882 "src/core/parser.y"
                {
			(yyval.term) = ast_accept_int_term((yyvsp[0].ival));
			debug("term: int");
		}
#line 3294 "src/core/parser.tab.c"
    break;

  case 129: /* term: TOKEN_LONG  */
#line 887 "src/core/parser.y"
                {
			(yyval.term) = ast_accept_long_term((yyvsp[0].lval));
			debug("term: long");
		}
#line 3303 "src/core/parser.tab.c"
    break;

  case 130: /* term: TOKEN_FLOAT  */
#line 892 "src/core/parser.y"
                {
			(yyval.term) = ast_accept_float_term((yyvsp[0].fval));
			debug("term: float");
		}
#line 3312 "src/core/parser.tab.c"
    break;

  case 131: /* term: TOKEN_DOUBLE  */
#line 897 "src/core/parser.y"
                {
			(yyval.term) = ast_accept_double_term((yyvsp[0].lfval));
			debug("term: double");
		}
#line 3321 "src/core/parser.tab.c"
    break;

  case 132: /* term: TOKEN_STR  */
#line 902 "src/core/parser.y"
                {
			(yyval.term) = ast_accept_str_term((yyvsp[0].sval));
			debug("term: string");
		}
#line 3330 "src/core/parser.tab.c"
    break;

  case 133: /* term: TOKEN_SYMBOL  */
#line 907 "src/core/parser.y"
                {
			(yyval.term) = ast_accept_symbol_term((yyvsp[0].sval));
			debug("term: symbol");
		}
#line 3339 "src/core/parser.tab.c"
    break;

  case 134: /* term: TOKEN_LARR TOKEN_RARR  */
#line 912 "src/core/parser.y"
                {
			(yyval.term) = ast_accept_empty_array_term();
			debug("term: empty array symbol");
		}
#line 3348 "src/core/parser.tab.c"
    break;

  case 135: /* term: TOKEN_LBLK TOKEN_RBLK  */
#line 917 "src/core/parser.y"
                {
			(yyval.term) = ast_accept_empty_dict_term();
			debug("term: empty dict symbol");
		}
#line 3357 "src/core/parser.tab.c"
    break;


#line 3361 "src/core/parser.tab.c"

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

#line 922 "src/core/parser.y"


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
