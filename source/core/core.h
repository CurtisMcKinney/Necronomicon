/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>

#include "ast.h"
#include "parser.h"
#include "infer.h"

#if 0
//////////////////////
// Types
//////////////////////

typedef struct
{
    struct NecroCoreAST_Type* typeA;
    struct NecroCoreAST_Type* typeB;
} NecroCoreAST_AppType;

typedef struct  
{
    NecroVar id;
} NecroCoreAST_TypeCon;

typedef struct
{
    NecroCoreAST_TypeCon* typeCon;
    struct NecroCoreAST_TypeList* types;
} NecroCoreAST_TyConApp;

typedef struct  
{
    union
    {
        NecroVar tyvarTy;
        NecroAST_ConstantType liftTy;
        NecroCoreAST_AppType* appTy;
        NecroCoreAST_TyConApp* tyConApp;
    };

    enum
    {
        NECRO_CORE_AST_TYPE_VAR,
        NECRO_CORE_AST_TYPE_LIT,
        NECRO_CORE_AST_TYPE_APP,
        NECRO_CORE_AST_TYPE_TYCON_APP,
    } type;
} NecroCoreAST_Type;

typedef struct
{
    NecroCoreAST_Type* type;
    NecroCoreAST_Type* next;
} NecroCoreAST_TypeList;
#endif

//////////////////////
// Expressions
//////////////////////

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
    union
    {
        struct NecroCoreAST_DataCon* dataCon;
        struct NecroCoreAST_Expression* expr;
    };

    enum
    {
        NECRO_CORE_DATAEXPR_DATACON,
        NECRO_CORE_DATAEXPR_EXPR
    } dataExpr_type;

    struct NecroCoreAST_DataExpr* next;
} NecroCoreAST_DataExpr;

typedef struct
{
    NecroVar condid;
    NecroCoreAST_DataExpr arg_list;
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
    } altCon_type;
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
    struct NecroCoreAST_Type* type;
    NecroCoreAST_CaseAlt alts;
} NecroCoreAST_Case;

typedef struct
{
    NecroType type;
} NecroCoreAST_Type;

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
        NecroCoreAST_Type type;
    };

    enum
    {
        NECRO_CORE_EXPR_VAR,
        NECRO_CORE_EXPR_LIT,
        NECRO_CORE_EXPR_APP,
        NECRO_CORE_EXPR_LAM,
        NECRO_CORE_EXPR_LET,
        NECRO_CORE_EXPR_CASE,
        NECRO_CORE_EXPR_TYPE
    } expr_type;
} NecroCoreAST_Expression;