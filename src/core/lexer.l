%{
/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#include <noct/c89compat.h>
#include "parser.tab.h"

#ifdef _MSC_VER
#define strdup _strdup
#define fileno _fileno
#endif

#define YY_NO_INPUT
#define YY_NO_UNPUT
#define YY_NO_UNISTD_H

#define malloc   ast_malloc
#define realloc  ast_realloc
#define free     ast_free

void *ast_malloc(size_t size);
void *ast_realloc(void *p, size_t size);
char *ast_strdup(const char *s);
void ast_free(void *p);

extern YYLTYPE yylloc;

/*
 * Decode the escape sequences of a string literal in place.
 *
 * "s" holds the literal body without the surrounding quotes. The
 * decoded text is never longer than the source, so decoding in place
 * is safe. An unknown escape yields the escaped character itself.
 */
static void
lexer_unescape(char *s)
{
	char *r, *w;

	r = s;
	w = s;
	while (*r != '\0') {
		if (*r != '\\') {
			*w++ = *r++;
			continue;
		}

		r++;
		switch (*r) {
		case '\0':
			/* A trailing backslash. Keep it verbatim. */
			*w++ = '\\';
			continue;
		case 'n':	*w++ = '\n'; r++; break;
		case 'r':	*w++ = '\r'; r++; break;
		case 't':	*w++ = '\t'; r++; break;
		case 'b':	*w++ = '\b'; r++; break;
		case 'f':	*w++ = '\f'; r++; break;
		case 'v':	*w++ = '\v'; r++; break;
		case 'a':	*w++ = '\a'; r++; break;
		case '0':	*w++ = '\0'; r++; break;
		case '\n':
			/* A line continuation produces nothing. */
			r++;
			break;
		default:
			/* Covers \\ , \" and any other escaped character. */
			*w++ = *r++;
			break;
		}
	}
	*w = '\0';
}
%}

%option reentrant
%option never-interactive

%%
\/\/.*\n 	{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_line++;
			ast_yylloc.last_column = 0;
		}
[0-9]*\.[0-9]+lf {
                        ast_yylval.lfval = atof(yytext);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_DOUBLE;
		}
[0-9]*\.[0-9]+f {
                        ast_yylval.fval = (float)atof(yytext);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_FLOAT;
		}
[0-9]*\.[0-9]+	{
                        ast_yylval.fval = (float)atof(yytext);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_FLOAT;
		}
[0-9]+[lL]      {
			ast_yylval.lval = atoll(yytext);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_LONG;
		}
[0-9]*		{
                        int64_t v = atoll(yytext);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
                        ast_yylloc.last_column += yyleng;
                        if (v > INT_MAX) {
                                ast_yylval.lval = v;
                                return TOKEN_LONG;
                        }
                        ast_yylval.ival = (int)(uint64_t)v;
 			return TOKEN_INT;
		}
0x[0-9a-fA-F]{1,16}l {
                        ast_yylval.lval = atoll(yytext);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_LONG;
		}
0x[0-9a-fA-F]{1,8} {
			sscanf(yytext, "%x", &ast_yylval.ival);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_INT;
		}
["]([^"\\\n]|\\(.|\n))*["] {
			ast_yylval.sval = ast_strdup(yytext + 1);
			ast_yylval.sval[yyleng - 2] = '\0';
			lexer_unescape(ast_yylval.sval);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_STR;
		}
"+"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_PLUS;
		}
"-"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_MINUS;
		}
"*"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_MUL;
		}
"/"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_DIV;
		}
"%"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_MOD;
		}
"<<"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_SHL;
		}
"<<<"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_GPU_LAUNCH_L;
		}
">>"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_SHR;
		}
">>>"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_GPU_LAUNCH_R;
		}
"="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ASSIGN;
		}
"+="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_PLUSASSIGN;
		}
"-="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_MINUSASSIGN;
		}
"*="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_MULASSIGN;
		}
"/="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_DIVASSIGN;
		}
"%="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_MODASSIGN;
		}
"&="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ANDASSIGN;
		}
"|="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ORASSIGN;
		}
"<<="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_SHLASSIGN;
		}
">>="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_SHRASSIGN;
		}
">"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_GT;
		}
">="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_GTE;
		}
"<"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_LT;
		}
"<="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_LTE;
		}
"=="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_EQ;
		}
"!="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_NEQ;
		}
"&&"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ANDAND;
		}
"||"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_OROR;
		}
"&"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_AND;
		}
"|"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_OR;
		}
"^"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_XOR;
		}
"!"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_NOT;
		}
"++"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_PLUSPLUS;
		}
"--"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_MINUSMINUS;
		}
";"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_SEMICOLON;
		}
":"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_COLON;
		}
"\."		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_DOT;
		}
")"[ \t\r\n]*"=>"[ \t\r\n]*"{" {
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_RPAR_DARROW_LBLK;
		}
"func"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_FUNC;
		}
"static"	{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_STATIC;
		}
"inline"	{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_INLINE;
		}
"__accel"	{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_DUNDER_ACCEL;
		}
"__gpu"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_DUNDER_GPU;
		}
"__fast"	{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_DUNDER_FAST;
		}
"accel"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ACCEL;
		}
"gpu"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_GPU;
		}
"require"	{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_REQUIRE;
		}
"new"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_NEW;
		}
"extend"	{
			/* extend: like new, but the result is frozen. */
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_EXTEND;
		}
"class"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_CLASS;
		}
"("		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_LPAR;
		}
")"[ \t\r\n]*"{" {
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_RPAR_LBLK;
		}
")"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_RPAR;
		}
"{"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_LBLK;
		}
"}"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_RBLK;
		}
"["		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_LARR;
		}
"]"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_RARR;
		}
"else"[ \t\r\n]+"if" {
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ELSEIF;
		}
"else"[ \t\r\n]+"{" {
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ELSE_LBLK;
		}
"else"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ELSE;
		}
"if"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_IF;
		}
"while"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_WHILE;
		}
"for"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_FOR;
		}
","		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_COMMA;
		}
"in"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_IN;
		}
".."		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_DOTDOT;
		}
"return"	{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_RETURN;
		}
"break"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_BREAK;
		}
"continue"	{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_CONTINUE;
		}
"let"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_LET;
		}
"__shared"	{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_SHARED;
		}
"var"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_VAR;
		}
[ \t\r]		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
		}
"\n"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_line++;
			ast_yylloc.last_column = 0;
		}
"null"		{
			ast_yylval.ival = 0;
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_INT;
		}
"true"		{
			ast_yylval.ival = 1;
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_INT;
		}
"false"		{
			ast_yylval.ival = 0;
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_INT;
		}
[a-zA-Z_0-9]+	{
			ast_yylval.sval = ast_strdup(yytext);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_SYMBOL;
		}
%%

int ast_yywrap(yyscan_t scanner)
{
	(void)scanner;
	return 1;
}
