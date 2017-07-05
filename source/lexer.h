/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef LEXER_H
#define LEXER_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "utility.h"

//=====================================================
// Theoretical Necro Runtime structs
//=====================================================
typedef enum
{
	NECRO_OBJECT_NULL,
	NECRO_OBJECT_BOOL,
	NECRO_OBJECT_FLOAT,
	NECRO_OBJECT_PAIR
} NECRO_OBJECT_TYPE;

typedef struct NecroObject NecroObject;

typedef struct
{
	float        value;
	NecroObject* next;
} NecroFloat;

typedef struct
{
	bool         value;
	NecroObject* next;
} NecroBool;

typedef struct
{
	NecroObject* first;
	NecroObject* second;
} NecroPair;

struct NecroObject
{
	union
	{
		NecroFloat necro_float;
		NecroBool  necro_bool;
	};
	uint32_t          ref_count;
	NECRO_OBJECT_TYPE type;
};

//=====================================================
// Lexing
//=====================================================
typedef enum
{
	NECRO_LEX_ADD,
    NECRO_LEX_SUB,
    NECRO_LEX_MUL,
    NECRO_LEX_DIV,
    NECRO_LEX_MOD,
    NECRO_LEX_GT,
    NECRO_LEX_LT,
    NECRO_LEX_GTE,
    NECRO_LEX_LTE,
    NECRO_LEX_COLON,
    NECRO_LEX_SEMI_COLON,
    NECRO_LEX_INTEGER_LITERAL,
    NECRO_LEX_FLOAT_LITERAL,
    NECRO_LEX_IDENTIFIER,
    NECRO_LEX_RIGHT_ARROW,
    NECRO_LEX_LEFT_BRACKET,
    NECRO_LEX_RIGHT_BRACKET,
    NECRO_LEX_LEFT_PAREN,
    NECRO_LEX_RIGHT_PAREN,
    NECRO_LEX_LEFT_BRACE,
    NECRO_LEX_RIGHT_BRACE,
    NECRO_LEX_COMMA,
    NECRO_LEX_UNDER_SCORE,
	NECRO_LEX_ASSIGN,
    NECRO_LEX_EQUALS,
    NECRO_LEX_QUESTION_MARK,
    NECRO_LEX_EXCLAMATION,
    NECRO_LEX_QUOTE,
    NECRO_LEX_DOUBLE_QUOTE,
    NECRO_LEX_HASH,
    NECRO_LEX_INDENT,
    NECRO_LEX_DEDENT,
    NECRO_LEX_NEW_LINE,
	NECRO_LEX_DOUBLE_COLON,
	NECRO_LEX_LEFT_SHIFT,
	NECRO_LEX_RIGHT_SHIFT
} NECRO_LEX_TOKEN_TYPE;

typedef struct
{
	union
	{
		int64_t          int_literal;
		double           double_literal;
		size_t           intern_symbol;
		bool             boolean_literal;
	};
	size_t               character_number;
	size_t               line_number;
	NECRO_LEX_TOKEN_TYPE token;
} NecroLexToken;
NECRO_DECLARE_VECTOR(NecroLexToken, NecroLexToken, lex_token)

// TODO: Need Indent level!
typedef struct
{
	size_t              character_number;
	size_t              line_number;
	size_t              pos;
	const char*         str;
	NecroLexTokenVector tokens;
} NecroLexer;

const char* necro_lex_token_type_string(NECRO_LEX_TOKEN_TYPE token);
NecroLexer  necro_create_lexer(const char* str);
void        necro_destroy_lexer(NecroLexer* lexer);
void        necro_print_lexer(NecroLexer* lexer);
void        necro_add_single_character_token(NecroLexer* lexer, NECRO_LEX_TOKEN_TYPE token);
bool        necro_lex_single_character(NecroLexer* lexer);
bool        necro_lex_token_with_pattern(NecroLexer* lexer, const char* pattern, NECRO_LEX_TOKEN_TYPE token_type);
bool        necro_lex_multi_character_token(NecroLexer* lexer);
void        necro_lex(NecroLexer* lexer);

#endif // LEXER_H
