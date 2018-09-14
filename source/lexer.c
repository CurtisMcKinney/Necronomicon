 /* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <ctype.h>
#include "lexer.h"
#include "unicode_properties.h"

void necro_lex_print(NecroLexer* lexer);
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

NECRO_LEX_LAYOUT necro_token_type_to_layout(NECRO_LEX_TOKEN_TYPE type)
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

uint32_t necro_lex_next_char(NecroLexer* lexer);
NecroLexer necro_lexer_create(const char* str, size_t str_length, NecroIntern* intern)
{
    NecroLexer lexer  =
    {
        .str                 = str,
        .str_length          = str_length,
        .loc                 = (NecroSourceLoc) { .pos = 0, .character = 1, .line = 1 },
        .prev_loc            = (NecroSourceLoc) { .pos = 0, .character = 1, .line = 1 },
        .tokens              = necro_create_lex_token_vector(),
        .layout_fixed_tokens = necro_create_lex_token_vector(),
        .intern              = intern,
    };

    // Intern keywords, in the same order as their listing in the NECRO_LEX_TOKEN_TYPE enum
    // MAKE SURE THAT THE FIRST N ENTRIES IN NECRO_LEX_TOKEN_TYPE ARE THE KEYWORD TYPES AND THAT THEY EXACTLY MATCH THEIR SYMBOLS MINUS ONE!!!!
    necro_intern_string(lexer.intern, "let");
    necro_intern_string(lexer.intern, "where");
    necro_intern_string(lexer.intern, "of");
    necro_intern_string(lexer.intern, "do");
    necro_intern_string(lexer.intern, "case");
    necro_intern_string(lexer.intern, "class");
    necro_intern_string(lexer.intern, "data");
    necro_intern_string(lexer.intern, "deriving");
    necro_intern_string(lexer.intern, "forall");
    necro_intern_string(lexer.intern, "if");
    necro_intern_string(lexer.intern, "else");
    necro_intern_string(lexer.intern, "then");
    necro_intern_string(lexer.intern, "import");
    necro_intern_string(lexer.intern, "instance");
    necro_intern_string(lexer.intern, "in");
    necro_intern_string(lexer.intern, "module");
    necro_intern_string(lexer.intern, "newtype");
    necro_intern_string(lexer.intern, "type");
    necro_intern_string(lexer.intern, "pat");
    necro_intern_string(lexer.intern, "delay");
    necro_intern_string(lexer.intern, "trimDelay");
    return lexer;
}

NecroLexer necro_lexer_empty()
{
    return (NecroLexer)
    {
        .str                 = NULL,
        .str_length          = 0,
        .loc                 = (NecroSourceLoc) { .pos = 0, .character = 1, .line = 1 },
        .prev_loc            = (NecroSourceLoc) { .pos = 0, .character = 1, .line = 1 },
        .tokens              = necro_empty_lex_token_vector(),
        .layout_fixed_tokens = necro_empty_lex_token_vector(),
        .intern              = NULL,
    };
}

void necro_lexer_destroy(NecroLexer* lexer)
{
    necro_destroy_lex_token_vector(&lexer->layout_fixed_tokens);
    // Ownership of lex tokens is passed out after lex phase
    // necro_destroy_lex_token_vector(&lexer->tokens);
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

uint32_t necro_lex_next_char(NecroLexer* lexer)
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

NecroSourceLoc necro_lex_gobble_up_whitespace_and_comments(NecroLexer* lexer)
{
    NecroSourceLoc loc = lexer->loc;
    while (true)
    {
        uint32_t code_point = necro_lex_next_char(lexer);
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
    uint32_t code_point = necro_lex_next_char(lexer);
    if (code_point == '\t' || code_point == '\r')
    {
        return necro_lex_commit(lexer);
    }
    else if (necro_is_whitespace(code_point) && code_point != '\n')
    {
        necro_lex_commit(lexer);
        while (necro_is_whitespace(code_point = necro_lex_next_char(lexer)) && code_point != '\n')
        {
            necro_lex_commit(lexer);
        }
        necro_lex_rewind(lexer);
        return true;
    }
    else if (code_point == '\n')
    {
        necro_lex_commit(lexer);
        NecroSourceLoc white_loc = necro_lex_gobble_up_whitespace_and_comments(lexer);
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
    if (necro_lex_next_char(lexer) != '-' || necro_lex_next_char(lexer) != '-')
        return necro_lex_rewind(lexer);
    necro_lex_commit(lexer);
    uint32_t code_point;
    do
    {
        code_point = necro_lex_next_char(lexer);
    }
    while (code_point != '\n' && code_point != '\0' && lexer->loc.pos < lexer->str_length);
    return necro_lex_commit(lexer);
}

NecroResult(bool) necro_lex_string(NecroLexer* lexer)
{
    NecroSourceLoc source_loc = lexer->loc;
    uint32_t       code_point = necro_lex_next_char(lexer);
    if (code_point != '\"')
        return ok_bool(necro_lex_rewind(lexer));
    size_t         beginning  = lexer->loc.pos;
    size_t         end        = lexer->loc.pos;
    code_point = necro_lex_next_char(lexer);
    while (code_point != '\"' && code_point != '\0' && code_point != '\n')
    {
        end        = lexer->loc.pos;
        code_point = necro_lex_next_char(lexer);
    }
    if (code_point == '\0' || code_point == '\n')
    {
        return necro_malformed_string_error(source_loc, lexer->loc);
    }
    size_t           length = end - beginning;
    NecroLexToken    token  = (NecroLexToken) { .source_loc = source_loc, .end_loc = lexer->loc, .token = NECRO_LEX_STRING_LITERAL };
    NecroStringSlice slice  = { lexer->str + beginning, length };
    token.symbol            = necro_intern_string_slice(lexer->intern, slice);
    necro_push_lex_token_vector(&lexer->tokens, &token);
    return ok_bool(necro_lex_commit(lexer));
}

bool necro_lex_char(NecroLexer* lexer)
{
    NecroSourceLoc char_loc    = lexer->loc;
    uint32_t       code_point1 = necro_lex_next_char(lexer);
    uint32_t       code_point2 = necro_lex_next_char(lexer);
    uint32_t       code_point3 = necro_lex_next_char(lexer);
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
    uint32_t            code_point    = necro_lex_next_char(lexer);
    if (code_point == '-')
    {
        code_point = necro_lex_next_char(lexer);
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
        code_point = necro_lex_next_char(lexer);
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

bool necro_lex_add_single_character_token(NecroLexer* lexer, NecroSourceLoc source_loc, NECRO_LEX_TOKEN_TYPE token, const char* string)
{
    NecroLexToken lex_token = (NecroLexToken)
    {
        .source_loc = source_loc,
        .end_loc    = source_loc,
        .token      = token,
        .symbol     = necro_intern_string(lexer->intern, string)
    };
    necro_push_lex_token_vector(&lexer->tokens, &lex_token);
    return necro_lex_commit(lexer);
}

bool necro_lex_single_character(NecroLexer* lexer)
{
    NecroSourceLoc source_loc = lexer->loc;
    switch (necro_lex_next_char(lexer))
    {
    case '+':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_ADD, "+");
    case '-':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_SUB, "-");
    case '*':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_MUL, "*");
    case '/':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_DIV, "/");
    case '%':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_MOD, "%");
    case '<':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_LT, "<");
    case '>':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_GT, ">");
    case ':':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_COLON, ":");
    case ';':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_SEMI_COLON, ";");
    case '[':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_LEFT_BRACKET, "[");
    case ']':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_RIGHT_BRACKET, "]");
    case '(':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_LEFT_PAREN, "(");
    case ')':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_RIGHT_PAREN, ")");
    case '{':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_LEFT_BRACE, "{");
    case '}':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_RIGHT_BRACE, "}");
    case ',':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_COMMA, ",");
    case '_':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_UNDER_SCORE, "_");
    case '=':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_ASSIGN, "=");
    case '?':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_QUESTION_MARK, "?");
    case '!':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_EXCLAMATION, "!");
    case '#':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_HASH, "#");
    case '|':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_PIPE, "|");
    case '.':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_DOT, ".");
    case '@':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_AT, "@");
    case '$':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_DOLLAR, "$");
    case '&':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_AMPERSAND, "&");
    case '^':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_CARET, "^");
    case '\\': return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_BACK_SLASH, "\\");
    case '~':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_TILDE, "~");
    case '`':  return necro_lex_add_single_character_token(lexer, source_loc, NECRO_LEX_ACCENT, "`");
    default:   return necro_lex_rewind(lexer);
    }
}

bool necro_lex_token_with_pattern(NecroLexer* lexer, const char* pattern, NECRO_LEX_TOKEN_TYPE token_type)
{
    NecroSourceLoc source_loc = lexer->loc;
    size_t i = 0;
    while (pattern[i])
    {
        if (necro_lex_next_char(lexer) != (uint8_t)pattern[i]) // All built-in compiler patterns are ASCII characters only
        {
            return necro_lex_rewind(lexer);
        }
        i++;
    }
    NecroLexToken lex_token = (NecroLexToken) { .source_loc = source_loc, .end_loc = lexer->loc, .token = token_type, .symbol = necro_intern_string(lexer->intern, pattern) };
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
    uint32_t       code_point       = necro_lex_next_char(lexer);
    if (!necro_is_id_start(code_point))
        return necro_lex_rewind(lexer);
    bool is_type_identifier = necro_is_alphabetical(code_point) && necro_is_uppercase(code_point);
    while (necro_is_id_continue(code_point) || code_point == '_' || code_point == '\'')
    {
        loc_snapshot = lexer->loc;
        code_point   = necro_lex_next_char(lexer);
    }

    lexer->loc               = loc_snapshot; // Manual rewind
    size_t identifier_length = lexer->loc.pos - identifier_begin;
    assert(identifier_length > 0);

    // Create Lex token
    NecroLexToken    lex_token = (NecroLexToken) { .source_loc = source_loc, .end_loc = lexer->loc, .token = NECRO_LEX_IDENTIFIER };
    NecroStringSlice slice     = { lexer->str + source_loc.pos, identifier_length };
    lex_token.symbol           = necro_intern_string_slice(lexer->intern, slice);
    if (lex_token.symbol.id - 1 < NECRO_LEX_END_OF_KEY_WORDS)
        lex_token.token = lex_token.symbol.id - 1;
    else if (is_type_identifier)
        lex_token.token = NECRO_LEX_TYPE_IDENTIFIER;
    necro_push_lex_token_vector(&lexer->tokens, &lex_token);
    necro_lex_commit(lexer);

    // If the identifier is a layout keyword, update indentation levels and add left brace
    if (lex_token.token <= NECRO_LEX_DO)
    {
        necro_lex_gobble_up_whitespace_and_comments(lexer);
        necro_lex_commit(lexer);
        uint32_t keyword_code_point = necro_lex_next_char(lexer);
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

NecroResult(void) necro_lex_unrecognized_character_sequence(NecroLexer* lexer)
{
    NecroSourceLoc source_loc = lexer->loc;
    uint32_t code_point = necro_lex_next_char(lexer);
    while (code_point != '\n' && code_point != '\0' && !necro_is_whitespace(code_point))
    {
        code_point = necro_lex_next_char(lexer);
    }
    NecroSourceLoc end_loc = lexer->loc;
    return necro_unrecognized_character_sequence_error(source_loc, end_loc);
}

NecroResult(void) necro_lex_go(NecroLexer* lexer)
{
    while (lexer->loc.pos < lexer->str_length)
    {
        if (necro_lex_whitespace(lexer))
            continue;
        if (necro_lex_comments(lexer))
            continue;
        bool is_string = necro_try_map(bool, void, necro_lex_string(lexer));
        if (is_string)
            continue;
        if (necro_lex_char(lexer))
            continue;
        if (necro_lex_identifier(lexer))
            continue;
        bool is_num = necro_try_map(bool, void, necro_lex_number(lexer));
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
    return ok_void();
}

NECRO_DECLARE_VECTOR(NecroLexLayoutContext, NecroLexLayoutContext, lex_layout_context);

NecroResult(void) necro_lex_fixup_layout(NecroLexer* lexer)
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
                context_stack.data[stack_pos].layout      = necro_token_type_to_layout(type);
                pos++;
            }
            else if (n > 0 && stack_pos == 0)
            {
                NecroLexToken token = { .token = NECRO_LEX_LEFT_BRACE,.source_loc = loc };
                necro_push_lex_token_vector(&lexer->layout_fixed_tokens, &token);
                stack_pos++;
                context_stack.data[stack_pos].indentation = n;
                context_stack.data[stack_pos].layout      = necro_token_type_to_layout(type);
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
    return ok_void();
}

NecroResult(void) necro_lex(const char* str, size_t str_length, NecroIntern* intern, NecroLexTokenVector* out_tokens, NecroCompileInfo info)
{
    NecroLexer lexer = necro_lexer_create(str, str_length, intern);
    necro_try(void, necro_lex_go(&lexer));
    necro_try(void, necro_lex_fixup_layout(&lexer));
    if (info.compilation_phase == NECRO_PHASE_LEX && info.verbosity > 0)
        necro_lex_print(&lexer);
    necro_lexer_destroy(&lexer);
    *out_tokens = lexer.tokens;
    return ok_void();
}

///////////////////////////////////////////////////////
// Printing
///////////////////////////////////////////////////////
void necro_lex_print_token(NecroLexer* lexer, size_t token_id)
{
    printf("(line: %zu, char: %zu, pos: %zu), ", lexer->tokens.data[token_id].source_loc.line, lexer->tokens.data[token_id].source_loc.character, lexer->tokens.data[token_id].source_loc.pos);
    if (lexer->tokens.data[token_id].token == NECRO_LEX_STRING_LITERAL)
    {
        printf("STRING:     \"%s\"\n", necro_intern_get_string(lexer->intern, lexer->tokens.data[token_id].symbol));
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_IDENTIFIER)
    {
        printf("IDENTIFIER: %s\n", necro_intern_get_string(lexer->intern, lexer->tokens.data[token_id].symbol));
    }
    else if (lexer->tokens.data[token_id].token == NECRO_LEX_TYPE_IDENTIFIER)
    {
        printf("TYPE:       %s\n", necro_intern_get_string(lexer->intern, lexer->tokens.data[token_id].symbol));
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

void necro_lex_print(NecroLexer* lexer)
{
    printf("NecroLexer\n{\n");
    printf("    line:      %zu,\n", lexer->loc.line);
    printf("    character: %zu,\n", lexer->loc.character);
    printf("    tokens:\n    [\n");
    for (size_t i = 0; i < lexer->tokens.length; ++i)
    {
        printf("        ");
        necro_lex_print_token(lexer, i);
    }
    printf("    ]\n");
    printf("}\n\n");
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_lex_test()
{
    necro_announce_phase("NecroLexer");

    // Test 1
    {
        const char* str    = "    303. This is a test";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        NecroResult(void) result = necro_lex_go(&lexer);
        assert(result.type == NECRO_RESULT_ERROR);
        assert(result.error.type == NECRO_LEX_MALFORMED_FLOAT);
        // necro_print_lexer(&lexer);
        // necro_print_result_errors(result.errors, result.num_errors, str, "lexerTest.necro");
        printf("Lex float error test: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 2
    {
        const char* str    = "3 4 56 \"Hello all you fuckers";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        NecroResult(void) result = necro_lex_go(&lexer);
        assert(result.type == NECRO_RESULT_ERROR);
        assert(result.error.type == NECRO_LEX_MALFORMED_STRING);
        // necro_print_lexer(&lexer);
        // necro_print_result_errors(result.errors, result.num_errors, str, "lexerTest.necro");
        printf("Lex string error test: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 3
    {
        const char* str    = "123 456 789";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
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
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 4
    {
        const char* str    = "helloWorld";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
        assert(lexer.tokens.length == 2);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(lexer.intern, "helloWorld").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_END_OF_STREAM);
        // necro_print_lexer(&lexer);
        printf("Lex identifier test: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Useful site for testing / comparison http://www.cogsci.ed.ac.uk/~richard/utf-8.cgi?input=e9&mode=hex

    // TODO: normalize / canonicalize?

    // Test 5
    {
        const char* str    = "grav\xC3\xA9";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
        assert(lexer.tokens.length == 2);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(lexer.intern, "grav\xC3\xA9").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_END_OF_STREAM);
        // necro_print_lexer(&lexer);
        printf("Lex unicode identifier test: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 6
    {
        const char* str    = "!@#$%^&*(~)_+-=";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
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
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 7
    {
        const char* str    = "+ - * / % > < >= <= :: << >> | |> <| == /= && || >>= =<< !! <>";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
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
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 8
    {
        const char* str    = "\'a\' \'b\' \'c\' \'!\'";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
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
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 9
    {
        const char* str    = "\'\xF0\x9F\x98\x80\' yep";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 3);
        assert(lexer.tokens.data[0].token == NECRO_LEX_CHAR_LITERAL);
        assert(lexer.tokens.data[0].char_literal == 0x0001F600); // Grinning face
        assert(lexer.tokens.data[1].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[1].symbol.id == necro_intern_string(lexer.intern, "yep").id);
        assert(lexer.tokens.data[1].source_loc.character == 5);
        assert(lexer.tokens.data[2].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex unicode char literal test: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 10
    {
        const char* str    = "case class data deriving forall if else then import instance in module newtype type pat";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
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
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 11
    {
        const char* str    = "\xE0\xA6\x95\xE0\xA7\x80"; // BENGALI LETTER KA + BENGALI VOWEL SIGN II, Combining characters
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 2);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(lexer.intern, "\xE0\xA6\x95\xE0\xA7\x80").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_END_OF_STREAM);
        assert(lexer.loc.character == 3);
        printf("Lex combining character test: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 12
    {
        const char* str    = "x = y where\n  y = 10";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 9);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(lexer.intern, "x").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[2].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[2].symbol.id == necro_intern_string(lexer.intern, "y").id);
        assert(lexer.tokens.data[3].token == NECRO_LEX_WHERE);
        assert(lexer.tokens.data[4].token == NECRO_LEX_CONTROL_BRACE_MARKER_WHERE);
        assert(lexer.tokens.data[4].brace_marker_n == 3);
        assert(lexer.tokens.data[5].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[5].symbol.id == necro_intern_string(lexer.intern, "y").id);
        assert(lexer.tokens.data[6].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[7].token == NECRO_LEX_INTEGER_LITERAL); // 10
        assert(lexer.tokens.data[7].int_literal == 10);
        assert(lexer.tokens.data[8].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex where test 1: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 13
    {
        const char* str    = "x = y where\n  y = 10\n  z = 20\nnext :: Int";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 17);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(lexer.intern, "x").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[2].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[2].symbol.id == necro_intern_string(lexer.intern, "y").id);
        assert(lexer.tokens.data[3].token == NECRO_LEX_WHERE);
        assert(lexer.tokens.data[4].token == NECRO_LEX_CONTROL_BRACE_MARKER_WHERE);
        assert(lexer.tokens.data[4].brace_marker_n == 3);
        assert(lexer.tokens.data[5].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[5].symbol.id == necro_intern_string(lexer.intern, "y").id);
        assert(lexer.tokens.data[6].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[7].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[7].int_literal == 10);
        assert(lexer.tokens.data[8].token == NECRO_LEX_CONTROL_WHITE_MARKER);
        assert(lexer.tokens.data[8].white_marker_n == 3);
        assert(lexer.tokens.data[9].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[9].symbol.id == necro_intern_string(lexer.intern, "z").id);
        assert(lexer.tokens.data[10].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[11].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[11].int_literal == 20);
        assert(lexer.tokens.data[12].token == NECRO_LEX_CONTROL_WHITE_MARKER);
        assert(lexer.tokens.data[12].white_marker_n == 1);
        assert(lexer.tokens.data[13].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[13].symbol.id == necro_intern_string(lexer.intern, "next").id);
        assert(lexer.tokens.data[14].token == NECRO_LEX_DOUBLE_COLON);
        assert(lexer.tokens.data[15].token == NECRO_LEX_TYPE_IDENTIFIER);
        assert(lexer.tokens.data[15].symbol.id == necro_intern_string(lexer.intern, "Int").id);
        assert(lexer.tokens.data[16].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex where test 2: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Test 13
    {
        const char* str    = "let\n  x = 10\n  y = 20\nin\n  x";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 14);
        assert(lexer.tokens.data[0].token == NECRO_LEX_LET);
        assert(lexer.tokens.data[1].token == NECRO_LEX_CONTROL_BRACE_MARKER_LET);
        assert(lexer.tokens.data[1].brace_marker_n == 3);
        assert(lexer.tokens.data[2].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[2].symbol.id == necro_intern_string(lexer.intern, "x").id);
        assert(lexer.tokens.data[3].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[4].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[4].int_literal == 10);
        assert(lexer.tokens.data[5].token == NECRO_LEX_CONTROL_WHITE_MARKER);
        assert(lexer.tokens.data[5].white_marker_n == 3);
        assert(lexer.tokens.data[6].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[6].symbol.id == necro_intern_string(lexer.intern, "y").id);
        assert(lexer.tokens.data[7].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[8].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[8].int_literal == 20);
        assert(lexer.tokens.data[9].token == NECRO_LEX_CONTROL_WHITE_MARKER);
        assert(lexer.tokens.data[9].white_marker_n == 1);
        assert(lexer.tokens.data[10].token == NECRO_LEX_IN);
        assert(lexer.tokens.data[11].token == NECRO_LEX_CONTROL_WHITE_MARKER);
        assert(lexer.tokens.data[11].white_marker_n == 3);
        assert(lexer.tokens.data[12].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[12].symbol.id == necro_intern_string(lexer.intern, "x").id);
        assert(lexer.tokens.data[13].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex let test: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
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
            NecroIntern intern = necro_create_intern();
            NecroLexer  lexer  = necro_lexer_create(noise, 4095, &intern);
            necro_lex_go(&lexer);
            // NecroResult(bool) result = necro_lex_go(&lexer);
            // necro_print_result_errors(result.errors, result.num_errors, noise, "noiseTest.necro");
            // necro_print_lexer(&lexer);
            necro_lexer_destroy(&lexer);
            necro_destroy_intern(&intern);
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
            NecroIntern intern = necro_create_intern();
            NecroLexer  lexer  = necro_lexer_create(noise, 4095, &intern);
            necro_lex_go(&lexer);
            // NecroResult(bool) result = necro_lex_go(&lexer);
            // necro_print_result_errors(result.errors, result.num_errors, noise, "noiseTest.necro");
            // necro_print_lexer(&lexer);
            necro_lexer_destroy(&lexer);
            necro_destroy_intern(&intern);
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
            NecroIntern intern = necro_create_intern();
            NecroLexer  lexer  = necro_lexer_create(noise, 4095, &intern);
            unwrap(void, necro_lex_go(&lexer)); // Should be no errors!
            // NecroResult(bool) result = necro_lex_go(&lexer);
            // necro_print_result_errors(result.errors, result.num_errors, noise, "noiseTest.necro");
            // necro_print_lexer(&lexer);
            necro_lexer_destroy(&lexer);
            necro_destroy_intern(&intern);
        }
        printf("Lex string fuzz test: passed\n");
    }

    // Layout test 1
    {
        const char* str    = "test = test2 where\n  test2 = 300";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
        unwrap(void, necro_lex_fixup_layout(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 10);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(lexer.intern, "test").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[2].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[2].symbol.id == necro_intern_string(lexer.intern, "test2").id);
        assert(lexer.tokens.data[3].token == NECRO_LEX_WHERE);
        assert(lexer.tokens.data[4].token == NECRO_LEX_LEFT_BRACE);
        assert(lexer.tokens.data[5].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[5].symbol.id == necro_intern_string(lexer.intern, "test2").id);
        assert(lexer.tokens.data[6].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[7].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[7].int_literal == 300);
        assert(lexer.tokens.data[8].token == NECRO_LEX_RIGHT_BRACE);
        assert(lexer.tokens.data[9].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex layout test 1: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Layout test 2
    {
        const char* str    = "test = test2 + test3 + test4 + test5 where\n  test2 = 2 where\n    test3 = 3\n    test4 = 4.0\n  test5 = 5";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
        unwrap(void, necro_lex_fixup_layout(&lexer));
        // necro_print_lexer(&lexer);
        assert(lexer.tokens.length == 30);
        assert(lexer.tokens.data[0].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[0].symbol.id == necro_intern_string(lexer.intern, "test").id);
        assert(lexer.tokens.data[1].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[2].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[2].symbol.id == necro_intern_string(lexer.intern, "test2").id);
        assert(lexer.tokens.data[3].token == NECRO_LEX_ADD);
        assert(lexer.tokens.data[4].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[4].symbol.id == necro_intern_string(lexer.intern, "test3").id);
        assert(lexer.tokens.data[5].token == NECRO_LEX_ADD);
        assert(lexer.tokens.data[6].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[6].symbol.id == necro_intern_string(lexer.intern, "test4").id);
        assert(lexer.tokens.data[7].token == NECRO_LEX_ADD);
        assert(lexer.tokens.data[8].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[8].symbol.id == necro_intern_string(lexer.intern, "test5").id);
        assert(lexer.tokens.data[9].token == NECRO_LEX_WHERE);
        assert(lexer.tokens.data[10].token == NECRO_LEX_LEFT_BRACE);
        assert(lexer.tokens.data[11].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[11].symbol.id == necro_intern_string(lexer.intern, "test2").id);
        assert(lexer.tokens.data[12].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[13].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[13].int_literal == 2);
        assert(lexer.tokens.data[14].token == NECRO_LEX_WHERE);
        assert(lexer.tokens.data[15].token == NECRO_LEX_LEFT_BRACE);
        assert(lexer.tokens.data[16].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[16].symbol.id == necro_intern_string(lexer.intern, "test3").id);
        assert(lexer.tokens.data[17].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[18].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[18].int_literal == 3);
        assert(lexer.tokens.data[19].token == NECRO_LEX_SEMI_COLON);
        assert(lexer.tokens.data[20].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[20].symbol.id == necro_intern_string(lexer.intern, "test4").id);
        assert(lexer.tokens.data[21].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[22].token == NECRO_LEX_FLOAT_LITERAL);
        assert(lexer.tokens.data[22].double_literal == 4.0);
        assert(lexer.tokens.data[23].token == NECRO_LEX_RIGHT_BRACE);
        assert(lexer.tokens.data[24].token == NECRO_LEX_SEMI_COLON);
        assert(lexer.tokens.data[25].token == NECRO_LEX_IDENTIFIER);
        assert(lexer.tokens.data[25].symbol.id == necro_intern_string(lexer.intern, "test5").id);
        assert(lexer.tokens.data[26].token == NECRO_LEX_ASSIGN);
        assert(lexer.tokens.data[27].token == NECRO_LEX_INTEGER_LITERAL);
        assert(lexer.tokens.data[27].int_literal == 5);
        assert(lexer.tokens.data[28].token == NECRO_LEX_RIGHT_BRACE);
        assert(lexer.tokens.data[29].token == NECRO_LEX_END_OF_STREAM);
        printf("Lex layout test 2: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

    // Mixed braces error test
    {
        const char* str    = "test = x where\n  x = 4 }";
        NecroIntern intern = necro_create_intern();
        NecroLexer  lexer  = necro_lexer_create(str, strlen(str), &intern);
        unwrap(void, necro_lex_go(&lexer));
        NecroResult(void) result = necro_lex_fixup_layout(&lexer);
        assert(result.type == NECRO_RESULT_ERROR);
        assert(result.error.type == NECRO_LEX_MIXED_BRACES);
        // necro_print_result_errors(result.errors, result.num_errors, str, "mixedBracesErrorTest.necro");
        // necro_print_lexer(&lexer);
        printf("Lex mixed braces error test: Passed\n");
        necro_lexer_destroy(&lexer);
        necro_destroy_intern(&intern);
    }

}
