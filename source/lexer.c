/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <ctype.h>
#include "lexer.h"

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
	case NECRO_LEX_INTEGER_LITERAL:  return "INT";
	case NECRO_LEX_FLOAT_LITERAL:    return "FLOAT";
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
	case NECRO_LEX_ASSIGN:           return "ASSIGN";
	case NECRO_LEX_QUESTION_MARK:    return "QUESTION_MARK";
	case NECRO_LEX_EXCLAMATION:      return "EXCLAMATION";
	case NECRO_LEX_QUOTE:            return "QUOTE";
	case NECRO_LEX_DOUBLE_QUOTE:     return "DOUBLE_QUOTE";
	case NECRO_LEX_HASH:             return "HASH";
	case NECRO_LEX_INDENT:           return "INDENT";
	case NECRO_LEX_DEDENT:           return "DEDENT";
	case NECRO_LEX_NEW_LINE:         return "NEW_LINE";
	case NECRO_LEX_DOUBLE_COLON:     return "DOUBLE_COLON";
	case NECRO_LEX_LEFT_SHIFT:       return "LEFT_SHIFT";
	case NECRO_LEX_RIGHT_SHIFT:      return "RIGHT_SHIFT";
	default:                         return "UNRECOGNIZED TOKEN";
	}
}

NecroLexer necro_create_lexer(const char* str)
{
	return (NecroLexer)
	{
		0,
		0,
		0,
		str,
		necro_create_lex_token_vector()
	};
}

void necro_destroy_lexer(NecroLexer* lexer)
{
	lexer->character_number = 0;
	lexer->line_number      = 0;
	lexer->pos              = 0;
	necro_destroy_lex_token_vector(&lexer->tokens);
}

void necro_print_lexer(NecroLexer* lexer)
{
	printf("NecroLexer\n{\n");
	printf("    line_number:      %d,\n", lexer->line_number);
	printf("    character_number: %d,\n", lexer->character_number);
	printf("    tokens:\n    [\n");
	for (size_t i = 0; i < lexer->tokens.length; ++i)
	{
		printf("        %s\n", necro_lex_token_type_string(lexer->tokens.data[i].token));
	}
	printf("    ]\n");
	printf("}\n\n");
}

void necro_add_single_character_token(NecroLexer* lexer, NECRO_LEX_TOKEN_TYPE token)
{
	NecroLexToken lex_token = { lexer->character_number, lexer->line_number, 0, token };
	necro_push_lex_token_vector(&lexer->tokens, &lex_token);
	if (token == NECRO_LEX_NEW_LINE)
	{
		lexer->character_number = 0;
		lexer->line_number++;
	}
	else
	{
		lexer->character_number++;
	}
	lexer->pos++;
}

bool necro_lex_single_character(NecroLexer* lexer)
{
	if (!lexer->str[lexer->pos])
		return false;
	switch (lexer->str[lexer->pos])
	{
	case '+':  necro_add_single_character_token(lexer, NECRO_LEX_ADD);           break;
	case '-':  necro_add_single_character_token(lexer, NECRO_LEX_SUB);           break;
	case '*':  necro_add_single_character_token(lexer, NECRO_LEX_MUL);           break;
	case '/':  necro_add_single_character_token(lexer, NECRO_LEX_DIV);           break;
	case '%':  necro_add_single_character_token(lexer, NECRO_LEX_MOD);           break;
	case '<':  necro_add_single_character_token(lexer, NECRO_LEX_LT);            break;
	case '>':  necro_add_single_character_token(lexer, NECRO_LEX_GT);            break;
	case ':':  necro_add_single_character_token(lexer, NECRO_LEX_COLON);         break;
	case ';':  necro_add_single_character_token(lexer, NECRO_LEX_SEMI_COLON);    break;
	case '[':  necro_add_single_character_token(lexer, NECRO_LEX_LEFT_BRACKET);  break;
	case ']':  necro_add_single_character_token(lexer, NECRO_LEX_RIGHT_BRACKET); break;
	case '(':  necro_add_single_character_token(lexer, NECRO_LEX_LEFT_PAREN);    break;
	case ')':  necro_add_single_character_token(lexer, NECRO_LEX_RIGHT_PAREN);   break;
	case '{':  necro_add_single_character_token(lexer, NECRO_LEX_LEFT_BRACE);    break;
	case '}':  necro_add_single_character_token(lexer, NECRO_LEX_RIGHT_BRACE);   break;
	case ',':  necro_add_single_character_token(lexer, NECRO_LEX_COMMA);         break;
	case '_':  necro_add_single_character_token(lexer, NECRO_LEX_UNDER_SCORE);   break;
	case '=':  necro_add_single_character_token(lexer, NECRO_LEX_ASSIGN);        break;
	case '?':  necro_add_single_character_token(lexer, NECRO_LEX_QUESTION_MARK); break;
	case '!':  necro_add_single_character_token(lexer, NECRO_LEX_EXCLAMATION);   break;
	case '#':  necro_add_single_character_token(lexer, NECRO_LEX_HASH);          break;
	case '\'': necro_add_single_character_token(lexer, NECRO_LEX_QUOTE);         break;
	case '\"': necro_add_single_character_token(lexer, NECRO_LEX_DOUBLE_QUOTE);  break;
	case '\n': necro_add_single_character_token(lexer, NECRO_LEX_NEW_LINE);      break;
	default:   return false;
	}
	return true;
}

bool necro_lex_token_with_pattern(NecroLexer* lexer, const char* pattern, NECRO_LEX_TOKEN_TYPE token_type)
{
	size_t i = 0;
	while (pattern[i])
	{
		if (lexer->str[lexer->pos + i] != pattern[i])
		{
			return false;
		}
		i++;
	}
	NecroLexToken lex_token = { lexer->character_number, lexer->line_number, 0, token_type };
	necro_push_lex_token_vector(&lexer->tokens, &lex_token);
	lexer->character_number += i;
	lexer->pos              += i;
	return true;
}

bool necro_lex_multi_character_token(NecroLexer* lexer)
{
	return necro_lex_token_with_pattern(lexer, "==", NECRO_LEX_EQUALS)       ||
		   necro_lex_token_with_pattern(lexer, "<=", NECRO_LEX_LTE)          ||
		   necro_lex_token_with_pattern(lexer, ">=", NECRO_LEX_GTE)          ||
		   necro_lex_token_with_pattern(lexer, "::", NECRO_LEX_DOUBLE_COLON) ||
		   necro_lex_token_with_pattern(lexer, ">>", NECRO_LEX_RIGHT_SHIFT)  ||
		   necro_lex_token_with_pattern(lexer, "<<", NECRO_LEX_LEFT_SHIFT)   ||
		   necro_lex_token_with_pattern(lexer, "->", NECRO_LEX_RIGHT_ARROW);
}

bool necro_lex_integer(NecroLexer* lexer)
{
	char*   start_str_pos   = (char*)(lexer->str + lexer->pos);
	char*   new_str_pos     = start_str_pos;
	int64_t integer_literal = strtol(start_str_pos, &new_str_pos, 10);
	size_t  count           = (size_t)(new_str_pos - start_str_pos);
	// printf("pos: %d, start: %p, end: %p, count: %d, int: %d\n", lexer->pos, start_str_pos, new_str_pos, count, integer_literal);
	if (count <= 0)
		return false;
	NecroLexToken lex_token = { lexer->character_number, lexer->line_number, 0, NECRO_LEX_INTEGER_LITERAL };
	lex_token.int_literal   = integer_literal;
	necro_push_lex_token_vector(&lexer->tokens, &lex_token);
	lexer->character_number += count;
	lexer->pos              += count;
	return true;
}

bool necro_lex_float(NecroLexer* lexer)
{
	char*  start_str_pos   = (char*)(lexer->str + lexer->pos);
	char*  new_str_pos     = start_str_pos;
	double float_value     = strtof(start_str_pos, &new_str_pos);
	size_t count           = (size_t)(new_str_pos - start_str_pos);
	// printf("pos: %d, start: %p, end: %p, count: %d, int: %d\n", lexer->pos, start_str_pos, new_str_pos, count, integer_literal);
	if (count <= 0)
		return false;
	NecroLexToken lex_token  = { lexer->character_number, lexer->line_number, 0, NECRO_LEX_FLOAT_LITERAL };
	lex_token.double_literal = float_value;
	necro_push_lex_token_vector(&lexer->tokens, &lex_token);
	lexer->character_number += count;
	lexer->pos              += count;
	return true;
}

bool necro_lex_number(NecroLexer* lexer)
{
	// Must start with digit or with minus symbol
	bool        contains_dot = false;
	const char* current_char = lexer->str + lexer->pos;
	if (*current_char == '-')
		current_char++;
	if (!isdigit(*current_char))
		return false;
	while (isdigit(*current_char) || *current_char == '.')
	{
		if (!contains_dot && *current_char == '.')
			contains_dot = true;
		else if (contains_dot && *current_char == '.')
			break;
		current_char++;
	}
	if (contains_dot)
		return necro_lex_float(lexer);
	else
		return necro_lex_integer(lexer);
}

// bool necro_lex_identifier(NecroLexer* lexer)
// {
// 	if (lexer->str[lexer->pos] != )
// 	size_t identifier_lenth = 0;
// }

void necro_lex(NecroLexer* lexer)
{
	while (lexer->str[lexer->pos])
	{
		bool matched =
			necro_lex_number(lexer)                ||
			necro_lex_multi_character_token(lexer) ||
			necro_lex_single_character(lexer);
		if (!matched)
		{
			// We reached a character we don't know how to parse
			printf("Unrecognized character: %c, found at line number: %d, character number: %d\n", lexer->str[lexer->pos], lexer->line_number, lexer->character_number + 1);
			break;
		}
	}
}