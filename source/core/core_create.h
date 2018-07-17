/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_CREATE_H
#define CORE_CREATE_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core.h"

///////////////////////////////////////////////////////
// NecroCore create (Canonical constructors, probably should be in core.h ....)
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_create_core_bind(NecroPagedArena* arena, NecroCoreAST_Expression* expr, NecroVar var);
NecroCoreAST_Expression* necro_create_core_let(NecroPagedArena* arena, NecroCoreAST_Expression* bind, NecroCoreAST_Expression* in);
NecroCoreAST_Expression* necro_create_core_lam(NecroPagedArena* arena, NecroCoreAST_Expression* arg, NecroCoreAST_Expression* expr);
NecroCoreAST_Expression* necro_create_core_app(NecroPagedArena* arena, NecroCoreAST_Expression* expr1, NecroCoreAST_Expression* expr2);
NecroCoreAST_Expression* necro_create_core_var(NecroPagedArena* arena, NecroVar var);
NecroCoreAST_Expression* necro_create_core_lit(NecroPagedArena* arena, NecroAST_Constant_Reified lit);
NecroCoreAST_Expression* necro_create_core_list(NecroPagedArena* arena, NecroCoreAST_Expression* item, NecroCoreAST_Expression* next);
NecroCoreAST_Expression* necro_create_core_data_con(NecroPagedArena* arena, NecroVar conid, NecroCoreAST_Expression* arg_list, NecroCoreAST_DataCon* next);
NecroCoreAST_Expression* necro_create_core_data_decl(NecroPagedArena* arena, NecroVar data_id, NecroCoreAST_DataCon* con_list);
NecroCoreAST_Expression* necro_create_core_case(NecroPagedArena* arena, NecroCoreAST_Expression* expr, NecroCoreAST_CaseAlt* alts);
NecroCoreAST_CaseAlt*    necro_create_core_case_alt(NecroPagedArena* arena, NecroCoreAST_Expression* expr, NecroCoreAST_Expression* alt_con, NecroCoreAST_CaseAlt* next);
NecroCoreAST_Expression* necro_create_core_necro_type(NecroPagedArena* arena, NecroType* necro_type);

#endif // CORE_CREATE_H
