/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <ctype.h>
#include "lexer.h"

NecroLexer necro_create_lexer(const char* str)
{
    NecroLexer lexer  =
    {
        .character_number          = 0,
        .line_number               = 0,
        .pos                       = 0,
        .str                       = str,
        .tokens                    = necro_create_lex_token_vector(),
        .layout_fixed_tokens       = necro_create_lex_token_vector(),
        .intern                    = necro_create_intern()
    };

    // Intern keywords, in the same order as their listing in the NECRO_LEX_TOKEN_TYPE enum
    // MAKE SURE THAT THE FIRST N ENTRIES IN NECRO_LEX_TOKEN_TYPE ARE THE KEYWORD TYPES AND THAT THEY EXACTLY MATCH THEIR SYMBOLS MINUS ONE!!!!
    necro_intern_string(&lexer.intern, "let");
    necro_intern_string(&lexer.intern, "where");
    necro_intern_string(&lexer.intern, "of");
    necro_intern_string(&lexer.intern, "do");
    necro_intern_string(&lexer.intern, "case");
    necro_intern_string(&lexer.intern, "class");
    necro_intern_string(&lexer.intern, "data");
    necro_intern_string(&lexer.intern, "deriving");
    necro_intern_string(&lexer.intern, "forall");
    necro_intern_string(&lexer.intern, "if");
    necro_intern_string(&lexer.intern, "else");
    necro_intern_string(&lexer.intern, "then");
    necro_intern_string(&lexer.intern, "import");
    necro_intern_string(&lexer.intern, "instance");
    necro_intern_string(&lexer.intern, "in");
    necro_intern_string(&lexer.intern, "module");
    necro_intern_string(&lexer.intern, "newtype");
    necro_intern_string(&lexer.intern, "type");

    return lexer;
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
    case NECRO_LEX_ADD:                return "ADD";
    case NECRO_LEX_SUB:                return "SUB";
    case NECRO_LEX_MUL:                return "MUL";
    case NECRO_LEX_DIV:                return "DIV";
    case NECRO_LEX_MOD:                return "MOD";
    case NECRO_LEX_GT:                 return "GT";
    case NECRO_LEX_LT:                 return "LT";
    case NECRO_LEX_GTE:                return "GTE";
    case NECRO_LEX_LTE:                return "LTE";
    case NECRO_LEX_COLON:              return "COLON";
    case NECRO_LEX_SEMI_COLON:         return "SEMI_COLON";
    case NECRO_LEX_INTEGER_LITERAL:    return "INT";
    case NECRO_LEX_FLOAT_LITERAL:      return "FLOAT";
    case NECRO_LEX_IDENTIFIER:         return "IDENTIFIER";
    case NECRO_LEX_LEFT_ARROW:         return "LEFT_ARROW";
    case NECRO_LEX_RIGHT_ARROW:        return "RIGHT_ARROW";
    case NECRO_LEX_FAT_RIGHT_ARROW:    return "FAT_RIGHT_ARROW";
    case NECRO_LEX_LEFT_BRACKET:       return "LEFT_BRACKET";
    case NECRO_LEX_RIGHT_BRACKET:      return "RIGHT_BRACKET";
    case NECRO_LEX_LEFT_PAREN:         return "LEFT_PAREN";
    case NECRO_LEX_RIGHT_PAREN:        return "RIGHT_PAREN";
    case NECRO_LEX_LEFT_BRACE:         return "LEFT_BRACE";
    case NECRO_LEX_RIGHT_BRACE:        return "RIGHT_BRACE";
    case NECRO_LEX_COMMA:              return "COMMA";
    case NECRO_LEX_UNDER_SCORE:        return "UNDER_SCORE";
    case NECRO_LEX_EQUALS:             return "EQUALS";
    case NECRO_LEX_NOT_EQUALS:         return "NOT_EQUALS";
    case NECRO_LEX_ASSIGN:             return "ASSIGN";
    case NECRO_LEX_QUESTION_MARK:      return "QUESTION_MARK";
    case NECRO_LEX_EXCLAMATION:        return "EXCLAMATION";
    case NECRO_LEX_HASH:               return "HASH";
    case NECRO_LEX_DOUBLE_COLON:       return "DOUBLE_COLON";
    case NECRO_LEX_LEFT_SHIFT:         return "LEFT_SHIFT";
    case NECRO_LEX_RIGHT_SHIFT:        return "RIGHT_SHIFT";
    case NECRO_LEX_PIPE:               return "PIPE";
    case NECRO_LEX_FORWARD_PIPE:       return "FORWARD_PIPE";
    case NECRO_LEX_BACK_PIPE:          return "BACK_PIPE";
    case NECRO_LEX_DOT:                return "DOT";
    case NECRO_LEX_DOUBLE_DOT:         return "DOUBLE_DOT";
    case NECRO_LEX_AMPERSAND:          return "AMPERSAND";
    case NECRO_LEX_AT:                 return "AT";
    case NECRO_LEX_DOLLAR:             return "DOLLAR";
    case NECRO_LEX_CARET:              return "CARET";
    case NECRO_LEX_BACK_SLASH:         return "BACK_SLASH";
    case NECRO_LEX_TILDE:              return "TILDE";
    case NECRO_LEX_AND:                return "AND";
    case NECRO_LEX_OR:                 return "OR";
    case NECRO_LEX_BIND_RIGHT:         return "BIND_RIGHT";
    case NECRO_LEX_BIND_LEFT:          return "BIND_LEFT";
    case NECRO_LEX_UNIT:               return "UNIT";
    case NECRO_LEX_DOUBLE_EXCLAMATION: return "DOUBLE_EXCLAMATION";
    case NECRO_LEX_APPEND:             return "APPEND";
    case NECRO_LEX_ACCENT:             return "ACCENT";
    case NECRO_LEX_END_OF_STREAM:      return "END OF STREAM";
    case NECRO_LEX_CASE:               return "CASE";
    case NECRO_LEX_OF:                 return "OF";
    case NECRO_LEX_CLASS:              return "CLASS";
    case NECRO_LEX_DATA:               return "DATA";
    case NECRO_LEX_DERIVING:           return "DERIVING";
    case NECRO_LEX_FORALL:             return "FORALL";
    case NECRO_LEX_DO:                 return "DO,";
    case NECRO_LEX_IF:                 return "IF";
    case NECRO_LEX_ELSE:               return "ELSE";
    case NECRO_LEX_THEN:               return "THEN";
    case NECRO_LEX_IMPORT:             return "IMPORT";
    case NECRO_LEX_INSTANCE:           return "INSTANCE";
    case NECRO_LEX_LET:                return "LET";
    case NECRO_LEX_IN:                 return "IN";
    case NECRO_LEX_MODULE:             return "MODULE";
    case NECRO_LEX_NEWTYPE:            return "NEWTYPE";
    case NECRO_LEX_TYPE:               return "TYPE";
    case NECRO_LEX_WHERE:              return "WHERE";
    default:                           return "UNRECOGNIZED TOKEN";
    }
}

void necro_print_lex_token(NecroLexer* lexer, size_t token_id)
{
    if (lexer->tokens.data[token_id].token == NECRO_LEX_STRING_LITERAL)
    {
        printf("STRING:     \"%s\"\n", necro_intern_get_string(&lexer->intern, lexer->tokens.data[token_id].symbol));
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_IDENTIFIER)
    {
        printf("IDENTIFIER: %s\n", necro_intern_get_string(&lexer->intern, lexer->tokens.data[token_id].symbol));
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_TYPE_IDENTIFIER)
    {
        printf("TYPE:       %s\n", necro_intern_get_string(&lexer->intern, lexer->tokens.data[token_id].symbol));
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CHAR_LITERAL)
    {
        printf("CHAR:       %c\n", lexer->tokens.data[token_id].char_literal);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_FLOAT_LITERAL)
    {
        printf("FLOAT:      %f\n", lexer->tokens.data[token_id].double_literal);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_INTEGER_LITERAL)
    {
        printf("INT:        %lli\n", lexer->tokens.data[token_id].int_literal);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_LET)
    {
        printf("let {n}:    %d\n", lexer->tokens.data[token_id].brace_marker_n);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_WHERE)
    {
        printf("where {n}:  %d\n", lexer->tokens.data[token_id].brace_marker_n);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_OF)
    {
        printf("of {n}:     %d\n", lexer->tokens.data[token_id].brace_marker_n);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_DO)
    {
        printf("do {n}:     %d\n", lexer->tokens.data[token_id].brace_marker_n);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_WHITE_MARKER)
    {
        printf("<n>:        %d\n", lexer->tokens.data[token_id].brace_marker_n);
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

void necro_add_single_character_token(NecroLexer* lexer, NECRO_LEX_TOKEN_TYPE token, const char* string)
{
    NecroLexToken lex_token = (NecroLexToken)
    {
        .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
        .token      = token,
        .symbol     = necro_intern_string(&lexer->intern, string)
    };
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
    case '+':  necro_add_single_character_token(lexer, NECRO_LEX_ADD, "+");           break;
    case '-':  necro_add_single_character_token(lexer, NECRO_LEX_SUB, "-");           break;
    case '*':  necro_add_single_character_token(lexer, NECRO_LEX_MUL, "*");           break;
    case '/':  necro_add_single_character_token(lexer, NECRO_LEX_DIV, "/");           break;
    case '%':  necro_add_single_character_token(lexer, NECRO_LEX_MOD, "%");           break;
    case '<':  necro_add_single_character_token(lexer, NECRO_LEX_LT, "<");            break;
    case '>':  necro_add_single_character_token(lexer, NECRO_LEX_GT, ">");            break;
    case ':':  necro_add_single_character_token(lexer, NECRO_LEX_COLON, ":");         break;
    case ';':  necro_add_single_character_token(lexer, NECRO_LEX_SEMI_COLON, ";");    break;
    case '[':  necro_add_single_character_token(lexer, NECRO_LEX_LEFT_BRACKET, "[");  break;
    case ']':  necro_add_single_character_token(lexer, NECRO_LEX_RIGHT_BRACKET, "]"); break;
    case '(':  necro_add_single_character_token(lexer, NECRO_LEX_LEFT_PAREN, "(");    break;
    case ')':  necro_add_single_character_token(lexer, NECRO_LEX_RIGHT_PAREN, ")");   break;
    case '{':  necro_add_single_character_token(lexer, NECRO_LEX_LEFT_BRACE, "{");    break;
    case '}':  necro_add_single_character_token(lexer, NECRO_LEX_RIGHT_BRACE, "}");   break;
    case ',':  necro_add_single_character_token(lexer, NECRO_LEX_COMMA, ",");         break;
    case '_':  necro_add_single_character_token(lexer, NECRO_LEX_UNDER_SCORE, "_");   break;
    case '=':  necro_add_single_character_token(lexer, NECRO_LEX_ASSIGN, "=");        break;
    case '?':  necro_add_single_character_token(lexer, NECRO_LEX_QUESTION_MARK, "?"); break;
    case '!':  necro_add_single_character_token(lexer, NECRO_LEX_EXCLAMATION, "!");   break;
    case '#':  necro_add_single_character_token(lexer, NECRO_LEX_HASH, "#");          break;
    case '|':  necro_add_single_character_token(lexer, NECRO_LEX_PIPE, "|");          break;
    case '.':  necro_add_single_character_token(lexer, NECRO_LEX_DOT, ".");           break;
    case '@':  necro_add_single_character_token(lexer, NECRO_LEX_AT, "@");            break;
    case '$':  necro_add_single_character_token(lexer, NECRO_LEX_DOLLAR, "$");        break;
    case '&':  necro_add_single_character_token(lexer, NECRO_LEX_AMPERSAND, "&");     break;
    case '^':  necro_add_single_character_token(lexer, NECRO_LEX_CARET, "^");         break;
    case '\\': necro_add_single_character_token(lexer, NECRO_LEX_BACK_SLASH, "\\");   break;
    case '~':  necro_add_single_character_token(lexer, NECRO_LEX_TILDE, "~");         break;
    case '`':  necro_add_single_character_token(lexer, NECRO_LEX_ACCENT, "`");        break;
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
    NecroLexToken lex_token = (NecroLexToken)
    {
        .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
        .token      = token_type,
        .symbol     = necro_intern_string(&lexer->intern, pattern)
    };
    necro_push_lex_token_vector(&lexer->tokens, &lex_token);
    lexer->character_number += i;
    lexer->pos              += i;
    return true;
}

bool necro_lex_multi_character_token(NecroLexer* lexer)
{
    return necro_lex_token_with_pattern(lexer, ">>=", NECRO_LEX_BIND_RIGHT)         ||
           necro_lex_token_with_pattern(lexer, "=<<", NECRO_LEX_BIND_LEFT)          ||
           necro_lex_token_with_pattern(lexer, "==",  NECRO_LEX_EQUALS)             ||
           necro_lex_token_with_pattern(lexer, "/=",  NECRO_LEX_NOT_EQUALS)         ||
           necro_lex_token_with_pattern(lexer, "<=",  NECRO_LEX_LTE)                ||
           necro_lex_token_with_pattern(lexer, ">=",  NECRO_LEX_GTE)                ||
           necro_lex_token_with_pattern(lexer, "::",  NECRO_LEX_DOUBLE_COLON)       ||
           necro_lex_token_with_pattern(lexer, ">>",  NECRO_LEX_RIGHT_SHIFT)        ||
           necro_lex_token_with_pattern(lexer, "<<",  NECRO_LEX_LEFT_SHIFT)         ||
           necro_lex_token_with_pattern(lexer, "<|",  NECRO_LEX_FORWARD_PIPE)       ||
           necro_lex_token_with_pattern(lexer, "|>",  NECRO_LEX_BACK_PIPE)          ||
           necro_lex_token_with_pattern(lexer, "&&",  NECRO_LEX_AND)                ||
           necro_lex_token_with_pattern(lexer, "||",  NECRO_LEX_OR)                 ||
           necro_lex_token_with_pattern(lexer, "()",  NECRO_LEX_UNIT)               ||
           necro_lex_token_with_pattern(lexer, "<>",  NECRO_LEX_APPEND)             ||
           necro_lex_token_with_pattern(lexer, "!!",  NECRO_LEX_DOUBLE_EXCLAMATION) ||
           necro_lex_token_with_pattern(lexer, "<-",  NECRO_LEX_LEFT_ARROW)         ||
           necro_lex_token_with_pattern(lexer, "->",  NECRO_LEX_RIGHT_ARROW)        ||
           necro_lex_token_with_pattern(lexer, "=>",  NECRO_LEX_FAT_RIGHT_ARROW)    ||
           necro_lex_token_with_pattern(lexer, "..",  NECRO_LEX_DOUBLE_DOT);
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
    NecroLexToken lex_token = (NecroLexToken)
    {
        .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
        .token      = NECRO_LEX_INTEGER_LITERAL
    };
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
    double float_value     = strtod(start_str_pos, &new_str_pos);
    size_t count           = (size_t)(new_str_pos - start_str_pos);
    // printf("pos: %d, start: %p, end: %p, count: %d, int: %d\n", lexer->pos, start_str_pos, new_str_pos, count, integer_literal);
    if (count <= 0 || isalpha((uint8_t)(*new_str_pos)))
        return false;
    NecroLexToken lex_token = (NecroLexToken)
    {
        .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
        .token      = NECRO_LEX_FLOAT_LITERAL
    };
    lex_token.double_literal = float_value;
    necro_push_lex_token_vector(&lexer->tokens, &lex_token);
    lexer->character_number += count;
    lexer->pos              += count;
    return true;
}

bool necro_lex_number(NecroLexer* lexer)
{
    // Must start with digit or with minus symbol
    NECRO_LEX_NUM_STATE lex_num_state = NECRO_LEX_NUM_STATE_INT;
    const char* current_char = lexer->str + lexer->pos;
    if (*current_char == '-')
        current_char++;
    if (!isdigit((uint8_t) (*current_char)))
        return false;
    while (isdigit((uint8_t)(*current_char)) || *current_char == '.')
    {
        if (*current_char == '.')
        {
            if (lex_num_state == NECRO_LEX_NUM_STATE_INT)
            {
                lex_num_state = NECRO_LEX_NUM_STATE_FLOAT_AT_DOT;
            }
            else if (lex_num_state == NECRO_LEX_NUM_STATE_FLOAT_AT_DOT)
            {
                // Back-track to lexing int and cough up the double dots for later lexing
                current_char--;
                lex_num_state = NECRO_LEX_NUM_STATE_INT;
                break;
            }
            else if (lex_num_state == NECRO_LEX_NUM_STATE_FLOAT_POST_DOT)
            {
                break;
            }
        }
        else if (lex_num_state == NECRO_LEX_NUM_STATE_FLOAT_AT_DOT)
        {
            lex_num_state = NECRO_LEX_NUM_STATE_FLOAT_POST_DOT;
        }
        current_char++;
    }
    switch (lex_num_state)
    {
    case NECRO_LEX_NUM_STATE_INT:            return necro_lex_integer(lexer);
    case NECRO_LEX_NUM_STATE_FLOAT_POST_DOT: return necro_lex_float(lexer);
    case NECRO_LEX_NUM_STATE_FLOAT_AT_DOT:   return false;
    }
    return false;
}

bool necro_lex_comments(NecroLexer* lexer)
{
    if (lexer->str[lexer->pos] != '-' || lexer->str[lexer->pos + 1] != '-')
        return false;
    lexer->pos += 2;
    while (lexer->str[lexer->pos] != '\n' && lexer->str[lexer->pos] != '\0')
        lexer->pos++;
    return true;
}

void necro_gobble_up_whitespace_and_comments(NecroLexer* lexer)
{
    do
    {
        if (lexer->str[lexer->pos] == '\t')
        {
            lexer->character_number += 4;
        }
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
    } while (lexer->str[lexer->pos] == ' ' || lexer->str[lexer->pos] == '\t' || lexer->str[lexer->pos] == '\n' || necro_lex_comments(lexer));
}

bool necro_lex_identifier(NecroLexer* lexer)
{
    //first character must be a letter
    if (!isalpha((uint8_t) lexer->str[lexer->pos]))
        return false;

    // Get length of the identifier
    size_t identifier_length  = 0;
    bool   is_type_identifier = isupper((uint8_t)lexer->str[lexer->pos]);
    while (isalnum((uint8_t)lexer->str[lexer->pos + identifier_length]) || lexer->str[lexer->pos + identifier_length] == '_' || lexer->str[lexer->pos + identifier_length] == '\'')
    {
        identifier_length++;
    }
    if (identifier_length == 0)
        return false;

    // Create Lex token
    NecroLexToken lex_token = (NecroLexToken)
    {
        .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
        .token      = NECRO_LEX_IDENTIFIER
    };
    NecroStringSlice slice     = { lexer->str + lexer->pos, identifier_length };
    lex_token.symbol           = necro_intern_string_slice(&lexer->intern, slice);
    if (lex_token.symbol.id - 1 < NECRO_LEX_END_OF_KEY_WORDS)
        lex_token.token = lex_token.symbol.id - 1;
    else if (is_type_identifier)
        lex_token.token = NECRO_LEX_TYPE_IDENTIFIER;
    necro_push_lex_token_vector(&lexer->tokens, &lex_token);

    // Increment character number and string position
    lexer->pos              += identifier_length;
    lexer->character_number += identifier_length;

    // If the identifier is a layout keyword, update indentation levels and add left brace
    if (lex_token.token <= NECRO_LEX_DO)
    {
        necro_gobble_up_whitespace_and_comments(lexer);
        // Insert implicit brace marker if an explicit one is not given
        if (lexer->str[lexer->pos] != '{')
        {
            // BRACE MARKER
            NECRO_LEX_TOKEN_TYPE brace_type;
            switch (lex_token.token)
            {
            case NECRO_LEX_LET:   brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_LET;   break;
            case NECRO_LEX_WHERE: brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_WHERE; break;
            case NECRO_LEX_OF:    brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_OF;    break;
            case NECRO_LEX_DO:    brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_DO;    break;
            }
            NecroLexToken token = (NecroLexToken)
            {
                .source_loc     = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
                .token          = brace_type,
                .brace_marker_n = lexer->character_number + 1,
            };
            necro_push_lex_token_vector(&lexer->tokens, &token);
        }
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
        necro_gobble_up_whitespace_and_comments(lexer);

        char c = lexer->str[lexer->pos];
        assert(c != '\n');
        assert(c != ' ');
        assert(c != '\t');
        // Add whitespace markers
        NecroLexToken token = (NecroLexToken)
        {
            .source_loc     = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
            .token          = NECRO_LEX_CONTROL_WHITE_MARKER,
            .white_marker_n = lexer->character_number + 1,
        };
        necro_push_lex_token_vector(&lexer->tokens, &token);
        return true;
    }
    return false;
}

// TODO: Control character handling
bool necro_lex_char(NecroLexer* lexer)
{
    if (lexer->str[lexer->pos]     != '\''             ||
        iscntrl((uint8_t) lexer->str[lexer->pos + 1])  ||
        lexer->str[lexer->pos + 2] != '\'')
        return false;
    lexer->pos              += 3;
    lexer->character_number += 3;
    NecroLexToken token = (NecroLexToken)
    {
        .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
        .token      = NECRO_LEX_CHAR_LITERAL
    };
    token.char_literal = lexer->str[lexer->pos - 2];
    necro_push_lex_token_vector(&lexer->tokens, &token);
    return true;
}

bool necro_lex_string(NecroLexer* lexer)
{
    if (lexer->str[lexer->pos] != '\"')
        return false;
    size_t beginning = lexer->pos;
    lexer->pos++;
    while (lexer->str[lexer->pos] != '\"' && lexer->str[lexer->pos] != '\0' && lexer->str[lexer->pos] != '\n')
    {
        lexer->pos++;
    }
    if (lexer->str[lexer->pos] == '\0' || lexer->str[lexer->pos] == '\n')
    {
        lexer->pos = beginning;
        return false;
    }
    lexer->pos++;
    size_t           length  = lexer->pos - beginning;
    NecroLexToken token = (NecroLexToken)
    {
        .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
        .token      = NECRO_LEX_STRING_LITERAL
    };
    NecroStringSlice slice   = { lexer->str + beginning + 1, length - 2 };
    token.symbol             = necro_intern_string_slice(&lexer->intern, slice);
    lexer->character_number += length;
    necro_push_lex_token_vector(&lexer->tokens, &token);
    return true;
}

bool necro_lex_non_ascii(NecroLexer* lexer)
{
    // Lex control characters
    uint8_t c = lexer->str[lexer->pos];
    if (iscntrl(c) || c > 127)
    {
        // printf("Found control character: %d, skipping...\n", c);
        lexer->pos++;
        lexer->character_number++;
        return true;
    }
    return false;
}

#define NECRO_MAX_INDENTATIONS 512

typedef enum
{
    NECRO_LEX_LAYOUT_LET,
    NECRO_LEX_LAYOUT_WHERE,
    NECRO_LEX_LAYOUT_OF,
    NECRO_LEX_LAYOUT_DO,
    NECRO_LEX_LAYOUT_MANUAL
} NECRO_LEX_LAYOUT;

typedef struct
{
    size_t           indentation;
    NECRO_LEX_LAYOUT layout;
} NecroLexLayoutContext;

NECRO_LEX_LAYOUT token_type_to_layout(NECRO_LEX_TOKEN_TYPE type)
{
    switch(type)
    {
    case NECRO_LEX_CONTROL_BRACE_MARKER_LET:   return NECRO_LEX_LAYOUT_LET;
    case NECRO_LEX_CONTROL_BRACE_MARKER_WHERE: return NECRO_LEX_LAYOUT_WHERE;
    case NECRO_LEX_CONTROL_BRACE_MARKER_OF:    return NECRO_LEX_LAYOUT_OF;
    case NECRO_LEX_CONTROL_BRACE_MARKER_DO:    return NECRO_LEX_LAYOUT_LET;
    default:                                   assert(false);
    }
    return NECRO_LEX_LAYOUT_LET;
}

NECRO_RETURN_CODE necro_lex_fixup_layout(NecroLexer* lexer)
{
    size_t                pos       = 0;
    size_t                stack_pos = 1;
    NecroLexLayoutContext context_stack[NECRO_MAX_INDENTATIONS];
    for (size_t i = 0; i < NECRO_MAX_INDENTATIONS; ++i)
        context_stack[i] = (NecroLexLayoutContext){ .indentation = 1, .layout = NECRO_LEX_LAYOUT_LET };
    do
    {
        NECRO_LEX_TOKEN_TYPE type = lexer->tokens.data[pos].token;
        NecroSourceLoc       loc  = lexer->tokens.data[pos].source_loc;
        size_t               n    = lexer->tokens.data[pos].white_marker_n;
        size_t               m    = context_stack[stack_pos].indentation;
        if (stack_pos >= NECRO_MAX_INDENTATIONS)
        {
            necro_error(&lexer->error, loc, "Context stack overflow in lexer, max indentations: %s\n", NECRO_MAX_INDENTATIONS);
            return NECRO_ERROR;
        }
        else if (type == NECRO_LEX_CONTROL_WHITE_MARKER)
        {
            if (pos == 0)
            {
                pos++;
            }
            else if (n == m && stack_pos != 0)
            {
                NecroLexToken token = { .token = NECRO_LEX_SEMI_COLON,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                pos++;
            }
            else if (n < m)
            {
                NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                stack_pos--;
            }
            else
            {
                pos++;
            }
        }
        else if (pos > 0 &&
                 type == NECRO_LEX_IN &&
                 context_stack[stack_pos].layout == NECRO_LEX_LAYOUT_LET &&
                 (lexer->tokens.data[pos - 1].token != NECRO_LEX_CONTROL_WHITE_MARKER &&
                  lexer->tokens.data[pos - 1].token != NECRO_LEX_RIGHT_BRACE))
        {
            NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE,.source_loc = loc };
            necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
            necro_push_lex_token_vector(&lexer->layout_fixed_tokens, lexer->tokens.data + pos);
            pos++;
            stack_pos--;
        }
        else if (type == NECRO_LEX_CONTROL_BRACE_MARKER_LET ||
                 type == NECRO_LEX_CONTROL_BRACE_MARKER_WHERE ||
                 type == NECRO_LEX_CONTROL_BRACE_MARKER_OF ||
                 type == NECRO_LEX_CONTROL_BRACE_MARKER_DO)
        {
            if (n > m)
            {
                NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                stack_pos++;
                context_stack[stack_pos].indentation = n;
                context_stack[stack_pos].layout      = token_type_to_layout(type);
                pos++;
            }
            else if (n > 0 && stack_pos == 0)
            {
                NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                stack_pos++;
                context_stack[stack_pos].indentation = n;
                context_stack[stack_pos].layout      = token_type_to_layout(type);
                pos++;
            }
            else
            {
                NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                NecroLexToken token2 = { .token = NECRO_LEX_RIGHT_BRACE,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token2);
                lexer->tokens.data[pos].token = NECRO_LEX_CONTROL_WHITE_MARKER;
            }
        }
        else if (type == NECRO_LEX_RIGHT_BRACE)
        {

            if (m == 0)
            {
                NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                stack_pos--;
                pos++;
            }
            else
            {
                necro_error(&lexer->error, lexer->tokens.data[pos].source_loc, "Mixing implicit and explicit layouts: Explicit '}' matched against implicit '{'");
                return NECRO_ERROR;
            }
        }
        else if (type == NECRO_LEX_LEFT_BRACE)
        {
            NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
            necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
            pos++;
            stack_pos++;
            context_stack[stack_pos].indentation = 0;
            context_stack[stack_pos].layout      = NECRO_LEX_LAYOUT_MANUAL;
        }
        // Note 5 error check?
        // else (???)
        // {
        //     necro_push_lex_token_vector(&lexer->layout_fixed_tokens, lexer->tokens.data + pos);
        // }
        else if (type == NECRO_LEX_END_OF_STREAM)
        {
            if (stack_pos > 1 && m != 0)
            {
                NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                stack_pos--;
            }
            else
            {
                NecroLexToken token = { .token = NECRO_LEX_END_OF_STREAM,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                break;
            }
        }
        else
        {
            necro_push_lex_token_vector(&lexer->layout_fixed_tokens, lexer->tokens.data + pos);
            pos++;
        }
    }
    while (true);
    // swap tokens and layout_fixed_tokens
    NecroLexTokenVector tokens              = lexer->tokens;
    NecroLexTokenVector layout_fixed_tokens = lexer->layout_fixed_tokens;
    lexer->layout_fixed_tokens              = tokens;
    lexer->tokens                           = layout_fixed_tokens;
    return NECRO_SUCCESS;
}

NECRO_RETURN_CODE necro_lex(NecroLexer* lexer)
{
    while (lexer->str[lexer->pos])
    {
        bool matched =
            necro_lex_whitespace(lexer)            ||
            necro_lex_non_ascii(lexer)             ||
            necro_lex_comments(lexer)              ||
            necro_lex_string(lexer)                ||
            necro_lex_char(lexer)                  ||
            necro_lex_identifier(lexer)            ||
            necro_lex_number(lexer)                ||
            necro_lex_multi_character_token(lexer) ||
            necro_lex_single_character(lexer);
        // Encountered error
        if (lexer->error.return_code != NECRO_SUCCESS)
        {
            return NECRO_ERROR;
        }
        // No Match Error
        else if (!matched)
        {
            necro_error(&lexer->error, (NecroSourceLoc){ lexer->line_number, lexer->character_number, lexer->pos },"Unrecognized character sequence");
            return NECRO_ERROR;
        }
    }
    if (lexer->tokens.length <= 0)
    {
        necro_error(&lexer->error, (NecroSourceLoc) { lexer->line_number, lexer->character_number, lexer->pos }, "Empty File");
        return NECRO_ERROR;
    }
    NecroLexToken lex_eos_token = (NecroLexToken)
    {
        .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
        .token      = NECRO_LEX_END_OF_STREAM
    };
    necro_push_lex_token_vector(&lexer->tokens, &lex_eos_token);
    return NECRO_SUCCESS;
}

//=====================================================
// Testing
//=====================================================
void necro_test_lexer()
{
    necro_announce_phase("NecroLexer");

    // ASCII noise test
    {
        // puts("-------------------");
        // puts("ASCII Noise test");
        // puts("-------------------\n");

        srand(666);
        char noise[4096];
        for (size_t i = 0; i < 4096; ++i)
        {
            char c = rand();
            if (c == NECRO_LEX_END_OF_STREAM || c == '\0')
                c = ' ';
            noise[i] = c;
        }
        noise[4095] = '\0';
        NecroLexer lexer = necro_create_lexer(noise);
        necro_lex(&lexer);
        printf("noise test: passed\n");
        // necro_print_lexer(&lexer);
        // necro_destroy_lexer(&lexer);
    }
}
