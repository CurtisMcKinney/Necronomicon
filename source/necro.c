/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "necro.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

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
NecroStringSlice necro_create_string_slice(const char* str)
{
	int32_t length = 0;
	while (str[length])
	{
		length++;
	}
	return (NecroStringSlice) { str, length };
}

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
type##Vector necro_create_##snake_type##_vector()                                  \
{                                                                                  \
	return (type##Vector)                                                          \
	{                                                                              \
		(type*) malloc(NECRO_INTIAL_VECTOR_SIZE * sizeof(type)),                   \
		0,                                                                         \
		NECRO_INTIAL_VECTOR_SIZE                                                   \
	};                                                                             \
}                                                                                  \
                                                                                   \
void necro_destroy_##snake_type##_vector(type##Vector* vec)                        \
{                                                                                  \
	vec->length   = 0;                                                             \
	vec->capacity = 0;                                                             \
	free(vec->data);                                                               \
	vec->data     = NULL;                                                          \
}                                                                                  \
                                                                                   \
void necro_push_##snake_type##_vector(type##Vector* vec, type* item)               \
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

const char* necro_lex_token_type_string(NECRO_LEX_TOKEN_TYPE token)
{
	switch(token)
	{
	case NECRO_LEX_ADD:              return "ADD";
	case NECRO_LEX_SUB:              return "SUB";
	case NECRO_LEX_MUL:              return "MUL";
	case NECRO_LEX_DIV:              return "DIV";
	case NECRO_LEX_MOD:              return "MOD";
	case NECRO_LEX_GT:               return "GT";
	case NECRO_LEX_LT:               return "LT";
	case NECRO_LEX_GTE:              return "GTE";
	case NECRO_LEX_LTE:              return "LTE";
	case NECRO_LEX_COLON:            return "COLON";
	case NECRO_LEX_SEMI_COLON:       return "SEMI_COLON";
	case NECRO_LEX_NUMERIC_LITERAL:  return "NUMBER";
	case NECRO_LEX_IDENTIFIER:       return "IDENTIFIER";
	case NECRO_LEX_RIGHT_ARROW:      return "RIGHT_ARROW";
	case NECRO_LEX_LEFT_BRACKET:     return "LEFT_BRACKET";
	case NECRO_LEX_RIGHT_BRACKET:    return "RIGHT_BRACKET";
	case NECRO_LEX_LEFT_PAREN:       return "LEFT_PAREN";
	case NECRO_LEX_RIGHT_PAREN:      return "RIGHT_PAREN";
	case NECRO_LEX_LEFT_BRACE:       return "LEFT_BRACE";
	case NECRO_LEX_RIGHT_BRACE:      return "RIGHT_BRACE";
	case NECRO_LEX_COMMA:            return "COMMA";
	case NECRO_LEX_UNDER_SCORE:      return "UNDER_SCORE";
	case NECRO_LEX_EQUALS:           return "EQUALS";
	case NECRO_LEX_QUESTION_MARK:    return "QUESTION_MARK";
	case NECRO_LEX_EXCLAMATION:      return "EXCLAMATION";
	case NECRO_LEX_QUOTE:            return "QUOTE";
	case NECRO_LEX_DOUBLE_QUOTE:     return "DOUBLE_QUOTE";
	case NECRO_LEX_HASH:             return "HASH";
	case NECRO_LEX_INDENT:           return "INDENT";
	case NECRO_LEX_DEDENT:           return "DEDENT";
	case NECRO_LEX_NEW_LINE:         return "NEW_LINE";
	default:                         return "????";
	}
}

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

NecroLexState necro_create_lex_state(const char* str)
{
	return (NecroLexState)
	{
		0,
		0,
		0,
		str,
		necro_create_lex_token_vector()
	};
}

void necro_destroy_lex_state(NecroLexState* lex_state)
{
	lex_state->character_number = 0;
	lex_state->line_number      = 0;
	lex_state->pos              = 0;
	necro_destroy_lex_token_vector(&lex_state->tokens);
}

void necro_print_lex_state(NecroLexState* lex_state)
{
	printf("NecroLexState\n{\n");
	printf("    line_number:      %d,\n", lex_state->line_number);
	printf("    character_number: %d,\n", lex_state->character_number);
	printf("    tokens:\n    [\n");
	for (int32_t i = 0; i < lex_state->tokens.length; ++i)
	{
		printf("        %s\n", necro_lex_token_type_string(lex_state->tokens.data[i].token));
	}
	printf("    ]\n");
	printf("}\n\n");
}

void necro_add_single_character_token(NecroLexState* lex_state, NECRO_LEX_TOKEN_TYPE token)
{
	NecroLexToken lex_token = { lex_state->character_number, lex_state->line_number, 0, token };
	necro_push_lex_token_vector(&lex_state->tokens, &lex_token);
	if (token == NECRO_LEX_NEW_LINE)
	{
		lex_state->character_number = 0;
		lex_state->line_number++;
	}
	else
	{
		lex_state->character_number++;
	}
	lex_state->pos++;
}

bool necro_lex_single_character(NecroLexState* lex_state)
{
	if (!lex_state->str[lex_state->pos])
		return false;
	switch (lex_state->str[lex_state->pos])
	{
	case '+':  necro_add_single_character_token(lex_state, NECRO_LEX_ADD);           break;
	case '-':  necro_add_single_character_token(lex_state, NECRO_LEX_SUB);           break;
	case '*':  necro_add_single_character_token(lex_state, NECRO_LEX_MUL);           break;
	case '/':  necro_add_single_character_token(lex_state, NECRO_LEX_DIV);           break;
	case '%':  necro_add_single_character_token(lex_state, NECRO_LEX_MOD);           break;
	case '<':  necro_add_single_character_token(lex_state, NECRO_LEX_LT);            break;
	case '>':  necro_add_single_character_token(lex_state, NECRO_LEX_GT);            break;
	case ':':  necro_add_single_character_token(lex_state, NECRO_LEX_COLON);         break;
	case ';':  necro_add_single_character_token(lex_state, NECRO_LEX_SEMI_COLON);    break;
	case '[':  necro_add_single_character_token(lex_state, NECRO_LEX_LEFT_BRACKET);  break;
	case ']':  necro_add_single_character_token(lex_state, NECRO_LEX_RIGHT_BRACKET); break;
	case '(':  necro_add_single_character_token(lex_state, NECRO_LEX_LEFT_PAREN);    break;
	case ')':  necro_add_single_character_token(lex_state, NECRO_LEX_RIGHT_PAREN);   break;
	case '{':  necro_add_single_character_token(lex_state, NECRO_LEX_LEFT_BRACE);    break;
	case '}':  necro_add_single_character_token(lex_state, NECRO_LEX_RIGHT_BRACE);   break;
	case ',':  necro_add_single_character_token(lex_state, NECRO_LEX_COMMA);         break;
	case '_':  necro_add_single_character_token(lex_state, NECRO_LEX_UNDER_SCORE);   break;
	case '=':  necro_add_single_character_token(lex_state, NECRO_LEX_EQUALS);        break;
	case '?':  necro_add_single_character_token(lex_state, NECRO_LEX_QUESTION_MARK); break;
	case '!':  necro_add_single_character_token(lex_state, NECRO_LEX_EXCLAMATION);   break;
	case '#':  necro_add_single_character_token(lex_state, NECRO_LEX_HASH);          break;
	case '\'': necro_add_single_character_token(lex_state, NECRO_LEX_QUOTE);         break;
	case '\"': necro_add_single_character_token(lex_state, NECRO_LEX_DOUBLE_QUOTE);  break;
	case '\n': necro_add_single_character_token(lex_state, NECRO_LEX_NEW_LINE);      break;
	default:   return false;
	}
	return true;
}

bool necro_lex_token_with_pattern(NecroLexState* lex_state, const char* pattern, NECRO_LEX_TOKEN_TYPE token_type)
{
	int32_t i = 0;
	while (pattern[i])
	{
		if (lex_state->str[lex_state->pos + i] != pattern[i])
		{
			return false;
		}
		i++;
	}
	NecroLexToken lex_token = { lex_state->character_number, lex_state->line_number, 0, token_type };
	necro_push_lex_token_vector(&lex_state->tokens, &lex_token);
	lex_state->character_number += i + 1;
	lex_state->pos              += i + 1;
	return true;
}

bool necro_lex_multi_character_token(NecroLexState* lex_state)
{
	return necro_lex_token_with_pattern(lex_state, "<=", NECRO_LEX_LTE) ||
		   necro_lex_token_with_pattern(lex_state, ">=", NECRO_LEX_GTE) ||
		   necro_lex_token_with_pattern(lex_state, "->", NECRO_LEX_RIGHT_ARROW);
}

void necro_lex(NecroLexState* lex_state)
{
	while (lex_state->str[lex_state->pos])
	{
		bool matched =
			necro_lex_multi_character_token(lex_state) ||
			necro_lex_single_character(lex_state);
		if (!matched)
		{
			// We reached a character we don't know how to parse
			printf("Unrecognized character: %c, found at line number: %d, character number: %d\n", lex_state->str[lex_state->pos], lex_state->line_number, lex_state->character_number + 1);
			exit(1);
		}
	}
}

void necro_test_lex()
{
	const char*   input_string = "+-*/\n!?-><=>=";
	NecroLexState lex_state    = necro_create_lex_state(input_string);
	necro_lex(&lex_state);
	printf("input_string:\n%s\n\n", input_string);
	necro_print_lex_state(&lex_state);
	necro_destroy_lex_state(&lex_state);
}

//=====================================================
// Parsing
//=====================================================

typedef enum
{
	LAMBDA_AST_VARIABLE,
	LAMBDA_AST_CONSTANT,
	LAMBDA_AST_APPLY,
	LAMBDA_AST_LAMBDA
} LAMBDA_AST;


//=====================================================
// TypeChecking
//=====================================================


//=====================================================
// Main
//=====================================================
int main(int argc, char** argv)
{
	necro_test_lex();
	return 0;
}

