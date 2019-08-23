/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "unicode_properties.h"
#include "arena.h"
#include <ctype.h>
#include <inttypes.h>
#include <string.h>

///////////////////////////////////////////////////////
// Unicode Property Parser
///////////////////////////////////////////////////////
typedef uint32_t uproperty_field;
typedef struct
{
    char*               str;
    size_t              str_length;
    NecroSourceLoc      loc;
    NecroSourceLoc      prev_loc;
    uproperty_field*    properties;
    size_t              properties_count;
} NecroUnicodePropertyParser;

typedef struct
{
    uint32_t code_point;
    uint32_t code_point_end;
    enum
    {
        NECRO_UNICODE_PROPERTY_PARSER_CODE_POINT,
        NECRO_UNICODE_PROPERTY_PARSER_RANGE
    } type;
} NecroUnicodePropertyParserCodePoint;

// As defined by Unicode 11.0 standard: https://www.unicode.org/versions/Unicode11.0.0/UnicodeStandard-11.0.pdf
#define NECRO_MAX_UNICODE_CODE_POINT 0x10FFFF

NecroUnicodePropertyParser necro_create_unicode_property_parser()
{
    NecroUnicodePropertyParser uparser  =
    {
        .str              = NULL,
        .str_length       = 0,
        .loc              = (NecroSourceLoc) { .pos = 0, .character = 0, .line = 1 },
        .prev_loc         = (NecroSourceLoc) { .pos = 0, .character = 0, .line = 1 },
        .properties       = emalloc(NECRO_MAX_UNICODE_CODE_POINT * sizeof(uproperty_field)),
        .properties_count = 0
    };
    memset(uparser.properties, 0, NECRO_MAX_UNICODE_CODE_POINT * sizeof(uproperty_field));
    return uparser;
}

void necro_destroy_unicode_property_parser(NecroUnicodePropertyParser* uparser)
{
    if (uparser->properties != NULL)
    {
        free(uparser->properties);
        uparser->properties = NULL;
    }
    if (uparser->str != NULL)
    {
        free(uparser->str);
        uparser->str = NULL;
        uparser->str_length = 0;
    }
}

void necro_set_str_unicode_property_parser(NecroUnicodePropertyParser* uparser, char* str, size_t str_length)
{
    if (uparser->str != NULL)
    {
        free(uparser->str);
    }
    uparser->str         = str;
    uparser->str_length  = str_length;
    uparser->loc         = (NecroSourceLoc) { .pos = 0, .character = 0, .line = 1 };
    uparser->prev_loc    = (NecroSourceLoc) { .pos = 0, .character = 0, .line = 1 };
}

///////////////////////////////////////////////////////
// PROPERTIES
///////////////////////////////////////////////////////
typedef enum
{
    NECRO_UNICODE_DEFAULT                  = 0,
    NECRO_UNICODE_MATH                     = 1 << 0,
    NECRO_UNICODE_ALPHABETIC               = 1 << 1,
    NECRO_UNICODE_LOWERCASE                = 1 << 2,
    NECRO_UNICODE_UPPERCASE                = 1 << 3,
    NECRO_UNICODE_CASED                    = 1 << 4,
    NECRO_UNICODE_CASE_IGNOREABLE          = 1 << 5,
    NECRO_UNICODE_CHANGES_WHEN_LOWERCASED  = 1 << 6,
    NECRO_UNICODE_CHANGES_WHEN_UPPERCASED  = 1 << 7,
    NECRO_UNICODE_CHANGES_WHEN_TITLE_CASED = 1 << 8,
    NECRO_UNICODE_CHANGES_WHEN_CASE_FOLDED = 1 << 9,
    NECRO_UNICODE_CHANGES_WHEN_CASE_MAPPED = 1 << 10,
    NECRO_UNICODE_ID_START                 = 1 << 11,
    NECRO_UNICODE_ID_CONTINUE              = 1 << 12,
    NECRO_UNICODE_XID_START                = 1 << 13,
    NECRO_UNICODE_XID_CONTINUE             = 1 << 14,
    NECRO_UNICODE_GRAPHEME_EXTEND          = 1 << 15,
    NECRO_UNICODE_GRAPHEME_BASE            = 1 << 16,
    NECRO_UNICODE_WHITE_SPACE              = 1 << 17,
    NECRO_UNICODE_NUMERIC                  = 1 << 18,
    NECRO_UNICODE_DIGIT                    = 1 << 19,
    NECRO_UNICODE_DECIMAL                  = 1 << 20,
} NECRO_UNICODE_PROPERTY_FIELD_VALUE;

void necro_add_unicode_property(NecroUnicodePropertyParser* uparser, uint32_t code_point, uproperty_field property_field)
{
    if (uparser->properties_count < code_point)
    {
        uparser->properties_count = code_point + 1;
    }
    uparser->properties[code_point] |= property_field;
    // if (code_point < 256)
    //     printf("code point: %c, property: ", (char) code_point);
    // else
    //     printf("code point: %u, property: ", code_point);
    // switch (property_field)
    // {
    // case NECRO_UNICODE_DEFAULT: printf("NECRO_UNICODE_DEFAULT\n"); break;
    // case NECRO_UNICODE_MATH: printf("NECRO_UNICODE_MATH\n"); break;
    // case NECRO_UNICODE_ALPHABETIC: printf("NECRO_UNICODE_ALPHABETIC\n"); break;
    // case NECRO_UNICODE_LOWERCASE: printf("NECRO_UNICODE_LOWERCASE\n"); break;
    // case NECRO_UNICODE_UPPERCASE: printf("NECRO_UNICODE_UPPERCASE\n"); break;
    // case NECRO_UNICODE_CASED: printf("NECRO_UNICODE_CASED\n"); break;
    // case NECRO_UNICODE_CASE_IGNOREABLE: printf("NECRO_UNICODE_CASE_IGNOREABLE\n"); break;
    // case NECRO_UNICODE_CHANGES_WHEN_LOWERCASED: printf("NECRO_UNICODE_CHANGES_WHEN_LOWERCASED\n"); break;
    // case NECRO_UNICODE_CHANGES_WHEN_UPPERCASED: printf("NECRO_UNICODE_CHANGES_WHEN_UPPERCASED\n"); break;
    // case NECRO_UNICODE_CHANGES_WHEN_TITLE_CASED: printf("NECRO_UNICODE_CHANGES_WHEN_TITLE_CASED\n"); break;
    // case NECRO_UNICODE_CHANGES_WHEN_CASE_FOLDED: printf("NECRO_UNICODE_CHANGES_WHEN_CASE_FOLDED\n"); break;
    // case NECRO_UNICODE_CHANGES_WHEN_CASE_MAPPED: printf("NECRO_UNICODE_CHANGES_WHEN_CASE_MAPPED\n"); break;
    // case NECRO_UNICODE_ID_START: printf("NECRO_UNICODE_ID_START\n"); break;
    // case NECRO_UNICODE_ID_CONTINUE: printf("NECRO_UNICODE_ID_CONTINUE\n"); break;
    // case NECRO_UNICODE_XID_START: printf("NECRO_UNICODE_XID_START\n"); break;
    // case NECRO_UNICODE_XID_CONTINUE: printf("NECRO_UNICODE_XID_CONTINUE\n"); break;
    // case NECRO_UNICODE_GRAPHEME_EXTEND: printf("NECRO_UNICODE_GRAPHEME_EXTEND\n"); break;
    // case NECRO_UNICODE_GRAPHEME_BASE: printf("NECRO_UNICODE_GRAPHEME_BASE\n"); break;
    // case NECRO_UNICODE_WHITE_SPACE: printf("NECRO_UNICODE_WHITE_SPACE\n"); break;
    // case NECRO_UNICODE_NUMERIC: printf("NECRO_UNICODE_NUMERIC\n"); break;
    // case NECRO_UNICODE_DIGIT: printf("NECRO_UNICODE_DIGIT\n"); break;
    // case NECRO_UNICODE_DECIMAL: printf("NECRO_UNICODE_DECIMAL\n"); break;
    // default:
    //     assert(false);
    // }
}

///////////////////////////////////////////////////////
// NecroPropTable
///////////////////////////////////////////////////////
NECRO_DECLARE_VECTOR(uint64_t, NecroUInt64, uint64);
typedef struct NecroPropTrie
{
    size_t            table1[256];
    NecroUInt64Vector table2;
    NecroUInt64Vector leaves;
} NecroPropTrie;

NecroPropTrie necro_create_prop_trie()
{
    NecroPropTrie trie = (NecroPropTrie)
    {
        .table1 = { 0 },
        .table2 = necro_create_uint64_vector(),
        .leaves = necro_create_uint64_vector(),
    };
    for (size_t i = 0; i < 256; ++i)
    {
        trie.table1[i] = i * 256;
    }
    for (size_t i = 0; i < 65536; ++i)
    {
        uint64_t leaf_num = 0;
        necro_push_uint64_vector(&trie.table2, &leaf_num);
    }
    return trie;
}

void necro_destroy_prop_trie(NecroPropTrie* trie)
{
    necro_destroy_uint64_vector(&trie->table2);
    necro_destroy_uint64_vector(&trie->leaves);
}

void necro_insert_property_leaf(NecroPropTrie* trie, uint32_t code_point_start, uint64_t leaf)
{
    bool   found    = false;
    size_t leaf_num = 0;
    for (size_t i = 0; i < trie->leaves.length; ++i)
    {
        if (trie->leaves.data[i] == leaf)
        {
            leaf_num = i;
            found    = true;
        }
    }

    if (!found)
    {
        necro_push_uint64_vector(&trie->leaves, &leaf);
        leaf_num = trie->leaves.length - 1;
    }

    size_t table1_index = code_point_start >> 14;
    assert((table1_index >> 8) < 256);
    size_t  table2_index = trie->table1[table1_index] + ((code_point_start >> 6) & 0xFF);
    assert(table2_index < 65536);
    trie->table2.data[table2_index] = leaf_num;
    assert(leaf_num < trie->leaves.length);
}

void necro_collapse_table2(NecroPropTrie* trie)
{
    // TODO: Test!
    NecroUInt64Vector collapsed_table2 = necro_create_uint64_vector();
    for (size_t i = 0; i < trie->table2.length; i += 256)
    {
        bool   found     = false;
        size_t found_num = 0;
        for (size_t c = 0; c < collapsed_table2.length; c ++)
        {
            bool matches = true;
            for (size_t l = 0; l < 256 && matches; ++l)
            {
                uint64_t leaf1 = trie->table2.data[i + l];
                uint64_t leaf2 = collapsed_table2.data[c + l];
                matches = matches && (leaf1 == leaf2);
            }
            if (matches)
            {
                found     = true;
                found_num = c;
                break;
            }
        }
        if (!found)
        {
            for (size_t l = 0; l < 256; ++l)
            {
                uint64_t leaf_index = trie->table2.data[i + l];
                necro_push_uint64_vector(&collapsed_table2, &leaf_index);
            }
        }
        else
        {
            size_t table1_index = i >> 8;
            trie->table1[table1_index] = found_num;
        }
    }
    necro_destroy_uint64_vector(&trie->table2);
    trie->table2 = collapsed_table2;
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
char necro_next_char_unicode_property_parser(NecroUnicodePropertyParser* uparser);

///////////////////////////////////////////////////////
// Parse
// Note on Unicode files from the docs:
//     * The files use UTF-8, with the exception of NamesList.txt, which is encoded in Latin-1.
//       Unless otherwise noted, non-ASCII characters only appear in comments.
///////////////////////////////////////////////////////
void necro_rewind_unicode_property_parser(NecroUnicodePropertyParser* uparser)
{
    uparser->loc = uparser->prev_loc;
}

void necro_commit_unicode_property_parser(NecroUnicodePropertyParser* uparser)
{
    uparser->prev_loc = uparser->loc;
}

char necro_next_char_unicode_property_parser(NecroUnicodePropertyParser* uparser)
{
    if (uparser->loc.pos >= uparser->str_length)
    {
        return 0;
    }
    char c = uparser->str[uparser->loc.pos];
    uparser->loc.pos++;
    return c;
}

void necro_parse_whitespace_unicode_property_parser(NecroUnicodePropertyParser* uparser)
{
    while (necro_next_char_unicode_property_parser(uparser) == ' ')
    {
        necro_commit_unicode_property_parser(uparser);
    }
    necro_rewind_unicode_property_parser(uparser);
}

bool necro_parse_comments_unicode_property_parser(NecroUnicodePropertyParser* uparser)
{
    if (necro_next_char_unicode_property_parser(uparser) != '#')
    {
        necro_rewind_unicode_property_parser(uparser);
        return false;
    }
    necro_commit_unicode_property_parser(uparser);
    while (true)
    {
        char c = necro_next_char_unicode_property_parser(uparser);
        if (c == '\n')
        {
            necro_rewind_unicode_property_parser(uparser);
            return true;
        }
        else if (c == '\0')
        {
            return true;
        }
        else
        {
            necro_commit_unicode_property_parser(uparser);
        }
    }
}

bool necro_parse_newline(NecroUnicodePropertyParser* uparser)
{
    char c = necro_next_char_unicode_property_parser(uparser);
    if (c == '\n')
    {
        necro_commit_unicode_property_parser(uparser);
        return true;
    }
    else
    {
        necro_rewind_unicode_property_parser(uparser);
        return false;
    }
}

bool necro_parse_hex_unicode_property_parser(NecroUnicodePropertyParser* uparser, uint32_t* out_code)
{
    size_t num_digits = 0;
    char digits[16];
    digits[0] = '0';
    digits[1] = 'x';
    while (true)
    {
        char c = necro_next_char_unicode_property_parser(uparser);
        // if ((num_digits == 0 && isdigit(c)) || (num_digits > 0 && (isdigit(c) || isalnum(c))))
        if (isdigit(c) || isalnum(c))
        {
            digits[num_digits + 2] = c;
            num_digits++;
            necro_commit_unicode_property_parser(uparser);
        }
        else
        {
            necro_rewind_unicode_property_parser(uparser);
            break;
        }
    }
    if (num_digits == 0)
        return false;
    digits[num_digits + 2] = '\0';
    char* new_str_pos = digits;
    *out_code = strtol(digits, &new_str_pos, 16);
    return true;
}

bool necro_parse_codepoint_unicode_property_parser(NecroUnicodePropertyParser* uparser, NecroUnicodePropertyParserCodePoint* out_code_point)
{
    if (!necro_parse_hex_unicode_property_parser(uparser, &out_code_point->code_point))
    {
        return false;
    }
    if (necro_next_char_unicode_property_parser(uparser) == '.')
    {
        // Range
        assert(necro_next_char_unicode_property_parser(uparser) == '.');
        necro_commit_unicode_property_parser(uparser);
        assert(necro_parse_hex_unicode_property_parser(uparser, &out_code_point->code_point_end));
        out_code_point->type = NECRO_UNICODE_PROPERTY_PARSER_RANGE;
        return true;
    }
    else
    {
        out_code_point->type = NECRO_UNICODE_PROPERTY_PARSER_CODE_POINT;
        necro_rewind_unicode_property_parser(uparser);
        return true;
    }
}

bool necro_parse_property_unicode_property_parser(NecroUnicodePropertyParser* uparser, const char* string_to_match, uproperty_field property_field, NecroUnicodePropertyParserCodePoint code_point)
{
    while (*string_to_match != '\0' && necro_next_char_unicode_property_parser(uparser) == *string_to_match)
    {
        string_to_match++;
    }
    if (*string_to_match == '\0')
    {
        if (code_point.type == NECRO_UNICODE_PROPERTY_PARSER_CODE_POINT)
        {
            necro_add_unicode_property(uparser, code_point.code_point, property_field);
        }
        else
        {
            for (uint32_t cp = code_point.code_point; cp <= code_point.code_point_end; ++cp)
            {
                necro_add_unicode_property(uparser, cp, property_field);
            }
        }
        necro_commit_unicode_property_parser(uparser);
        return true;
    }
    else
    {
        necro_rewind_unicode_property_parser(uparser);
        return false;
    }
}

bool necro_consume_line(NecroUnicodePropertyParser* uparser)
{
    while (necro_next_char_unicode_property_parser(uparser) != '\n' && necro_next_char_unicode_property_parser(uparser) != '\0')
    {
    }
    necro_commit_unicode_property_parser(uparser);
    return true;
}

NECRO_RETURN_CODE necro_unicode_property_parse_file(NecroUnicodePropertyParser* uparser)
{
    NecroUnicodePropertyParserCodePoint code_point;
    while (uparser->loc.pos < uparser->str_length)
    {
        necro_parse_whitespace_unicode_property_parser(uparser);
        // Has property
        if (necro_parse_codepoint_unicode_property_parser(uparser, &code_point))
        {
            necro_parse_whitespace_unicode_property_parser(uparser);
            char semi_colon = necro_next_char_unicode_property_parser(uparser);
            assert(semi_colon == ';');
            necro_parse_whitespace_unicode_property_parser(uparser);
            bool matched =
                // necro_parse_property_unicode_property_parser(uparser, "Math", NECRO_UNICODE_MATH, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Alphabetic", NECRO_UNICODE_ALPHABETIC, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Lowercase", NECRO_UNICODE_LOWERCASE, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Uppercase", NECRO_UNICODE_UPPERCASE, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Cased", NECRO_UNICODE_CASED, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Case_Ignorable", NECRO_UNICODE_CASE_IGNOREABLE, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Changes_When_Lowercased", NECRO_UNICODE_CHANGES_WHEN_LOWERCASED, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Changes_When_Uppercased", NECRO_UNICODE_CHANGES_WHEN_UPPERCASED, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Changes_When_Titlecased", NECRO_UNICODE_CHANGES_WHEN_TITLE_CASED, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Changes_When_Casefolded", NECRO_UNICODE_CHANGES_WHEN_CASE_FOLDED, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Changes_When_Casemapped", NECRO_UNICODE_CHANGES_WHEN_CASE_MAPPED, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "ID_Start", NECRO_UNICODE_ID_START, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "ID_Continue", NECRO_UNICODE_ID_CONTINUE, code_point) ||
                // necro_parse_property_unicode_property_parser(uparser, "XID_Start", NECRO_UNICODE_XID_START, code_point) ||
                // necro_parse_property_unicode_property_parser(uparser, "XID_Continue", NECRO_UNICODE_XID_CONTINUE, code_point) ||
                // necro_parse_property_unicode_property_parser(uparser, "Grapheme_Extend", NECRO_UNICODE_GRAPHEME_EXTEND, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Grapheme_Base", NECRO_UNICODE_GRAPHEME_BASE, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "White_Space", NECRO_UNICODE_WHITE_SPACE, code_point) ||
                // necro_parse_property_unicode_property_parser(uparser, "Numeric", NECRO_UNICODE_NUMERIC, code_point) ||
                // necro_parse_property_unicode_property_parser(uparser, "Digit", NECRO_UNICODE_DIGIT, code_point) ||
                necro_parse_property_unicode_property_parser(uparser, "Decimal", NECRO_UNICODE_DECIMAL, code_point) ||
                necro_consume_line(uparser);
            // TODO: Force eat until next line!
            assert(matched);
        }
        necro_parse_whitespace_unicode_property_parser(uparser);
        necro_parse_comments_unicode_property_parser(uparser);
        necro_parse_whitespace_unicode_property_parser(uparser);
        necro_parse_newline(uparser);
    }
    return NECRO_SUCCESS;
}

NECRO_RETURN_CODE necro_open_file_in_directory(const char* directory_name, const char* file_name, char** out_str, size_t* out_str_length)
{
    size_t dir_file_name_length = strlen(directory_name) + strlen(file_name) + 2;
    char*  file_name_buf        = emalloc(dir_file_name_length);
    file_name_buf[0]            = '\0';
    strcat(file_name_buf, directory_name);
    strcat(file_name_buf, file_name);

#ifdef WIN32
    FILE* file;
    if (fopen_s(&file, file_name_buf, "r, ccs = UTF-8") != 0)
    {
        fprintf(stderr, "Could not open file: %s\n", file_name_buf);
        necro_exit(1);
    }
#else
    FILE* file = fopen(file_name_buf, "r");
#endif
    if (!file)
    {
        fprintf(stderr, "Could not open file: %s\n", file_name_buf);
        necro_exit(1);
    }

    char*  str    = NULL;
    size_t length = 0;

    // Find length of file
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer
    str = emalloc(length + 2);

    // read contents of buffer
    length = fread(str, 1, length, file);
    str[length]     = '\n';
    str[length + 1] = '\0';
    *out_str        = str;
    *out_str_length = length;
    fclose(file);
    return NECRO_SUCCESS;
}

FILE* necro_open_write_file_in_director(const char* directory_name, const char* file_name)
{
    size_t dir_file_name_length = strlen(directory_name) + strlen(file_name) + 2;
    char*  file_name_buf        = emalloc(dir_file_name_length);
    file_name_buf[0]            = '\0';
    strcat(file_name_buf, directory_name);
    strcat(file_name_buf, file_name);

#ifdef WIN32
    FILE* file;
    if (fopen_s(&file, file_name_buf, "w") != 0)
    {
        assert(false);
    }
#else
    FILE* file = fopen(file_name_buf, "w");
#endif

    return file;
}

// TODO: Normalize identifiers!

void necro_prop_trie_from_parser(NecroUnicodePropertyParser* uparser, const char* directory_name, const char* file_name, const char* table_name, uint64_t property_flag)
{
    NecroPropTrie prop_trie = necro_create_prop_trie();
    for (size_t i = 0; i < uparser->properties_count; i += 64)
    {
        uint64_t leaf = 0;
        for (size_t b = 0; b < 64; ++b)
        {
            uproperty_field p        = uparser->properties[i + b];
            uint64_t        has_prop = (p & property_flag) != 0;
            leaf |= (has_prop << b);
        }
        assert(i <= UINT32_MAX);
        necro_insert_property_leaf(&prop_trie, (uint32_t) i, leaf);
    }
    necro_collapse_table2(&prop_trie);

    size_t mem_usage = 256 * sizeof(uint16_t) + prop_trie.table2.length * sizeof(uint16_t) + prop_trie.leaves.length * sizeof(uint64_t);
    printf("mem usage: %zu\n", mem_usage);

    FILE* out_file = necro_open_write_file_in_director(directory_name, file_name);

    // Write out table1
    fprintf(out_file, "uint16_t %s_table1[] = \n{\n", table_name);
    for (size_t i = 0; i < 256; ++i)
    {
        if (i == 0)
            fprintf(out_file, "    ");
        fprintf(out_file, "%zu", prop_trie.table1[i]);
        if (i < 255)
            fprintf(out_file, ", ");
        if (i % 32 == 0 && i != 0)
            fprintf(out_file, "\n    ");
    }
    fprintf(out_file, "\n};\n\n");

    // Write out table2
    fprintf(out_file, "uint16_t %s_table2[] = \n{\n", table_name);
    for (size_t i = 0; i < prop_trie.table2.length; ++i)
    {
        if (i == 0)
            fprintf(out_file, "    ");
        fprintf(out_file, "%" PRIu64 "", prop_trie.table2.data[i]);
        if (i < prop_trie.table2.length - 1)
            fprintf(out_file, ", ");
        if (i % 32 == 0 && i != 0)
            fprintf(out_file, "\n    ");
    }
    fprintf(out_file, "\n};\n\n");

    // Write out table2
    fprintf(out_file, "uint64_t %s_leaves[] = \n{\n", table_name);
    for (size_t i = 0; i < prop_trie.leaves.length; ++i)
    {
        if (i == 0)
            fprintf(out_file, "    ");
        fprintf(out_file, "%" PRIu64 "", prop_trie.leaves.data[i]);
        if (i < prop_trie.leaves.length - 1)
            fprintf(out_file, ", ");
        if (i % 4 == 0 && i != 0)
            fprintf(out_file, "\n    ");
    }
    fprintf(out_file, "\n};\n\n");

    fclose(out_file);
    // free(out_file);

    necro_destroy_prop_trie(&prop_trie);
}

// Dead simple and direct parsing of Unicode Character Property files
NECRO_RETURN_CODE necro_unicode_property_parse(const char* directory_name)
{
    NecroUnicodePropertyParser uparser = necro_create_unicode_property_parser();

    char* str         = NULL;
    size_t str_length = 0;

    // DerivedCoreProperties.txt
#ifdef WIN32
    necro_open_file_in_directory(directory_name, "\\DerivedCoreProperties.txt", &str, &str_length);
#else
    necro_open_file_in_directory(directory_name, "/DerivedCoreProperties.txt", &str, &str_length);
#endif
    necro_set_str_unicode_property_parser(&uparser, str, str_length);
    necro_unicode_property_parse_file(&uparser);

    // PropList.txt
#ifdef WIN32
    necro_open_file_in_directory(directory_name, "\\PropList.txt", &str, &str_length);
#else
    necro_open_file_in_directory(directory_name, "/PropList.txt", &str, &str_length);
#endif
    necro_set_str_unicode_property_parser(&uparser, str, str_length);
    necro_unicode_property_parse_file(&uparser);

    // DerivedNumericType.txt
#ifdef WIN32
    necro_open_file_in_directory(directory_name, "\\DerivedNumericType.txt", &str, &str_length);
#else
    necro_open_file_in_directory(directory_name, "/DerivedNumericType.txt", &str, &str_length);
#endif
    necro_set_str_unicode_property_parser(&uparser, str, str_length);
    necro_unicode_property_parse_file(&uparser);

    necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\alphabetic_table.txt", "alphabetical", NECRO_UNICODE_ALPHABETIC);
    necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\lowercase_table.txt", "lowercase", NECRO_UNICODE_LOWERCASE);
    necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\uppercase_table.txt", "uppercase", NECRO_UNICODE_UPPERCASE);
    necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\id_start_table.txt", "id_start", NECRO_UNICODE_ID_START);
    necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\id_continue_table.txt", "id_continue", NECRO_UNICODE_ID_CONTINUE);
    // necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\xid_start_table.txt", "xid_start", NECRO_UNICODE_XID_START);
    // necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\xid_continue_table.txt", "xid_continue", NECRO_UNICODE_XID_CONTINUE);
    necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\decimal_table.txt", "decimal", NECRO_UNICODE_DECIMAL);
    necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\math_table.txt", "math", NECRO_UNICODE_MATH);
    necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\grapheme_base_table.txt", "grapheme_base", NECRO_UNICODE_GRAPHEME_BASE);
    necro_prop_trie_from_parser(&uparser, directory_name, "\\tables\\whitespace_table.txt", "whitespace", NECRO_UNICODE_WHITE_SPACE);

    return NECRO_SUCCESS;
}


///////////////////////////////////////////////////////
// Get Properties using generated tables
///////////////////////////////////////////////////////

uint16_t alphabetical_table1[] =
{
    0, 256, 512, 768, 1024, 1280, 1536, 1792, 312, 312, 2560, 2816, 3072, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537
};

uint16_t alphabetical_table2[] =
{
    0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 5, 0, 6, 7, 8, 4, 4, 9, 4, 10, 11, 12, 13, 14, 15, 4, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0, 52,
    53, 54, 55, 4, 4, 4, 4, 4, 56, 57, 58, 59, 60, 61, 62, 63, 4, 4, 4, 4, 4, 4, 4, 4, 64, 65, 66, 67, 68, 69, 70, 71,
    72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 0, 82, 83, 84, 85, 74, 86, 87, 88, 4, 4, 4, 89, 4, 4, 4, 4, 90, 91, 92, 93, 0,
    94, 95, 0, 96, 97, 98, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 99, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 101, 102, 4, 103, 104, 105, 106, 107, 108, 0, 0, 0, 0, 0, 0, 0, 109,
    63, 110, 111, 112, 4, 113, 114, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 74, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 115, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 116, 117, 4, 4, 4, 4, 118, 119, 4, 115, 120, 4, 121, 122, 123,
    124, 4, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 4, 136, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 137, 138, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 4, 4, 4, 4, 4, 139, 4, 140, 141, 142, 19, 143, 4, 4, 4, 4, 144, 17, 145, 146, 0, 147, 4, 148, 149, 150, 132, 151, 152,
    153, 4, 154, 0, 155, 0, 0, 0, 0, 156, 157, 158, 159, 160, 161, 4, 4, 162, 163, 164, 165, 0, 0, 4, 4, 4, 4, 130, 166, 0, 0, 167,
    168, 169, 170, 171, 0, 172, 0, 173, 174, 175, 176, 74, 177, 178, 0, 4, 98, 179, 179, 180, 0, 0, 0, 0, 0, 0, 0, 181, 182, 0, 0, 4,
    182, 183, 184, 179, 185, 4, 186, 187, 0, 188, 189, 190, 191, 0, 0, 4, 192, 4, 193, 0, 0, 194, 195, 132, 196, 74, 0, 197, 0, 0, 0, 72,
    0, 71, 198, 0, 0, 0, 0, 199, 17, 200, 72, 0, 0, 0, 0, 201, 202, 203, 0, 204, 205, 206, 0, 0, 0, 0, 207, 0, 0, 0, 0, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 140, 0, 4, 208, 4, 4, 4, 209, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 208, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 210, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 4, 4, 4, 4, 4, 72, 169, 0, 211, 130, 212, 213, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 4, 214, 215, 216, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 19, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 179, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 4, 169, 114, 4, 4, 4, 4, 4, 217, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 218, 219, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 220, 221, 222, 223, 224, 4, 4, 4, 4, 225, 226, 227, 228, 229, 230, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 231,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 232, 4, 233, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 234, 235, 236, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 114, 237, 78, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 238, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 155, 4, 4, 4, 160,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 239, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 240, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 4, 4, 4, 4, 4, 241, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint64_t alphabetical_leaves[] =
{
    0ull, 576460743847706622ull, 297241973452963840ull, 18410715276682199039ull, 18446744073709551615ull,
    88094074470339ull, 13609596598936928288ull, 18446744056529672000ull, 18428729675200069631ull,
    18446744073709550595ull, 18446462598732840959ull, 18446744069456527359ull, 13834776580305453567ull,
    2119858418286774ull, 18446744069548736512ull, 18446673709243564031ull, 11241233151490523135ull,
    18446744073709486080ull, 18446744073709543424ull, 1125899906842623ull, 301749971126844416ull,
    35184321757183ull, 8791831609343ull, 4602678814877679616ull, 18446466966713532416ull,
    17293822569102704639ull, 18446181192473632767ull, 16412803692974677999ull, 1153765996922689951ull,
    14082190885810440174ull, 17732925109967239ull, 16424062692043243502ull, 2161727885562420159ull,
    16424062692043104238ull, 563017343310239ull, 14123225865944680428ull, 8461767ull,
    16429129241624174575ull, 64548249055ull, 16424625641996804079ull, 1688915364814303ull,
    16717361816799141871ull, 18158513764145585631ull, 3457638613854978028ull, 3377704004976767ull,
    576460752303423486ull, 8319ull, 4323434403644581270ull, 4026540127ull,
    1ull, 18446216308128218879ull, 2305843009196916483ull, 17978369712463020031ull,
    18446675800909348864ull, 18446744070219907199ull, 17870283321406070975ull, 18446744070446333439ull,
    9168765891372858879ull, 18446744073701162813ull, 18446744073696837631ull, 2281701375ull,
    18446744069414649855ull, 4557642822898941951ull, 18446744073709551614ull, 18446638520593285119ull,
    18446744069548802046ull, 144053615424700415ull, 4503595333443583ull, 3905461007941631ull,
    18433233274827440127ull, 276824575ull, 18446744069414584320ull, 144115188075855871ull,
    18446471394825863167ull, 18014398509481983ull, 143851303137705983ull, 8796093022142464ull,
    18446480190918885375ull, 1023ull, 18446744069683019775ull, 9007192812290047ull,
    549755813888ull, 18442240474082181119ull, 4079ull, 18158781978395017215ull,
    1125625028935679ull, 4611686018360336384ull, 16717361816799216127ull, 31487813996249088ull,
    9006649498927104ull, 18446744070475743231ull, 4611686017001275199ull, 6908521828386340863ull,
    2295745090394464220ull, 9223934986808197120ull, 536805376ull, 17581979622616071300ull,
    18446744069414601696ull, 511ull, 18428729675200069632ull, 4398046511103ull,
    18446603336221196287ull, 18446744071562067967ull, 3509778554814463ull, 18446498607738650623ull,
    141836999983103ull, 9187201948305063935ull, 18446744071553646463ull, 140737488355328ull,
    2251241253188403424ull, 18446744068886102015ull, 17870283321406128127ull, 18446462598732840928ull,
    576460748008488959ull, 18446462598732840960ull, 281474976710655ull, 8191ull,
    4611686018427322368ull, 13198434443263ull, 10371930679322607615ull, 18446744060816261120ull,
    288230376151710207ull, 18410715276690587648ull, 1099511625659ull, 4503599627370495ull,
    7564921474075590703ull, 18446471394825862144ull, 2305843004919250943ull, 18444492273895866367ull,
    8935422993945886720ull, 36028797018963967ull, 14159317224157888511ull, 9223372036854775807ull,
    17169970223906821ull, 18446602782178705022ull, 18446462873476530175ull, 8796093022207ull,
    18446462667452317695ull, 1152921504606845055ull, 18446532967477018623ull, 67108863ull,
    6881498031078244479ull, 18446744073709551579ull, 18446744073709027328ull, 4611686018427387903ull,
    18446744073709355007ull, 1152640029630136575ull, 18437455399478099968ull, 2305843009213693951ull,
    576460743713488896ull, 18446743798965862398ull, 486341884ull, 13258596753222922239ull,
    1073692671ull, 576460752303423487ull, 9007199254740991ull, 18446744069951455231ull,
    131071ull, 18446708893632430079ull, 576460752303359999ull, 18446744070488326143ull,
    4128527ull, 18446462599806582783ull, 1152921504591118335ull, 18446463698244468735ull,
    68719476735ull, 1095220854783ull, 10502394331027995967ull, 36028792728190975ull,
    2147483647ull, 15762594400829440ull, 288230371860938751ull, 13907115649320091647ull,
    18014398491652207ull, 2305843004918726656ull, 536870911ull, 137438953215ull,
    2251795522912255ull, 262143ull, 2251799813685247ull, 1099511627775ull,
    18446463149025525759ull, 63ull, 144115188075855868ull, 2199023190016ull,
    20266198323101808ull, 335544350ull, 4656722014700830719ull, 18446464796682337663ull,
    2199023255551ull, 16424062692043104239ull, 68191066527ull, 1979ull,
    179ull, 9169328841326329855ull, 1056964608ull, 17ull,
    8795690369023ull, 9223372041149743103ull, 9216616637413720063ull, 553648079ull,
    9187343239835811327ull, 18445618173802708993ull, 36027697507139583ull, 13006395723845991295ull,
    18446741595513422027ull, 24870911ull, 36028792723996672ull, 140737488355327ull,
    15ull, 127ull, 70368744112128ull, 16212958624174047247ull,
    65535ull, 9223372036854710303ull, 4294443008ull, 12884901888ull,
    1152921504606846975ull, 2305570330330005503ull, 1140785663ull, 18446744073707454463ull,
    17005555242810474495ull, 18446744073709551599ull, 8935141660164089791ull, 18446744073709419615ull,
    18446743249075830783ull, 17870283321271910397ull, 18437736874452713471ull, 18446603336221163519ull,
    18446741874686295551ull, 4087ull, 8641373536127ull, 31ull,
    143ull, 790380184120328175ull, 6843210385291930244ull, 1152917029519358975ull,
    18446466996779287551ull, 8388607ull, 18446462615912710143ull, 8589934591ull,
    1073741823ull
};

uint16_t lowercase_table1[] =
{
    0, 256, 512, 768, 1024, 1280, 181, 1792, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
    181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
    181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
    181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
    181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
    181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
    181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
    181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181
};

uint16_t lowercase_table2[] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 21, 0, 24, 24, 24, 0, 25, 25, 26, 25, 27, 28, 29, 30, 0,
    31, 32, 0, 33, 34, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 37, 25, 38, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 41, 0, 42, 43, 44, 45, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 46, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 47, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 49, 50, 0, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 52, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 70, 71, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint64_t lowercase_leaves[] =
{
    0ull, 576460743713488896ull, 297241973452963840ull, 18410715274543104000ull, 6172933889249159850ull,
    15324248332066007893ull, 16596095761559859497ull, 12261519110656315968ull, 10663022717737544362ull,
    18446744073709529733ull, 144115188074807295ull, 133143986179ull, 4362299189061746720ull,
    18446726481523572736ull, 1814856824841797631ull, 18446462598732840960ull, 12297829383904690175ull,
    12297829382473033730ull, 12297829382473045332ull, 187649984473770ull, 18446744069414584320ull,
    511ull, 16717361816799215616ull, 4539628424389459968ull, 18446744073709551615ull,
    12297829382473034410ull, 12297829382829550250ull, 71777214282006783ull, 4611405638684049471ull,
    4674456033467236607ull, 61925590106570972ull, 9223934986808197120ull, 536805376ull,
    3607524039012697088ull, 18446462598732858304ull, 16ull, 4398046445568ull,
    4601013482110844927ull, 2339875276368554ull, 36009005809663ull, 46912496118442ull,
    984263338ull, 12298110845996498944ull, 10808545280696953514ull, 189294853869949098ull,
    504403158265495552ull, 18446462873476530175ull, 16253055ull, 134217726ull,
    18446742974197923840ull, 65535ull, 1152921504590069760ull, 2251799813685247ull,
    4294967295ull, 4503599560261632ull, 1099509514240ull, 16987577794709946364ull,
    18446739675663106031ull, 72057592964186127ull, 17592185782272ull, 18158513701852807104ull,
    18446673704966422527ull, 1152921487426978047ull, 281474972516352ull, 274877905920ull,
    17293822586148356092ull, 18428729675466407935ull, 18446462598737002495ull, 18446739675663105535ull,
    3063ull, 18446744056529682432ull, 15ull
};

uint16_t uppercase_table1[] =
{
    0, 256, 512, 768, 1024, 1280, 180, 1792, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
    180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
    180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
    180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
    180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
    180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
    180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
    180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180
};

uint16_t uppercase_table2[] =
{
    0, 1, 0, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 9, 10, 11, 12, 13, 14, 15, 16, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 18, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 21, 0, 0, 0, 0, 0, 22, 22, 23, 22, 24, 25, 26, 27, 0,
    0, 0, 0, 28, 29, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 31, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 34, 22, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 36, 37, 0, 38, 39, 40, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 41, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42, 0, 43, 44, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 45, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 46, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 63, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 43, 64, 65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint64_t uppercase_leaves[] =
{
    0ull, 134217726ull, 2139095039ull, 12273810184460391765ull, 3122495741643543722ull,
    1274187559846268630ull, 6184099063146390672ull, 7783721355972007253ull, 21882ull,
    9242793810247811072ull, 17575006099264ull, 16613872850358272000ull, 281474976710655ull,
    6148914689804861440ull, 6148914691236516865ull, 6148914691236506283ull, 18446274948748367189ull,
    8388607ull, 18446744069414584320ull, 8383ull, 18014398509481983ull,
    16717361816799215616ull, 6148914691236517205ull, 6148914690880001365ull, 18374966856193736448ull,
    280378317225728ull, 1080863910568919040ull, 1080897995681042176ull, 13839347594782259332ull,
    281470681743392ull, 8ull, 18428729675200069632ull, 65535ull,
    140737488355327ull, 13845730589451223040ull, 1169903278445909ull, 23456248059221ull,
    89478485ull, 6148633210533183488ull, 7638198793012598101ull, 98935522281728085ull,
    576460743713488896ull, 1099511627775ull, 18446462598732840960ull, 1048575ull,
    2251799813685247ull, 4294967295ull, 18442240474149289983ull, 18446742974197940223ull,
    17977448100528131ull, 4398046445568ull, 8863084067199903664ull, 18446726481523637343ull,
    288230371856744511ull, 70368743129088ull, 17293822586282573568ull, 18446462598737035263ull,
    18446742974197924863ull, 576460735123554305ull, 9007198986305536ull, 140737484161024ull,
    2199023190016ull, 1024ull, 17179869183ull, 18446466996779287551ull,
    1023ull
};

uint16_t id_start_table1[] =
{
    0, 256, 512, 768, 1024, 1280, 1536, 1792, 312, 312, 2560, 2816, 3072, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537
};

uint16_t id_start_table2[] =
{
    0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 5, 0, 6, 7, 8, 4, 4, 9, 4, 10, 11, 12, 13, 14, 15, 4, 16, 17, 18, 19, 20, 21,
    22, 23, 0, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 43, 45, 46, 47, 48, 49, 0, 50,
    51, 52, 53, 4, 4, 4, 4, 4, 54, 55, 56, 57, 58, 59, 60, 61, 4, 4, 4, 4, 4, 4, 4, 4, 62, 63, 64, 65, 66, 67, 68, 14,
    69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 0, 79, 80, 81, 82, 83, 84, 85, 86, 4, 4, 4, 0, 4, 4, 4, 4, 87, 88, 89, 90, 0,
    91, 92, 0, 93, 94, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 95, 96, 4, 97, 98, 99, 100, 101, 0, 0, 0, 0, 0, 0, 0, 0, 102,
    61, 103, 104, 105, 4, 106, 107, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 71, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 108, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 109, 110, 4, 4, 4, 4, 111, 112, 113, 108, 114, 4, 115, 116, 117,
    67, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 4, 130, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 131, 132, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 4, 4, 4, 4, 4, 133, 4, 134, 135, 136, 137, 138, 4, 4, 4, 4, 139, 140, 141, 142, 0, 143, 4, 144, 145, 146, 147, 148, 149,
    150, 4, 151, 0, 152, 0, 0, 0, 0, 153, 154, 155, 156, 113, 157, 4, 4, 158, 159, 160, 83, 0, 0, 4, 4, 4, 4, 161, 162, 0, 0, 163,
    164, 72, 165, 166, 0, 167, 0, 168, 169, 170, 171, 71, 172, 173, 0, 4, 12, 174, 174, 83, 0, 0, 0, 0, 0, 0, 0, 175, 176, 0, 0, 177,
    0, 178, 179, 180, 181, 182, 183, 184, 0, 185, 72, 32, 186, 0, 0, 152, 187, 108, 188, 0, 0, 189, 190, 108, 191, 192, 0, 58, 0, 0, 0, 193,
    0, 14, 194, 0, 0, 0, 0, 195, 196, 197, 69, 0, 0, 0, 0, 198, 199, 200, 0, 201, 202, 203, 0, 0, 0, 0, 204, 0, 0, 0, 0, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 134, 0, 4, 189, 4, 4, 4, 205, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 189, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 43, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 4, 4, 4, 4, 4, 69, 72, 0, 206, 108, 207, 200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 4, 208, 209, 210, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 137, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 174, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 4, 72, 107, 4, 4, 4, 4, 4, 211, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 212, 213, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 214, 215, 216, 217, 218, 4, 4, 4, 4, 219, 220, 221, 222, 223, 224, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 225, 4, 205, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 226, 227, 228, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 229, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 152, 4, 4, 4, 113,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 230, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 231, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,
    4, 4, 4, 4, 4, 4, 4, 232, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint64_t id_start_leaves[] =
{
    0ull, 576460743847706622ull, 297241973452963840ull, 18410715276682199039ull, 18446744073709551615ull,
    88094074470339ull, 13609596598936928256ull, 18446744056529672000ull, 18428729675200069631ull,
    18446744073709550595ull, 18446462598732840959ull, 18446744069456527359ull, 511ull,
    2119858418286592ull, 18446744069414584320ull, 18446392229988665343ull, 11241196188469297151ull,
    281474976514048ull, 18446744073709543424ull, 563224831328255ull, 301749971126844416ull,
    1168302407679ull, 8791831609343ull, 4602678814877679616ull, 2594073385365405680ull,
    18446181140919287808ull, 2577745637692514273ull, 1153765945374687232ull, 247132830528276448ull,
    7881300924956672ull, 2589004636761079776ull, 144115200960823296ull, 2589004636760940512ull,
    562965791113216ull, 288167810662516712ull, 65536ull, 2594071186342010848ull,
    13002342400ull, 2589567586714640353ull, 1688863818907648ull, 2882303761516978144ull,
    18158513712597581824ull, 3457638613854978016ull, 127ull, 3940649673949182ull,
    2309762420256548246ull, 4026531935ull, 1ull, 35184372088575ull,
    7936ull, 9223380832947798015ull, 18438229877581611008ull, 18446744069414600707ull,
    17870283321406070975ull, 18446744070446333439ull, 9168765891372858879ull, 18446744073701162813ull,
    18446744073696837631ull, 134217727ull, 18446744069414649855ull, 4557642822898941951ull,
    18446744073709551614ull, 18446638520593285119ull, 18446744069548802046ull, 144053615424700415ull,
    1125895612129279ull, 527761286627327ull, 4503599627370495ull, 276824064ull,
    144115188075855871ull, 18446469195802607615ull, 18014398509481983ull, 2147483647ull,
    8796093022142464ull, 18446480190918885375ull, 1023ull, 18446744069422972927ull,
    2097151ull, 549755813888ull, 4503599627370464ull, 4064ull,
    18158724812380307448ull, 274877906943ull, 68719476735ull, 4611686018360336384ull,
    16717361816799216127ull, 28110114275721216ull, 18446744070475743231ull, 4611686017001275199ull,
    6908521828386340863ull, 2295745090394464220ull, 9223934986808197120ull, 536805376ull,
    17582049991377026180ull, 18446744069414601696ull, 18446603336221196287ull, 18446744071562067967ull,
    3509778554814463ull, 18446498607738650623ull, 141836999983103ull, 9187201948305063935ull,
    2139062143ull, 2251241253188403424ull, 18446744069288755199ull, 17870283321406128127ull,
    18446462598732840928ull, 576460748008488959ull, 18446462598732840960ull, 281474976710655ull,
    8191ull, 4611686018427322368ull, 13198434443263ull, 9223512774343131135ull,
    18446744070488326143ull, 18446744060816261120ull, 288230376151710207ull, 18410715276690587648ull,
    34359736251ull, 4503599627370492ull, 7564921474075590656ull, 18446462873610746880ull,
    2305843004918726783ull, 2251799813685232ull, 8935422993945886720ull, 2199023255551ull,
    14159317224157876215ull, 4495436853045886975ull, 7890092085477381ull, 18446602782178705022ull,
    18446462873476530175ull, 34359738367ull, 18446462667452317695ull, 1152921504606845055ull,
    18446532967477018623ull, 67108863ull, 6881498030004502655ull, 18446744073709551579ull,
    1125899906842623ull, 18446744073709027328ull, 4611686018427387903ull, 18446744073709486080ull,
    18446744073709355007ull, 1152640029630136575ull, 18437455399478099968ull, 2305843009213693951ull,
    576460743713488896ull, 18446743798965862398ull, 9223372036854775807ull, 486341884ull,
    13258596753222922239ull, 1073692671ull, 576460752303423487ull, 9007199254740991ull,
    18446744069951455231ull, 131071ull, 18446708893632430079ull, 18014398509418495ull,
    4128527ull, 18446462599806582783ull, 1152921504591118335ull, 18446463698244468735ull,
    36028797018963967ull, 1095220854783ull, 10502394331027995967ull, 36028792728190975ull,
    15762594400829440ull, 288230371860938751ull, 13907115649320091647ull, 18014398491590657ull,
    2305843004918726656ull, 536870911ull, 137438953215ull, 2251795522912255ull,
    262143ull, 2251799813685247ull, 18446463149025525759ull, 63ull,
    72057594037927928ull, 281474976710648ull, 2199023190016ull, 549755813880ull,
    20266198323101712ull, 2251799813685240ull, 335544350ull, 17592185782271ull,
    18446464796682337663ull, 16643063808ull, 1920ull, 176ull,
    140737488355327ull, 251658240ull, 16ull, 8796093022207ull,
    17592186044415ull, 9223372041149743103ull, 290482175965394945ull, 18446744073441181696ull,
    536871887ull, 140737488354815ull, 18445618173802708993ull, 65535ull,
    562949953420159ull, 18446741595513421888ull, 16778239ull, 2251795518717952ull,
    15ull, 70368744112128ull, 16212958624174047247ull, 65567ull,
    4294443008ull, 12884901888ull, 1152921504606846975ull, 2305570330330005503ull,
    67043839ull, 18446744073707454463ull, 17005555242810474495ull, 18446744073709551599ull,
    8935141660164089791ull, 18446744073709419615ull, 18446743249075830783ull, 17870283321271910397ull,
    18437736874452713471ull, 18446603336221163519ull, 18446741874686295551ull, 4087ull,
    31ull, 790380184120328175ull, 6843210385291930244ull, 1152917029519358975ull,
    8388607ull, 18446462615912710143ull, 8589934591ull, 1073741823ull
};

uint16_t id_continue_table1[] =
{
    0, 256, 512, 768, 1024, 1280, 1536, 1792, 312, 312, 2560, 2816, 3072, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 14336, 14592, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824
};

uint16_t id_continue_table2[] =
{
    0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 5, 4, 6, 7, 8, 4, 4, 9, 4, 10, 11, 12, 13, 14, 15, 4, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 4, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 4,
    52, 53, 54, 4, 4, 4, 4, 4, 55, 56, 57, 58, 59, 60, 61, 62, 4, 4, 4, 4, 4, 4, 4, 4, 63, 64, 65, 66, 67, 4, 68, 69,
    70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 4, 81, 4, 82, 83, 84, 85, 86, 4, 4, 4, 87, 4, 4, 4, 4, 88, 89, 90, 91, 92,
    93, 94, 95, 96, 97, 98, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 99, 100, 4, 101, 102, 103, 104, 105, 80, 80, 80, 80, 80, 80, 80, 80, 106,
    62, 107, 108, 109, 4, 110, 111, 80, 80, 80, 80, 80, 80, 80, 80, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 72, 80, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 112, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 113, 114, 4, 4, 4, 4, 115, 116, 4, 19, 117, 4, 118, 119, 120,
    82, 4, 121, 122, 123, 4, 124, 125, 126, 4, 127, 128, 129, 4, 130, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 131, 132, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 4, 4, 4, 4, 4, 122, 4, 133, 134, 135, 19, 136, 4, 4, 4, 4, 137, 17, 138, 139, 140, 141, 4, 142, 143, 144, 145, 146, 147,
    148, 4, 149, 80, 150, 80, 151, 80, 80, 152, 153, 154, 155, 53, 156, 4, 4, 157, 158, 159, 160, 80, 80, 4, 4, 4, 4, 125, 161, 80, 80, 162,
    163, 164, 165, 166, 80, 167, 80, 168, 169, 170, 171, 72, 172, 173, 80, 4, 98, 174, 174, 175, 80, 80, 80, 80, 80, 80, 80, 176, 177, 80, 80, 4,
    178, 149, 179, 180, 181, 4, 182, 183, 80, 184, 185, 186, 187, 80, 80, 4, 188, 4, 189, 80, 80, 190, 191, 4, 192, 83, 193, 194, 80, 80, 80, 149,
    80, 195, 196, 80, 80, 80, 80, 145, 197, 198, 70, 80, 80, 80, 80, 199, 200, 201, 80, 202, 203, 204, 80, 80, 80, 80, 205, 80, 80, 80, 80, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 133, 80, 4, 206, 4, 4, 4, 207, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 206, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 4, 4, 4, 4, 4, 4, 4, 4, 4, 208, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 4,
    4, 4, 4, 4, 4, 4, 4, 70, 209, 80, 210, 125, 211, 212, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 4, 80, 80, 4, 213, 214, 215, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 19, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 174, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 4,
    4, 4, 4, 164, 111, 4, 4, 4, 4, 4, 216, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 4, 217, 218, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 219, 220, 80, 80, 221, 80, 80, 80, 80, 80, 80, 4, 222, 223, 224, 225, 226, 4, 4, 4, 4, 227, 228, 229, 230, 231, 232, 80,
    80, 80, 80, 80, 80, 80, 80, 233, 234, 235, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 236,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 4,
    4, 4, 237, 4, 238, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 239, 240, 241, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 242, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 150, 4, 4, 4, 53,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 243, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 244, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 4,
    4, 4, 4, 4, 4, 4, 4, 245, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 4, 4, 4, 112, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint64_t id_continue_leaves[] =
{
    287948901175001088ull, 576460745995190270ull, 333270770471927808ull, 18410715276682199039ull, 18446744073709551615ull,
    88094074470339ull, 13609878073913638911ull, 18446744056529672128ull, 18428729675200069631ull,
    18446744073709550843ull, 18446462598732840959ull, 18446744069456527359ull, 13835058055282033151ull,
    2119858418286774ull, 18446744069548736512ull, 18446678103011885055ull, 11529212845433552895ull,
    18446744073709486080ull, 18446744073709545471ull, 1125899906842623ull, 2612087783874887679ull,
    70368744177663ull, 8792066490367ull, 4602678814877679616ull, 18446744056529158144ull,
    18446462392574410751ull, 17565725197581524975ull, 5765733215448889759ull, 15235112390417287150ull,
    18014125208779143ull, 17576984196650090478ull, 18302910150157089727ull, 17576984196649951214ull,
    844217442122143ull, 14123225865944680428ull, 281200107273671ull, 16429129241624174591ull,
    281264647060959ull, 17577547146603651055ull, 1970115463626207ull, 18446744073709412335ull,
    18158794964244397535ull, 3457638613854978028ull, 3658904103781503ull, 576460752303423486ull,
    67076095ull, 4323434403644581270ull, 4093591391ull, 14024213633433600001ull,
    18446216308128218879ull, 2305843009196916703ull, 64ull, 18446744073709487103ull,
    18446744070488326143ull, 17870283321406070975ull, 18446744070446333439ull, 9168765891372858879ull,
    18446744073701162813ull, 18446744073696837631ull, 1123704775901183ull, 18446744069414649855ull,
    4557642822898941951ull, 18446744073709551614ull, 18446638520593285119ull, 18446744069548802046ull,
    144053615424700415ull, 9007194961862655ull, 3905461007941631ull, 4394566287359ull,
    18446744069481641984ull, 144115188075855871ull, 18446471394825863167ull, 18014398509481983ull,
    1152657619668697087ull, 8796093022207936ull, 18446480190918885375ull, 134153215ull,
    18446744069683019775ull, 11529215043920986111ull, 4611405093273535487ull, 0ull,
    4494803601395711ull, 4503599627370495ull, 72057594037927935ull, 4611686018427380735ull,
    16717361816799216127ull, 288230376151121920ull, 18158513697557839871ull, 18446744070475743231ull,
    4611686017001275199ull, 6908521828386340863ull, 2295745090394464220ull, 9223372036854775808ull,
    9223934986809245697ull, 536805376ull, 562821641207808ull, 17582049991377026180ull,
    18446744069414601696ull, 511ull, 18446603336221196287ull, 18446744071562067967ull,
    4494940973301759ull, 18446498607738650623ull, 9223513873854758911ull, 9187201948305063935ull,
    18446744071553646463ull, 2251518330118602976ull, 18446744069389418495ull, 17870283321406128127ull,
    18446462598732840928ull, 576460748008488959ull, 18446462598732840960ull, 281474976710655ull,
    8191ull, 4611686018427322368ull, 17592185987071ull, 13830835930631503871ull,
    18446744060816261120ull, 288230376151710207ull, 18410715276690587648ull, 1099511627775ull,
    16789419406609285183ull, 18446532967477018623ull, 2305843004919775231ull, 9223372032626884609ull,
    36028797018963967ull, 18194542490348896255ull, 35184368733388807ull, 18446602782178705022ull,
    18446462873476530175ull, 288010473826156543ull, 18446462667452317695ull, 1152921504606845055ull,
    67108863ull, 6881498031078244479ull, 18446744073709551579ull, 18446744073709027328ull,
    4611686018427387903ull, 18446744073709355007ull, 1152640029630136575ull, 7036870122864639ull,
    18437455399478157312ull, 2305843009213693951ull, 9799832780635308032ull, 18446743798965862398ull,
    9223372036854775807ull, 486341884ull, 13258596753222922239ull, 1073692671ull,
    576460752303423487ull, 9007199254740991ull, 2305843009213693952ull, 18446744069951455231ull,
    4295098367ull, 18446708893632430079ull, 576460752303359999ull, 4128527ull,
    18446466993558126591ull, 1152921504591118335ull, 18446463698244468735ull, 68719476735ull,
    1095220854783ull, 10502394331027995967ull, 36028792728190975ull, 2147483647ull,
    15762594400829440ull, 288230371860938751ull, 13907115649320091647ull, 9745789593611923567ull,
    2305843004918726656ull, 536870911ull, 549755813631ull, 2251795522912255ull,
    262143ull, 2251799813685247ull, 287950000686628863ull, 18446463149025525759ull,
    131071ull, 9223653236953579647ull, 287951100198191104ull, 18437736874454810623ull,
    22517998136787056ull, 402595359ull, 4683743612465053695ull, 18446464796682337663ull,
    287957697268023295ull, 18153444948953374703ull, 8760701963286943ull, 1140787199ull,
    67043519ull, 18392700878181105663ull, 1056964609ull, 67043345ull,
    1023ull, 287966492958392319ull, 18446744069414584320ull, 9223376434901286911ull,
    18446744073709486208ull, 603979727ull, 18410715276690587135ull, 18445618173869752321ull,
    36027697507139583ull, 13006395723845991295ull, 18446741595580465407ull, 4393784803327ull,
    36028792723996672ull, 140737488355327ull, 15ull, 127ull,
    4395899027455ull, 8796093022142464ull, 16212958624241090575ull, 65535ull,
    9223372036854710303ull, 4294934528ull, 12884901888ull, 1152921504606846975ull,
    2305570330330005503ull, 1677656575ull, 17872504197455282176ull, 65970697670631ull,
    28ull, 18446744073707454463ull, 17005555242810474495ull, 18446744073709551599ull,
    8935141660164089791ull, 18446744073709419615ull, 18446743249075830783ull, 17870283321271910397ull,
    18437736874452713471ull, 18446603336221163519ull, 18446741874686295551ull, 18446744073709539319ull,
    17906312118425092095ull, 9042383626829823ull, 281470547525648ull, 8641373536127ull,
    8323103ull, 67045375ull, 790380184120328175ull, 6843210385291930244ull,
    1152917029519358975ull, 8388607ull, 18446462615912710143ull, 8589934591ull,
    1073741823ull
};

uint16_t whitespace_table1[] =
{
    0, 256, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193,
    193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 14336, 14592, 520, 520, 520, 520, 520, 520, 520,
    520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520,
    520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520,
    520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520,
    520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520,
    520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520,
    520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520
};

uint16_t whitespace_table2[] =
{
    0, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4,
    5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint64_t whitespace_leaves[] =
{
    4294983168ull, 0ull, 4294967328ull, 1ull, 144036023240703ull,
    2147483648ull
};

uint16_t decimal_table1[] =
{
    0, 256, 512, 768, 1024, 1280, 114, 1792, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114,
    114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 14336, 14592, 1800, 1800, 1800, 1800, 1800, 1800, 1800,
    1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800,
    1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800,
    1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800,
    1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800,
    1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800,
    1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800
};

uint16_t decimal_table2[] =
{
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 0, 1, 1, 1, 3, 1,
    1, 1, 1, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 5, 1, 5, 2, 1, 1, 1, 1,
    3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 5,
    1, 1, 1, 1, 6, 1, 5, 1, 1, 7, 1, 1, 5, 0, 1, 1, 7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 5, 3, 1, 1, 8, 1, 5, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    4, 1, 0, 9, 1, 1, 5, 1, 1, 1, 0, 1, 1, 1, 1, 1, 5, 1, 5, 1, 1, 1, 1, 1, 5, 1, 3, 0, 1, 1, 1, 1,
    1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 1, 1, 1, 5, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 10, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint64_t decimal_leaves[] =
{
    287948901175001088ull, 0ull, 4393751543808ull, 1023ull, 281200098803712ull,
    67043328ull, 65472ull, 67044351ull, 287948901242044416ull,
    18428729675200069632ull, 18446744073709535232ull
};

uint16_t grapheme_base_table1[] =
{
    0, 256, 512, 768, 1024, 1280, 1536, 1792, 311, 311, 2560, 2816, 3072, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537,
    2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 2537, 14336, 14592, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824,
    2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824, 2824
};

uint16_t grapheme_base_table2[] =
{
    0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 5, 6, 3, 3, 3, 7, 3, 8, 9, 10, 11, 12, 13, 3, 14, 15, 16, 17, 18, 19,
    20, 21, 4, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
    51, 52, 53, 3, 3, 3, 3, 3, 54, 55, 56, 57, 58, 59, 60, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 61, 62, 63, 64, 65, 66, 67,
    62, 68, 69, 70, 71, 72, 73, 74, 75, 76, 4, 77, 78, 79, 80, 81, 82, 83, 84, 3, 3, 3, 4, 3, 3, 3, 3, 85, 86, 87, 88, 89,
    90, 91, 4, 3, 3, 92, 3, 3, 3, 3, 3, 3, 3, 3, 3, 93, 94, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 95, 96, 97, 98, 99, 3, 100, 101, 102, 103, 104, 3, 105, 106, 107, 3, 3, 3, 108, 109,
    110, 111, 3, 112, 3, 113, 114, 99, 3, 3, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 69, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 115, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 116, 117, 3, 3, 3, 3, 118, 119, 120, 121, 3, 3, 122, 123, 124,
    125, 3, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 3, 137, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 114, 138, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 3, 3, 3, 3, 3, 139, 3, 140, 141, 142, 3, 143, 3, 3, 3, 3, 3, 144, 145, 146, 147, 148, 3, 149, 110, 3, 150, 151, 152,
    153, 3, 154, 155, 3, 156, 157, 4, 4, 61, 158, 159, 160, 161, 162, 3, 3, 163, 164, 165, 166, 4, 4, 3, 3, 3, 3, 167, 168, 4, 4, 169,
    170, 171, 172, 173, 4, 174, 145, 175, 176, 177, 178, 179, 180, 181, 4, 3, 182, 183, 184, 185, 4, 4, 4, 4, 186, 4, 4, 165, 187, 4, 4, 188,
    189, 190, 191, 192, 193, 194, 195, 196, 4, 197, 198, 199, 200, 4, 4, 125, 201, 202, 203, 4, 4, 204, 205, 206, 207, 208, 209, 210, 4, 4, 4, 211,
    4, 0, 212, 4, 4, 4, 4, 213, 214, 215, 62, 4, 4, 4, 4, 216, 217, 218, 4, 219, 220, 221, 4, 4, 4, 4, 222, 4, 4, 4, 4, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 140, 4, 3, 223, 3, 3, 3, 224, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 225, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 226, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3,
    3, 3, 3, 3, 3, 3, 3, 62, 227, 4, 228, 229, 230, 231, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 232, 4, 3, 233, 234, 235, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 236, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 183, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3,
    3, 3, 3, 237, 238, 3, 3, 3, 3, 3, 239, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 240, 241, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3,
    3, 3, 69, 242, 243, 244, 245, 3, 246, 4, 247, 3, 248, 4, 4, 3, 249, 250, 251, 252, 253, 3, 3, 3, 3, 254, 3, 3, 3, 3, 255, 3,
    3, 3, 3, 3, 3, 3, 3, 256, 257, 258, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3,
    3, 3, 259, 3, 260, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 261, 262, 4, 4, 4, 4, 4, 263, 264, 265, 266, 4, 4, 4, 4, 72,
    3, 267, 268, 116, 72, 269, 270, 271, 272, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 273, 3, 107, 3, 274, 92,
    275, 276, 4, 277, 278, 279, 280, 4, 281, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 282, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 262, 3, 3, 3, 120,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 283, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 284, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3,
    3, 3, 3, 3, 3, 3, 3, 285, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint64_t grapheme_base_leaves[] =
{
    18446744069414584320ull, 9223372036854775807ull, 18446708885042495488ull, 18446744073709551615ull, 0ull,
    18230289816619057152ull, 18446744056529672176ull, 18446744073709550599ull, 18446462598732840959ull,
    18446744073684385791ull, 4611686018427447295ull, 8875257859342409ull, 18446744072770092992ull,
    18446462594437875711ull, 18446676317383426047ull, 281474976530431ull, 18446744073709543424ull,
    563224831328255ull, 14407024004051238911ull, 9223091730180472831ull, 8792905351167ull,
    4602678814877679616ull, 17005592192950992888ull, 18446744022153289217ull, 11801117674547290093ull,
    4611685759387195777ull, 14082190885810440168ull, 26176899533242369ull, 16424062692043243496ull,
    145240825989765633ull, 2589004636760940524ull, 72057334997719425ull, 9511539847517292520ull,
    576460477425589702ull, 2594071186342010862ull, 18374967692772769822ull, 7201253605142028285ull,
    1970063917714843ull, 12105675798371753964ull, 18446743816002133441ull, 3457638613854978028ull,
    8162501577605247ull, 9227312686528724990ull, 268402815ull, 2309762420256548246ull,
    4093575263ull, 18257592889309659135ull, 9223407221226864383ull, 13835058055282171680ull,
    134209471ull, 11025410022128484351ull, 18438299816841707519ull, 18446744073172672411ull,
    18446744073709494463ull, 18446744070446333439ull, 9168765891372858879ull, 18446744073701162813ull,
    18446744073696837631ull, 2305843005052944383ull, 18446744069481693183ull, 4557642822898941951ull,
    18446744069951455231ull, 144115188075855871ull, 28147493376352255ull, 527761286627327ull,
    13857576053419016191ull, 287953295462367679ull, 18446744069481629695ull, 18446469195802607519ull,
    18014398509481983ull, 142723723898650623ull, 8796093022207985ull, 18446480190918885375ull,
    18446744072769963007ull, 18446744072744861695ull, 2216727123329023ull, 70364516254719ull,
    16730872615681392624ull, 2301348205679284219ull, 18446678944825475068ull, 17297302248526708735ull,
    17883811712474284031ull, 18446744073709544447ull, 16717361816799216127ull, 67516619605672191ull,
    18446744070475743231ull, 4611686017001275199ull, 18437736874454810623ull, 9213520412398321631ull,
    18446604435732760575ull, 18443084903307280383ull, 18446744069951422463ull, 18446744073709490175ull,
    549755813887ull, 18446744069414586367ull, 18433233274827440127ull, 18446744073696968703ull,
    9223372036854775295ull, 18446603336221196287ull, 18446744071562067967ull, 18306147322842578943ull,
    18446498607738650623ull, 423311976693759ull, 9187201948305063935ull, 2139062143ull,
    32767ull, 18446744073642442751ull, 4503599627370495ull, 1152640029634330623ull,
    18446466996779352063ull, 18446744073709551614ull, 18446744073583722495ull, 18446462598732840928ull,
    576460752303390719ull, 18446462667452317695ull, 281474976710655ull, 18446744073709494271ull,
    18446744073709486207ull, 17592186044415ull, 13837450592584204287ull, 18446744070488326143ull,
    71213169107795967ull, 288230376151711743ull, 18410715276690587648ull, 287966081044182971ull,
    72057594037927935ull, 9222246137015025679ull, 18446673979843280895ull, 2305843007066996863ull,
    17021354791646789624ull, 9223371898409172991ull, 7179810929377279ull, 17293822568901324791ull,
    4495436853045886975ull, 17961621817131013ull, 18446602782178705022ull, 18446462873610747903ull,
    287982848596508671ull, 1152921504606845055ull, 18446532967477018623ull, 67108863ull,
    6881500229027758207ull, 18446744073709551579ull, 18446744073709027331ull, 18446744073709486080ull,
    18446744073709355007ull, 4611404543450677503ull, 18446462598799884288ull, 18437472441907806207ull,
    2305843009213693951ull, 9223372033633550335ull, 3458904697744456956ull, 13258596753222922239ull,
    1073692671ull, 576460752303423487ull, 18415218876317958023ull, 4563369983ull,
    2305843009213628416ull, 1152921496017043455ull, 18446708958056939519ull, 18014398509418495ull,
    18446744072635809791ull, 4194063ull, 18446466993558126591ull, 1152921504591118335ull,
    18446463698244468735ull, 140806207832063ull, 36028797018963967ull, 1095220854783ull,
    10502394331027995967ull, 18446744073705357311ull, 280927368380415ull, 17886045915806957568ull,
    9511602411127439359ull, 17365880163140632575ull, 18014398491590657ull, 18446744069448073727ull,
    4294967295ull, 36020138364895231ull, 18320643284143177727ull, 18376938279472726015ull,
    279276457033727ull, 511ull, 2251799813685247ull, 18160765497371525119ull,
    287948969894477823ull, 9223372032559808512ull, 66977855ull, 72057594037927933ull,
    281474976464768ull, 15674778503063011324ull, 287951100198191107ull, 18428747817141927928ull,
    33776997205213311ull, 9241386435364257788ull, 9007194959716863ull, 4552154060852822015ull,
    18446466995705593215ull, 287948933387255807ull, 11812376673615716332ull, 68182686110ull,
    738197411ull, 6487153788250488831ull, 67043570ull, 5693535091414794239ull,
    268435454ull, 6343320075151343615ull, 35180144164894ull, 18251893021081599ull,
    1023ull, 18446462886629867519ull, 648659083829706751ull, 9225623836668461055ull,
    9657969400896026625ull, 18446744073466347647ull, 34301019087ull, 4611967493404098047ull,
    18446497783104864319ull, 5068748604112895ull, 562949953420159ull, 18446741595580465216ull,
    4393774120959ull, 137359784339832832ull, 8866461766385663ull, 15ull,
    140737488355327ull, 127ull, 215502131560447ull, 9077567998853120ull,
    18410996751667298303ull, 16212958641286742079ull, 65535ull, 134217727ull,
    9223372036854710303ull, 4294443008ull, 12884901888ull, 1125899906842623ull,
    2147483647ull, 18446462598732840960ull, 1152921504606846975ull, 2305570330330005503ull,
    2482962943ull, 18446742424442109951ull, 66383014526975ull, 18446678103011880984ull,
    2199023255551ull, 35ull, 4503595332403200ull, 144115183789277183ull,
    18446744073707454463ull, 17005555242810474495ull, 18446744073709551599ull, 8935141660164089791ull,
    18446744073709419615ull, 18446743249075830783ull, 18446744073709539327ull, 540431955284459520ull,
    18437701690082721792ull, 4079ull, 65439ull, 3288268815ull,
    18446181123756130304ull, 9007199254740991ull, 790380184120328175ull, 6843210385291930244ull,
    1152917029519358975ull, 844424930131968ull, 18446321856950566911ull, 18014398509416446ull,
    35184372088831ull, 18446743798831644672ull, 1152921504606781447ull, 270583136767ull,
    287984081254219775ull, 33554431ull, 18446744069481627903ull, 70368744112383ull,
    9223372036854714367ull, 17616392892413116415ull, 287948935534739455ull, 18446744073709486087ull,
    70364449210368ull, 8388607ull, 18446462615912710143ull, 8589934591ull,
    1073741823ull
};

bool necro_is_alphabetical(uint32_t code_point)
{
    uint32_t table1_index = code_point >> 14;
    assert((table1_index >> 8) < 256);
    uint32_t table2_index = alphabetical_table1[table1_index] + ((code_point >> 6) & 0xFF);
    assert(table2_index < 65536);
    uint32_t leaf_index = alphabetical_table2[table2_index];
    assert(leaf_index < 65536);
    uint64_t leaf = alphabetical_leaves[leaf_index];
    return (leaf & (((uint64_t)1) << (((uint64_t)code_point & 0x3F)))) != 0;
}

bool necro_is_control(uint32_t code_point)
{
    return (code_point <= 0x1F) || (code_point >= 0x7F && code_point <= 0x9F);
}

bool necro_is_lowercase(uint32_t code_point)
{
    uint32_t table1_index = code_point >> 14;
    assert((table1_index >> 8) < 256);
    uint32_t table2_index = lowercase_table1[table1_index] + ((code_point >> 6) & 0xFF);
    assert(table2_index < 65536);
    uint32_t leaf_index = lowercase_table2[table2_index];
    assert(leaf_index < 65536);
    uint64_t leaf = lowercase_leaves[leaf_index];
    return (leaf & (((uint64_t)1) << (((uint64_t)code_point & 0x3F)))) != 0;
}

bool necro_is_uppercase(uint32_t code_point)
{
    uint32_t table1_index = code_point >> 14;
    assert((table1_index >> 8) < 256);
    uint32_t table2_index = uppercase_table1[table1_index] + ((code_point >> 6) & 0xFF);
    assert(table2_index < 65536);
    uint32_t leaf_index = uppercase_table2[table2_index];
    assert(leaf_index < 65536);
    uint64_t leaf = uppercase_leaves[leaf_index];
    return (leaf & (((uint64_t)1) << (((uint64_t)code_point & 0x3F)))) != 0;
}

bool necro_is_id_start(uint32_t code_point)
{
    uint32_t table1_index = code_point >> 14;
    assert((table1_index >> 8) < 256);
    uint32_t table2_index = id_start_table1[table1_index] + ((code_point >> 6) & 0xFF);
    assert(table2_index < 65536);
    uint32_t leaf_index = id_start_table2[table2_index];
    assert(leaf_index < 65536);
    uint64_t leaf = id_start_leaves[leaf_index];
    return (leaf & (((uint64_t)1) << (((uint64_t)code_point & 0x3F)))) != 0;
}

bool necro_is_id_continue(uint32_t code_point)
{
    uint32_t table1_index = code_point >> 14;
    assert((table1_index >> 8) < 256);
    uint32_t table2_index = id_continue_table1[table1_index] + ((code_point >> 6) & 0xFF);
    assert(table2_index < 65536);
    uint32_t leaf_index = id_continue_table2[table2_index];
    assert(leaf_index < 65536);
    uint64_t leaf = id_continue_leaves[leaf_index];
    return (leaf & (((uint64_t)1) << (((uint64_t)code_point & 0x3F)))) != 0;
}

bool necro_is_whitespace(uint32_t code_point)
{
    uint32_t table1_index = code_point >> 14;
    assert((table1_index >> 8) < 256);
    uint32_t table2_index = whitespace_table1[table1_index] + ((code_point >> 6) & 0xFF);
    assert(table2_index < 65536);
    uint32_t leaf_index = whitespace_table2[table2_index];
    assert(leaf_index < 65536);
    uint64_t leaf = whitespace_leaves[leaf_index];
    return (leaf & (((uint64_t)1) << (((uint64_t)code_point & 0x3F)))) != 0;
}

bool necro_is_ascii_digit(uint32_t code_point)
{
    switch (code_point)
    {
    case '0': return true;
    case '1': return true;
    case '2': return true;
    case '3': return true;
    case '4': return true;
    case '5': return true;
    case '6': return true;
    case '7': return true;
    case '8': return true;
    case '9': return true;
    default:  return false;
    }
}

bool necro_is_decimal(uint32_t code_point)
{
    uint32_t table1_index = code_point >> 14;
    assert((table1_index >> 8) < 256);
    uint32_t table2_index = decimal_table1[table1_index] + ((code_point >> 6) & 0xFF);
    assert(table2_index < 65536);
    uint32_t leaf_index = decimal_table2[table2_index];
    assert(leaf_index < 65536);
    uint64_t leaf = decimal_leaves[leaf_index];
    return (leaf & (((uint64_t)1) << (((uint64_t)code_point & 0x3F)))) != 0;
}

bool necro_is_grapheme_base(uint32_t code_point)
{
    uint32_t table1_index = code_point >> 14;
    assert((table1_index >> 8) < 256);
    uint32_t table2_index = grapheme_base_table1[table1_index] + ((code_point >> 6) & 0xFF);
    assert(table2_index < 65536);
    uint32_t leaf_index = grapheme_base_table2[table2_index];
    assert(leaf_index < 65536);
    uint64_t leaf = grapheme_base_leaves[leaf_index];
    uint64_t mask = (((uint64_t)1) << (((uint64_t)code_point & 0x3F)));
    uint64_t leaf_s = leaf & mask;
    bool    is_base = (leaf_s & (((uint64_t)1) << (((uint64_t)code_point & 0x3F)))) != 0;
    return is_base;
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_test_unicode_properties()
{
    necro_announce_phase("Unicode");
    //-------------------
    // Alphabetical
    for (uint32_t c = 'a'; c <= 'z'; ++c)
        assert(necro_is_alphabetical(c));
    for (uint32_t c = 'A'; c <= 'Z'; ++c)
        assert(necro_is_alphabetical(c));
    for (uint32_t c = '0'; c <= '9'; ++c)
        assert(!necro_is_alphabetical(c));
    for (uint32_t c = 0; c < 128; ++c)
        assert(necro_is_alphabetical(c) == (isalpha(c) > 0));
    for (uint32_t c = 0x00C0; c <= 0x00D6; ++c) // Latin - 1 Supplement Uppercase
        assert(necro_is_alphabetical(c));
    for (uint32_t c = 0x00E0; c <= 0x00F6; ++c) // Latin - 1 Supplement Lowercase
        assert(necro_is_alphabetical(c));
    for (uint32_t c = 0x0100; c <= 0x0124; c+=2) // Latin Extended - A Uppercase
        assert(necro_is_alphabetical(c));
    for (uint32_t c = 0x0101; c <= 0x0125; c+=2) // Latin Extended - A Lowercase
        assert(necro_is_alphabetical(c));
    for (uint32_t c = 0x0250; c <= 0x02AF; c++) // IPA Extensions
        assert(necro_is_alphabetical(c));
    for (uint32_t c = 0x30A1; c <= 0x30FA; c++) // Katakana
        assert(necro_is_alphabetical(c));
    for (uint32_t c = 0x4E00; c <= 0x9FA5; c++) // CJK Unified Ideographs
        assert(necro_is_alphabetical(c));

    assert(!necro_is_alphabetical('!'));
    assert(!necro_is_alphabetical('@'));
    assert(!necro_is_alphabetical('#'));
    assert(!necro_is_alphabetical('$'));
    assert(!necro_is_alphabetical('%'));
    assert(!necro_is_alphabetical('^'));
    assert(!necro_is_alphabetical('&'));
    assert(!necro_is_alphabetical('*'));
    assert(!necro_is_alphabetical('('));
    assert(!necro_is_alphabetical(')'));
    assert(!necro_is_alphabetical('_'));
    assert(!necro_is_alphabetical('-'));
    assert(!necro_is_alphabetical('+'));
    assert(!necro_is_alphabetical('='));
    assert(!necro_is_alphabetical('\''));
    assert(!necro_is_alphabetical('\"'));

    assert(!necro_is_alphabetical(' '));
    assert(!necro_is_alphabetical('\t'));
    assert(!necro_is_alphabetical('\n'));

    assert(!necro_is_alphabetical(0));

    printf("Unicode Alphabetical test: Passed\n");

    //-------------------
    // Lowercase
    for (uint32_t c = 'a'; c <= 'z'; ++c)
        assert(necro_is_lowercase(c));
    for (uint32_t c = 'A'; c <= 'Z'; ++c)
        assert(!necro_is_lowercase(c));
    for (uint32_t c = '0'; c <= '9'; ++c)
        assert(!necro_is_lowercase(c));
    for (uint32_t c = 0; c < 128; ++c)
        assert(necro_is_lowercase(c) == (islower(c) > 0));
    for (uint32_t c = 0x00C0; c <= 0x00D6; ++c) // Latin - 1 Supplement Uppercase
        assert(!necro_is_lowercase(c));
    for (uint32_t c = 0x00E0; c <= 0x00F6; ++c) // Latin - 1 Supplement Lowercase
        assert(necro_is_lowercase(c));
    for (uint32_t c = 0x0100; c <= 0x0124; c+=2) // Latin Extended - A Uppercase
        assert(!necro_is_lowercase(c));
    for (uint32_t c = 0x0101; c <= 0x0125; c+=2) // Latin Extended - A Lowercase
        assert(necro_is_lowercase(c));
    for (uint32_t c = 0x4E00; c <= 0x9FA5; c++) // CJK Unified Ideographs
        assert(!necro_is_lowercase(c));

    assert(!necro_is_lowercase('!'));
    assert(!necro_is_lowercase('@'));
    assert(!necro_is_lowercase('#'));
    assert(!necro_is_lowercase('$'));
    assert(!necro_is_lowercase('%'));
    assert(!necro_is_lowercase('^'));
    assert(!necro_is_lowercase('&'));
    assert(!necro_is_lowercase('*'));
    assert(!necro_is_lowercase('('));
    assert(!necro_is_lowercase(')'));
    assert(!necro_is_lowercase('_'));
    assert(!necro_is_lowercase('-'));
    assert(!necro_is_lowercase('+'));
    assert(!necro_is_lowercase('='));
    assert(!necro_is_lowercase('\''));
    assert(!necro_is_lowercase('\"'));

    assert(!necro_is_lowercase(' '));
    assert(!necro_is_lowercase('\t'));
    assert(!necro_is_lowercase('\n'));

    assert(!necro_is_lowercase(0));

    printf("Unicode Lowercase test: Passed\n");

    //-------------------
    // Uppercase
    for (uint32_t c = 'a'; c <= 'z'; ++c)
        assert(!necro_is_uppercase(c));
    for (uint32_t c = 'A'; c <= 'Z'; ++c)
        assert(necro_is_uppercase(c));
    for (uint32_t c = '0'; c <= '9'; ++c)
        assert(!necro_is_uppercase(c));
    for (uint32_t c = 0; c < 128; ++c)
        assert(necro_is_uppercase(c) == (isupper(c) > 0));
    for (uint32_t c = 0x00C0; c <= 0x00D6; ++c) // Latin - 1 Supplement Uppercase
        assert(necro_is_uppercase(c));
    for (uint32_t c = 0x00E0; c <= 0x00F6; ++c) // Latin - 1 Supplement Lowercase
        assert(!necro_is_uppercase(c));
    for (uint32_t c = 0x0100; c <= 0x0124; c+=2) // Latin Extended - A Uppercase
        assert(necro_is_uppercase(c));
    for (uint32_t c = 0x0101; c <= 0x0125; c+=2) // Latin Extended - A Lowercase
        assert(!necro_is_uppercase(c));
    for (uint32_t c = 0x4E00; c <= 0x9FA5; c++) // CJK Unified Ideographs
        assert(!necro_is_uppercase(c));

    assert(!necro_is_uppercase('!'));
    assert(!necro_is_uppercase('@'));
    assert(!necro_is_uppercase('#'));
    assert(!necro_is_uppercase('$'));
    assert(!necro_is_uppercase('%'));
    assert(!necro_is_uppercase('^'));
    assert(!necro_is_uppercase('&'));
    assert(!necro_is_uppercase('*'));
    assert(!necro_is_uppercase('('));
    assert(!necro_is_uppercase(')'));
    assert(!necro_is_uppercase('_'));
    assert(!necro_is_uppercase('-'));
    assert(!necro_is_uppercase('+'));
    assert(!necro_is_uppercase('='));
    assert(!necro_is_uppercase('\''));
    assert(!necro_is_uppercase('\"'));

    assert(!necro_is_uppercase(' '));
    assert(!necro_is_uppercase('\t'));
    assert(!necro_is_uppercase('\n'));

    assert(!necro_is_uppercase(0));

    printf("Unicode Uppercase test: Passed\n");

    //-------------------
    // Whitespace
    for (uint32_t c = 'a'; c <= 'z'; ++c)
        assert(!necro_is_whitespace(c));
    for (uint32_t c = 'A'; c <= 'Z'; ++c)
        assert(!necro_is_whitespace(c));
    for (uint32_t c = '0'; c <= '9'; ++c)
        assert(!necro_is_whitespace(c));
    for (uint32_t c = 0; c < 128; ++c)
        assert(necro_is_whitespace(c) == (isspace(c) > 0));
    for (uint32_t c = 0x00C0; c <= 0x00D6; ++c) // Latin - 1 Supplement Uppercase
        assert(!necro_is_whitespace(c));
    for (uint32_t c = 0x00E0; c <= 0x00F6; ++c) // Latin - 1 Supplement Lowercase
        assert(!necro_is_whitespace(c));
    for (uint32_t c = 0x0100; c <= 0x0124; c+=2) // Latin Extended - A Uppercase
        assert(!necro_is_whitespace(c));
    for (uint32_t c = 0x0101; c <= 0x0125; c+=2) // Latin Extended - A Lowercase
        assert(!necro_is_whitespace(c));
    for (uint32_t c = 0x0250; c <= 0x02AF; c++) // IPA Extensions
        assert(!necro_is_whitespace(c));
    for (uint32_t c = 0x30A1; c <= 0x30FA; c++) // Katakana
        assert(!necro_is_whitespace(c));
    for (uint32_t c = 0x4E00; c <= 0x9FA5; c++) // CJK Unified Ideographs
        assert(!necro_is_whitespace(c));

    assert(!necro_is_whitespace('!'));
    assert(!necro_is_whitespace('@'));
    assert(!necro_is_whitespace('#'));
    assert(!necro_is_whitespace('$'));
    assert(!necro_is_whitespace('%'));
    assert(!necro_is_whitespace('^'));
    assert(!necro_is_whitespace('&'));
    assert(!necro_is_whitespace('*'));
    assert(!necro_is_whitespace('('));
    assert(!necro_is_whitespace(')'));
    assert(!necro_is_whitespace('_'));
    assert(!necro_is_whitespace('-'));
    assert(!necro_is_whitespace('+'));
    assert(!necro_is_whitespace('='));
    assert(!necro_is_whitespace('\''));
    assert(!necro_is_whitespace('\"'));

    assert(necro_is_whitespace(' '));
    assert(necro_is_whitespace('\t'));
    assert(necro_is_whitespace('\n'));

    assert(!necro_is_whitespace(0));

    printf("Unicode Whitespace test: Passed\n");

    //-------------------
    // Digit
    for (uint32_t c = 'a'; c <= 'z'; ++c)
        assert(!necro_is_ascii_digit(c));
    for (uint32_t c = 'A'; c <= 'Z'; ++c)
        assert(!necro_is_ascii_digit(c));
    for (uint32_t c = '0'; c <= '9'; ++c)
        assert(necro_is_ascii_digit(c));
    for (uint32_t c = 0; c < 128; ++c)
        assert(necro_is_ascii_digit(c) == (isdigit(c) > 0));
    for (uint32_t c = 0x00C0; c <= 0x00D6; ++c) // Latin - 1 Supplement Uppercase
        assert(!necro_is_ascii_digit(c));
    for (uint32_t c = 0x00E0; c <= 0x00F6; ++c) // Latin - 1 Supplement Lowercase
        assert(!necro_is_ascii_digit(c));
    for (uint32_t c = 0x0100; c <= 0x0124; c+=2) // Latin Extended - A Uppercase
        assert(!necro_is_ascii_digit(c));
    for (uint32_t c = 0x0101; c <= 0x0125; c+=2) // Latin Extended - A Lowercase
        assert(!necro_is_ascii_digit(c));
    for (uint32_t c = 0x0250; c <= 0x02AF; c++) // IPA Extensions
        assert(!necro_is_ascii_digit(c));
    for (uint32_t c = 0x30A1; c <= 0x30FA; c++) // Katakana
        assert(!necro_is_ascii_digit(c));
    for (uint32_t c = 0x4E00; c <= 0x9FA5; c++) // CJK Unified Ideographs
        assert(!necro_is_ascii_digit(c));

    assert(!necro_is_ascii_digit('!'));
    assert(!necro_is_ascii_digit('@'));
    assert(!necro_is_ascii_digit('#'));
    assert(!necro_is_ascii_digit('$'));
    assert(!necro_is_ascii_digit('%'));
    assert(!necro_is_ascii_digit('^'));
    assert(!necro_is_ascii_digit('&'));
    assert(!necro_is_ascii_digit('*'));
    assert(!necro_is_ascii_digit('('));
    assert(!necro_is_ascii_digit(')'));
    assert(!necro_is_ascii_digit('_'));
    assert(!necro_is_ascii_digit('-'));
    assert(!necro_is_ascii_digit('+'));
    assert(!necro_is_ascii_digit('='));
    assert(!necro_is_ascii_digit('\''));
    assert(!necro_is_ascii_digit('\"'));

    assert(!necro_is_ascii_digit(' '));
    assert(!necro_is_ascii_digit('\t'));
    assert(!necro_is_ascii_digit('\n'));

    assert(!necro_is_ascii_digit(0));

    printf("Unicode ASCII Digit test: Passed\n");

    //-------------------
    // Decimal
    for (uint32_t c = 'a'; c <= 'z'; ++c)
        assert(!necro_is_decimal(c));
    for (uint32_t c = 'A'; c <= 'Z'; ++c)
        assert(!necro_is_decimal(c));
    for (uint32_t c = '0'; c <= '9'; ++c)
        assert(necro_is_decimal(c));
    for (uint32_t c = 0x00C0; c <= 0x00D6; ++c) // Latin - 1 Supplement Uppercase
        assert(!necro_is_decimal(c));
    for (uint32_t c = 0x00E0; c <= 0x00F6; ++c) // Latin - 1 Supplement Lowercase
        assert(!necro_is_decimal(c));
    for (uint32_t c = 0x0100; c <= 0x0124; c+=2) // Latin Extended - A Uppercase
        assert(!necro_is_decimal(c));
    for (uint32_t c = 0x0101; c <= 0x0125; c+=2) // Latin Extended - A Lowercase
        assert(!necro_is_decimal(c));
    for (uint32_t c = 0x0250; c <= 0x02AF; c++) // IPA Extensions
        assert(!necro_is_decimal(c));
    for (uint32_t c = 0x30A1; c <= 0x30FA; c++) // Katakana
        assert(!necro_is_decimal(c));
    for (uint32_t c = 0x4E00; c <= 0x9FA5; c++) // CJK Unified Ideographs
        assert(!necro_is_decimal(c));

    assert(!necro_is_decimal('!'));
    assert(!necro_is_decimal('@'));
    assert(!necro_is_decimal('#'));
    assert(!necro_is_decimal('$'));
    assert(!necro_is_decimal('%'));
    assert(!necro_is_decimal('^'));
    assert(!necro_is_decimal('&'));
    assert(!necro_is_decimal('*'));
    assert(!necro_is_decimal('('));
    assert(!necro_is_decimal(')'));
    assert(!necro_is_decimal('_'));
    assert(!necro_is_decimal('-'));
    assert(!necro_is_decimal('+'));
    assert(!necro_is_decimal('='));
    assert(!necro_is_decimal('\''));
    assert(!necro_is_decimal('\"'));

    assert(!necro_is_decimal(' '));
    assert(!necro_is_decimal('\t'));
    assert(!necro_is_decimal('\n'));

    assert(!necro_is_decimal(0));

    printf("Unicode Decimal test: Passed\n");

    //-------------------
    // ID Start
    for (uint32_t c = 'a'; c <= 'z'; ++c)
        assert(necro_is_id_start(c));
    for (uint32_t c = 'A'; c <= 'Z'; ++c)
        assert(necro_is_id_start(c));
    for (uint32_t c = '0'; c <= '9'; ++c)
        assert(!necro_is_id_start(c));
    for (uint32_t c = 0x00C0; c <= 0x00D6; ++c) // Latin - 1 Supplement Uppercase
        assert(necro_is_id_start(c));
    for (uint32_t c = 0x00E0; c <= 0x00F6; ++c) // Latin - 1 Supplement Lowercase
        assert(necro_is_id_start(c));
    for (uint32_t c = 0x0100; c <= 0x0124; c+=2) // Latin Extended - A Uppercase
        assert(necro_is_id_start(c));
    for (uint32_t c = 0x0101; c <= 0x0125; c+=2) // Latin Extended - A Lowercase
        assert(necro_is_id_start(c));
    for (uint32_t c = 0x0250; c <= 0x02AF; c++) // IPA Extensions
        assert(necro_is_id_start(c));
    for (uint32_t c = 0x30A1; c <= 0x30FA; c++) // Katakana
        assert(necro_is_id_start(c));
    for (uint32_t c = 0x4E00; c <= 0x9FA5; c++) // CJK Unified Ideographs
        assert(necro_is_id_start(c));

    assert(!necro_is_id_start('!'));
    assert(!necro_is_id_start('@'));
    assert(!necro_is_id_start('#'));
    assert(!necro_is_id_start('$'));
    assert(!necro_is_id_start('%'));
    assert(!necro_is_id_start('^'));
    assert(!necro_is_id_start('&'));
    assert(!necro_is_id_start('*'));
    assert(!necro_is_id_start('('));
    assert(!necro_is_id_start(')'));
    assert(!necro_is_id_start('_'));
    assert(!necro_is_id_start('-'));
    assert(!necro_is_id_start('+'));
    assert(!necro_is_id_start('='));
    assert(!necro_is_id_start('\''));
    assert(!necro_is_id_start('\"'));

    assert(!necro_is_id_start(' '));
    assert(!necro_is_id_start('\t'));
    assert(!necro_is_id_start('\n'));

    assert(!necro_is_id_start(0));

    printf("Unicode ID Start test: Passed\n");

    //-------------------
    // ID Continue
    for (uint32_t c = 'a'; c <= 'z'; ++c)
        assert(necro_is_id_continue(c));
    for (uint32_t c = 'A'; c <= 'Z'; ++c)
        assert(necro_is_id_continue(c));
    for (uint32_t c = '0'; c <= '9'; ++c)
        assert(necro_is_id_continue(c));
    for (uint32_t c = 0x00C0; c <= 0x00D6; ++c) // Latin - 1 Supplement Uppercase
        assert(necro_is_id_continue(c));
    for (uint32_t c = 0x00E0; c <= 0x00F6; ++c) // Latin - 1 Supplement Lowercase
        assert(necro_is_id_continue(c));
    for (uint32_t c = 0x0100; c <= 0x0124; c+=2) // Latin Extended - A Uppercase
        assert(necro_is_id_continue(c));
    for (uint32_t c = 0x0101; c <= 0x0125; c+=2) // Latin Extended - A Lowercase
        assert(necro_is_id_continue(c));
    for (uint32_t c = 0x0250; c <= 0x02AF; c++) // IPA Extensions
        assert(necro_is_id_continue(c));
    for (uint32_t c = 0x30A1; c <= 0x30FA; c++) // Katakana
        assert(necro_is_id_continue(c));
    for (uint32_t c = 0x4E00; c <= 0x9FA5; c++) // CJK Unified Ideographs
        assert(necro_is_id_continue(c));

    assert(!necro_is_id_continue('!'));
    assert(!necro_is_id_continue('@'));
    assert(!necro_is_id_continue('#'));
    assert(!necro_is_id_continue('$'));
    assert(!necro_is_id_continue('%'));
    assert(!necro_is_id_continue('^'));
    assert(!necro_is_id_continue('&'));
    assert(!necro_is_id_continue('*'));
    assert(!necro_is_id_continue('('));
    assert(!necro_is_id_continue(')'));
    assert(necro_is_id_continue('_'));
    assert(!necro_is_id_continue('-'));
    assert(!necro_is_id_continue('+'));
    assert(!necro_is_id_continue('='));
    assert(!necro_is_id_continue('\''));
    assert(!necro_is_id_continue('\"'));

    assert(!necro_is_id_continue(' '));
    assert(!necro_is_id_continue('\t'));
    assert(!necro_is_id_continue('\n'));

    assert(!necro_is_id_continue(0));

    printf("Unicode ID Continue test: Passed\n");

    //-------------------
    // Grapheme Base
    for (uint32_t c = 'a'; c <= 'z'; ++c)
        assert(necro_is_grapheme_base(c));
    for (uint32_t c = 'A'; c <= 'Z'; ++c)
        assert(necro_is_grapheme_base(c));
    for (uint32_t c = '0'; c <= '9'; ++c)
        assert(necro_is_grapheme_base(c));
    for (uint32_t c = 0x00C0; c <= 0x00D6; ++c) // Latin - 1 Supplement Uppercase
        assert(necro_is_grapheme_base(c));
    for (uint32_t c = 0x00E0; c <= 0x00F6; ++c) // Latin - 1 Supplement Lowercase
        assert(necro_is_grapheme_base(c));
    for (uint32_t c = 0x0100; c <= 0x0124; c+=2) // Latin Extended - A Uppercase
        assert(necro_is_grapheme_base(c));
    for (uint32_t c = 0x0101; c <= 0x0125; c+=2) // Latin Extended - A Lowercase
        assert(necro_is_grapheme_base(c));
    for (uint32_t c = 0x0250; c <= 0x02AF; c++) // IPA Extensions
        assert(necro_is_grapheme_base(c));
    for (uint32_t c = 0x30A1; c <= 0x30FA; c++) // Katakana
        assert(necro_is_grapheme_base(c));
    for (uint32_t c = 0x4E00; c <= 0x9FA5; c++) // CJK Unified Ideographs
        assert(necro_is_grapheme_base(c));

    assert(necro_is_grapheme_base('!'));
    assert(necro_is_grapheme_base('@'));
    assert(necro_is_grapheme_base('#'));
    assert(necro_is_grapheme_base('$'));
    assert(necro_is_grapheme_base('%'));
    assert(necro_is_grapheme_base('^'));
    assert(necro_is_grapheme_base('&'));
    assert(necro_is_grapheme_base('*'));
    assert(necro_is_grapheme_base('('));
    assert(necro_is_grapheme_base(')'));
    assert(necro_is_grapheme_base('_'));
    assert(necro_is_grapheme_base('-'));
    assert(necro_is_grapheme_base('+'));
    assert(necro_is_grapheme_base('='));
    assert(necro_is_grapheme_base('\''));
    assert(necro_is_grapheme_base('\"'));

    assert(necro_is_grapheme_base(' '));
    assert(!necro_is_grapheme_base('\t'));
    assert(!necro_is_grapheme_base('\n'));

    assert(!necro_is_grapheme_base(0));

    // TODO: Compare to raw map!!!!
    printf("Unicode Grapheme Base test: Passed\n");
}
