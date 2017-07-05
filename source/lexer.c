/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
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
	default:                         return "UNRECOGNIZED TOKEN";
	}
}

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
	case '=':  necro_add_single_character_token(lex_state, NECRO_LEX_ASSIGN);        break;
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
	lex_state->character_number += i;
	lex_state->pos              += i;
	return true;
}

bool necro_lex_multi_character_token(NecroLexState* lex_state)
{
	return necro_lex_token_with_pattern(lex_state, "==", NECRO_LEX_EQUALS) ||
		   necro_lex_token_with_pattern(lex_state, "<=", NECRO_LEX_LTE)    ||
		   necro_lex_token_with_pattern(lex_state, ">=", NECRO_LEX_GTE)    ||
		   necro_lex_token_with_pattern(lex_state, "->", NECRO_LEX_RIGHT_ARROW);
}

bool necro_lex_integer(NecroLexState* lex_state)
{
	char*   new_str_pos     = (char*) lex_state->str;
	int32_t integer_literal = strtol(lex_state->str, &new_str_pos, 10);
	if (new_str_pos == lex_state->str)
		return false;
	int32_t       count     = (uint64_t)new_str_pos - (uint64_t)lex_state->str;
	NecroLexToken lex_token = { lex_state->character_number, lex_state->line_number, 0, NECRO_LEX_INTEGER_LITERAL };
	lex_token.int_literal   = integer_literal;
	necro_push_lex_token_vector(&lex_state->tokens, &lex_token);
	lex_state->character_number += count;
	lex_state->pos              += count;
	return true;
}

void necro_lex(NecroLexState* lex_state)
{
	while (lex_state->str[lex_state->pos])
	{
		bool matched =
			// necro_lex_integer(lex_state)               ||
			necro_lex_multi_character_token(lex_state) ||
			necro_lex_single_character(lex_state);
		if (!matched)
		{
			// We reached a character we don't know how to parse
			printf("Unrecognized character: %c, found at line number: %d, character number: %d\n", lex_state->str[lex_state->pos], lex_state->line_number, lex_state->character_number + 1);
			break;
		}
	}
}
