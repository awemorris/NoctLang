%{
/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

#include "parser.tab.h"

#ifdef _MSC_VER
#define strdup _strdup
#define fileno _fileno
#endif

#define YY_NO_INPUT
#define YY_NO_UNPUT

extern YYLTYPE yylloc;
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
[0-9]*\.[0-9]+	{
			sscanf(yytext, "%lf", &ast_yylval.fval);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_FLOAT;
		}
[0-9]*		{
			sscanf(yytext, "%d", &ast_yylval.ival);
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_INT;
		}
["]([^"\\\n]|\\(.|\n))*["] {
			ast_yylval.sval = strdup(yytext + 1);
			ast_yylval.sval[yyleng - 2] = '\0';
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
"="		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ASSIGN;
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
			return TOKEN_AND;
		}
"||"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_OR;
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
"->"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ARROW;
		}
"=>"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_DARROW;
		}
"func"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_FUNC;
		}
"lambda"	{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_LAMBDA;
		}
"("		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_LPAR;
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
"if"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_IF;
		}
"else"		{
			ast_yylloc.first_line = ast_yylloc.last_line;
			ast_yylloc.first_column = ast_yylloc.last_column + 1;
			ast_yylloc.last_column += yyleng;
			return TOKEN_ELSE;
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
[a-zA-Z_0-9]+	{
			ast_yylval.sval = strdup(yytext);
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
