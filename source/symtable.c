/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "symtable.h"

// Constants
#define NECRO_SYMTABLE_INITIAL_SIZE    512
#define NECRO_SYMTABLE_NULL_ID         0

NecroSymTable necro_create_symtable(NecroIntern* intern)
{
    NecroSymbolInfo* data = calloc(NECRO_SYMTABLE_INITIAL_SIZE, sizeof(NecroSymbolInfo));
    if (data == NULL)
    {
        fprintf(stderr, "Malloc returned null while allocating data in necro_create_symtable()\n");
        exit(1);
    }
    return (NecroSymTable)
    {
        data,
        NECRO_SYMTABLE_INITIAL_SIZE,
        0,
        intern,
    };
}

void necro_destroy_sym_table(NecroSymTable* table)
{
    if (table == NULL || table->data == NULL)
        return;
    free(table->data);
    table->data  = NULL;
    table->size  = 0;
    table->count = 0;
}

inline void necro_symtable_grow(NecroSymTable* table)
{
    table->size *= 2;
    NecroSymbolInfo* new_data = realloc(table->data, table->size * sizeof(NecroSymbolInfo));
    if (new_data == NULL)
    {
        if (table->data != NULL)
            free(table->data);
        fprintf(stderr, "Malloc returned NULL in necro_symtable_grow!\n");
        exit(1);
    }
    table->data = new_data;
    assert(table->data != NULL);
}

NecroID necro_symbtable_insert(NecroSymTable* table, NecroSymbolInfo info)
{
    if (table->count >= table->size)
        necro_symtable_grow(table);
    assert(table->count < table->size);
    table->data[table->count] = info;
    table->count++;
    return (NecroID) { table->count - 1 };
}

bool necro_symbtable_find(NecroSymTable* table, NecroID id, NecroSymbolInfo* out_info)
{
    if (id.id < table->count)
    {
        *out_info = table->data[id.id];
        return true;
    }
    else
    {
        return false;
    }
}

void necro_symtable_info_print(NecroSymbolInfo info, NecroIntern* intern, size_t whitespace)
{
    print_white_space(whitespace);
    printf("NecroSymbolInfo\n");

    print_white_space(whitespace);
    printf("{\n");

    print_white_space(whitespace + 4);
    printf("name:        %s",necro_intern_get_string(intern, info.reader_name));

    print_white_space(whitespace + 4);
    printf("unique_name: %s", necro_intern_get_string(intern, info.unique_name));

    print_white_space(whitespace + 4);
    printf("id:          %d", info.id.id);

    print_white_space(whitespace);
    printf("}\n");
}

void necro_symtable_print(NecroSymTable* table)
{
    printf("NecroSymTable\n{\n");
    printf("    size:  %d\n", table->size);
    printf("    count: %d\n", table->count);
    printf("    data:\n");
    printf("    [\n");
    for (size_t i = 0; i < table->count; ++i)
    {
        necro_symtable_info_print(table->data[i], table->intern, 8);
        // printf(", ");
    }
    printf("    ]\n");
    printf("}\n");
}

void necro_symtable_test()
{
    puts("---------------------------");
    puts("-- NecroSymTable");
    puts("---------------------------\n");

    NecroIntern   intern      = necro_create_intern();
    NecroSymTable symtable    = necro_create_symtable(&intern);
    necro_symtable_print(&symtable);
    NecroSymbol   test_symbol = necro_intern_string(&intern, "test");
}