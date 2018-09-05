/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "result.h"

NecroResultUnion global_result;

///////////////////////////////////////////////////////
// Construction
///////////////////////////////////////////////////////
// NecroResultError* necro_alloc_error(NecroPagedArena* arena, NecroResultError error)
// {
//     NecroResultError* error_ptr = necro_paged_arena_alloc(arena, sizeof(NecroResultError));
//     *error_ptr                  = error;
// #if NECRO_ASSERT_RESULT_ERROR
//     assert(false);
// #endif
//     return error_ptr;
// }

NecroResult(bool) necro_malformed_string_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroResultError error =
    {
        .type               = NECRO_LEX_MALFORMED_STRING,
        .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
    };
    return (NecroResult(bool)) { .error = error, .type = NECRO_RESULT_ERROR };
}

NecroResult(bool) necro_malformed_float_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroResultError error =
    {
        .type               = NECRO_LEX_MALFORMED_FLOAT,
        .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
    };
    return (NecroResult(bool)) { .error = error, .type = NECRO_RESULT_ERROR };
}

NecroResult(NecroUnit) necro_unrecognized_character_sequence_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroResultError error =
    {
        .type               = NECRO_LEX_UNRECOGNIZED_CHARACTER_SEQUENCE,
        .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
    };
    return (NecroResult(NecroUnit)) { .error = error, .type = NECRO_RESULT_ERROR };
}

NecroResult(NecroUnit) necro_mixed_braces_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroResultError error =
    {
        .type               = NECRO_LEX_MIXED_BRACES,
        .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
    };
    return (NecroResult(NecroUnit)) { .error = error, .type = NECRO_RESULT_ERROR };
}

NecroResult(NecroAST_LocalPtr) necro_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroResultError error =
    {
        .type               = NECRO_PARSE_ERROR,
        .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
    };
    return (NecroResult(NecroAST_LocalPtr)) { .error = error, .type = NECRO_RESULT_ERROR };
}

///////////////////////////////////////////////////////
// Printing
///////////////////////////////////////////////////////
#define N_L_CHAR "|"

void necro_print_blank_error_gutter(NecroSourceLoc source_loc)
{
    fprintf(stderr, N_L_CHAR " ");
    size_t line = source_loc.line;
    while (line > 0)
    {
        fprintf(stderr, " ");
        line = line / 10;
    }
    fprintf(stderr, "  ");
}

void necro_print_error_gutter(NecroSourceLoc source_loc)
{
    fprintf(stderr, N_L_CHAR " %zu | " , source_loc.line);
}

void necro_print_range_pointers(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_print_blank_error_gutter(source_loc);
    for (size_t i = 0; i < source_loc.character; ++i)
    {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^");
    for (size_t i = source_loc.character + 1; i < end_loc.character; ++i)
        fprintf(stderr, "-");
    fprintf(stderr, "\n");
}

void necro_print_line_at_source_loc(const char* source_str, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    size_t line_start = source_loc.pos;
    for (line_start = source_loc.pos; line_start > 0 && source_str[line_start] != '\0' && source_str[line_start] != '\n'; --line_start);
    if (source_str[line_start] == '\n')
        line_start++;
    size_t line_end   = line_start;
    for (line_end = line_start; source_str[line_end] != '\0' && source_str[line_end] != '\n'; ++line_end);
    necro_print_error_gutter(source_loc);
    fprintf(stderr, "%.*s\n", (int) (line_end - line_start), (source_str + line_start));
    necro_print_range_pointers(source_loc, end_loc);
}

void necro_print_malformed_float_error(NecroResultError error, const char* source_str, const char* source_name)
{
    NecroSourceLoc source_loc = error.default_error_data.source_loc;
    NecroSourceLoc end_loc    = error.default_error_data.end_loc;
    // fprintf(stderr, "error[NE-%04u]: Malformed float\n", NECRO_MALFORMED_FLOAT);
    fprintf(stderr, "\n ------Malformed Float------\n");
    fprintf(stderr, N_L_CHAR " (%s:%zu:%zu)\n", source_name, source_loc.line, source_loc.character);
    fprintf(stderr, N_L_CHAR "\n");
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, N_L_CHAR " Float literals require digits both before and after the period (.) character.\n");
}

void necro_print_malformed_string_error(NecroResultError error, const char* source_str, const char* source_name)
{
    NecroSourceLoc source_loc = error.default_error_data.source_loc;
    NecroSourceLoc end_loc    = error.default_error_data.end_loc;
    fprintf(stderr, "\n ------Malformed String------\n");
    fprintf(stderr, N_L_CHAR " (%s:%zu:%zu)\n", source_name, source_loc.line, source_loc.character);
    fprintf(stderr, N_L_CHAR "\n");
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, N_L_CHAR " Perhaps you forgot a closing quotation character (\") ?\n");
}

void necro_print_unrecognized_character_sequence_error(NecroResultError error, const char* source_str, const char* source_name)
{
    NecroSourceLoc source_loc = error.default_error_data.source_loc;
    NecroSourceLoc end_loc    = error.default_error_data.end_loc;
    // fprintf(stderr, "error[NE-%04u]: Unrecognized character sequence\n", NECRO_UNRECOGNIZED_CHARACTER_SEQUENCE);
    fprintf(stderr, "\n ------Unrecognized Character Sequence------\n");
    fprintf(stderr, N_L_CHAR " (%s:%zu:%zu)\n", source_name, source_loc.line, source_loc.character);
    fprintf(stderr, N_L_CHAR "\n");
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, N_L_CHAR " This is either an unholy message to Cthulhu or perhaps you mistyped some things...\n");
}

void necro_print_mixed_braces_error(NecroResultError error, const char* source_str, const char* source_name)
{
    NecroSourceLoc source_loc = error.default_error_data.source_loc;
    NecroSourceLoc end_loc    = error.default_error_data.end_loc;
    // fprintf(stderr, "error[NE-%04u]: Mixed implicit and explicit braces\n", NECRO_MIXED_BRACES);
    fprintf(stderr, "\n ------Mixed Braces------\n");
    fprintf(stderr, N_L_CHAR " (%s:%zu:%zu)\n", source_name, source_loc.line, source_loc.character);
    fprintf(stderr, N_L_CHAR "\n");
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, N_L_CHAR " Necrolang uses significant whitespace which converts into implicit opening and closing braces.\n");
    fprintf(stderr, N_L_CHAR " You're likely adding an explicit closing brace where none is required.\n");
}

void necro_print_result_error(NecroResultError error, const char* source_str, const char* source_name)
{
    switch (error.type)
    {
    case NECRO_LEX_MALFORMED_FLOAT:                 necro_print_malformed_float_error(error, source_str, source_name); break;
    case NECRO_LEX_MALFORMED_STRING:                necro_print_malformed_string_error(error, source_str, source_name); break;
    case NECRO_LEX_UNRECOGNIZED_CHARACTER_SEQUENCE: necro_print_unrecognized_character_sequence_error(error, source_str, source_name); break;
    case NECRO_LEX_MIXED_BRACES:                    necro_print_mixed_braces_error(error, source_str, source_name); break;
    default:
        assert(false);
        break;
    }
}

// void necro_print_result_errors(NecroResultError* errors, size_t num_errors, const char* source_str, const char* source_name)
// {
//     for (size_t i = 0; i < num_errors; ++i)
//     {
//         necro_print_result_error(errors[i], source_str, source_name);
//     }
// }
