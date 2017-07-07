/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <ctype.h>
#include "lexer.h"

NecroLexer necro_create_lexer(const char* str)
{
	return (NecroLexer)
	{
		0,
		0,
		0,
		{ 0 },
		0,
		str,
		necro_create_lex_token_vector(),
		necro_create_intern()
	};
}

void necro_destroy_lexer(NecroLexer* lexer)
{
	lexer->character_number = 0;
	lexer->line_number      = 0;
	lexer->pos              = 0;
	necro_destroy_lex_token_vector(&lexer->tokens);
	necro_destroy_intern(&lexer->intern);
}

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
	case NECRO_LEX_DOUBLE_COLON:     return "DOUBLE_COLON";
	case NECRO_LEX_LEFT_SHIFT:       return "LEFT_SHIFT";
	case NECRO_LEX_RIGHT_SHIFT:      return "RIGHT_SHIFT";
	case NECRO_LEX_PIPE:             return "PIPE";
	case NECRO_LEX_FORWARD_PIPE:     return "FORWARD_PIPE";
	case NECRO_LEX_BACK_PIPE:        return "BACK_PIPE";
	case NECRO_LEX_DOT:              return "Dot";
	case NECRO_LEX_AMPERSAND:        return "AMPERSAND";
	case NECRO_LEX_AT:               return "AT";
	case NECRO_LEX_DOLLAR:           return "DOLLAR";
	case NECRO_LEX_CARET:            return "CARET";
	case NECRO_LEX_BACK_SLASH:       return "BACK_SLASH";
	case NECRO_LEX_TILDE:            return "TILDE";
	case NECRO_LEX_AND:              return "AND";
	case NECRO_LEX_OR:               return "OR";
	default:                         return "UNRECOGNIZED TOKEN";
	}
}

void necro_print_lex_token(NecroLexer* lexer, size_t token_id)
{
	if (lexer->tokens.data[token_id].token == NECRO_LEX_IDENTIFIER)
	{
		printf("IDENTIFIER: %s\n", necro_intern_get_string(&lexer->intern, lexer->tokens.data[token_id].symbol));
	}
	else if (lexer->tokens.data[token_id].token == NECRO_LEX_FLOAT_LITERAL)
	{
		printf("FLOAT:      %f\n", lexer->tokens.data[token_id].double_literal);
	}
	else if (lexer->tokens.data[token_id].token == NECRO_LEX_INTEGER_LITERAL)
	{
		printf("INT:        %lli\n", lexer->tokens.data[token_id].int_literal);
	}
	else
	{
		printf("%s\n", necro_lex_token_type_string(lexer->tokens.data[token_id].token));
	}
}

void necro_print_lexer(NecroLexer* lexer)
{
	printf("NecroLexer\n{\n");
	printf("    line_number:      %d,\n", lexer->line_number);
	printf("    character_number: %d,\n", lexer->character_number);
	printf("    tokens:\n    [\n");
	for (size_t i = 0; i < lexer->tokens.length; ++i)
	{
		printf("        ");
		necro_print_lex_token(lexer, i);
	}
	printf("    ]\n");
	printf("}\n\n");
}

void necro_add_single_character_token(NecroLexer* lexer, NECRO_LEX_TOKEN_TYPE token)
{
	NecroLexToken lex_token = { lexer->character_number, lexer->line_number, 0, token };
	necro_push_lex_token_vector(&lexer->tokens, &lex_token);
	lexer->pos++;
    lexer->character_number++;
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
	case '|':  necro_add_single_character_token(lexer, NECRO_LEX_PIPE);          break;
	case '.':  necro_add_single_character_token(lexer, NECRO_LEX_DOT);           break;
	case '@':  necro_add_single_character_token(lexer, NECRO_LEX_AT);            break;
	case '$':  necro_add_single_character_token(lexer, NECRO_LEX_DOLLAR);        break;
	case '&':  necro_add_single_character_token(lexer, NECRO_LEX_AMPERSAND);     break;
	case '^':  necro_add_single_character_token(lexer, NECRO_LEX_CARET);         break;
	case '\\': necro_add_single_character_token(lexer, NECRO_LEX_BACK_SLASH);    break;
	case '~':  necro_add_single_character_token(lexer, NECRO_LEX_TILDE);         break;
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
		   necro_lex_token_with_pattern(lexer, "<|", NECRO_LEX_FORWARD_PIPE) ||
		   necro_lex_token_with_pattern(lexer, "|>", NECRO_LEX_BACK_PIPE)    ||
		   necro_lex_token_with_pattern(lexer, "&&", NECRO_LEX_AND)          ||
		   necro_lex_token_with_pattern(lexer, "||", NECRO_LEX_OR)           ||
		   necro_lex_token_with_pattern(lexer, "->", NECRO_LEX_RIGHT_ARROW);
}

bool necro_lex_integer(NecroLexer* lexer)
{
	char*   start_str_pos   = (char*)(lexer->str + lexer->pos);
	char*   new_str_pos     = start_str_pos;
	int64_t integer_literal = strtol(start_str_pos, &new_str_pos, 10);
	size_t  count           = (size_t)(new_str_pos - start_str_pos);
	// printf("pos: %d, start: %p, end: %p, count: %d, int: %d\n", lexer->pos, start_str_pos, new_str_pos, count, integer_literal);
	if (count <= 0 || isalpha((uint8_t) (*new_str_pos)))
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
	if (count <= 0 || isalpha((uint8_t)(*new_str_pos)))
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
	if (!isdigit((uint8_t) (*current_char)))
		return false;
	while (isdigit((uint8_t)(*current_char)) || *current_char == '.')
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

bool necro_lex_identifier(NecroLexer* lexer)
{
	if (!isalpha((uint8_t) lexer->str[lexer->pos]))
		return false;
	size_t identifier_length = 0;
	while (isalnum((uint8_t)lexer->str[lexer->pos + identifier_length]))
	{
		identifier_length++;
	}
	if (identifier_length == 0)
		return false;
	NecroLexToken    lex_token = { lexer->character_number, lexer->line_number, 0, NECRO_LEX_IDENTIFIER };
	NecroStringSlice slice     = { lexer->str + lexer->pos, identifier_length };

	lex_token.symbol           = necro_intern_string_slice(&lexer->intern, slice);
	necro_push_lex_token_vector(&lexer->tokens, &lex_token);

	lexer->character_number += identifier_length;
	lexer->pos              += identifier_length;

	// If the identifier is a layout keyword, update indentation levels and add left brace
	if (slice.length == 3 && strncmp(slice.data, "let",   slice.length) == 0 ||
		slice.length == 5 && strncmp(slice.data, "where", slice.length) == 0 ||
		slice.length == 2 && strncmp(slice.data, "of",    slice.length) == 0 ||
		slice.length == 2 && strncmp(slice.data, "do",    slice.length) == 0)
	{
		while (lexer->str[lexer->pos] == ' ' || lexer->str[lexer->pos] == '\n')
		{
			if (lexer->str[lexer->pos] == '\n')
			{
				lexer->line_number++;
				lexer->character_number = 0;
			}
			else
			{
				lexer->character_number++;
			}
			lexer->pos++;
		}
		lexer->current_indentation_block++;
		lexer->block_indentation_levels[lexer->current_indentation_block] = lexer->character_number;
		NecroLexToken token = { lexer->character_number, lexer->line_number, 0, NECRO_LEX_LEFT_BRACE };
		necro_push_lex_token_vector(&lexer->tokens, &token);
	}

	return true;
}

bool necro_lex_whitespace(NecroLexer* lexer)
{
	// Tabs
	if (lexer->str[lexer->pos] == '\t')
	{
        lexer->pos++;
        lexer->character_number += 4;
        return true;
	}
	// Intermediate white space
	else if (lexer->str[lexer->pos] == ' ')
	{
		while (lexer->str[lexer->pos] == ' ')
		{
			lexer->pos++;
			lexer->character_number++;
		}
		return true;
	}
	// Newline
	else if (lexer->str[lexer->pos] == '\n')
	{
        lexer->pos++;
        lexer->line_number++;
        lexer->character_number = 0;

		while (lexer->str[lexer->pos] == ' ' || lexer->str[lexer->pos] == '\t')
		{
            if (lexer->str[lexer->pos] == '\t')
            {
                lexer->pos++;
                lexer->character_number += 4;
            }
            else
            {
                lexer->pos++;
                lexer->character_number++;
            }
		}

		// printf("newline, line_number: %d, character_number: %d, block_indentation_level: %d, indentation_block: %d\n", lexer->line_number, lexer->character_number, lexer->block_indentation_levels[lexer->current_indentation_block], lexer->current_indentation_block);

		// Indented at the same as block level, end of expression (add SemiColon)
		if (lexer->character_number <= lexer->block_indentation_levels[lexer->current_indentation_block])
		{
            bool prev_semi_colon = lexer->tokens.length > 0 && lexer->tokens.data[lexer->tokens.length - 1].token == NECRO_LEX_SEMI_COLON;
            if (!prev_semi_colon)
            {
                NecroLexToken token = { lexer->character_number, lexer->line_number, 0, NECRO_LEX_SEMI_COLON };
                necro_push_lex_token_vector(&lexer->tokens, &token);
            }
		}

		// Add right brace if we are further back than the current block
		while (lexer->character_number < lexer->block_indentation_levels[lexer->current_indentation_block] && lexer->current_indentation_block > 0)
		{
			NecroLexToken token = { lexer->character_number, lexer->line_number, 0, NECRO_LEX_RIGHT_BRACE };
			necro_push_lex_token_vector(&lexer->tokens, &token);
			lexer->current_indentation_block--;

		    if (lexer->character_number <= lexer->block_indentation_levels[lexer->current_indentation_block])
		    {
		    	NecroLexToken token = { lexer->character_number, lexer->line_number, 0, NECRO_LEX_SEMI_COLON };
		    	necro_push_lex_token_vector(&lexer->tokens, &token);
		    }
		}

		return true;
	}
	return false;
}

void necro_print_lex_error(NecroLexer* lexer, const char* error_message)
{
    // Find the length of the character sequence we were currently on.
	size_t sequence_length = 0;
	for (char* current_char = (char*)lexer->str + lexer->pos; !isblank((uint8_t)(*current_char)) && *current_char != '\0' && *current_char != '\n'; ++current_char)
		sequence_length++;

	// Find the beginning and end of the line we were current on.
	size_t line_start = lexer->pos;
	size_t line_end   = lexer->pos;
	for (line_start = lexer->pos; line_start > 0 && (lexer->str[line_start] != '\0' && lexer->str[line_start] != '\n'); --line_start);
    if (lexer->str[line_start] == '\n')
        line_start++;
	for (line_end = lexer->pos; lexer->str[line_end] != '\0' && lexer->str[line_end] != '\n'; ++line_end);
    if (lexer->str[line_end] == '\n')
        line_start--;

	fprintf(stderr, "=============================================\n");
	fprintf(stderr, "= SYNTAX ERROR: %s\n=    character sequence: ", error_message);
	fprintf(stderr, "%.*s", sequence_length, lexer->str + lexer->pos);
	fprintf(stderr, "\n=    line number:        %d,\n=    character number:   %d\n", lexer->line_number, lexer->character_number + 1);
	fprintf(stderr, "=    while parsing line:\n=      (line %d) %.*s\n", lexer->line_number, line_end - line_start, lexer->str + line_start);
	fprintf(stderr, "=============================================\n\n");
}

bool necro_lex_non_ascii(NecroLexer* lexer)
{
    // Lex control characters
    uint8_t c = lexer->str[lexer->pos];
    if (iscntrl(c) || c > 127)
    {
        printf("Found control character: %d, skipping...\n", c);
        lexer->pos++;
        lexer->character_number++;
        return true;
    }
    return false;
}

// TODO: Comment Lexing and String Lexing!
NECRO_LEX_RESULT necro_lex(NecroLexer* lexer)
{
	while (lexer->str[lexer->pos])
	{
		bool matched =
            necro_lex_whitespace(lexer)            ||
            necro_lex_non_ascii(lexer)             ||
			necro_lex_identifier(lexer)            ||
			necro_lex_number(lexer)                ||
			necro_lex_multi_character_token(lexer) ||
			necro_lex_single_character(lexer);
		// No Match Error
		if (!matched)
		{
            necro_print_lex_error(lexer, "Unrecognized character sequence");
			return NECRO_LEX_RESULT_ERROR;
		}
	}
	return NECRO_LEX_RESULT_SUCCESSFUL;
}