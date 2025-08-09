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
#line 1 "/Users/tabata/src/NoctLang/src/parser.y"

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
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

extern int ast_error_line;
extern int ast_error_column;

int ast_yylex(void *);
void ast_yyerror(void *, char *s);
void *ast_malloc(size_t size); 

/* Internal: called back from the parser. */
struct ast_func_list *ast_accept_func_list(struct ast_func_list *impl_list, struct ast_func *func);
struct ast_func *ast_accept_func(char *name, struct ast_param_list *param_list, struct ast_stmt_list *stmt_list);
struct ast_param_list *ast_accept_param_list(struct ast_param_list *param_list, char *name);
struct ast_stmt_list *ast_accept_stmt_list(struct ast_stmt_list *stmt_list, struct ast_stmt *stmt);
void ast_accept_stmt(struct ast_stmt *stmt, int line);
struct ast_stmt *ast_accept_expr_stmt(int line, struct ast_expr *expr);
struct ast_stmt *ast_accept_assign_stmt(int line, struct ast_expr *lhs, struct ast_expr *rhs, bool is_var);
struct ast_stmt *ast_accept_if_stmt(int line, struct ast_expr *cond, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_elif_stmt(int line, struct ast_expr *cond, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_else_stmt(int line, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_while_stmt(int line, struct ast_expr *cond, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_for_kv_stmt(int line, char *key_sym, char *val_sym, struct ast_expr *array, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_for_v_stmt(int line, char *iter_sym, struct ast_expr *array, struct ast_stmt_list *stmt_list);
struct ast_stmt *ast_accept_for_range_stmt(int line, char *counter_sym, struct ast_expr *start, struct ast_expr *stop, struct ast_stmt_list *stmt_list);
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
struct ast_expr *ast_accept_or_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_neg_expr(struct ast_expr *expr);
struct ast_expr *ast_accept_not_expr(struct ast_expr *expr);
struct ast_expr *ast_accept_par_expr(struct ast_expr *expr);
struct ast_expr *ast_accept_subscr_expr(struct ast_expr *expr1, struct ast_expr *expr2);
struct ast_expr *ast_accept_dot_expr(struct ast_expr *obj, char *symbol);
struct ast_expr *ast_accept_call_expr(struct ast_expr *func, struct ast_arg_list *arg_list);
struct ast_expr *ast_accept_thiscall_expr(struct ast_expr *obj, char *func, struct ast_arg_list *arg_list);
struct ast_expr *ast_accept_array_expr(struct ast_arg_list *arg_list);
struct ast_expr *ast_accept_dict_expr(struct ast_kv_list *kv_list);
struct ast_expr *ast_accept_func_expr(struct ast_param_list *param_list, struct ast_stmt_list *stmt_list);
struct ast_kv_list *ast_accept_kv_list(struct ast_kv_list *kv_list, struct ast_kv *kv);
struct ast_kv *ast_accept_kv(char *key, struct ast_expr *value);
struct ast_term *ast_accept_int_term(int i);
struct ast_term *ast_accept_float_term(float f);
struct ast_term *ast_accept_str_term(char *s);
struct ast_term *ast_accept_symbol_term(char *symbol);
struct ast_term *ast_accept_empty_array_term(void);
struct ast_term *ast_accept_empty_dict_term(void);
struct ast_arg_list *ast_accept_arg_list(struct ast_arg_list *arg_list, struct ast_expr *expr);

#line 83 "/Users/tabata/src/NoctLang/src/parser.y"

#include "stdio.h"
extern void ast_yyerror(void *scanner, char *s);

#line 165 "/Users/tabata/src/NoctLang/build/parser.tab.c"

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
  YYSYMBOL_TOKEN_FLOAT = 6,                /* TOKEN_FLOAT  */
  YYSYMBOL_TOKEN_FUNC = 7,                 /* TOKEN_FUNC  */
  YYSYMBOL_TOKEN_LAMBDA = 8,               /* TOKEN_LAMBDA  */
  YYSYMBOL_TOKEN_LARR = 9,                 /* TOKEN_LARR  */
  YYSYMBOL_TOKEN_RARR = 10,                /* TOKEN_RARR  */
  YYSYMBOL_TOKEN_PLUS = 11,                /* TOKEN_PLUS  */
  YYSYMBOL_TOKEN_MINUS = 12,               /* TOKEN_MINUS  */
  YYSYMBOL_TOKEN_MUL = 13,                 /* TOKEN_MUL  */
  YYSYMBOL_TOKEN_DIV = 14,                 /* TOKEN_DIV  */
  YYSYMBOL_TOKEN_MOD = 15,                 /* TOKEN_MOD  */
  YYSYMBOL_TOKEN_ASSIGN = 16,              /* TOKEN_ASSIGN  */
  YYSYMBOL_TOKEN_LPAR = 17,                /* TOKEN_LPAR  */
  YYSYMBOL_TOKEN_RPAR = 18,                /* TOKEN_RPAR  */
  YYSYMBOL_TOKEN_LBLK = 19,                /* TOKEN_LBLK  */
  YYSYMBOL_TOKEN_RBLK = 20,                /* TOKEN_RBLK  */
  YYSYMBOL_TOKEN_SEMICOLON = 21,           /* TOKEN_SEMICOLON  */
  YYSYMBOL_TOKEN_COLON = 22,               /* TOKEN_COLON  */
  YYSYMBOL_TOKEN_DOT = 23,                 /* TOKEN_DOT  */
  YYSYMBOL_TOKEN_COMMA = 24,               /* TOKEN_COMMA  */
  YYSYMBOL_TOKEN_IF = 25,                  /* TOKEN_IF  */
  YYSYMBOL_TOKEN_ELSE = 26,                /* TOKEN_ELSE  */
  YYSYMBOL_TOKEN_WHILE = 27,               /* TOKEN_WHILE  */
  YYSYMBOL_TOKEN_FOR = 28,                 /* TOKEN_FOR  */
  YYSYMBOL_TOKEN_IN = 29,                  /* TOKEN_IN  */
  YYSYMBOL_TOKEN_DOTDOT = 30,              /* TOKEN_DOTDOT  */
  YYSYMBOL_TOKEN_GT = 31,                  /* TOKEN_GT  */
  YYSYMBOL_TOKEN_GTE = 32,                 /* TOKEN_GTE  */
  YYSYMBOL_TOKEN_LT = 33,                  /* TOKEN_LT  */
  YYSYMBOL_TOKEN_LTE = 34,                 /* TOKEN_LTE  */
  YYSYMBOL_TOKEN_EQ = 35,                  /* TOKEN_EQ  */
  YYSYMBOL_TOKEN_NEQ = 36,                 /* TOKEN_NEQ  */
  YYSYMBOL_TOKEN_RETURN = 37,              /* TOKEN_RETURN  */
  YYSYMBOL_TOKEN_BREAK = 38,               /* TOKEN_BREAK  */
  YYSYMBOL_TOKEN_CONTINUE = 39,            /* TOKEN_CONTINUE  */
  YYSYMBOL_TOKEN_ARROW = 40,               /* TOKEN_ARROW  */
  YYSYMBOL_TOKEN_DARROW = 41,              /* TOKEN_DARROW  */
  YYSYMBOL_TOKEN_AND = 42,                 /* TOKEN_AND  */
  YYSYMBOL_TOKEN_OR = 43,                  /* TOKEN_OR  */
  YYSYMBOL_TOKEN_VAR = 44,                 /* TOKEN_VAR  */
  YYSYMBOL_UNARYMINUS = 45,                /* UNARYMINUS  */
  YYSYMBOL_TOKEN_NOT = 46,                 /* TOKEN_NOT  */
  YYSYMBOL_YYACCEPT = 47,                  /* $accept  */
  YYSYMBOL_func_list = 48,                 /* func_list  */
  YYSYMBOL_func = 49,                      /* func  */
  YYSYMBOL_param_list = 50,                /* param_list  */
  YYSYMBOL_stmt_list = 51,                 /* stmt_list  */
  YYSYMBOL_stmt = 52,                      /* stmt  */
  YYSYMBOL_expr_stmt = 53,                 /* expr_stmt  */
  YYSYMBOL_assign_stmt = 54,               /* assign_stmt  */
  YYSYMBOL_if_stmt = 55,                   /* if_stmt  */
  YYSYMBOL_elif_stmt = 56,                 /* elif_stmt  */
  YYSYMBOL_else_stmt = 57,                 /* else_stmt  */
  YYSYMBOL_while_stmt = 58,                /* while_stmt  */
  YYSYMBOL_for_stmt = 59,                  /* for_stmt  */
  YYSYMBOL_return_stmt = 60,               /* return_stmt  */
  YYSYMBOL_break_stmt = 61,                /* break_stmt  */
  YYSYMBOL_continue_stmt = 62,             /* continue_stmt  */
  YYSYMBOL_expr = 63,                      /* expr  */
  YYSYMBOL_arg_list = 64,                  /* arg_list  */
  YYSYMBOL_kv_list = 65,                   /* kv_list  */
  YYSYMBOL_kv = 66,                        /* kv  */
  YYSYMBOL_term = 67                       /* term  */
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
typedef yytype_uint8 yy_state_t;

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
#define YYFINAL  5
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1810

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  47
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  21
/* YYNRULES -- Number of rules.  */
#define YYNRULES  82
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  199

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   301


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
      45,    46
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   173,   173,   178,   184,   189,   194,   199,   205,   210,
     216,   221,   227,   231,   235,   239,   243,   247,   251,   255,
     259,   263,   268,   274,   279,   285,   290,   296,   301,   307,
     312,   318,   323,   329,   334,   339,   344,   349,   354,   360,
     366,   372,   378,   383,   388,   393,   398,   403,   408,   413,
     418,   423,   428,   433,   438,   443,   448,   453,   458,   463,
     468,   473,   478,   483,   488,   493,   498,   503,   508,   513,
     518,   524,   529,   535,   540,   546,   551,   557,   562,   567,
     572,   577,   582
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
  "TOKEN_STR", "TOKEN_INT", "TOKEN_FLOAT", "TOKEN_FUNC", "TOKEN_LAMBDA",
  "TOKEN_LARR", "TOKEN_RARR", "TOKEN_PLUS", "TOKEN_MINUS", "TOKEN_MUL",
  "TOKEN_DIV", "TOKEN_MOD", "TOKEN_ASSIGN", "TOKEN_LPAR", "TOKEN_RPAR",
  "TOKEN_LBLK", "TOKEN_RBLK", "TOKEN_SEMICOLON", "TOKEN_COLON",
  "TOKEN_DOT", "TOKEN_COMMA", "TOKEN_IF", "TOKEN_ELSE", "TOKEN_WHILE",
  "TOKEN_FOR", "TOKEN_IN", "TOKEN_DOTDOT", "TOKEN_GT", "TOKEN_GTE",
  "TOKEN_LT", "TOKEN_LTE", "TOKEN_EQ", "TOKEN_NEQ", "TOKEN_RETURN",
  "TOKEN_BREAK", "TOKEN_CONTINUE", "TOKEN_ARROW", "TOKEN_DARROW",
  "TOKEN_AND", "TOKEN_OR", "TOKEN_VAR", "UNARYMINUS", "TOKEN_NOT",
  "$accept", "func_list", "func", "param_list", "stmt_list", "stmt",
  "expr_stmt", "assign_stmt", "if_stmt", "elif_stmt", "else_stmt",
  "while_stmt", "for_stmt", "return_stmt", "break_stmt", "continue_stmt",
  "expr", "arg_list", "kv_list", "kv", "term", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-78)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-9)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      -2,     4,     9,   -78,    18,   -78,   -78,    -1,   -78,    29,
      33,   162,    30,    57,   -78,   -78,   -78,   -78,    45,    35,
    1147,  1165,    82,   -78,    46,    -4,    74,    88,  1147,    85,
      87,  1147,  1147,   206,   -78,   -78,   -78,   -78,   -78,   -78,
     -78,   -78,   -78,   -78,   -78,  1174,   -78,   250,   -78,     0,
     -78,  1629,     3,  1629,    40,    66,  1244,    65,    94,   -78,
      26,   -78,  1147,   294,   101,  1147,   125,  1279,   -78,   -78,
    1314,  1629,   -78,   -78,  1147,  1147,  1147,  1147,  1147,  1147,
    1147,    92,   -78,   128,  1147,  1147,  1147,  1147,  1147,  1147,
     136,  1147,  1147,   -78,   338,   107,    70,   -78,  1147,   108,
     -78,  1147,  1147,   -78,    22,  1349,   -78,   382,  1147,  1384,
     111,   -78,  1147,  1419,   163,   207,   251,   295,    -3,  1454,
     -78,    89,   -78,   110,  1740,  1703,  1733,    19,  1770,   126,
    1696,  1664,   -78,   133,   113,  1629,   137,  1629,  1629,   -78,
     138,   -78,  1489,   139,   156,  1147,  1524,   -78,   -78,   -78,
    1130,   426,   141,  1086,   470,   150,   514,   144,  1209,   -78,
     -78,   102,   -78,   558,   164,   602,   -78,   646,   690,   -78,
     734,  1147,   166,  1147,   -78,   -78,   -78,   -78,   -78,   -78,
     778,   -78,  1559,   822,  1594,   -78,   172,   -78,   866,   173,
     910,   -78,   954,   -78,   998,   -78,  1042,   -78,   -78
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,     0,     0,     2,     0,     1,     3,     0,     8,     0,
       0,     0,     0,     0,    80,    79,    77,    78,     0,     0,
       0,     0,     0,     7,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    10,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,     0,    42,     0,     9,     0,
      81,    71,     0,    58,    80,     0,     0,     0,     0,    82,
       0,    73,     0,     0,     0,     0,     0,     0,    40,    41,
       0,    59,     6,    11,     0,     0,     0,     0,     0,     0,
       0,     0,    22,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     5,     0,     0,     0,    65,     0,     0,
      43,     0,     0,    66,     0,     0,    30,     0,     0,     0,
       0,    39,     0,     0,    53,    54,    55,    56,    57,     0,
      62,     0,    60,    49,    50,    47,    48,    51,    52,     0,
      46,    45,     4,     0,     0,    72,     0,    76,    75,    74,
       0,    29,     0,     0,     0,     0,     0,    44,    23,    61,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    24,
      64,     0,    70,     0,     0,     0,    26,     0,     0,    32,
       0,     0,     0,     0,    63,    68,    69,    67,    25,    28,
       0,    31,     0,     0,     0,    27,     0,    36,     0,     0,
       0,    35,     0,    34,     0,    38,     0,    33,    37
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -78,   -78,   181,   -20,   -39,   -33,   -78,   -78,   -78,   -78,
     -78,   -78,   -78,   -78,   -78,   -78,    -9,   -77,   -78,    90,
     -78
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,     2,     3,    10,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    52,    60,    61,
      46
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      73,    55,     8,     8,   121,     1,    74,     4,    94,     5,
      51,    53,    56,    97,    81,    63,     1,     9,    95,    67,
      83,    64,    70,    71,   107,    57,    58,    98,    74,    96,
      75,    76,    77,    78,    79,     7,    81,    90,    14,    15,
      16,    17,    83,    18,    19,    50,   103,    20,    11,    47,
     104,    12,    21,   105,    22,    89,   109,    13,    -8,    90,
      48,    73,    49,    62,    -8,   113,   114,   115,   116,   117,
     118,   119,    51,   161,    73,   123,   124,   125,   126,   127,
     128,    32,   130,   131,    99,    57,    58,   101,   134,   135,
      13,    65,   137,   138,    13,    14,    15,    16,    17,   142,
      18,    19,    59,   146,    20,    66,    68,   149,    69,    21,
     120,    22,   163,    98,   165,   167,   102,   170,   108,    74,
     174,    75,    76,    77,    78,    79,    98,    81,   110,   180,
      73,   122,    73,    83,    73,   144,   158,    73,    32,   129,
     145,    51,    85,   150,   188,    88,    89,    73,   133,   136,
      90,   194,   151,   196,   152,    73,   153,   154,   156,   157,
     164,    73,   182,    73,   184,    14,    15,    16,    17,   168,
      18,    19,    74,   171,    20,    76,    77,    78,    79,    21,
      81,    22,    23,     6,   176,   183,    83,    24,    25,    26,
      27,   190,   192,     0,   139,     0,     0,     0,     0,    28,
      29,    30,     0,    90,     0,     0,    31,     0,    32,    14,
      15,    16,    17,     0,    18,    19,    74,     0,    20,     0,
      77,    78,    79,    21,    81,    22,    72,     0,     0,     0,
      83,    24,    25,    26,    27,     0,     0,     0,     0,     0,
       0,     0,     0,    28,    29,    30,     0,    90,     0,     0,
      31,     0,    32,    14,    15,    16,    17,     0,    18,    19,
      74,     0,    20,     0,     0,    78,    79,    21,    81,    22,
      93,     0,     0,     0,    83,    24,    25,    26,    27,     0,
       0,     0,     0,     0,     0,     0,     0,    28,    29,    30,
       0,    90,     0,     0,    31,     0,    32,    14,    15,    16,
      17,     0,    18,    19,    74,     0,    20,     0,     0,     0,
      79,    21,    81,    22,   106,     0,     0,     0,    83,    24,
      25,    26,    27,     0,     0,     0,     0,     0,     0,     0,
       0,    28,    29,    30,     0,    90,     0,     0,    31,     0,
      32,    14,    15,    16,    17,     0,    18,    19,     0,     0,
      20,     0,     0,     0,     0,    21,     0,    22,   132,     0,
       0,     0,     0,    24,    25,    26,    27,     0,     0,     0,
       0,     0,     0,     0,     0,    28,    29,    30,     0,     0,
       0,     0,    31,     0,    32,    14,    15,    16,    17,     0,
      18,    19,     0,     0,    20,     0,     0,     0,     0,    21,
       0,    22,   141,     0,     0,     0,     0,    24,    25,    26,
      27,     0,     0,     0,     0,     0,     0,     0,     0,    28,
      29,    30,     0,     0,     0,     0,    31,     0,    32,    14,
      15,    16,    17,     0,    18,    19,     0,     0,    20,     0,
       0,     0,     0,    21,     0,    22,   162,     0,     0,     0,
       0,    24,    25,    26,    27,     0,     0,     0,     0,     0,
       0,     0,     0,    28,    29,    30,     0,     0,     0,     0,
      31,     0,    32,    14,    15,    16,    17,     0,    18,    19,
       0,     0,    20,     0,     0,     0,     0,    21,     0,    22,
     166,     0,     0,     0,     0,    24,    25,    26,    27,     0,
       0,     0,     0,     0,     0,     0,     0,    28,    29,    30,
       0,     0,     0,     0,    31,     0,    32,    14,    15,    16,
      17,     0,    18,    19,     0,     0,    20,     0,     0,     0,
       0,    21,     0,    22,   169,     0,     0,     0,     0,    24,
      25,    26,    27,     0,     0,     0,     0,     0,     0,     0,
       0,    28,    29,    30,     0,     0,     0,     0,    31,     0,
      32,    14,    15,    16,    17,     0,    18,    19,     0,     0,
      20,     0,     0,     0,     0,    21,     0,    22,   175,     0,
       0,     0,     0,    24,    25,    26,    27,     0,     0,     0,
       0,     0,     0,     0,     0,    28,    29,    30,     0,     0,
       0,     0,    31,     0,    32,    14,    15,    16,    17,     0,
      18,    19,     0,     0,    20,     0,     0,     0,     0,    21,
       0,    22,   177,     0,     0,     0,     0,    24,    25,    26,
      27,     0,     0,     0,     0,     0,     0,     0,     0,    28,
      29,    30,     0,     0,     0,     0,    31,     0,    32,    14,
      15,    16,    17,     0,    18,    19,     0,     0,    20,     0,
       0,     0,     0,    21,     0,    22,   178,     0,     0,     0,
       0,    24,    25,    26,    27,     0,     0,     0,     0,     0,
       0,     0,     0,    28,    29,    30,     0,     0,     0,     0,
      31,     0,    32,    14,    15,    16,    17,     0,    18,    19,
       0,     0,    20,     0,     0,     0,     0,    21,     0,    22,
     179,     0,     0,     0,     0,    24,    25,    26,    27,     0,
       0,     0,     0,     0,     0,     0,     0,    28,    29,    30,
       0,     0,     0,     0,    31,     0,    32,    14,    15,    16,
      17,     0,    18,    19,     0,     0,    20,     0,     0,     0,
       0,    21,     0,    22,   181,     0,     0,     0,     0,    24,
      25,    26,    27,     0,     0,     0,     0,     0,     0,     0,
       0,    28,    29,    30,     0,     0,     0,     0,    31,     0,
      32,    14,    15,    16,    17,     0,    18,    19,     0,     0,
      20,     0,     0,     0,     0,    21,     0,    22,   185,     0,
       0,     0,     0,    24,    25,    26,    27,     0,     0,     0,
       0,     0,     0,     0,     0,    28,    29,    30,     0,     0,
       0,     0,    31,     0,    32,    14,    15,    16,    17,     0,
      18,    19,     0,     0,    20,     0,     0,     0,     0,    21,
       0,    22,   187,     0,     0,     0,     0,    24,    25,    26,
      27,     0,     0,     0,     0,     0,     0,     0,     0,    28,
      29,    30,     0,     0,     0,     0,    31,     0,    32,    14,
      15,    16,    17,     0,    18,    19,     0,     0,    20,     0,
       0,     0,     0,    21,     0,    22,   191,     0,     0,     0,
       0,    24,    25,    26,    27,     0,     0,     0,     0,     0,
       0,     0,     0,    28,    29,    30,     0,     0,     0,     0,
      31,     0,    32,    14,    15,    16,    17,     0,    18,    19,
       0,     0,    20,     0,     0,     0,     0,    21,     0,    22,
     193,     0,     0,     0,     0,    24,    25,    26,    27,     0,
       0,     0,     0,     0,     0,     0,     0,    28,    29,    30,
       0,     0,     0,     0,    31,     0,    32,    14,    15,    16,
      17,     0,    18,    19,     0,     0,    20,     0,     0,     0,
       0,    21,     0,    22,   195,     0,     0,     0,     0,    24,
      25,    26,    27,     0,     0,     0,     0,     0,     0,     0,
       0,    28,    29,    30,     0,     0,     0,     0,    31,     0,
      32,    14,    15,    16,    17,     0,    18,    19,     0,     0,
      20,     0,     0,     0,     0,    21,     0,    22,   197,     0,
       0,     0,     0,    24,    25,    26,    27,     0,     0,     0,
       0,     0,     0,     0,     0,    28,    29,    30,     0,     0,
       0,     0,    31,     0,    32,    14,    15,    16,    17,     0,
      18,    19,     0,     0,    20,     0,     0,     0,     0,    21,
       0,    22,   198,     0,     0,     0,     0,    24,    25,    26,
      27,     0,     0,     0,     0,     0,     0,     0,     0,    28,
      29,    30,     0,     0,     0,     0,    31,     0,    32,    14,
      15,    16,    17,     0,    18,    19,     0,     0,    20,     0,
       0,     0,     0,    21,     0,    22,     0,     0,     0,     0,
       0,    24,    25,    26,    27,     0,     0,     0,     0,     0,
       0,     0,     0,    28,    29,    30,     0,     0,     0,     0,
      31,     0,    32,    14,    15,    16,    17,     0,    18,    19,
       0,     0,    20,     0,     0,     0,     0,    21,   160,    22,
      14,    15,    16,    17,     0,    18,    19,     0,     0,    20,
       0,     0,     0,     0,    21,     0,    22,     0,    54,    15,
      16,    17,     0,    18,    19,     0,    32,    20,     0,     0,
       0,     0,    21,    74,    22,    75,    76,    77,    78,    79,
      80,    81,     0,    32,     0,    82,     0,    83,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,    87,    88,
      89,    32,     0,     0,    90,     0,    91,    92,    74,     0,
      75,    76,    77,    78,    79,     0,    81,   172,     0,     0,
       0,     0,    83,     0,     0,     0,     0,     0,     0,   173,
      84,    85,    86,    87,    88,    89,     0,     0,     0,    90,
       0,    91,    92,    74,     0,    75,    76,    77,    78,    79,
       0,    81,   100,     0,     0,     0,     0,    83,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,    87,    88,
      89,     0,     0,     0,    90,     0,    91,    92,    74,     0,
      75,    76,    77,    78,    79,     0,    81,     0,     0,     0,
     111,     0,    83,     0,     0,     0,     0,     0,     0,     0,
      84,    85,    86,    87,    88,    89,     0,     0,     0,    90,
       0,    91,    92,    74,     0,    75,    76,    77,    78,    79,
     112,    81,     0,     0,     0,     0,     0,    83,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,    87,    88,
      89,     0,     0,     0,    90,     0,    91,    92,    74,     0,
      75,    76,    77,    78,    79,     0,    81,   140,     0,     0,
       0,     0,    83,     0,     0,     0,     0,     0,     0,     0,
      84,    85,    86,    87,    88,    89,     0,     0,     0,    90,
       0,    91,    92,    74,     0,    75,    76,    77,    78,    79,
       0,    81,   143,     0,     0,     0,     0,    83,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,    87,    88,
      89,     0,     0,     0,    90,     0,    91,    92,    74,   147,
      75,    76,    77,    78,    79,     0,    81,     0,     0,     0,
       0,     0,    83,     0,     0,     0,     0,     0,     0,     0,
      84,    85,    86,    87,    88,    89,     0,     0,     0,    90,
       0,    91,    92,    74,     0,    75,    76,    77,    78,    79,
       0,    81,     0,     0,     0,   148,     0,    83,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,    87,    88,
      89,     0,     0,     0,    90,     0,    91,    92,    74,     0,
      75,    76,    77,    78,    79,     0,    81,   155,     0,     0,
       0,     0,    83,     0,     0,     0,     0,     0,     0,     0,
      84,    85,    86,    87,    88,    89,     0,     0,     0,    90,
       0,    91,    92,    74,     0,    75,    76,    77,    78,    79,
       0,    81,     0,     0,     0,   159,     0,    83,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,    87,    88,
      89,     0,     0,     0,    90,     0,    91,    92,    74,     0,
      75,    76,    77,    78,    79,     0,    81,   186,     0,     0,
       0,     0,    83,     0,     0,     0,     0,     0,     0,     0,
      84,    85,    86,    87,    88,    89,     0,     0,     0,    90,
       0,    91,    92,    74,     0,    75,    76,    77,    78,    79,
       0,    81,   189,     0,     0,     0,     0,    83,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,    87,    88,
      89,     0,     0,     0,    90,     0,    91,    92,    74,     0,
      75,    76,    77,    78,    79,     0,    81,     0,     0,     0,
       0,     0,    83,     0,     0,     0,     0,     0,     0,     0,
      84,    85,    86,    87,    88,    89,     0,     0,     0,    90,
       0,    91,    92,    74,     0,    75,    76,    77,    78,    79,
       0,    81,     0,     0,     0,     0,     0,    83,     0,     0,
       0,     0,     0,     0,     0,    84,    85,    86,    87,    88,
      89,     0,     0,     0,    90,    74,    91,    75,    76,    77,
      78,    79,    74,    81,    75,    76,    77,    78,    79,    83,
      81,     0,     0,     0,     0,     0,    83,    84,    85,    86,
      87,    88,    89,     0,    84,    85,    90,    87,    88,    89,
       0,     0,    74,    90,    75,    76,    77,    78,    79,    74,
      81,    75,    76,    77,    78,    79,    83,    81,     0,     0,
       0,     0,     0,    83,    84,    85,     0,     0,    88,    89,
       0,     0,     0,    90,     0,    88,    89,     0,     0,    74,
      90,    75,    76,    77,    78,    79,     0,    81,     0,     0,
       0,     0,     0,    83,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      90
};

static const yytype_int16 yycheck[] =
{
      33,    21,     3,     3,    81,     7,     9,     3,    47,     0,
      19,    20,    21,    10,    17,    19,     7,    18,    18,    28,
      23,    25,    31,    32,    63,     3,     4,    24,     9,    49,
      11,    12,    13,    14,    15,    17,    17,    40,     3,     4,
       5,     6,    23,     8,     9,    10,    20,    12,    19,    19,
      24,    18,    17,    62,    19,    36,    65,    24,    18,    40,
       3,    94,    17,    17,    24,    74,    75,    76,    77,    78,
      79,    80,    81,   150,   107,    84,    85,    86,    87,    88,
      89,    46,    91,    92,    18,     3,     4,    22,    18,    98,
      24,    17,   101,   102,    24,     3,     4,     5,     6,   108,
       8,     9,    20,   112,    12,    17,    21,    18,    21,    17,
      18,    19,   151,    24,   153,   154,    22,   156,    17,     9,
      18,    11,    12,    13,    14,    15,    24,    17,     3,   168,
     163,     3,   165,    23,   167,    24,   145,   170,    46,     3,
      29,   150,    32,    17,   183,    35,    36,   180,    41,    41,
      40,   190,    19,   192,    41,   188,    19,    19,    19,     3,
      19,   194,   171,   196,   173,     3,     4,     5,     6,    19,
       8,     9,     9,    29,    12,    12,    13,    14,    15,    17,
      17,    19,    20,     2,    20,    19,    23,    25,    26,    27,
      28,    19,    19,    -1,   104,    -1,    -1,    -1,    -1,    37,
      38,    39,    -1,    40,    -1,    -1,    44,    -1,    46,     3,
       4,     5,     6,    -1,     8,     9,     9,    -1,    12,    -1,
      13,    14,    15,    17,    17,    19,    20,    -1,    -1,    -1,
      23,    25,    26,    27,    28,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    39,    -1,    40,    -1,    -1,
      44,    -1,    46,     3,     4,     5,     6,    -1,     8,     9,
       9,    -1,    12,    -1,    -1,    14,    15,    17,    17,    19,
      20,    -1,    -1,    -1,    23,    25,    26,    27,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    39,
      -1,    40,    -1,    -1,    44,    -1,    46,     3,     4,     5,
       6,    -1,     8,     9,     9,    -1,    12,    -1,    -1,    -1,
      15,    17,    17,    19,    20,    -1,    -1,    -1,    23,    25,
      26,    27,    28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    39,    -1,    40,    -1,    -1,    44,    -1,
      46,     3,     4,     5,     6,    -1,     8,     9,    -1,    -1,
      12,    -1,    -1,    -1,    -1,    17,    -1,    19,    20,    -1,
      -1,    -1,    -1,    25,    26,    27,    28,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    39,    -1,    -1,
      -1,    -1,    44,    -1,    46,     3,     4,     5,     6,    -1,
       8,     9,    -1,    -1,    12,    -1,    -1,    -1,    -1,    17,
      -1,    19,    20,    -1,    -1,    -1,    -1,    25,    26,    27,
      28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    39,    -1,    -1,    -1,    -1,    44,    -1,    46,     3,
       4,     5,     6,    -1,     8,     9,    -1,    -1,    12,    -1,
      -1,    -1,    -1,    17,    -1,    19,    20,    -1,    -1,    -1,
      -1,    25,    26,    27,    28,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    39,    -1,    -1,    -1,    -1,
      44,    -1,    46,     3,     4,     5,     6,    -1,     8,     9,
      -1,    -1,    12,    -1,    -1,    -1,    -1,    17,    -1,    19,
      20,    -1,    -1,    -1,    -1,    25,    26,    27,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    39,
      -1,    -1,    -1,    -1,    44,    -1,    46,     3,     4,     5,
       6,    -1,     8,     9,    -1,    -1,    12,    -1,    -1,    -1,
      -1,    17,    -1,    19,    20,    -1,    -1,    -1,    -1,    25,
      26,    27,    28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    39,    -1,    -1,    -1,    -1,    44,    -1,
      46,     3,     4,     5,     6,    -1,     8,     9,    -1,    -1,
      12,    -1,    -1,    -1,    -1,    17,    -1,    19,    20,    -1,
      -1,    -1,    -1,    25,    26,    27,    28,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    39,    -1,    -1,
      -1,    -1,    44,    -1,    46,     3,     4,     5,     6,    -1,
       8,     9,    -1,    -1,    12,    -1,    -1,    -1,    -1,    17,
      -1,    19,    20,    -1,    -1,    -1,    -1,    25,    26,    27,
      28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    39,    -1,    -1,    -1,    -1,    44,    -1,    46,     3,
       4,     5,     6,    -1,     8,     9,    -1,    -1,    12,    -1,
      -1,    -1,    -1,    17,    -1,    19,    20,    -1,    -1,    -1,
      -1,    25,    26,    27,    28,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    39,    -1,    -1,    -1,    -1,
      44,    -1,    46,     3,     4,     5,     6,    -1,     8,     9,
      -1,    -1,    12,    -1,    -1,    -1,    -1,    17,    -1,    19,
      20,    -1,    -1,    -1,    -1,    25,    26,    27,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    39,
      -1,    -1,    -1,    -1,    44,    -1,    46,     3,     4,     5,
       6,    -1,     8,     9,    -1,    -1,    12,    -1,    -1,    -1,
      -1,    17,    -1,    19,    20,    -1,    -1,    -1,    -1,    25,
      26,    27,    28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    39,    -1,    -1,    -1,    -1,    44,    -1,
      46,     3,     4,     5,     6,    -1,     8,     9,    -1,    -1,
      12,    -1,    -1,    -1,    -1,    17,    -1,    19,    20,    -1,
      -1,    -1,    -1,    25,    26,    27,    28,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    39,    -1,    -1,
      -1,    -1,    44,    -1,    46,     3,     4,     5,     6,    -1,
       8,     9,    -1,    -1,    12,    -1,    -1,    -1,    -1,    17,
      -1,    19,    20,    -1,    -1,    -1,    -1,    25,    26,    27,
      28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    39,    -1,    -1,    -1,    -1,    44,    -1,    46,     3,
       4,     5,     6,    -1,     8,     9,    -1,    -1,    12,    -1,
      -1,    -1,    -1,    17,    -1,    19,    20,    -1,    -1,    -1,
      -1,    25,    26,    27,    28,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    39,    -1,    -1,    -1,    -1,
      44,    -1,    46,     3,     4,     5,     6,    -1,     8,     9,
      -1,    -1,    12,    -1,    -1,    -1,    -1,    17,    -1,    19,
      20,    -1,    -1,    -1,    -1,    25,    26,    27,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    39,
      -1,    -1,    -1,    -1,    44,    -1,    46,     3,     4,     5,
       6,    -1,     8,     9,    -1,    -1,    12,    -1,    -1,    -1,
      -1,    17,    -1,    19,    20,    -1,    -1,    -1,    -1,    25,
      26,    27,    28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    39,    -1,    -1,    -1,    -1,    44,    -1,
      46,     3,     4,     5,     6,    -1,     8,     9,    -1,    -1,
      12,    -1,    -1,    -1,    -1,    17,    -1,    19,    20,    -1,
      -1,    -1,    -1,    25,    26,    27,    28,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    39,    -1,    -1,
      -1,    -1,    44,    -1,    46,     3,     4,     5,     6,    -1,
       8,     9,    -1,    -1,    12,    -1,    -1,    -1,    -1,    17,
      -1,    19,    20,    -1,    -1,    -1,    -1,    25,    26,    27,
      28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    39,    -1,    -1,    -1,    -1,    44,    -1,    46,     3,
       4,     5,     6,    -1,     8,     9,    -1,    -1,    12,    -1,
      -1,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,    -1,
      -1,    25,    26,    27,    28,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    39,    -1,    -1,    -1,    -1,
      44,    -1,    46,     3,     4,     5,     6,    -1,     8,     9,
      -1,    -1,    12,    -1,    -1,    -1,    -1,    17,    18,    19,
       3,     4,     5,     6,    -1,     8,     9,    -1,    -1,    12,
      -1,    -1,    -1,    -1,    17,    -1,    19,    -1,     3,     4,
       5,     6,    -1,     8,     9,    -1,    46,    12,    -1,    -1,
      -1,    -1,    17,     9,    19,    11,    12,    13,    14,    15,
      16,    17,    -1,    46,    -1,    21,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    31,    32,    33,    34,    35,
      36,    46,    -1,    -1,    40,    -1,    42,    43,     9,    -1,
      11,    12,    13,    14,    15,    -1,    17,    18,    -1,    -1,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    40,
      -1,    42,    43,     9,    -1,    11,    12,    13,    14,    15,
      -1,    17,    18,    -1,    -1,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    31,    32,    33,    34,    35,
      36,    -1,    -1,    -1,    40,    -1,    42,    43,     9,    -1,
      11,    12,    13,    14,    15,    -1,    17,    -1,    -1,    -1,
      21,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    40,
      -1,    42,    43,     9,    -1,    11,    12,    13,    14,    15,
      16,    17,    -1,    -1,    -1,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    31,    32,    33,    34,    35,
      36,    -1,    -1,    -1,    40,    -1,    42,    43,     9,    -1,
      11,    12,    13,    14,    15,    -1,    17,    18,    -1,    -1,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    40,
      -1,    42,    43,     9,    -1,    11,    12,    13,    14,    15,
      -1,    17,    18,    -1,    -1,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    31,    32,    33,    34,    35,
      36,    -1,    -1,    -1,    40,    -1,    42,    43,     9,    10,
      11,    12,    13,    14,    15,    -1,    17,    -1,    -1,    -1,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    40,
      -1,    42,    43,     9,    -1,    11,    12,    13,    14,    15,
      -1,    17,    -1,    -1,    -1,    21,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    31,    32,    33,    34,    35,
      36,    -1,    -1,    -1,    40,    -1,    42,    43,     9,    -1,
      11,    12,    13,    14,    15,    -1,    17,    18,    -1,    -1,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    40,
      -1,    42,    43,     9,    -1,    11,    12,    13,    14,    15,
      -1,    17,    -1,    -1,    -1,    21,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    31,    32,    33,    34,    35,
      36,    -1,    -1,    -1,    40,    -1,    42,    43,     9,    -1,
      11,    12,    13,    14,    15,    -1,    17,    18,    -1,    -1,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    40,
      -1,    42,    43,     9,    -1,    11,    12,    13,    14,    15,
      -1,    17,    18,    -1,    -1,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    31,    32,    33,    34,    35,
      36,    -1,    -1,    -1,    40,    -1,    42,    43,     9,    -1,
      11,    12,    13,    14,    15,    -1,    17,    -1,    -1,    -1,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    40,
      -1,    42,    43,     9,    -1,    11,    12,    13,    14,    15,
      -1,    17,    -1,    -1,    -1,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    31,    32,    33,    34,    35,
      36,    -1,    -1,    -1,    40,     9,    42,    11,    12,    13,
      14,    15,     9,    17,    11,    12,    13,    14,    15,    23,
      17,    -1,    -1,    -1,    -1,    -1,    23,    31,    32,    33,
      34,    35,    36,    -1,    31,    32,    40,    34,    35,    36,
      -1,    -1,     9,    40,    11,    12,    13,    14,    15,     9,
      17,    11,    12,    13,    14,    15,    23,    17,    -1,    -1,
      -1,    -1,    -1,    23,    31,    32,    -1,    -1,    35,    36,
      -1,    -1,    -1,    40,    -1,    35,    36,    -1,    -1,     9,
      40,    11,    12,    13,    14,    15,    -1,    17,    -1,    -1,
      -1,    -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      40
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     7,    48,    49,     3,     0,    49,    17,     3,    18,
      50,    19,    18,    24,     3,     4,     5,     6,     8,     9,
      12,    17,    19,    20,    25,    26,    27,    28,    37,    38,
      39,    44,    46,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    67,    19,     3,    17,
      10,    63,    64,    63,     3,    50,    63,     3,     4,    20,
      65,    66,    17,    19,    25,    17,    17,    63,    21,    21,
      63,    63,    20,    52,     9,    11,    12,    13,    14,    15,
      16,    17,    21,    23,    31,    32,    33,    34,    35,    36,
      40,    42,    43,    20,    51,    18,    50,    10,    24,    18,
      18,    22,    22,    20,    24,    63,    20,    51,    17,    63,
       3,    21,    16,    63,    63,    63,    63,    63,    63,    63,
      18,    64,     3,    63,    63,    63,    63,    63,    63,     3,
      63,    63,    20,    41,    18,    63,    41,    63,    63,    66,
      18,    20,    63,    18,    24,    29,    63,    10,    21,    18,
      17,    19,    41,    19,    19,    18,    19,     3,    63,    21,
      18,    64,    20,    51,    19,    51,    20,    51,    19,    20,
      51,    29,    18,    30,    18,    20,    20,    20,    20,    20,
      51,    20,    63,    19,    63,    20,    18,    20,    51,    18,
      19,    20,    19,    20,    51,    20,    51,    20,    20
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    47,    48,    48,    49,    49,    49,    49,    50,    50,
      51,    51,    52,    52,    52,    52,    52,    52,    52,    52,
      52,    52,    53,    54,    54,    55,    55,    56,    56,    57,
      57,    58,    58,    59,    59,    59,    59,    59,    59,    60,
      61,    62,    63,    63,    63,    63,    63,    63,    63,    63,
      63,    63,    63,    63,    63,    63,    63,    63,    63,    63,
      63,    63,    63,    63,    63,    63,    63,    63,    63,    63,
      63,    64,    64,    65,    65,    66,    66,    67,    67,    67,
      67,    67,    67
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     2,     8,     7,     7,     6,     1,     3,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     4,     5,     7,     6,     8,     7,     4,
       3,     7,     6,    11,    10,     9,     8,    11,    10,     3,
       2,     2,     1,     3,     4,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     2,     2,
       3,     4,     3,     6,     5,     3,     3,     7,     7,     7,
       6,     1,     3,     1,     3,     3,     3,     1,     1,     1,
       1,     2,     2
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
#line 167 "/Users/tabata/src/NoctLang/src/parser.y"
{
	ast_yylloc.last_line = yylloc.first_line = 0;
	ast_yylloc.last_column = yylloc.first_column = 0;
}

#line 1584 "/Users/tabata/src/NoctLang/build/parser.tab.c"

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
#line 174 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.func_list) = ast_accept_func_list(NULL, (yyvsp[0].func));
			debug("func_list: class");
		}
#line 1800 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 3: /* func_list: func_list func  */
#line 179 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.func_list) = ast_accept_func_list((yyvsp[-1].func_list), (yyvsp[0].func));
			debug("func_list: func_list func");
		}
#line 1809 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 4: /* func: TOKEN_FUNC TOKEN_SYMBOL TOKEN_LPAR param_list TOKEN_RPAR TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 185 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-6].sval), (yyvsp[-4].param_list), (yyvsp[-1].stmt_list));
			debug("func: func name(param_list) { stmt_list }");
		}
#line 1818 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 5: /* func: TOKEN_FUNC TOKEN_SYMBOL TOKEN_LPAR param_list TOKEN_RPAR TOKEN_LBLK TOKEN_RBLK  */
#line 190 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-5].sval), (yyvsp[-3].param_list), NULL);
			debug("func: func name(param_list) { empty }");
		}
#line 1827 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 6: /* func: TOKEN_FUNC TOKEN_SYMBOL TOKEN_LPAR TOKEN_RPAR TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 195 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-5].sval), NULL, (yyvsp[-1].stmt_list));
			debug("func: func name() { stmt_list }");
		}
#line 1836 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 7: /* func: TOKEN_FUNC TOKEN_SYMBOL TOKEN_LPAR TOKEN_RPAR TOKEN_LBLK TOKEN_RBLK  */
#line 200 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.func) = ast_accept_func((yyvsp[-4].sval), NULL, NULL);
			debug("func: func name() { empty }");
		}
#line 1845 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 8: /* param_list: TOKEN_SYMBOL  */
#line 206 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.param_list) = ast_accept_param_list(NULL, (yyvsp[0].sval));
			debug("param_list: symbol");
		}
#line 1854 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 9: /* param_list: param_list TOKEN_COMMA TOKEN_SYMBOL  */
#line 211 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.param_list) = ast_accept_param_list((yyvsp[-2].param_list), (yyvsp[0].sval));
			debug("param_list: param_list symbol");
		}
#line 1863 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 10: /* stmt_list: stmt  */
#line 217 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt_list) = ast_accept_stmt_list(NULL, (yyvsp[0].stmt));
			debug("stmt_list: stmt");
		}
#line 1872 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 11: /* stmt_list: stmt_list stmt  */
#line 222 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt_list) = ast_accept_stmt_list((yyvsp[-1].stmt_list), (yyvsp[0].stmt));
			debug("stmt_list: stmt_list stmt");
		}
#line 1881 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 12: /* stmt: expr_stmt  */
#line 228 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 1889 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 13: /* stmt: assign_stmt  */
#line 232 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 1897 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 14: /* stmt: if_stmt  */
#line 236 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 1905 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 15: /* stmt: elif_stmt  */
#line 240 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 1913 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 16: /* stmt: else_stmt  */
#line 244 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 1921 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 17: /* stmt: while_stmt  */
#line 248 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 1929 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 18: /* stmt: for_stmt  */
#line 252 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 1937 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 19: /* stmt: return_stmt  */
#line 256 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 1945 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 20: /* stmt: break_stmt  */
#line 260 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 1953 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 21: /* stmt: continue_stmt  */
#line 264 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = (yyvsp[0].stmt);
		}
#line 1961 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 22: /* expr_stmt: expr TOKEN_SEMICOLON  */
#line 269 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_expr_stmt((yylsp[-1]).first_line + 1, (yyvsp[-1].expr));
			debug("expr_stmt");
		}
#line 1970 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 23: /* assign_stmt: expr TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 275 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt((yylsp[-3]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr), false);
			debug("assign_stmt");
		}
#line 1979 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 24: /* assign_stmt: TOKEN_VAR expr TOKEN_ASSIGN expr TOKEN_SEMICOLON  */
#line 280 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_assign_stmt((yylsp[-4]).first_line + 1, (yyvsp[-3].expr), (yyvsp[-1].expr), true);
			debug("var assign_stmt");
		}
#line 1988 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 25: /* if_stmt: TOKEN_IF TOKEN_LPAR expr TOKEN_RPAR TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 286 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_if_stmt((yylsp[-6]).first_line + 1, (yyvsp[-4].expr), (yyvsp[-1].stmt_list));
			debug("if_stmt: stmt_list");
		}
#line 1997 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 26: /* if_stmt: TOKEN_IF TOKEN_LPAR expr TOKEN_RPAR TOKEN_LBLK TOKEN_RBLK  */
#line 291 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_if_stmt((yylsp[-5]).first_line + 1, (yyvsp[-3].expr), NULL);
			debug("if_stmt: empty");
		}
#line 2006 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 27: /* elif_stmt: TOKEN_ELSE TOKEN_IF TOKEN_LPAR expr TOKEN_RPAR TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 297 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_elif_stmt((yylsp[-7]).first_line + 1, (yyvsp[-4].expr), (yyvsp[-1].stmt_list));
			debug("elif_stmt: stmt_list");
		}
#line 2015 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 28: /* elif_stmt: TOKEN_ELSE TOKEN_IF TOKEN_LPAR expr TOKEN_RPAR TOKEN_LBLK TOKEN_RBLK  */
#line 302 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_elif_stmt((yylsp[-6]).first_line + 1, (yyvsp[-3].expr), NULL);
			debug("elif_stmt: empty");
		}
#line 2024 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 29: /* else_stmt: TOKEN_ELSE TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 308 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_else_stmt((yylsp[-3]).first_line + 1, (yyvsp[-1].stmt_list));
			debug("else_stmt: stmt_list");
		}
#line 2033 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 30: /* else_stmt: TOKEN_ELSE TOKEN_LBLK TOKEN_RBLK  */
#line 313 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_else_stmt((yylsp[-2]).first_line + 1, NULL);
			debug("else_stmt: empty");
		}
#line 2042 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 31: /* while_stmt: TOKEN_WHILE TOKEN_LPAR expr TOKEN_RPAR TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 319 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_while_stmt((yylsp[-6]).first_line + 1, (yyvsp[-4].expr), (yyvsp[-1].stmt_list));
			debug("while_stmt: stmt_list");
		}
#line 2051 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 32: /* while_stmt: TOKEN_WHILE TOKEN_LPAR expr TOKEN_RPAR TOKEN_LBLK TOKEN_RBLK  */
#line 324 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_while_stmt((yylsp[-5]).first_line + 1, (yyvsp[-3].expr), NULL);
			debug("while_stmt: empty");
		}
#line 2060 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 33: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_COMMA TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 330 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_for_kv_stmt((yylsp[-10]).first_line + 1, (yyvsp[-8].sval), (yyvsp[-6].sval), (yyvsp[-4].expr), (yyvsp[-1].stmt_list));
			debug("for_stmt: for(k, v in array) { stmt_list }");
		}
#line 2069 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 34: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_COMMA TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR TOKEN_LBLK TOKEN_RBLK  */
#line 335 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_for_kv_stmt((yylsp[-9]).first_line + 1, (yyvsp[-7].sval), (yyvsp[-5].sval), (yyvsp[-3].expr), NULL);
			debug("for_stmt: for(k, v in array) { empty }");
		}
#line 2078 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 35: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 340 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_for_v_stmt((yylsp[-8]).first_line + 1, (yyvsp[-6].sval), (yyvsp[-4].expr), (yyvsp[-1].stmt_list));
			debug("for_stmt: for(v in array) { stmt_list }");
		}
#line 2087 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 36: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_RPAR TOKEN_LBLK TOKEN_RBLK  */
#line 345 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_for_v_stmt((yylsp[-7]).first_line + 1, (yyvsp[-5].sval), (yyvsp[-3].expr), NULL);
			debug("for_stmt: for(v in array) { empty }");
		}
#line 2096 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 37: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_DOTDOT expr TOKEN_RPAR TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 350 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_for_range_stmt((yylsp[-10]).first_line + 1, (yyvsp[-8].sval), (yyvsp[-6].expr), (yyvsp[-4].expr), (yyvsp[-1].stmt_list));
			debug("for_stmt: for(i in x..y) { stmt_list }");
		}
#line 2105 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 38: /* for_stmt: TOKEN_FOR TOKEN_LPAR TOKEN_SYMBOL TOKEN_IN expr TOKEN_DOTDOT expr TOKEN_RPAR TOKEN_LBLK TOKEN_RBLK  */
#line 355 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_for_range_stmt((yylsp[-9]).first_line + 1, (yyvsp[-7].sval), (yyvsp[-5].expr), (yyvsp[-3].expr), NULL);
			debug("for_stmt: for(i in x..y) { empty}");
		}
#line 2114 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 39: /* return_stmt: TOKEN_RETURN expr TOKEN_SEMICOLON  */
#line 361 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_return_stmt((yylsp[-2]).first_line + 1, (yyvsp[-1].expr));
			debug("rerurn_stmt:");
		}
#line 2123 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 40: /* break_stmt: TOKEN_BREAK TOKEN_SEMICOLON  */
#line 367 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_break_stmt((yylsp[-1]).first_line + 1);
			debug("break_stmt:");
		}
#line 2132 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 41: /* continue_stmt: TOKEN_CONTINUE TOKEN_SEMICOLON  */
#line 373 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.stmt) = ast_accept_continue_stmt((yylsp[-1]).first_line + 1);
			debug("continue_stmt");
		}
#line 2141 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 42: /* expr: term  */
#line 379 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_term_expr((yyvsp[0].term));
			debug("expr: term");
		}
#line 2150 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 43: /* expr: TOKEN_LPAR expr TOKEN_RPAR  */
#line 384 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = (yyvsp[-1].expr);
			debug("expr: (expr)");
		}
#line 2159 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 44: /* expr: expr TOKEN_LARR expr TOKEN_RARR  */
#line 389 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_subscr_expr((yyvsp[-3].expr), (yyvsp[-1].expr));
			debug("expr: array[subscript]");
		}
#line 2168 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 45: /* expr: expr TOKEN_OR expr  */
#line 394 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_or_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr or expr");
		}
#line 2177 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 46: /* expr: expr TOKEN_AND expr  */
#line 399 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_and_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr and expr");
		}
#line 2186 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 47: /* expr: expr TOKEN_LT expr  */
#line 404 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_lt_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr lt expr");
		}
#line 2195 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 48: /* expr: expr TOKEN_LTE expr  */
#line 409 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_lte_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr lte expr");
		}
#line 2204 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 49: /* expr: expr TOKEN_GT expr  */
#line 414 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_gt_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr gt expr");
		}
#line 2213 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 50: /* expr: expr TOKEN_GTE expr  */
#line 419 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_gte_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr gte expr");
		}
#line 2222 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 51: /* expr: expr TOKEN_EQ expr  */
#line 424 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_eq_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr eq expr");
		}
#line 2231 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 52: /* expr: expr TOKEN_NEQ expr  */
#line 429 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_neq_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr neq expr");
		}
#line 2240 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 53: /* expr: expr TOKEN_PLUS expr  */
#line 434 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_plus_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr plus expr");
		}
#line 2249 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 54: /* expr: expr TOKEN_MINUS expr  */
#line 439 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_minus_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr sub expr");
		}
#line 2258 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 55: /* expr: expr TOKEN_MUL expr  */
#line 444 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_mul_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr mul expr");
		}
#line 2267 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 56: /* expr: expr TOKEN_DIV expr  */
#line 449 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_div_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr div expr");
		}
#line 2276 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 57: /* expr: expr TOKEN_MOD expr  */
#line 454 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_mod_expr((yyvsp[-2].expr), (yyvsp[0].expr));
			debug("expr: expr div expr");
		}
#line 2285 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 58: /* expr: TOKEN_MINUS expr  */
#line 459 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_neg_expr((yyvsp[0].expr));
			debug("expr: neg expr");
		}
#line 2294 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 59: /* expr: TOKEN_NOT expr  */
#line 464 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_not_expr((yyvsp[0].expr));
			debug("expr: not expr");
		}
#line 2303 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 60: /* expr: expr TOKEN_DOT TOKEN_SYMBOL  */
#line 469 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_dot_expr((yyvsp[-2].expr), (yyvsp[0].sval));
			debug("expr: expr.symbol");
		}
#line 2312 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 61: /* expr: expr TOKEN_LPAR arg_list TOKEN_RPAR  */
#line 474 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_call_expr((yyvsp[-3].expr), (yyvsp[-1].arg_list));
			debug("expr: call(param_list)");
		}
#line 2321 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 62: /* expr: expr TOKEN_LPAR TOKEN_RPAR  */
#line 479 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_call_expr((yyvsp[-2].expr), NULL);
			debug("expr: call()");
		}
#line 2330 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 63: /* expr: expr TOKEN_ARROW TOKEN_SYMBOL TOKEN_LPAR arg_list TOKEN_RPAR  */
#line 484 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_thiscall_expr((yyvsp[-5].expr), (yyvsp[-3].sval), (yyvsp[-1].arg_list));
			debug("expr: thiscall(param_list)");
		}
#line 2339 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 64: /* expr: expr TOKEN_ARROW TOKEN_SYMBOL TOKEN_LPAR TOKEN_RPAR  */
#line 489 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_thiscall_expr((yyvsp[-4].expr), (yyvsp[-2].sval), NULL);
			debug("expr: thiscall(param_list)");
		}
#line 2348 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 65: /* expr: TOKEN_LARR arg_list TOKEN_RARR  */
#line 494 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_array_expr((yyvsp[-1].arg_list));
			debug("expr: array");
		}
#line 2357 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 66: /* expr: TOKEN_LBLK kv_list TOKEN_RBLK  */
#line 499 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_dict_expr((yyvsp[-1].kv_list));
			debug("expr: dict");
		}
#line 2366 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 67: /* expr: TOKEN_LPAR param_list TOKEN_RPAR TOKEN_DARROW TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 504 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_func_expr((yyvsp[-5].param_list), (yyvsp[-1].stmt_list));
			debug("expr: func param_list stmt_list");
		}
#line 2375 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 68: /* expr: TOKEN_LAMBDA TOKEN_LPAR TOKEN_RPAR TOKEN_DARROW TOKEN_LBLK stmt_list TOKEN_RBLK  */
#line 509 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_func_expr(NULL, (yyvsp[-1].stmt_list));
			debug("expr: func stmt_list");
		}
#line 2384 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 69: /* expr: TOKEN_LAMBDA TOKEN_LPAR param_list TOKEN_RPAR TOKEN_DARROW TOKEN_LBLK TOKEN_RBLK  */
#line 514 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_func_expr((yyvsp[-4].param_list), NULL);
			debug("expr: func param_list");
		}
#line 2393 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 70: /* expr: TOKEN_LAMBDA TOKEN_LPAR TOKEN_RPAR TOKEN_DARROW TOKEN_LBLK TOKEN_RBLK  */
#line 519 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.expr) = ast_accept_func_expr(NULL, NULL);
			debug("expr: func");
		}
#line 2402 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 71: /* arg_list: expr  */
#line 525 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.arg_list) = ast_accept_arg_list(NULL, (yyvsp[0].expr));
			debug("arg_list: expr");
		}
#line 2411 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 72: /* arg_list: arg_list TOKEN_COMMA expr  */
#line 530 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.arg_list) = ast_accept_arg_list((yyvsp[-2].arg_list), (yyvsp[0].expr));
			debug("arg_list: arg_list arg");
		}
#line 2420 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 73: /* kv_list: kv  */
#line 536 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.kv_list) = ast_accept_kv_list(NULL, (yyvsp[0].kv));
			debug("kv_list: kv");
		}
#line 2429 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 74: /* kv_list: kv_list TOKEN_COMMA kv  */
#line 541 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.kv_list) = ast_accept_kv_list((yyvsp[-2].kv_list), (yyvsp[0].kv));
			debug("kv_list: kv_list kv");
		}
#line 2438 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 75: /* kv: TOKEN_STR TOKEN_COLON expr  */
#line 547 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.kv) = ast_accept_kv((yyvsp[-2].sval), (yyvsp[0].expr));
			debug("kv");
		}
#line 2447 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 76: /* kv: TOKEN_SYMBOL TOKEN_COLON expr  */
#line 552 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.kv) = ast_accept_kv((yyvsp[-2].sval), (yyvsp[0].expr));
			debug("kv");
		}
#line 2456 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 77: /* term: TOKEN_INT  */
#line 558 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.term) = ast_accept_int_term((yyvsp[0].ival));
			debug("term: int");
		}
#line 2465 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 78: /* term: TOKEN_FLOAT  */
#line 563 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.term) = ast_accept_float_term((float)(yyvsp[0].fval));
			debug("term: float");
		}
#line 2474 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 79: /* term: TOKEN_STR  */
#line 568 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.term) = ast_accept_str_term((yyvsp[0].sval));
			debug("term: string");
		}
#line 2483 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 80: /* term: TOKEN_SYMBOL  */
#line 573 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.term) = ast_accept_symbol_term((yyvsp[0].sval));
			debug("term: symbol");
		}
#line 2492 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 81: /* term: TOKEN_LARR TOKEN_RARR  */
#line 578 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.term) = ast_accept_empty_array_term();
			debug("term: empty array symbol");
		}
#line 2501 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;

  case 82: /* term: TOKEN_LBLK TOKEN_RBLK  */
#line 583 "/Users/tabata/src/NoctLang/src/parser.y"
                {
			(yyval.term) = ast_accept_empty_dict_term();
			debug("term: empty dict symbol");
		}
#line 2510 "/Users/tabata/src/NoctLang/build/parser.tab.c"
    break;


#line 2514 "/Users/tabata/src/NoctLang/build/parser.tab.c"

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

#line 588 "/Users/tabata/src/NoctLang/src/parser.y"


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
		strcpy(ast_error_message, N_TR(s));
	else
		strcpy(ast_error_message, "");
}
