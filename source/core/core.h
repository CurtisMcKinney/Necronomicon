/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>

#include "ast.h"
#include "parser.h"
#include "infer.h"


typedef struct 
{
    struct NecroCoreAST_Expression* exprA;
    struct NecroCoreAST_Expression* exprB;
} NecroCoreAST_Application;

typedef struct
{
    NecroVar var;
    struct NecroCoreAST_Expression* expr;
} NecroCoreAST_Lambda;

typedef struct
{
    NecroVar bind;
    struct NecroCoreAST_Expression* expr;
} NecroCoreAST_Let;

typedef struct
{
    NecroVar id;
} NecroCoreAST_DataCon;

typedef struct
{
    union
    {
        NecroCoreAST_DataCon* dataCon;
        NecroAST_Constant_Reified* lit;
        struct NecroCoreAST_CaseAltCon* _defaultPadding;
    };

    enum
    {
        NECRO_CORE_CASE_ALT_DATA,
        NECRO_CORE_CASE_ALT_LITERAL,
        NECRO_CORE_CASE_ALT_DEFAULT,
    } type;
} NecroCoreAST_CaseAltCon;

typedef struct
{
    NecroCoreAST_CaseAltCon altCon;
    struct NecroCoreAST_Expression* expr;
    struct NecroCoreAST_CaseAlt* next;
} NecroCoreAST_CaseAlt;

typedef struct 
{
    struct NecroCoreAST_Expression* expr;
    NecroVar var;
    NecroCoreAST_CaseAlt alts;
} NecroCoreAST_Case;

typedef struct
{
    union
    {
        NecroVar var;
        NecroAST_Constant_Reified lit;
        NecroCoreAST_Application app;
        NecroCoreAST_Lambda lambda;
        NecroCoreAST_Let let;
        NecroCoreAST_Case caseExpr;
    };

    enum
    {
        NECRO_CORE_EXPR_VAR,
        NECRO_CORE_EXPR_LIT,
        NECRO_CORE_EXPR_APP,
        NECRO_CORE_EXPR_LAM,
        NECRO_CORE_EXPR_LET,
        NECRO_CORE_EXPR_CASE
    } type;
} NecroCoreAST_Expression;