 /* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <ctype.h>
#include "lexer.h"
#include "unicode_properties.h"

// NecroLexer necro_create_lexer_u(const char* str, size_t str_length);

// NecroLexer necro_create_lexer(const char* str, size_t str_length)
// {
//     NecroLexer lexer  =
//     {
//         .character_number          = 0,
//         .line_number               = 0,
//         .pos                       = 0,
//         .str                       = str,
//         .str_length                = str_length,
//         .tokens                    = necro_create_lex_token_vector(),
//         .layout_fixed_tokens       = necro_create_lex_token_vector(),
//         .intern                    = necro_create_intern()
//     };

//     // Intern keywords, in the same order as their listing in the NECRO_LEX_TOKEN_TYPE enum
//     // MAKE SURE THAT THE FIRST N ENTRIES IN NECRO_LEX_TOKEN_TYPE ARE THE KEYWORD TYPES AND THAT THEY EXACTLY MATCH THEIR SYMBOLS MINUS ONE!!!!
//     necro_intern_string(&lexer.intern, "let");
//     necro_intern_string(&lexer.intern, "where");
//     necro_intern_string(&lexer.intern, "of");
//     necro_intern_string(&lexer.intern, "do");
//     necro_intern_string(&lexer.intern, "case");
//     necro_intern_string(&lexer.intern, "class");
//     necro_intern_string(&lexer.intern, "data");
//     necro_intern_string(&lexer.intern, "deriving");
//     necro_intern_string(&lexer.intern, "forall");
//     necro_intern_string(&lexer.intern, "if");
//     necro_intern_string(&lexer.intern, "else");
//     necro_intern_string(&lexer.intern, "then");
//     necro_intern_string(&lexer.intern, "import");
//     necro_intern_string(&lexer.intern, "instance");
//     necro_intern_string(&lexer.intern, "in");
//     necro_intern_string(&lexer.intern, "module");
//     necro_intern_string(&lexer.intern, "newtype");
//     necro_intern_string(&lexer.intern, "type");
//     necro_intern_string(&lexer.intern, "pat");
//     necro_intern_string(&lexer.intern, "delay");
//     necro_intern_string(&lexer.intern, "trimDelay");

//     return lexer;
// }

// void necro_destroy_lexer(NecroLexer* lexer)
// {
//     lexer->character_number = 0;
//     lexer->line_number      = 0;
//     lexer->pos              = 0;
//     necro_destroy_lex_token_vector(&lexer->tokens);
//     necro_destroy_intern(&lexer->intern);
// }

void necro_print_lexer(NecroLexer* lexer);
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
    case NECRO_LEX_PAT:                return "PAT";
    case NECRO_LEX_WHERE:              return "WHERE";
    default:                           return "UNRECOGNIZED TOKEN";
    }
}

// void necro_print_lex_token(NecroLexer* lexer, size_t token_id)
// {
//     if (lexer->tokens.data[token_id].token == NECRO_LEX_STRING_LITERAL)
//     {
//         printf("STRING:     \"%s\"\n", necro_intern_get_string(&lexer->intern, lexer->tokens.data[token_id].symbol));
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_IDENTIFIER)
//     {
//         printf("IDENTIFIER: %s\n", necro_intern_get_string(&lexer->intern, lexer->tokens.data[token_id].symbol));
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_TYPE_IDENTIFIER)
//     {
//         printf("TYPE:       %s\n", necro_intern_get_string(&lexer->intern, lexer->tokens.data[token_id].symbol));
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_CHAR_LITERAL)
//     {
//         printf("CHAR:       %c\n", lexer->tokens.data[token_id].char_literal);
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_FLOAT_LITERAL)
//     {
//         printf("FLOAT:      %f\n", lexer->tokens.data[token_id].double_literal);
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_INTEGER_LITERAL)
//     {
//         printf("INT:        %lli\n", lexer->tokens.data[token_id].int_literal);
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_LET)
//     {
//         printf("let {n}:    %zu\n", lexer->tokens.data[token_id].brace_marker_n);
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_WHERE)
//     {
//         printf("where {n}:  %zu\n", lexer->tokens.data[token_id].brace_marker_n);
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_OF)
//     {
//         printf("of {n}:     %zu\n", lexer->tokens.data[token_id].brace_marker_n);
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_DO)
//     {
//         printf("do {n}:     %zu\n", lexer->tokens.data[token_id].brace_marker_n);
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_DO)
//     {
//         printf("pat\n");
//     }
//     else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_WHITE_MARKER)
//     {
//         printf("<n>:        %zu\n", lexer->tokens.data[token_id].brace_marker_n);
//     }
//     else
//     {
//         printf("%s\n", necro_lex_token_type_string(lexer->tokens.data[token_id].token));
//     }
// }

// void necro_print_lexer(NecroLexer* lexer)
// {
//     printf("NecroLexer\n{\n");
//     printf("    line_number:      %zu,\n", lexer->line_number);
//     printf("    character_number: %zu,\n", lexer->character_number);
//     printf("    tokens:\n    [\n");
//     for (size_t i = 0; i < lexer->tokens.length; ++i)
//     {
//         printf("        ");
//         necro_print_lex_token(lexer, i);
//     }
//     printf("    ]\n");
//     printf("}\n\n");
// }

// void necro_add_single_character_token(NecroLexer* lexer, NECRO_LEX_TOKEN_TYPE token, const char* string)
// {
//     NecroLexToken lex_token = (NecroLexToken)
//     {
//         .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
//         .token      = token,
//         .symbol     = necro_intern_string(&lexer->intern, string)
//     };
//     necro_push_lex_token_vector(&lexer->tokens, &lex_token);
//     lexer->pos++;
//     lexer->character_number++;
// }

// bool necro_lex_single_character(NecroLexer* lexer)
// {
//     if (!lexer->str[lexer->pos])
//         return false;
//     switch (lexer->str[lexer->pos])
//     {
//     case '+':  necro_add_single_character_token(lexer, NECRO_LEX_ADD, "+");           break;
//     case '-':  necro_add_single_character_token(lexer, NECRO_LEX_SUB, "-");           break;
//     case '*':  necro_add_single_character_token(lexer, NECRO_LEX_MUL, "*");           break;
//     case '/':  necro_add_single_character_token(lexer, NECRO_LEX_DIV, "/");           break;
//     case '%':  necro_add_single_character_token(lexer, NECRO_LEX_MOD, "%");           break;
//     case '<':  necro_add_single_character_token(lexer, NECRO_LEX_LT, "<");            break;
//     case '>':  necro_add_single_character_token(lexer, NECRO_LEX_GT, ">");            break;
//     case ':':  necro_add_single_character_token(lexer, NECRO_LEX_COLON, ":");         break;
//     case ';':  necro_add_single_character_token(lexer, NECRO_LEX_SEMI_COLON, ";");    break;
//     case '[':  necro_add_single_character_token(lexer, NECRO_LEX_LEFT_BRACKET, "[");  break;
//     case ']':  necro_add_single_character_token(lexer, NECRO_LEX_RIGHT_BRACKET, "]"); break;
//     case '(':  necro_add_single_character_token(lexer, NECRO_LEX_LEFT_PAREN, "(");    break;
//     case ')':  necro_add_single_character_token(lexer, NECRO_LEX_RIGHT_PAREN, ")");   break;
//     case '{':  necro_add_single_character_token(lexer, NECRO_LEX_LEFT_BRACE, "{");    break;
//     case '}':  necro_add_single_character_token(lexer, NECRO_LEX_RIGHT_BRACE, "}");   break;
//     case ',':  necro_add_single_character_token(lexer, NECRO_LEX_COMMA, ",");         break;
//     case '_':  necro_add_single_character_token(lexer, NECRO_LEX_UNDER_SCORE, "_");   break;
//     case '=':  necro_add_single_character_token(lexer, NECRO_LEX_ASSIGN, "=");        break;
//     case '?':  necro_add_single_character_token(lexer, NECRO_LEX_QUESTION_MARK, "?"); break;
//     case '!':  necro_add_single_character_token(lexer, NECRO_LEX_EXCLAMATION, "!");   break;
//     case '#':  necro_add_single_character_token(lexer, NECRO_LEX_HASH, "#");          break;
//     case '|':  necro_add_single_character_token(lexer, NECRO_LEX_PIPE, "|");          break;
//     case '.':  necro_add_single_character_token(lexer, NECRO_LEX_DOT, ".");           break;
//     case '@':  necro_add_single_character_token(lexer, NECRO_LEX_AT, "@");            break;
//     case '$':  necro_add_single_character_token(lexer, NECRO_LEX_DOLLAR, "$");        break;
//     case '&':  necro_add_single_character_token(lexer, NECRO_LEX_AMPERSAND, "&");     break;
//     case '^':  necro_add_single_character_token(lexer, NECRO_LEX_CARET, "^");         break;
//     case '\\': necro_add_single_character_token(lexer, NECRO_LEX_BACK_SLASH, "\\");   break;
//     case '~':  necro_add_single_character_token(lexer, NECRO_LEX_TILDE, "~");         break;
//     case '`':  necro_add_single_character_token(lexer, NECRO_LEX_ACCENT, "`");        break;
//     default:   return false;
//     }
//     return true;
// }

// bool necro_lex_token_with_pattern(NecroLexer* lexer, const char* pattern, NECRO_LEX_TOKEN_TYPE token_type)
// {
//     size_t i = 0;
//     while (pattern[i])
//     {
//         if (lexer->str[lexer->pos + i] != pattern[i])
//         {
//             return false;
//         }
//         i++;
//     }
//     NecroLexToken lex_token = (NecroLexToken)
//     {
//         .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
//         .token      = token_type,
//         .symbol     = necro_intern_string(&lexer->intern, pattern)
//     };
//     necro_push_lex_token_vector(&lexer->tokens, &lex_token);
//     lexer->character_number += i;
//     lexer->pos              += i;
//     return true;
// }

// bool necro_lex_multi_character_token(NecroLexer* lexer)
// {
//     return necro_lex_token_with_pattern(lexer, ">>=", NECRO_LEX_BIND_RIGHT)         ||
//            necro_lex_token_with_pattern(lexer, "=<<", NECRO_LEX_BIND_LEFT)          ||
//            necro_lex_token_with_pattern(lexer, "==",  NECRO_LEX_EQUALS)             ||
//            necro_lex_token_with_pattern(lexer, "/=",  NECRO_LEX_NOT_EQUALS)         ||
//            necro_lex_token_with_pattern(lexer, "<=",  NECRO_LEX_LTE)                ||
//            necro_lex_token_with_pattern(lexer, ">=",  NECRO_LEX_GTE)                ||
//            necro_lex_token_with_pattern(lexer, "::",  NECRO_LEX_DOUBLE_COLON)       ||
//            necro_lex_token_with_pattern(lexer, ">>",  NECRO_LEX_RIGHT_SHIFT)        ||
//            necro_lex_token_with_pattern(lexer, "<<",  NECRO_LEX_LEFT_SHIFT)         ||
//            necro_lex_token_with_pattern(lexer, "<|",  NECRO_LEX_FORWARD_PIPE)       ||
//            necro_lex_token_with_pattern(lexer, "|>",  NECRO_LEX_BACK_PIPE)          ||
//            necro_lex_token_with_pattern(lexer, "&&",  NECRO_LEX_AND)                ||
//            necro_lex_token_with_pattern(lexer, "||",  NECRO_LEX_OR)                 ||
//            necro_lex_token_with_pattern(lexer, "()",  NECRO_LEX_UNIT)               ||
//            necro_lex_token_with_pattern(lexer, "<>",  NECRO_LEX_APPEND)             ||
//            necro_lex_token_with_pattern(lexer, "!!",  NECRO_LEX_DOUBLE_EXCLAMATION) ||
//            necro_lex_token_with_pattern(lexer, "<-",  NECRO_LEX_LEFT_ARROW)         ||
//            necro_lex_token_with_pattern(lexer, "->",  NECRO_LEX_RIGHT_ARROW)        ||
//            necro_lex_token_with_pattern(lexer, "=>",  NECRO_LEX_FAT_RIGHT_ARROW)    ||
//            necro_lex_token_with_pattern(lexer, "..",  NECRO_LEX_DOUBLE_DOT);
//            // necro_lex_token_with_pattern(lexer, ">->", NECRO_LEX_FBY)                ||
// }

// bool necro_lex_integer(NecroLexer* lexer)
// {
//     char*   start_str_pos   = (char*)(lexer->str + lexer->pos);
//     char*   new_str_pos     = start_str_pos;
//     int64_t integer_literal = strtol(start_str_pos, &new_str_pos, 10);
//     size_t  count           = (size_t)(new_str_pos - start_str_pos);
//     assert(count > 0);
//     // printf("pos: %d, start: %p, end: %p, count: %d, int: %d\n", lexer->pos, start_str_pos, new_str_pos, count, integer_literal);
//     // if (count <= 0 || isalpha((uint8_t) (*new_str_pos)))
//     //     return false;
//     NecroLexToken lex_token = (NecroLexToken)
//     {
//         .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
//         .token      = NECRO_LEX_INTEGER_LITERAL
//     };
//     lex_token.int_literal   = integer_literal;
//     necro_push_lex_token_vector(&lexer->tokens, &lex_token);
//     lexer->character_number += count;
//     lexer->pos              += count;
//     return true;
// }

// bool necro_lex_float(NecroLexer* lexer)
// {
//     char*  start_str_pos   = (char*)(lexer->str + lexer->pos);
//     char*  new_str_pos     = start_str_pos;
//     double float_value     = strtod(start_str_pos, &new_str_pos);
//     size_t count           = (size_t)(new_str_pos - start_str_pos);
//     assert(count > 0);
//     // printf("pos: %d, start: %p, end: %p, count: %d, int: %d\n", lexer->pos, start_str_pos, new_str_pos, count, integer_literal);
//     // if (count <= 0 || isalpha((uint8_t)(*new_str_pos)))
//     //     return false;
//     NecroLexToken lex_token = (NecroLexToken)
//     {
//         .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
//         .token      = NECRO_LEX_FLOAT_LITERAL
//     };
//     lex_token.double_literal = float_value;
//     necro_push_lex_token_vector(&lexer->tokens, &lex_token);
//     lexer->character_number += count;
//     lexer->pos              += count;
//     return true;
// }

// bool necro_lex_number(NecroLexer* lexer)
// {
//     // Must start with digit or with minus symbol
//     NECRO_LEX_NUM_STATE lex_num_state = NECRO_LEX_NUM_STATE_INT;
//     const char* current_char = lexer->str + lexer->pos;
//     if (*current_char == '-')
//         current_char++;
//     if (!isdigit((uint8_t) (*current_char)))
//         return false;
//     while (isdigit((uint8_t)(*current_char)) || *current_char == '.')
//     {
//         if (*current_char == '.')
//         {
//             if (lex_num_state == NECRO_LEX_NUM_STATE_INT)
//             {
//                 lex_num_state = NECRO_LEX_NUM_STATE_FLOAT_AT_DOT;
//             }
//             else if (lex_num_state == NECRO_LEX_NUM_STATE_FLOAT_AT_DOT)
//             {
//                 // Back-track to lexing int and cough up the double dots for later lexing
//                 current_char--;
//                 lex_num_state = NECRO_LEX_NUM_STATE_INT;
//                 break;
//             }
//             else if (lex_num_state == NECRO_LEX_NUM_STATE_FLOAT_POST_DOT)
//             {
//                 break;
//             }
//         }
//         else if (lex_num_state == NECRO_LEX_NUM_STATE_FLOAT_AT_DOT)
//         {
//             lex_num_state = NECRO_LEX_NUM_STATE_FLOAT_POST_DOT;
//         }
//         current_char++;
//     }
//     switch (lex_num_state)
//     {
//     case NECRO_LEX_NUM_STATE_INT:            return necro_lex_integer(lexer);
//     case NECRO_LEX_NUM_STATE_FLOAT_POST_DOT: return necro_lex_float(lexer);
//     case NECRO_LEX_NUM_STATE_FLOAT_AT_DOT:   return false;
//     }
//     return false;
// }

// bool necro_lex_comments(NecroLexer* lexer)
// {
//     if (lexer->str[lexer->pos] != '-' || lexer->str[lexer->pos + 1] != '-')
//         return false;
//     lexer->pos += 2;
//     while (lexer->str[lexer->pos] != '\n' && lexer->str[lexer->pos] != '\0')
//         lexer->pos++;
//     return true;
// }

// void necro_gobble_up_whitespace_and_comments(NecroLexer* lexer)
// {
//     do
//     {
//         if (lexer->str[lexer->pos] == '\t')
//         {
//             lexer->character_number += 4;
//         }
//         if (lexer->str[lexer->pos] == '\n')
//         {
//             lexer->line_number++;
//             lexer->character_number = 0;
//         }
//         else
//         {
//             lexer->character_number++;
//         }
//         lexer->pos++;
//     } while (lexer->str[lexer->pos] == ' ' || lexer->str[lexer->pos] == '\t'  || lexer->str[lexer->pos] == '\r' || lexer->str[lexer->pos] == '\n' || necro_lex_comments(lexer));
// }

// bool necro_lex_identifier(NecroLexer* lexer)
// {
//     //first character must be a letter
//     if (!isalpha((uint8_t) lexer->str[lexer->pos]))
//         return false;

//     // Get length of the identifier
//     size_t identifier_length  = 0;
//     bool   is_type_identifier = isupper((uint8_t)lexer->str[lexer->pos]);
//     while (isalnum((uint8_t)lexer->str[lexer->pos + identifier_length]) || lexer->str[lexer->pos + identifier_length] == '_' || lexer->str[lexer->pos + identifier_length] == '\'')
//     {
//         identifier_length++;
//     }
//     if (identifier_length == 0)
//         return false;

//     // Create Lex token
//     NecroLexToken lex_token = (NecroLexToken)
//     {
//         .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
//         .token      = NECRO_LEX_IDENTIFIER
//     };
//     NecroStringSlice slice     = { lexer->str + lexer->pos, identifier_length };
//     lex_token.symbol           = necro_intern_string_slice(&lexer->intern, slice);
//     if (lex_token.symbol.id - 1 < NECRO_LEX_END_OF_KEY_WORDS)
//         lex_token.token = lex_token.symbol.id - 1;
//     else if (is_type_identifier)
//         lex_token.token = NECRO_LEX_TYPE_IDENTIFIER;
//     necro_push_lex_token_vector(&lexer->tokens, &lex_token);

//     // Increment character number and string position
//     lexer->pos              += identifier_length;
//     lexer->character_number += identifier_length;

//     // If the identifier is a layout keyword, update indentation levels and add left brace
//     if (lex_token.token <= NECRO_LEX_DO)
//     {
//         necro_gobble_up_whitespace_and_comments(lexer);
//         // Insert implicit brace marker if an explicit one is not given
//         if (lexer->str[lexer->pos] != '{')
//         {
//             // BRACE MARKER
//             NECRO_LEX_TOKEN_TYPE brace_type = NECRO_LEX_INVALID;
//             switch (lex_token.token)
//             {
//             case NECRO_LEX_LET:   brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_LET;   break;
//             case NECRO_LEX_WHERE: brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_WHERE; break;
//             case NECRO_LEX_OF:    brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_OF;    break;
//             case NECRO_LEX_DO:    brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_DO;    break;
//             default:              assert(false); break;
//             }
//             NecroLexToken token = (NecroLexToken)
//             {
//                 .source_loc     = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
//                 .token          = brace_type,
//                 .brace_marker_n = lexer->character_number + 1,
//             };
//             necro_push_lex_token_vector(&lexer->tokens, &token);
//         }
//     }

//     return true;
// }

// bool necro_lex_whitespace(NecroLexer* lexer)
// {
//     // Tabs
//     if (lexer->str[lexer->pos] == '\t')
//     {
//         lexer->pos++;
//         lexer->character_number += 4;
//         return true;
//     }
//     // Intermediate white space
//     else if (lexer->str[lexer->pos] == ' ')
//     {
//         while (lexer->str[lexer->pos] == ' ')
//         {
//             lexer->pos++;
//             lexer->character_number++;
//         }
//         return true;
//     }
//     // Newline
//     else if (lexer->str[lexer->pos] == '\r')
//     {
//         lexer->pos++;
//         return true;
//     }
//     else if (lexer->str[lexer->pos] == '\n')
//     {
//         necro_gobble_up_whitespace_and_comments(lexer);

//         char c = lexer->str[lexer->pos];
//         assert(c != '\n');
//         assert(c != ' ');
//         assert(c != '\t');
//         // Add whitespace markers
//         NecroLexToken token = (NecroLexToken)
//         {
//             .source_loc     = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
//             .token          = NECRO_LEX_CONTROL_WHITE_MARKER,
//             .white_marker_n = lexer->character_number + 1,
//         };
//         necro_push_lex_token_vector(&lexer->tokens, &token);
//         return true;
//     }
//     return false;
// }

// // TODO: Control character handling
// bool necro_lex_char(NecroLexer* lexer)
// {
//     if (lexer->str[lexer->pos]     != '\''             ||
//         iscntrl((uint8_t) lexer->str[lexer->pos + 1])  ||
//         lexer->str[lexer->pos + 2] != '\'')
//         return false;
//     lexer->pos              += 3;
//     lexer->character_number += 3;
//     NecroLexToken token = (NecroLexToken)
//     {
//         .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
//         .token      = NECRO_LEX_CHAR_LITERAL
//     };
//     token.char_literal = lexer->str[lexer->pos - 2];
//     necro_push_lex_token_vector(&lexer->tokens, &token);
//     return true;
// }

// bool necro_lex_string(NecroLexer* lexer)
// {
//     if (lexer->str[lexer->pos] != '\"')
//         return false;
//     size_t beginning = lexer->pos;
//     lexer->pos++;
//     while (lexer->str[lexer->pos] != '\"' && lexer->str[lexer->pos] != '\0' && lexer->str[lexer->pos] != '\n')
//     {
//         lexer->pos++;
//     }
//     if (lexer->str[lexer->pos] == '\0' || lexer->str[lexer->pos] == '\n')
//     {
//         lexer->pos = beginning;
//         return false;
//     }
//     lexer->pos++;
//     size_t           length  = lexer->pos - beginning;
//     NecroLexToken token = (NecroLexToken)
//     {
//         .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
//         .token      = NECRO_LEX_STRING_LITERAL
//     };
//     NecroStringSlice slice   = { lexer->str + beginning + 1, length - 2 };
//     token.symbol             = necro_intern_string_slice(&lexer->intern, slice);
//     lexer->character_number += length;
//     necro_push_lex_token_vector(&lexer->tokens, &token);
//     return true;
// }

// bool necro_lex_non_ascii(NecroLexer* lexer)
// {
//     // Lex control characters
//     uint8_t c = lexer->str[lexer->pos];
//     if (iscntrl(c) || c > 127)
//     {
//         // printf("Found control character: %d, skipping...\n", c);
//         lexer->pos++;
//         lexer->character_number++;
//         return true;
//     }
//     return false;
// }

// #define NECRO_MAX_INDENTATIONS 512

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

// NECRO_RETURN_CODE necro_lex_fixup_layout(NecroLexer* lexer)
// {
//     size_t                pos       = 0;
//     size_t                stack_pos = 1;
//     NecroLexLayoutContext context_stack[NECRO_MAX_INDENTATIONS];
//     for (size_t i = 0; i < NECRO_MAX_INDENTATIONS; ++i)
//         context_stack[i] = (NecroLexLayoutContext){ .indentation = 1, .layout = NECRO_LEX_LAYOUT_LET };
//     do
//     {
//         NECRO_LEX_TOKEN_TYPE type = lexer->tokens.data[pos].token;
//         NecroSourceLoc       loc  = lexer->tokens.data[pos].source_loc;
//         size_t               n    = lexer->tokens.data[pos].white_marker_n;
//         size_t               m    = context_stack[stack_pos].indentation;
//         if (stack_pos >= NECRO_MAX_INDENTATIONS)
//         {
//             necro_error(&lexer->error, loc, "Context stack overflow in lexer, max indentations: %s\n", NECRO_MAX_INDENTATIONS);
//             return NECRO_ERROR;
//         }
//         else if (type == NECRO_LEX_CONTROL_WHITE_MARKER)
//         {
//             if (pos == 0)
//             {
//                 pos++;
//             }
//             else if (n == m && stack_pos != 0)
//             {
//                 NecroLexToken token = { .token = NECRO_LEX_SEMI_COLON,.source_loc = loc };
//                 necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
//                 pos++;
//             }
//             else if (n < m)
//             {
//                 NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE,.source_loc = loc };
//                 necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
//                 stack_pos--;
//             }
//             else
//             {
//                 pos++;
//             }
//         }
//         else if (pos > 0 &&
//                  type == NECRO_LEX_IN &&
//                  context_stack[stack_pos].layout == NECRO_LEX_LAYOUT_LET &&
//                  (lexer->tokens.data[pos - 1].token != NECRO_LEX_CONTROL_WHITE_MARKER &&
//                   lexer->tokens.data[pos - 1].token != NECRO_LEX_RIGHT_BRACE))
//         {
//             NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE,.source_loc = loc };
//             necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
//             necro_push_lex_token_vector(&lexer->layout_fixed_tokens, lexer->tokens.data + pos);
//             pos++;
//             stack_pos--;
//         }
//         else if (type == NECRO_LEX_CONTROL_BRACE_MARKER_LET ||
//                  type == NECRO_LEX_CONTROL_BRACE_MARKER_WHERE ||
//                  type == NECRO_LEX_CONTROL_BRACE_MARKER_OF ||
//                  type == NECRO_LEX_CONTROL_BRACE_MARKER_DO)
//         {
//             if (n > m)
//             {
//                 NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
//                 necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
//                 stack_pos++;
//                 context_stack[stack_pos].indentation = n;
//                 context_stack[stack_pos].layout      = token_type_to_layout(type);
//                 pos++;
//             }
//             else if (n > 0 && stack_pos == 0)
//             {
//                 NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
//                 necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
//                 stack_pos++;
//                 context_stack[stack_pos].indentation = n;
//                 context_stack[stack_pos].layout      = token_type_to_layout(type);
//                 pos++;
//             }
//             else
//             {
//                 NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
//                 necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
//                 NecroLexToken token2 = { .token = NECRO_LEX_RIGHT_BRACE,.source_loc = loc };
//                 necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token2);
//                 lexer->tokens.data[pos].token = NECRO_LEX_CONTROL_WHITE_MARKER;
//             }
//         }
//         else if (type == NECRO_LEX_RIGHT_BRACE)
//         {

//             if (m == 0)
//             {
//                 NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE,.source_loc = loc };
//                 necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
//                 stack_pos--;
//                 pos++;
//             }
//             else
//             {
//                 necro_error(&lexer->error, lexer->tokens.data[pos].source_loc, "Mixing implicit and explicit layouts: Explicit '}' matched against implicit '{'");
//                 return NECRO_ERROR;
//             }
//         }
//         else if (type == NECRO_LEX_LEFT_BRACE)
//         {
//             NecroLexToken token = (NecroLexToken) { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
//             necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
//             pos++;
//             stack_pos++;
//             context_stack[stack_pos].indentation = 0;
//             context_stack[stack_pos].layout      = NECRO_LEX_LAYOUT_MANUAL;
//         }
//         // Note 5 error check?
//         // else (???)
//         // {
//         //     necro_push_lex_token_vector(&lexer->layout_fixed_tokens, lexer->tokens.data + pos);
//         // }
//         else if (type == NECRO_LEX_END_OF_STREAM)
//         {
//             if (stack_pos > 1 && m != 0)
//             {
//                 NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE,.source_loc = loc };
//                 necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
//                 stack_pos--;
//             }
//             else
//             {
//                 NecroLexToken token = { .token = NECRO_LEX_END_OF_STREAM,.source_loc = loc };
//                 necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
//                 break;
//             }
//         }
//         else
//         {
//             necro_push_lex_token_vector(&lexer->layout_fixed_tokens, lexer->tokens.data + pos);
//             pos++;
//         }
//     }
//     while (true);
//     // swap tokens and layout_fixed_tokens
//     NecroLexTokenVector tokens              = lexer->tokens;
//     NecroLexTokenVector layout_fixed_tokens = lexer->layout_fixed_tokens;
//     lexer->layout_fixed_tokens              = tokens;
//     lexer->tokens                           = layout_fixed_tokens;
//     return NECRO_SUCCESS;
// }

// NECRO_RETURN_CODE necro_lex_go(NecroLexer* lexer)
// {
//     while (lexer->str[lexer->pos])
//     {
//         bool matched =
//             necro_lex_whitespace(lexer)            ||
//             // necro_lex_non_ascii(lexer)             ||
//             necro_lex_comments(lexer)              ||
//             necro_lex_string(lexer)                ||
//             necro_lex_char(lexer)                  ||
//             necro_lex_identifier(lexer)            ||
//             necro_lex_number(lexer)                ||
//             necro_lex_multi_character_token(lexer) ||
//             necro_lex_single_character(lexer);
//         // Encountered error
//         if (lexer->error.return_code != NECRO_SUCCESS)
//         {
//             return NECRO_ERROR;
//         }
//         // No Match Error
//         else if (!matched)
//         {
//             necro_error(&lexer->error, (NecroSourceLoc){ lexer->line_number, lexer->character_number, lexer->pos }, "Unrecognized character sequence");
//             return NECRO_ERROR;
//         }
//     }
//     if (lexer->tokens.length <= 0)
//     {
//         necro_error(&lexer->error, (NecroSourceLoc) { lexer->line_number, lexer->character_number, lexer->pos }, "Empty File");
//         return NECRO_ERROR;
//     }
//     NecroLexToken lex_eos_token = (NecroLexToken)
//     {
//         .source_loc = { .line = lexer->line_number,.character = lexer->character_number,.pos = lexer->pos },
//         .token      = NECRO_LEX_END_OF_STREAM
//     };
//     necro_push_lex_token_vector(&lexer->tokens, &lex_eos_token);
//     return NECRO_SUCCESS;
// }

///////////////////////////////////////////////////////
// NecroLexer
///////////////////////////////////////////////////////
uint32_t necro_next_char(NecroLexer* lexer);
NecroLexer necro_create_lexer(const char* str, size_t str_length)
{
    NecroLexer lexer  =
    {
        .str                 = str,
        .str_length          = str_length,
        .loc                 = (NecroSourceLoc) { .pos = 0, .character = 1, .line = 1 },
        .prev_loc            = (NecroSourceLoc) { .pos = 0, .character = 1, .line = 1 },
        .tokens              = necro_create_lex_token_vector(),
        .layout_fixed_tokens = necro_create_lex_token_vector(),
        .intern              = necro_create_intern(),
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
    necro_intern_string(&lexer.intern, "pat");
    necro_intern_string(&lexer.intern, "delay");
    necro_intern_string(&lexer.intern, "trimDelay");
    return lexer;
}

NecroLexer necro_empty_lexer()
{
    return (NecroLexer)
    {
        .str                 = NULL,
        .str_length          = 0,
        .loc                 = (NecroSourceLoc) { .pos = 0, .character = 1, .line = 1 },
        .prev_loc            = (NecroSourceLoc) { .pos = 0, .character = 1, .line = 1 },
        .tokens              = necro_empty_lex_token_vector(),
        .layout_fixed_tokens = necro_empty_lex_token_vector(),
        .intern              = necro_empty_intern(),
    };
}

void necro_destroy_lexer(NecroLexer* lexer)
{
    necro_destroy_lex_token_vector(&lexer->layout_fixed_tokens);
    // These are passed out and are no longer owned by the lexer after lexing!
    // necro_destroy_lex_token_vector(&lexer->layout_fixed_tokens);
    // necro_destroy_intern(&lexer->intern);
}

bool necro_lex_rewind(NecroLexer* lexer)
{
    lexer->loc = lexer->prev_loc;
    return false;
}

bool necro_lex_commit(NecroLexer* lexer)
{
    lexer->prev_loc = lexer->loc;
    return true;
}

uint32_t necro_next_char(NecroLexer* lexer)
{
    if (lexer->loc.pos >= lexer->str_length)
    {
        return 0;
    }
    size_t   num_bytes;
    uint32_t uchar;
    const uint8_t* curr = (uint8_t*) (lexer->str + lexer->loc.pos);

    /*
        Unicode 11, UTF8 - Decoding
        Referencing:
            * http://www.unicode.org/versions/Unicode11.0.0/UnicodeStandard-11.0.pdf
                - Table 3-6. UTF-8 Bit Distribution
                - Table 3-7. Well-Formed UTF-8 Byte Sequences
        If an illegal sequence is found, replace the sequence with 0xFFFD, in accordance with:
            * Page 128: U+FFFD Substitution of Maximal Subparts
    */
    if (curr[0] <= 0x7F)
    {
        num_bytes = 1;
        uchar     = curr[0];
    }
    else if (curr[0] >= 0xC2 && curr[0] <= 0xDF)
    {
        num_bytes = 2;
        if (curr[1] >= 0x80 && curr[1] <= 0xBF)
            uchar =
                ((((uint32_t)curr[0]) & 0x1F) << 6) |
                 (((uint32_t)curr[1]) & 0x3F);
        else
            uchar = 0xFFFD;
    }
    else if (curr[0] == 0xE0)
    {
        num_bytes = 3;
        if ((curr[1] >= 0xA0 && curr[1] <= 0xBF) &&
            (curr[2] >= 0x80 && curr[2] <= 0xBF))
            uchar =
                ((((uint32_t)curr[0]) & 0x1F) << 12) |
                ((((uint32_t)curr[1]) & 0x3F) <<  6) |
                 (((uint32_t)curr[2]) & 0x3F);
        else
            uchar = 0xFFFD;
    }
    else if (curr[0] >= 0xE1 && curr[0] <= 0xEC)
    {
        num_bytes = 3;
        if ((curr[1] >= 0x80 && curr[1] <= 0xBF) &&
            (curr[2] >= 0x80 && curr[2] <= 0xBF))
            uchar =
                ((((uint32_t)curr[0]) & 0x1F) << 12) |
                ((((uint32_t)curr[1]) & 0x3F) <<  6) |
                 (((uint32_t)curr[2]) & 0x3F);
        else
            uchar = 0xFFFD;
    }
    else if (curr[0] == 0xED)
    {
        num_bytes = 3;
        if ((curr[1] >= 0x80 && curr[1] <= 0x9F) &&
            (curr[2] >= 0x80 && curr[2] <= 0xBF))
            uchar =
                ((((uint32_t)curr[0]) & 0x1F) << 12) |
                ((((uint32_t)curr[1]) & 0x3F) <<  6) |
                 (((uint32_t)curr[2]) & 0x3F);
        else
            uchar = 0xFFFD;
    }
    else if (curr[0] >= 0xEE && curr[0] <= 0xEF)
    {
        num_bytes = 3;
        if ((curr[1] >= 0x80 && curr[1] <= 0xBF) &&
            (curr[2] >= 0x80 && curr[2] <= 0xBF))
            uchar =
                ((((uint32_t)curr[0]) & 0x1F) << 12) |
                ((((uint32_t)curr[1]) & 0x3F) <<  6) |
                 (((uint32_t)curr[2]) & 0x3F);
        else
            uchar = 0xFFFD;
    }
    else if (curr[0] == 0xF0)
    {
        num_bytes = 4;
        if ((curr[1] >= 0x90 && curr[1] <= 0xBF) &&
            (curr[2] >= 0x80 && curr[2] <= 0xBF) &&
            (curr[3] >= 0x80 && curr[3] <= 0xBF))
            uchar =
                ((((uint32_t)curr[0]) & 0x7)  << 18) |
                ((((uint32_t)curr[1]) & 0x3F) << 12) |
                ((((uint32_t)curr[2]) & 0x3F) <<  6) |
                 (((uint32_t)curr[3]) & 0x3F);
        else
            uchar = 0xFFFD;
    }
    else if (curr[0] >= 0xF1 && curr[0] <= 0xF3)
    {
        num_bytes = 4;
        if ((curr[1] >= 0x80 && curr[1] <= 0xBF) &&
            (curr[2] >= 0x80 && curr[2] <= 0xBF) &&
            (curr[3] >= 0x80 && curr[3] <= 0xBF))
            uchar =
                ((((uint32_t)curr[0]) & 0x7)  << 18) |
                ((((uint32_t)curr[1]) & 0x3F) << 12) |
                ((((uint32_t)curr[2]) & 0x3F) <<  6) |
                 (((uint32_t)curr[3]) & 0x3F);
        else
            uchar = 0xFFFD;
    }
    else if (curr[0] == 0xF4)
    {
        num_bytes = 4;
        if ((curr[1] >= 0x80 && curr[1] <= 0x8F) &&
            (curr[2] >= 0x80 && curr[2] <= 0xBF) &&
            (curr[3] >= 0x80 && curr[3] <= 0xBF))
            uchar =
                ((((uint32_t)curr[0]) & 0x7)  << 18) |
                ((((uint32_t)curr[1]) & 0x3F) << 12) |
                ((((uint32_t)curr[2]) & 0x3F) <<  6) |
                 (((uint32_t)curr[3]) & 0x3F);
        else
            uchar = 0xFFFD;
    }
    else
    {
        // Illegal pattern, replace with dummy value
        num_bytes = 1;
        uchar     = 0xFFFD;
    }

    lexer->loc.pos += num_bytes;
    if (uchar == (uint32_t)'\n')
    {
        lexer->loc.line++;
        lexer->loc.character = 1;
    }
    else if (necro_is_grapheme_base(uchar))
    {
        lexer->loc.character++;
    }
    else if (uchar == (uint32_t)'\t')
    {
        lexer->loc.character++;
    }
    // printf("code point: %#08X, pos: %u, character: %u, line: %u\n", uchar, lexer->loc.pos, lexer->loc.character, lexer->loc.line);
    return uchar;
}

NecroSourceLoc necro_gobble_up_whitespace_and_comments(NecroLexer* lexer)
{
    NecroSourceLoc loc = lexer->loc;
    while (true)
    {
        uint32_t code_point = necro_next_char(lexer);
        if (necro_is_whitespace(code_point) || code_point == '\n' || code_point == '\r' || code_point == '\t')
        {
            necro_lex_commit(lexer);
            loc = lexer->loc;
        }
        else
        {
            necro_lex_rewind(lexer);
            return loc;
        }
    }
}

bool necro_lex_whitespace(NecroLexer* lexer)
{
    uint32_t code_point = necro_next_char(lexer);
    if (code_point == '\t' || code_point == '\r')
    {
        return necro_lex_commit(lexer);
    }
    else if (necro_is_whitespace(code_point) && code_point != '\n')
    {
        necro_lex_commit(lexer);
        while (necro_is_whitespace(code_point = necro_next_char(lexer)) && code_point != '\n')
        {
            necro_lex_commit(lexer);
        }
        necro_lex_rewind(lexer);
        return true;
    }
    else if (code_point == '\n')
    {
        necro_lex_commit(lexer);
        NecroSourceLoc white_loc = necro_gobble_up_whitespace_and_comments(lexer);
        NecroLexToken  token     = (NecroLexToken)
        {
            .source_loc     = lexer->loc,
            .end_loc        = lexer->loc,
            .token          = NECRO_LEX_CONTROL_WHITE_MARKER,
            .white_marker_n = white_loc.character,
        };
        necro_push_lex_token_vector(&lexer->tokens, &token);
        return necro_lex_commit(lexer);
    }
    return necro_lex_rewind(lexer);
}

bool necro_lex_comments(NecroLexer* lexer)
{
    if (necro_next_char(lexer) != '-' || necro_next_char(lexer) != '-')
        return necro_lex_rewind(lexer);
    necro_lex_commit(lexer);
    uint32_t code_point;
    do
    {
        code_point = necro_next_char(lexer);
    }
    while (code_point != '\n' && code_point != '\0' && lexer->loc.pos < lexer->str_length);
    return necro_lex_commit(lexer);
}

NecroResult(bool) necro_lex_string(NecroLexer* lexer)
{
    NecroSourceLoc source_loc = lexer->loc;
    uint32_t       code_point = necro_next_char(lexer);
    if (code_point != '\"')
        return ok_bool(necro_lex_rewind(lexer));
    size_t         beginning  = lexer->loc.pos;
    code_point = necro_next_char(lexer);
    while (code_point != '\"' && code_point != '\0' && code_point != '\n')
    {
        code_point = necro_next_char(lexer);
    }
    if (code_point == '\0' || code_point == '\n')
    {
        return necro_malformed_string_error(source_loc, lexer->loc);
    }
    size_t           length = lexer->loc.pos - beginning;
    NecroLexToken    token  = (NecroLexToken) { .source_loc = source_loc, .end_loc = lexer->loc, .token = NECRO_LEX_STRING_LITERAL };
    NecroStringSlice slice  = { lexer->str + beginning, length };
    token.symbol            = necro_intern_string_slice(&lexer->intern, slice);
    necro_push_lex_token_vector(&lexer->tokens, &token);
    return ok_bool(necro_lex_commit(lexer));
}

bool necro_lex_char(NecroLexer* lexer)
{
    NecroSourceLoc char_loc    = lexer->loc;
    uint32_t       code_point1 = necro_next_char(lexer);
    uint32_t       code_point2 = necro_next_char(lexer);
    uint32_t       code_point3 = necro_next_char(lexer);
    if (code_point1 != '\''           ||
        necro_is_control(code_point2) ||
        code_point3 != '\'')
        return necro_lex_rewind(lexer);
    NecroLexToken token = (NecroLexToken) { .source_loc = char_loc, .end_loc = lexer->loc, . token = NECRO_LEX_CHAR_LITERAL };
    token.char_literal  = code_point2;
    necro_push_lex_token_vector(&lexer->tokens, &token);
    return necro_lex_commit(lexer);
}

bool necro_lex_int(NecroLexer* lexer, const char* start_str_pos, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    char*   new_str_pos      = NULL;
    int64_t int_value        = strtol(start_str_pos, &new_str_pos, 10);
    assert(((size_t)(new_str_pos - start_str_pos)) > 0);
    NecroLexToken lex_token  = (NecroLexToken) { .source_loc = source_loc, .end_loc = end_loc, .token = NECRO_LEX_INTEGER_LITERAL };
    lex_token.int_literal    = int_value;
    necro_push_lex_token_vector(&lexer->tokens, &lex_token);
    return necro_lex_commit(lexer);
}

bool necro_lex_float(NecroLexer* lexer, const char* start_str_pos, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    char*  new_str_pos       = NULL;
    double float_value       = strtod(start_str_pos, &new_str_pos);
    assert(((size_t)(new_str_pos - start_str_pos)) > 0);
    NecroLexToken lex_token  = (NecroLexToken) { .source_loc = source_loc, .end_loc = end_loc, .token = NECRO_LEX_FLOAT_LITERAL };
    lex_token.double_literal = float_value;
    necro_push_lex_token_vector(&lexer->tokens, &lex_token);
    return necro_lex_commit(lexer);
}

typedef enum
{
    NECRO_LEX_NUM_STATE_INT,
    NECRO_LEX_NUM_STATE_FLOAT_AT_DOT,
    NECRO_LEX_NUM_STATE_FLOAT_POST_DOT,
} NECRO_LEX_NUM_STATE;

NecroResult(bool) necro_lex_number(NecroLexer* lexer)
{
    // Must start with digit or with minus symbol
    NECRO_LEX_NUM_STATE lex_num_state = NECRO_LEX_NUM_STATE_INT;
    NecroSourceLoc      source_loc    = lexer->loc;
    NecroSourceLoc      end_loc       = lexer->loc;
    const char*         num_string    = lexer->str + lexer->loc.pos;
    uint32_t            code_point    = necro_next_char(lexer);
    if (code_point == '-')
    {
        code_point = necro_next_char(lexer);
    }
    if (!necro_is_ascii_digit(code_point))
        return ok_bool(necro_lex_rewind(lexer));
    while (necro_is_ascii_digit(code_point) || code_point == '.')
    {
        if (code_point == '.')
        {
            if (lex_num_state == NECRO_LEX_NUM_STATE_INT)
            {
                lex_num_state = NECRO_LEX_NUM_STATE_FLOAT_AT_DOT;
            }
            else if (lex_num_state == NECRO_LEX_NUM_STATE_FLOAT_AT_DOT)
            {
                // Back-track to lexing int and cough up the double dots for later lexing
                // necro_lex_rewind(lexer);
                lexer->loc.pos       -= 2;
                lexer->loc.character -= 2;
                lexer->prev_loc       = lexer->loc;
                lex_num_state = NECRO_LEX_NUM_STATE_INT;
                break;
            }
            else if (lex_num_state == NECRO_LEX_NUM_STATE_FLOAT_POST_DOT)
            {
                necro_lex_rewind(lexer);
                break;
            }
        }
        else if (lex_num_state == NECRO_LEX_NUM_STATE_FLOAT_AT_DOT)
        {
            lex_num_state = NECRO_LEX_NUM_STATE_FLOAT_POST_DOT;
        }
        end_loc = lexer->loc;
        necro_lex_commit(lexer);
        code_point = necro_next_char(lexer);
    }
    necro_lex_rewind(lexer);
    switch (lex_num_state)
    {
    case NECRO_LEX_NUM_STATE_INT:            return ok_bool(necro_lex_int(lexer, num_string, source_loc, end_loc));
    case NECRO_LEX_NUM_STATE_FLOAT_POST_DOT: return ok_bool(necro_lex_float(lexer, num_string, source_loc, end_loc));
    case NECRO_LEX_NUM_STATE_FLOAT_AT_DOT:   return necro_malformed_float_error(source_loc, end_loc);
    default:                                 return ok_bool(false);
    }
}

bool necro_add_single_character_token(NecroLexer* lexer, NecroSourceLoc source_loc, NECRO_LEX_TOKEN_TYPE token, const char* string)
{
    NecroLexToken lex_token = (NecroLexToken)
    {
        .source_loc = source_loc,
        .end_loc    = source_loc,
        .token      = token,
        .symbol     = necro_intern_string(&lexer->intern, string)
    };
    necro_push_lex_token_vector(&lexer->tokens, &lex_token);
    return necro_lex_commit(lexer);
}

bool necro_lex_single_character(NecroLexer* lexer)
{
    NecroSourceLoc source_loc = lexer->loc;
    switch (necro_next_char(lexer))
    {
    case '+':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_ADD, "+");
    case '-':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_SUB, "-");
    case '*':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_MUL, "*");
    case '/':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_DIV, "/");
    case '%':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_MOD, "%");
    case '<':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_LT, "<");
    case '>':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_GT, ">");
    case ':':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_COLON, ":");
    case ';':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_SEMI_COLON, ";");
    case '[':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_LEFT_BRACKET, "[");
    case ']':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_RIGHT_BRACKET, "]");
    case '(':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_LEFT_PAREN, "(");
    case ')':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_RIGHT_PAREN, ")");
    case '{':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_LEFT_BRACE, "{");
    case '}':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_RIGHT_BRACE, "}");
    case ',':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_COMMA, ",");
    case '_':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_UNDER_SCORE, "_");
    case '=':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_ASSIGN, "=");
    case '?':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_QUESTION_MARK, "?");
    case '!':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_EXCLAMATION, "!");
    case '#':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_HASH, "#");
    case '|':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_PIPE, "|");
    case '.':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_DOT, ".");
    case '@':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_AT, "@");
    case '$':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_DOLLAR, "$");
    case '&':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_AMPERSAND, "&");
    case '^':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_CARET, "^");
    case '\\': return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_BACK_SLASH, "\\");
    case '~':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_TILDE, "~");
    case '`':  return necro_add_single_character_token(lexer, source_loc, NECRO_LEX_ACCENT, "`");
    default:   return necro_lex_rewind(lexer);
    }
}

bool necro_lex_token_with_pattern(NecroLexer* lexer, const char* pattern, NECRO_LEX_TOKEN_TYPE token_type)
{
    NecroSourceLoc source_loc = lexer->loc;
    size_t i = 0;
    while (pattern[i])
    {
        if (necro_next_char(lexer) != (uint8_t)pattern[i]) // All built-in compiler patterns are ASCII characters only
        {
            return necro_lex_rewind(lexer);
        }
        i++;
    }
    NecroLexToken lex_token = (NecroLexToken) { .source_loc = source_loc, .end_loc = lexer->loc, .token = token_type, .symbol = necro_intern_string(&lexer->intern, pattern) };
    necro_push_lex_token_vector(&lexer->tokens, &lex_token);
    return necro_lex_commit(lexer);
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

bool necro_lex_identifier(NecroLexer* lexer)
{
    NecroSourceLoc loc_snapshot     = lexer->loc;
    NecroSourceLoc source_loc       = lexer->loc;
    size_t         identifier_begin = source_loc.pos;
    uint32_t       code_point       = necro_next_char(lexer);
    if (!necro_is_id_start(code_point))
        return necro_lex_rewind(lexer);
    bool is_type_identifier = necro_is_alphabetical(code_point) && necro_is_uppercase(code_point);
    while (necro_is_id_continue(code_point) || code_point == '_' || code_point == '\'')
    {
        loc_snapshot = lexer->loc;
        code_point   = necro_next_char(lexer);
    }

    lexer->loc               = loc_snapshot; // Manual rewind
    size_t identifier_length = lexer->loc.pos - identifier_begin;
    assert(identifier_length > 0);

    // Create Lex token
    NecroLexToken    lex_token = (NecroLexToken) { .source_loc = source_loc, .end_loc = lexer->loc, .token = NECRO_LEX_IDENTIFIER };
    NecroStringSlice slice     = { lexer->str + source_loc.pos, identifier_length };
    lex_token.symbol           = necro_intern_string_slice(&lexer->intern, slice);
    if (lex_token.symbol.id - 1 < NECRO_LEX_END_OF_KEY_WORDS)
        lex_token.token = lex_token.symbol.id - 1;
    else if (is_type_identifier)
        lex_token.token = NECRO_LEX_TYPE_IDENTIFIER;
    necro_push_lex_token_vector(&lexer->tokens, &lex_token);
    necro_lex_commit(lexer);

    // If the identifier is a layout keyword, update indentation levels and add left brace
    if (lex_token.token <= NECRO_LEX_DO)
    {
        necro_gobble_up_whitespace_and_comments(lexer);
        necro_lex_commit(lexer);
        uint32_t keyword_code_point = necro_next_char(lexer);
        // Insert implicit brace marker if an explicit one is not given
        if (keyword_code_point != '{')
        {
            necro_lex_rewind(lexer);
            // BRACE MARKER
            NECRO_LEX_TOKEN_TYPE brace_type;
            switch (lex_token.token)
            {
            case NECRO_LEX_LET:   brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_LET;   break;
            case NECRO_LEX_WHERE: brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_WHERE; break;
            case NECRO_LEX_OF:    brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_OF;    break;
            case NECRO_LEX_DO:    brace_type = NECRO_LEX_CONTROL_BRACE_MARKER_DO;    break;
            default:              return necro_lex_rewind(lexer);
            }
            NecroLexToken token = (NecroLexToken)
            {
                .source_loc     = lexer->loc,
                .end_loc        = lexer->loc,
                .token          = brace_type,
                .brace_marker_n = lexer->loc.character,
            };
            necro_push_lex_token_vector(&lexer->tokens, &token);
        }
    }
    return necro_lex_commit(lexer);
}

NecroResult(NecroUnit) necro_lex_unrecognized_character_sequence(NecroLexer* lexer)
{
    NecroSourceLoc source_loc = lexer->loc;
    uint32_t code_point = necro_next_char(lexer);
    while (code_point != '\n' && code_point != '\0' && !necro_is_whitespace(code_point))
    {
        code_point = necro_next_char(lexer);
    }
    NecroSourceLoc end_loc = lexer->loc;
    return necro_unrecognized_character_sequence_error(source_loc, end_loc);
}

NecroResult(NecroUnit) necro_lex_go(NecroLexer* lexer)
{
    while (lexer->loc.pos < lexer->str_length)
    {
        if (necro_lex_whitespace(lexer))
            continue;
        if (necro_lex_comments(lexer))
            continue;
        bool is_string = necro_try_map(bool, NecroUnit, necro_lex_string(lexer));
        if (is_string)
            continue;
        if (necro_lex_char(lexer))
            continue;
        if (necro_lex_identifier(lexer))
            continue;
        bool is_num = necro_try_map(bool, NecroUnit, necro_lex_number(lexer));
        if (is_num)
            continue;
        if (necro_lex_multi_character_token(lexer))
            continue;
        if (necro_lex_single_character(lexer))
            continue;
        return necro_lex_unrecognized_character_sequence(lexer);
    }
    NecroLexToken lex_eos_token = (NecroLexToken) { .source_loc = lexer->loc, .end_loc = lexer->loc, .token = NECRO_LEX_END_OF_STREAM };
    necro_push_lex_token_vector(&lexer->tokens, &lex_eos_token);
    return ok_NecroUnit(necro_unit);
}

NECRO_DECLARE_VECTOR(NecroLexLayoutContext, NecroLexLayoutContext, lex_layout_context);

NecroResult(NecroUnit) necro_lex_fixup_layout(NecroLexer* lexer)
{
    size_t                      pos                  = 0;
    size_t                      stack_pos            = 1;
    const size_t                initial_size         = 64;
    NecroLexLayoutContextVector context_stack        = necro_create_lex_layout_context_vector();
    NecroLexLayoutContext       initial_context_data = (NecroLexLayoutContext){ .indentation = 1, .layout = NECRO_LEX_LAYOUT_LET };
    for (size_t i = 0; i < initial_size; ++i)
        necro_push_lex_layout_context_vector(&context_stack, &initial_context_data);
    do
    {
        NECRO_LEX_TOKEN_TYPE type    = lexer->tokens.data[pos].token;
        NecroSourceLoc       loc     = lexer->tokens.data[pos].source_loc;
        NecroSourceLoc       end_loc = lexer->tokens.data[pos].end_loc;
        size_t               n       = lexer->tokens.data[pos].white_marker_n;
        size_t               m       = context_stack.data[stack_pos].indentation;
        while (stack_pos >= context_stack.capacity)
        {
            (NecroLexLayoutContext){ .indentation = 1, .layout = NECRO_LEX_LAYOUT_LET };
            necro_push_lex_layout_context_vector(&context_stack, &initial_context_data);
        }
        if (type == NECRO_LEX_CONTROL_WHITE_MARKER)
        {
            if (pos == 0)
            {
                pos++;
            }
            else if (n == m && stack_pos != 0)
            {
                NecroLexToken token = { .token = NECRO_LEX_SEMI_COLON, .source_loc = loc, .end_loc = end_loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                pos++;
            }
            else if (n < m)
            {
                NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE, .source_loc = loc, .end_loc = end_loc };
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
                 context_stack.data[stack_pos].layout == NECRO_LEX_LAYOUT_LET &&
                 (lexer->tokens.data[pos - 1].token != NECRO_LEX_CONTROL_WHITE_MARKER &&
                  lexer->tokens.data[pos - 1].token != NECRO_LEX_RIGHT_BRACE))
        {
            NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE, .source_loc = loc, .end_loc = end_loc };
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
                NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE, .source_loc = loc, .end_loc = end_loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                stack_pos++;
                context_stack.data[stack_pos].indentation = n;
                context_stack.data[stack_pos].layout      = token_type_to_layout(type);
                pos++;
            }
            else if (n > 0 && stack_pos == 0)
            {
                NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                stack_pos++;
                context_stack.data[stack_pos].indentation = n;
                context_stack.data[stack_pos].layout      = token_type_to_layout(type);
                pos++;
            }
            else
            {
                NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE, .source_loc = loc, .end_loc = end_loc};
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                NecroLexToken token2 = { .token = NECRO_LEX_RIGHT_BRACE, .source_loc = loc, .end_loc = end_loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token2);
                lexer->tokens.data[pos].token = NECRO_LEX_CONTROL_WHITE_MARKER;
            }
        }
        else if (type == NECRO_LEX_RIGHT_BRACE)
        {

            if (m == 0)
            {
                NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE, .source_loc = loc, .end_loc = end_loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                stack_pos--;
                pos++;
            }
            else
            {
                return necro_mixed_braces_error(lexer->tokens.data[pos].source_loc, lexer->tokens.data[pos].source_loc);
            }
        }
        else if (type == NECRO_LEX_LEFT_BRACE)
        {
            NecroLexToken token = (NecroLexToken) { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
            necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
            pos++;
            stack_pos++;
            context_stack.data[stack_pos].indentation = 0;
            context_stack.data[stack_pos].layout      = NECRO_LEX_LAYOUT_MANUAL;
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
                NecroLexToken token = { .token = NECRO_LEX_RIGHT_BRACE, .source_loc = loc, .end_loc = end_loc};
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                stack_pos--;
            }
            else
            {
                NecroLexToken token = { .token = NECRO_LEX_END_OF_STREAM, .source_loc = loc, .end_loc = end_loc };
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
    necro_destroy_lex_layout_context_vector(&context_stack);
    return ok_NecroUnit(necro_unit);
}

NecroResult(NecroUnit) necro_lex(const char* str, size_t str_length, NecroIntern* out_intern, NecroLexTokenVector* out_tokens, NecroCompileInfo info)
{
    NecroLexer lexer = necro_create_lexer(str, str_length);
    necro_try(NecroUnit, necro_lex_go(&lexer));
    necro_try(NecroUnit, necro_lex_fixup_layout(&lexer));
    if (info.verbosity > 0)
        necro_print_lexer(&lexer);
    necro_destroy_lexer(&lexer);
    *out_intern = lexer.intern;
    *out_tokens = lexer.tokens;
    return ok_NecroUnit(necro_unit);
}

///////////////////////////////////////////////////////
// Printing
///////////////////////////////////////////////////////
void necro_print_lex_token(NecroLexer* lexer, size_t token_id)
{
    printf("(line: %zu, char: %zu, pos: %zu), ", lexer->tokens.data[token_id].source_loc.line, lexer->tokens.data[token_id].source_loc.character, lexer->tokens.data[token_id].source_loc.pos);
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
        printf("let {n}:    %zu\n", lexer->tokens.data[token_id].brace_marker_n);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_WHERE)
    {
        printf("where {n}:  %zu\n", lexer->tokens.data[token_id].brace_marker_n);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_OF)
    {
        printf("of {n}:     %zu\n", lexer->tokens.data[token_id].brace_marker_n);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_DO)
    {
        printf("do {n}:     %zu\n", lexer->tokens.data[token_id].brace_marker_n);
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_BRACE_MARKER_DO)
    {
        printf("pat\n");
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_CONTROL_WHITE_MARKER)
    {
        printf("<n>:        %zu\n", lexer->tokens.data[token_id].brace_marker_n);
    }
    else
    {
        printf("%s\n", necro_lex_token_type_string(lexer->tokens.data[token_id].token));
    }
}

void necro_print_lexer(NecroLexer* lexer)
{
    printf("NecroLexer\n{\n");
    printf("    line:      %zu,\n", lexer->loc.line);
    printf("    character: %zu,\n", lexer->loc.character);
    printf("    tokens:\n    [\n");
    for (size_t i = 0; i < lexer->tokens.length; ++i)
    {
        printf("        ");
        necro_print_lex_token(lexer, i);
    }
    printf("    ]\n");
    printf("}\n\n");
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_test_lexer()
{
    necro_announce_phase("NecroLexer");

    // Test 1
    {
        const char* str    = "    303. This is a test";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        NecroResult(NecroUnit) result = necro_lex_go(&lexer);
        assert(result.type == NECRO_RESULT_ERROR);
        assert(result.error.type == NECRO_LEX_MALFORMED_FLOAT);
        // necro_print_lexer(&lexer);
        // necro_print_result_errors(result.errors, result.num_errors, str, "lexerTest.necro");
        printf("Lex float error test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 2
    {
        const char* str    = "3 4 56 \"Hello all you fuckers";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        NecroResult(NecroUnit) result = necro_lex_go(&lexer);
        assert(result.type == NECRO_RESULT_ERROR);
        assert(result.error.type == NECRO_LEX_MALFORMED_STRING);
        // necro_print_lexer(&lexer);
        // necro_print_result_errors(result.errors, result.num_errors, str, "lexerTest.necro");
        printf("Lex string error test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 3
    {
        const char* str    = "123 456 789";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        assert(lexer.tokens.length == 4);
        assert(lexer.tokens.data[0].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[0].int_literal == 123);
        assert(lexer.tokens.data[1].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[1].int_literal == 456);
        assert(lexer.tokens.data[2].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[2].int_literal == 789);
        assert(lexer.tokens.data[3].token == NECRO_LEX_END_OF_STREAM);
        // necro_print_lexer(&lexer);
        printf("Lex number test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 4
    {
        const char* str    = "helloWorld";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        assert(lexer.tokens.length == 2);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(&lexer.intern, "helloWorld").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_END_OF_STREAM);
        // necro_print_lexer(&lexer);
        printf("Lex identifier test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Useful site for testing / comparison http://www.cogsci.ed.ac.uk/~richard/utf-8.cgi?input=e9&mode=hex

    // TODO: normalize / canonicalize?

    // Test 5
    {
        const char* str    = "grav\xC3\xA9";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        assert(lexer.tokens.length == 2);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(&lexer.intern, "grav\xC3\xA9").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_END_OF_STREAM);
        // necro_print_lexer(&lexer);
        printf("Lex unicode identifier test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 6
    {
        const char* str    = "!@#$%^&*(~)_+-=";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 16);
        assert(lexer.tokens.data[0].token == NECRO_LEX_EXCLAMATION);
        assert(lexer.tokens.data[1].token == NECRO_LEX_AT);
        assert(lexer.tokens.data[2].token == NECRO_LEX_HASH);
        assert(lexer.tokens.data[3].token == NECRO_LEX_DOLLAR);
        assert(lexer.tokens.data[4].token == NECRO_LEX_MOD);
        assert(lexer.tokens.data[5].token == NECRO_LEX_CARET);
        assert(lexer.tokens.data[6].token == NECRO_LEX_AMPERSAND);
        assert(lexer.tokens.data[7].token == NECRO_LEX_MUL);
        assert(lexer.tokens.data[8].token == NECRO_LEX_LEFT_PAREN);
        assert(lexer.tokens.data[9].token == NECRO_LEX_TILDE);
        assert(lexer.tokens.data[10].token == NECRO_LEX_RIGHT_PAREN);
        assert(lexer.tokens.data[11].token == NECRO_LEX_UNDER_SCORE);
        assert(lexer.tokens.data[12].token == NECRO_LEX_ADD);
        assert(lexer.tokens.data[13].token == NECRO_LEX_SUB);
        assert(lexer.tokens.data[14].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[15].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex single char test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 7
    {
        const char* str    = "+ - * / % > < >= <= :: << >> | |> <| == /= && || >>= =<< !! <>";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 24);
        assert(lexer.tokens.data[0].token == NECRO_LEX_ADD);
        assert(lexer.tokens.data[1].token == NECRO_LEX_SUB);
        assert(lexer.tokens.data[2].token == NECRO_LEX_MUL);
        assert(lexer.tokens.data[3].token == NECRO_LEX_DIV);
        assert(lexer.tokens.data[4].token == NECRO_LEX_MOD);
        assert(lexer.tokens.data[5].token == NECRO_LEX_GT);
        assert(lexer.tokens.data[6].token == NECRO_LEX_LT);
        assert(lexer.tokens.data[7].token == NECRO_LEX_GTE);
        assert(lexer.tokens.data[8].token == NECRO_LEX_LTE);
        assert(lexer.tokens.data[9].token == NECRO_LEX_DOUBLE_COLON);
        assert(lexer.tokens.data[10].token == NECRO_LEX_LEFT_SHIFT);
        assert(lexer.tokens.data[11].token == NECRO_LEX_RIGHT_SHIFT);
        assert(lexer.tokens.data[12].token == NECRO_LEX_PIPE);
        assert(lexer.tokens.data[13].token == NECRO_LEX_BACK_PIPE);
        assert(lexer.tokens.data[14].token == NECRO_LEX_FORWARD_PIPE);
        assert(lexer.tokens.data[15].token == NECRO_LEX_EQUALS);
        assert(lexer.tokens.data[16].token == NECRO_LEX_NOT_EQUALS);
        assert(lexer.tokens.data[17].token == NECRO_LEX_AND);
        assert(lexer.tokens.data[18].token == NECRO_LEX_OR);
        assert(lexer.tokens.data[19].token == NECRO_LEX_BIND_RIGHT);
        assert(lexer.tokens.data[20].token == NECRO_LEX_BIND_LEFT);
        assert(lexer.tokens.data[21].token == NECRO_LEX_DOUBLE_EXCLAMATION);
        assert(lexer.tokens.data[22].token == NECRO_LEX_APPEND);
        assert(lexer.tokens.data[23].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex multi-char test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 8
    {
        const char* str    = "\'a\' \'b\' \'c\' \'!\'";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 5);
        assert(lexer.tokens.data[0].token == NECRO_LEX_CHAR_LITERAL);
        assert(lexer.tokens.data[0].char_literal == 'a');
        assert(lexer.tokens.data[1].token == NECRO_LEX_CHAR_LITERAL);
        assert(lexer.tokens.data[1].char_literal == 'b');
        assert(lexer.tokens.data[2].token == NECRO_LEX_CHAR_LITERAL);
        assert(lexer.tokens.data[2].char_literal == 'c');
        assert(lexer.tokens.data[3].token == NECRO_LEX_CHAR_LITERAL);
        assert(lexer.tokens.data[3].char_literal == '!');
        assert(lexer.tokens.data[4].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex char literal test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 9
    {
        const char* str    = "\'\xF0\x9F\x98\x80\' yep";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 3);
        assert(lexer.tokens.data[0].token == NECRO_LEX_CHAR_LITERAL);
        assert(lexer.tokens.data[0].char_literal == 0x0001F600); // Grinning face
        assert(lexer.tokens.data[1].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[1].symbol.id == necro_intern_string(&lexer.intern, "yep").id);
        assert(lexer.tokens.data[1].source_loc.character == 5);
        assert(lexer.tokens.data[2].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex unicode char literal test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 10
    {
        const char* str    = "case class data deriving forall if else then import instance in module newtype type pat";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 16);
        assert(lexer.tokens.data[0].token == NECRO_LEX_CASE);
        assert(lexer.tokens.data[1].token == NECRO_LEX_CLASS);
        assert(lexer.tokens.data[2].token == NECRO_LEX_DATA);
        assert(lexer.tokens.data[3].token == NECRO_LEX_DERIVING);
        assert(lexer.tokens.data[4].token == NECRO_LEX_FORALL);
        assert(lexer.tokens.data[5].token == NECRO_LEX_IF);
        assert(lexer.tokens.data[6].token == NECRO_LEX_ELSE);
        assert(lexer.tokens.data[7].token == NECRO_LEX_THEN);
        assert(lexer.tokens.data[8].token == NECRO_LEX_IMPORT);
        assert(lexer.tokens.data[9].token == NECRO_LEX_INSTANCE);
        assert(lexer.tokens.data[10].token == NECRO_LEX_IN);
        assert(lexer.tokens.data[11].token == NECRO_LEX_MODULE);
        assert(lexer.tokens.data[12].token == NECRO_LEX_NEWTYPE);
        assert(lexer.tokens.data[13].token == NECRO_LEX_TYPE);
        assert(lexer.tokens.data[14].token == NECRO_LEX_PAT);
        assert(lexer.tokens.data[15].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex keyword test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 11
    {
        const char* str    = "\xE0\xA6\x95\xE0\xA7\x80"; // BENGALI LETTER KA + BENGALI VOWEL SIGN II, Combining characters
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 2);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(&lexer.intern, "\xE0\xA6\x95\xE0\xA7\x80").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_END_OF_STREAM);
        assert(lexer.loc.character == 3);
        printf("Lex combining character test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 12
    {
        const char* str    = "x = y where\n  y = 10";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 9);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(&lexer.intern, "x").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[2].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[2].symbol.id == necro_intern_string(&lexer.intern, "y").id);
        assert(lexer.tokens.data[3].token == NECRO_LEX_WHERE);
        assert(lexer.tokens.data[4].token == NECRO_LEX_CONTROL_BRACE_MARKER_WHERE);
        assert(lexer.tokens.data[4].brace_marker_n == 3);
        assert(lexer.tokens.data[5].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[5].symbol.id == necro_intern_string(&lexer.intern, "y").id);
        assert(lexer.tokens.data[6].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[7].token == NECRO_LEX_INTEGER_LITERAL); // 10
        assert(lexer.tokens.data[7].int_literal == 10);
        assert(lexer.tokens.data[8].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex where test 1: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 13
    {
        const char* str    = "x = y where\n  y = 10\n  z = 20\nnext :: Int";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 17);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(&lexer.intern, "x").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[2].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[2].symbol.id == necro_intern_string(&lexer.intern, "y").id);
        assert(lexer.tokens.data[3].token == NECRO_LEX_WHERE);
        assert(lexer.tokens.data[4].token == NECRO_LEX_CONTROL_BRACE_MARKER_WHERE);
        assert(lexer.tokens.data[4].brace_marker_n == 3);
        assert(lexer.tokens.data[5].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[5].symbol.id == necro_intern_string(&lexer.intern, "y").id);
        assert(lexer.tokens.data[6].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[7].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[7].int_literal == 10);
        assert(lexer.tokens.data[8].token == NECRO_LEX_CONTROL_WHITE_MARKER);
        assert(lexer.tokens.data[8].white_marker_n == 3);
        assert(lexer.tokens.data[9].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[9].symbol.id == necro_intern_string(&lexer.intern, "z").id);
        assert(lexer.tokens.data[10].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[11].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[11].int_literal == 20);
        assert(lexer.tokens.data[12].token == NECRO_LEX_CONTROL_WHITE_MARKER);
        assert(lexer.tokens.data[12].white_marker_n == 1);
        assert(lexer.tokens.data[13].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[13].symbol.id == necro_intern_string(&lexer.intern, "next").id);
        assert(lexer.tokens.data[14].token == NECRO_LEX_DOUBLE_COLON);
        assert(lexer.tokens.data[15].token == NECRO_LEX_TYPE_IDENTIFIER);
        assert(lexer.tokens.data[15].symbol.id == necro_intern_string(&lexer.intern, "Int").id);
        assert(lexer.tokens.data[16].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex where test 2: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Test 13
    {
        const char* str    = "let\n  x = 10\n  y = 20\nin\n  x";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 14);
        assert(lexer.tokens.data[0].token == NECRO_LEX_LET);
        assert(lexer.tokens.data[1].token == NECRO_LEX_CONTROL_BRACE_MARKER_LET);
        assert(lexer.tokens.data[1].brace_marker_n == 3);
        assert(lexer.tokens.data[2].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[2].symbol.id == necro_intern_string(&lexer.intern, "x").id);
        assert(lexer.tokens.data[3].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[4].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[4].int_literal == 10);
        assert(lexer.tokens.data[5].token == NECRO_LEX_CONTROL_WHITE_MARKER);
        assert(lexer.tokens.data[5].white_marker_n == 3);
        assert(lexer.tokens.data[6].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[6].symbol.id == necro_intern_string(&lexer.intern, "y").id);
        assert(lexer.tokens.data[7].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[8].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[8].int_literal == 20);
        assert(lexer.tokens.data[9].token == NECRO_LEX_CONTROL_WHITE_MARKER);
        assert(lexer.tokens.data[9].white_marker_n == 1);
        assert(lexer.tokens.data[10].token == NECRO_LEX_IN);
        assert(lexer.tokens.data[11].token == NECRO_LEX_CONTROL_WHITE_MARKER);
        assert(lexer.tokens.data[11].white_marker_n == 3);
        assert(lexer.tokens.data[12].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[12].symbol.id == necro_intern_string(&lexer.intern, "x").id);
        assert(lexer.tokens.data[13].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex let test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Fuzz test
    {
        for (size_t n = 0; n < 20; ++n)
        {
            srand(666 + n);
            char noise[4096];
            for (size_t i = 0; i < 4096; ++i)
            {
                char c = (char)(rand() % 127);
                if (c == NECRO_LEX_END_OF_STREAM || c == '\0')
                    c = ' ';
                noise[i] = c;
            }
            noise[4095] = '\0';
            NecroLexer lexer = necro_create_lexer(noise, 4095);
            necro_lex_go(&lexer);
            // NecroResult(bool) result = necro_lex_go(&lexer);
            // necro_print_result_errors(result.errors, result.num_errors, noise, "noiseTest.necro");
            // necro_print_lexer(&lexer);
            necro_destroy_lexer(&lexer);
        }
        printf("Lex fuzz test 1: passed\n");
    }

    // Fuzz test
    {
        for (size_t n = 0; n < 20; ++n)
        {
            srand(666 + n);
            char noise[4096];
            for (size_t i = 0; i < 4096; ++i)
            {
                char c = (char)(rand() % 256);
                if (c == NECRO_LEX_END_OF_STREAM || c == '\0')
                    c = ' ';
                noise[i] = c;
            }
            noise[4095] = '\0';
            NecroLexer lexer = necro_create_lexer(noise, 4095);
            necro_lex_go(&lexer);
            // NecroResult(bool) result = necro_lex_go(&lexer);
            // necro_print_result_errors(result.errors, result.num_errors, noise, "noiseTest.necro");
            // necro_print_lexer(&lexer);
            necro_destroy_lexer(&lexer);
        }
        printf("Lex fuzz test 2: passed\n");
    }

    // String Fuzz test
    {
        for (size_t n = 0; n < 20; ++n)
        {
            srand(666 + n);
            char noise[4096];
            for (size_t i = 0; i < 4096; ++i)
            {
                char c = (char)(rand() % 127);
                if (c == NECRO_LEX_END_OF_STREAM || c == '\0' || c == '\"' || c == '\n')
                    c = ' ';
                noise[i] = c;
            }
            noise[0] = '\"';
            noise[4094] = '\"';
            noise[4095] = '\0';
            NecroLexer lexer = necro_create_lexer(noise, 4095);
            unwrap(NecroUnit, necro_lex_go(&lexer)); // Should be no errors!
            // NecroResult(bool) result = necro_lex_go(&lexer);
            // necro_print_result_errors(result.errors, result.num_errors, noise, "noiseTest.necro");
            // necro_print_lexer(&lexer);
            necro_destroy_lexer(&lexer);
        }
        printf("Lex string fuzz test: passed\n");
    }

    // Layout test 1
    {
        const char* str    = "test = test2 where\n  test2 = 300";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        unwrap(NecroUnit, necro_lex_fixup_layout(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 10);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(&lexer.intern, "test").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[2].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[2].symbol.id == necro_intern_string(&lexer.intern, "test2").id);
        assert(lexer.tokens.data[3].token == NECRO_LEX_WHERE);
        assert(lexer.tokens.data[4].token == NECRO_LEX_LEFT_BRACE);
        assert(lexer.tokens.data[5].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[5].symbol.id == necro_intern_string(&lexer.intern, "test2").id);
        assert(lexer.tokens.data[6].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[7].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[7].int_literal == 300);
        assert(lexer.tokens.data[8].token == NECRO_LEX_RIGHT_BRACE);
        assert(lexer.tokens.data[9].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex layout test 1: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Layout test 2
    {
        const char* str    = "test = test2 + test3 + test4 + test5 where\n  test2 = 2 where\n    test3 = 3\n    test4 = 4.0\n  test5 = 5";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        unwrap(NecroUnit, necro_lex_fixup_layout(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 30);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(&lexer.intern, "test").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[2].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[2].symbol.id == necro_intern_string(&lexer.intern, "test2").id);
        assert(lexer.tokens.data[3].token == NECRO_LEX_ADD);
        assert(lexer.tokens.data[4].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[4].symbol.id == necro_intern_string(&lexer.intern, "test3").id);
        assert(lexer.tokens.data[5].token == NECRO_LEX_ADD);
        assert(lexer.tokens.data[6].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[6].symbol.id == necro_intern_string(&lexer.intern, "test4").id);
        assert(lexer.tokens.data[7].token == NECRO_LEX_ADD);
        assert(lexer.tokens.data[8].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[8].symbol.id == necro_intern_string(&lexer.intern, "test5").id);
        assert(lexer.tokens.data[9].token == NECRO_LEX_WHERE);
        assert(lexer.tokens.data[10].token == NECRO_LEX_LEFT_BRACE);
        assert(lexer.tokens.data[11].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[11].symbol.id == necro_intern_string(&lexer.intern, "test2").id);
        assert(lexer.tokens.data[12].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[13].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[13].int_literal == 2);
        assert(lexer.tokens.data[14].token == NECRO_LEX_WHERE);
        assert(lexer.tokens.data[15].token == NECRO_LEX_LEFT_BRACE);
        assert(lexer.tokens.data[16].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[16].symbol.id == necro_intern_string(&lexer.intern, "test3").id);
        assert(lexer.tokens.data[17].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[18].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[18].int_literal == 3);
        assert(lexer.tokens.data[19].token == NECRO_LEX_SEMI_COLON);
        assert(lexer.tokens.data[20].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[20].symbol.id == necro_intern_string(&lexer.intern, "test4").id);
        assert(lexer.tokens.data[21].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[22].token == NECRO_LEX_FLOAT_LITERAL);
        assert(lexer.tokens.data[22].double_literal == 4.0);
        assert(lexer.tokens.data[23].token == NECRO_LEX_RIGHT_BRACE);
        assert(lexer.tokens.data[24].token == NECRO_LEX_SEMI_COLON);
        assert(lexer.tokens.data[25].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[25].symbol.id == necro_intern_string(&lexer.intern, "test5").id);
        assert(lexer.tokens.data[26].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[27].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[27].int_literal == 5);
        assert(lexer.tokens.data[28].token == NECRO_LEX_RIGHT_BRACE);
        assert(lexer.tokens.data[29].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex layout test 2: Passed\n");
        necro_destroy_lexer(&lexer);
    }

    // Mixed braces error test
    {
        const char* str    = "test = x where\n  x = 4 }";
        NecroLexer  lexer  = necro_create_lexer(str, strlen(str));
        unwrap(NecroUnit, necro_lex_go(&lexer));
        NecroResult(NecroUnit) result = necro_lex_fixup_layout(&lexer);
        assert(result.type == NECRO_RESULT_ERROR);
        assert(result.error.type == NECRO_LEX_MIXED_BRACES);
        // necro_print_result_errors(result.errors, result.num_errors, str, "mixedBracesErrorTest.necro");
        // necro_print_lexer(&lexer);
        printf("Lex mixed braces error test: Passed\n");
        necro_destroy_lexer(&lexer);
    }

}
