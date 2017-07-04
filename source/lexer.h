/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef LEXER_H
#define LEXER_H 1

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

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
// NecroStringSlice
//=====================================================
typedef struct
{
	const char* data;
	int32_t     length;
} NecroStringSlice;

// Takes null-terminated const char* and converts it to a NecroStringSlice
NecroStringSlice necro_create_string_slice(const char* str);

//=====================================================
// NecroVector
//=====================================================
#define NECRO_INTIAL_VECTOR_SIZE 512
#define NECRO_DECLARE_VECTOR(type, snake_type)                                     \
typedef struct                                                                     \
{                                                                                  \
	type*   data;                                                                  \
	int32_t length;                                                                \
	int32_t capacity;                                                              \
} type##Vector;                                                                    \
                                                                                   \
static type##Vector necro_create_##snake_type##_vector()                           \
{                                                                                  \
	return (type##Vector)                                                          \
	{                                                                              \
		(type*) malloc(NECRO_INTIAL_VECTOR_SIZE * sizeof(type)),                   \
		0,                                                                         \
		NECRO_INTIAL_VECTOR_SIZE                                                   \
	};                                                                             \
}                                                                                  \
                                                                                   \
static void necro_destroy_##snake_type##_vector(type##Vector* vec)                 \
{                                                                                  \
	vec->length   = 0;                                                             \
	vec->capacity = 0;                                                             \
	free(vec->data);                                                               \
	vec->data     = NULL;                                                          \
}                                                                                  \
                                                                                   \
static void necro_push_##snake_type##_vector(type##Vector* vec, type* item)        \
{                                                                                  \
	if (vec->length >= vec->capacity)                                              \
	{                                                                              \
		vec->capacity = vec->capacity * 2;                                         \
		vec->data     = (type*) realloc(vec->data, vec->capacity * sizeof(type));  \
	}                                                                              \
	assert(vec->data != NULL);                                                     \
	vec->data[vec->length] = *item;                                                \
	vec->length++;                                                                 \
}

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
    NECRO_LEX_NUMERIC_LITERAL,
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
    NECRO_LEX_EQUALS,
    NECRO_LEX_QUESTION_MARK,
    NECRO_LEX_EXCLAMATION,
    NECRO_LEX_QUOTE,
    NECRO_LEX_DOUBLE_QUOTE,
    NECRO_LEX_HASH,
    NECRO_LEX_INDENT,
    NECRO_LEX_DEDENT,
    NECRO_LEX_NEW_LINE
} NECRO_LEX_TOKEN_TYPE;

const char* necro_lex_token_type_string(NECRO_LEX_TOKEN_TYPE token);

typedef struct
{
	int32_t character_number;
	int32_t line_number;
	union
	{
		int64_t          int_literal;
		double           double_literal;
		NecroStringSlice str;
		bool             boolean_literal;
	};
	NECRO_LEX_TOKEN_TYPE token;
} NecroLexToken;
NECRO_DECLARE_VECTOR(NecroLexToken, lex_token)

// Need Indent level!
typedef struct
{
	int32_t             character_number;
	int32_t             line_number;
	int32_t             pos;
	const char*         str;
	NecroLexTokenVector tokens;
} NecroLexState;

NecroLexState necro_create_lex_state(const char* str);
void necro_destroy_lex_state(NecroLexState* lex_state);
void necro_print_lex_state(NecroLexState* lex_state);
void necro_add_single_character_token(NecroLexState* lex_state, NECRO_LEX_TOKEN_TYPE token);
bool necro_lex_single_character(NecroLexState* lex_state);
bool necro_lex_token_with_pattern(NecroLexState* lex_state, const char* pattern, NECRO_LEX_TOKEN_TYPE token_type);
bool necro_lex_multi_character_token(NecroLexState* lex_state);
void necro_lex(NecroLexState* lex_state);

#endif // LEXER_H
