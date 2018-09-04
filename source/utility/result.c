/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "result.h"

NecroResultUnion global_result;

NecroResultError* necro_alloc_error(NecroPagedArena* arena, NecroResultError error)
{
    NecroResultError* error_ptr = necro_paged_arena_alloc(arena, sizeof(NecroResultError));
    *error_ptr                  = error;
#if NECRO_ASSERT_RESULT_ERROR
    assert(false);
#endif
    return error_ptr;
}

NecroResult(bool) necro_malformed_string_error(NecroPagedArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroResultError error =
    {
        .type             = NECRO_MALFORMED_STRING,
        .malformed_string = (NecroMalformedString) { .source_loc = source_loc, .end_loc = end_loc },
    };
    return (NecroResult(bool)) { .errors = necro_alloc_error(arena, error), .num_errors = 1 };
}

NecroResult(bool) necro_malformed_float_error(NecroPagedArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroResultError error =
    {
        .type            = NECRO_MALFORMED_FLOAT,
        .malformed_float = (NecroMalformedFloat) { .source_loc = source_loc, .end_loc = end_loc },
    };
    return (NecroResult(bool)) { .errors = necro_alloc_error(arena, error), .num_errors = 1 };
}

NecroResult(bool) necro_unrecognized_character_sequence_error(NecroPagedArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroResultError error =
    {
        .type                            = NECRO_UNRECOGNIZED_CHARACTER_SEQUENCE,
        .unrecognized_character_sequence = (NecroUnrecognizedCharacterSequence) { .source_loc = source_loc, .end_loc = end_loc },
    };
    return (NecroResult(bool)) { .errors = necro_alloc_error(arena, error), .num_errors = 1 };
}

NecroResult(bool) necro_mixed_braces_error(NecroPagedArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroResultError error =
    {
        .type         = NECRO_MIXED_BRACES,
        .mixed_braces = (NecroMixedBraces) { .source_loc = source_loc, .end_loc = end_loc },
    };
    return (NecroResult(bool)) { .errors = necro_alloc_error(arena, error), .num_errors = 1 };
}

///////////////////////////////////////////////////////
// Printing
///////////////////////////////////////////////////////
void necro_print_error_gutter_padding(NecroSourceLoc source_loc)
{
    size_t line = source_loc.line;
    while (line > 0)
    {
        fprintf(stderr, " ");
        line = line / 10;
    }
}

void necro_print_blank_error_gutter(NecroSourceLoc source_loc)
{
    fprintf(stderr, " ");
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
    fprintf(stderr, " %zu | ", source_loc.line);
}

void necro_print_range_pointers(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    // fprintf(stderr, "   |");
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
    // fprintf(stderr, " %zu | %.*s\n", source_loc.line, (int) (line_end - line_start), (source_str + line_start));
    necro_print_range_pointers(source_loc, end_loc);
}

void necro_print_malformed_float_error(NecroResultError error, const char* source_str, const char* source_name)
{
    NecroSourceLoc source_loc = error.malformed_float.source_loc;
    NecroSourceLoc end_loc    = error.malformed_float.end_loc;
    fprintf(stderr, "error[NE-%04u]: Malformed float\n", NECRO_MALFORMED_FLOAT);
    necro_print_error_gutter_padding(source_loc);
    fprintf(stderr, "--> %s:%zu:%zu\n", source_name, source_loc.line, source_loc.character);
    // fprintf(stderr, "   |\n");
    necro_print_blank_error_gutter(source_loc);
    fprintf(stderr, "\n");
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, "\n");
}

void necro_print_malformed_string_error(NecroResultError error, const char* source_str, const char* source_name)
{
    NecroSourceLoc source_loc = error.malformed_string.source_loc;
    NecroSourceLoc end_loc    = error.malformed_string.end_loc;
    fprintf(stderr, "error[NE-%04u]: Malformed string\n", NECRO_MALFORMED_STRING);
    necro_print_error_gutter_padding(source_loc);
    fprintf(stderr, "--> %s:%zu:%zu\n", source_name, source_loc.line, source_loc.character);
    // fprintf(stderr, "   |\n");
    necro_print_blank_error_gutter(source_loc);
    fprintf(stderr, "\n");
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, "This error likely means that you forgot to add an enclosing \'\"\' character to terminate a string.\n");
}

void necro_print_unrecognized_character_sequence_error(NecroResultError error, const char* source_str, const char* source_name)
{
    NecroSourceLoc source_loc = error.unrecognized_character_sequence.source_loc;
    NecroSourceLoc end_loc    = error.unrecognized_character_sequence.end_loc;
    fprintf(stderr, "error[NE-%04u]: Unrecognized character sequence\n", NECRO_UNRECOGNIZED_CHARACTER_SEQUENCE);
    necro_print_error_gutter_padding(source_loc);
    fprintf(stderr, "--> %s:%zu:%zu\n", source_name, source_loc.line, source_loc.character);
    necro_print_blank_error_gutter(source_loc);
    fprintf(stderr, "\n");
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, "\n");
}

void necro_print_mixed_braces_error(NecroResultError error, const char* source_str, const char* source_name)
{
    NecroSourceLoc source_loc = error.mixed_braces.source_loc;
    NecroSourceLoc end_loc    = error.mixed_braces.end_loc;
    fprintf(stderr, "error[NE-%04u]: Mixed implicit and explicit braces\n", NECRO_MIXED_BRACES);
    fprintf(stderr, "--> %s:%zu:%zu\n", source_name, source_loc.line, source_loc.character);
    necro_print_blank_error_gutter(source_loc);
    fprintf(stderr, "\n");
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, "Necrolang uses significant whitespace which converts into implicit opening and closing braces.\n");
    fprintf(stderr, "This error likely means that you are either adding an extra brace where none is required,\n");
    fprintf(stderr, "or that you forgot to add an opening brace.\n\n");
}

void necro_print_result_error(NecroResultError error, const char* source_str, const char* source_name)
{
    switch (error.type)
    {
    case NECRO_MALFORMED_FLOAT:                 necro_print_malformed_float_error(error, source_str, source_name); break;
    case NECRO_MALFORMED_STRING:                necro_print_malformed_string_error(error, source_str, source_name); break;
    case NECRO_UNRECOGNIZED_CHARACTER_SEQUENCE: necro_print_unrecognized_character_sequence_error(error, source_str, source_name); break;
    case NECRO_MIXED_BRACES:                    necro_print_mixed_braces_error(error, source_str, source_name); break;
    // case NECRO_MULTIPLE_DEFINITIONS:
    default:
        assert(false);
        break;
    }
}

void necro_print_result_errors(NecroResultError* errors, size_t num_errors, const char* source_str, const char* source_name)
{
    for (size_t i = 0; i < num_errors; ++i)
    {
        necro_print_result_error(errors[i], source_str, source_name);
    }
}
