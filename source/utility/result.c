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

void necro_print_character_pointer(const char* source_str, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    printf("   |");
    // Arrow spaces
    for (size_t i = 0; i < source_loc.character; ++i)
    {
        printf(" ");
    }
    for (size_t i = source_loc.character; i < end_loc.character; ++i)
        printf("^");
    printf("\n");
}

void necro_print_line_at_source_loc(const char* source_str, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    size_t line_start = source_loc.pos;
    for (line_start = source_loc.pos; line_start > 0 && source_str[line_start] != '\0' && source_str[line_start] != '\n'; --line_start);
    if (source_str[line_start] == '\n')
        line_start++;
    size_t line_end   = line_start;
    for (line_end = line_start; source_str[line_end] != '\0' && source_str[line_end] != '\n'; ++line_end);
    printf(" %u | %.*s\n", source_loc.line, (line_end - line_start), (source_str + line_start));
    necro_print_character_pointer(source_str, source_loc, end_loc);
}

void necro_print_malformed_float_error(NecroResultError error, const char* source_str, const char* source_name)
{
    printf("error[NE-%04u]: Malformed float\n", NECRO_MALFORMED_FLOAT);
    printf(" --> %s:%d:%d\n", source_name, error.malformed_float.source_loc.line, error.malformed_float.source_loc.character);
    printf("   |\n");
    necro_print_line_at_source_loc(source_str, error.malformed_float.source_loc, error.malformed_float.end_loc);
    printf("\n");
}

void necro_print_malformed_string_error(NecroResultError error, const char* source_str, const char* source_name)
{
    printf("error[NE-%04u]: Malformed string\n", NECRO_MALFORMED_STRING);
    printf(" --> %s:%d:%d\n", source_name, error.malformed_string.source_loc.line, error.malformed_string.source_loc.character);
    printf("   |\n");
    necro_print_line_at_source_loc(source_str, error.malformed_string.source_loc, error.malformed_string.end_loc);
    printf("\n");
}

void necro_print_unrecognized_character_sequence_error(NecroResultError error, const char* source_str, const char* source_name)
{
    printf("error[NE-%04u]: Unrecognized character sequence\n", NECRO_UNRECOGNIZED_CHARACTER_SEQUENCE);
    printf(" --> %s:%d:%d\n", source_name, error.unrecognized_character_sequence.source_loc.line, error.unrecognized_character_sequence.source_loc.character);
    printf("   |\n");
    necro_print_line_at_source_loc(source_str, error.unrecognized_character_sequence.source_loc, error.unrecognized_character_sequence.end_loc);
    printf("\n");
}

void necro_print_result_error(NecroResultError error, const char* source_str, const char* source_name)
{
    switch (error.type)
    {
    case NECRO_MALFORMED_FLOAT:                 necro_print_malformed_float_error(error, source_str, source_name); break;
    case NECRO_MALFORMED_STRING:                necro_print_malformed_string_error(error, source_str, source_name); break;
    case NECRO_UNRECOGNIZED_CHARACTER_SEQUENCE: necro_print_unrecognized_character_sequence_error(error, source_str, source_name); break;
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
