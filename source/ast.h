/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_AST_H
#define NECRO_AST_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "lexer.h"
#include "intern.h"
#include "arena.h"
#include "parse/parser.h"

struct NecroAst;
struct NecroDeclarationGroup;
struct NecroDeclarationGroupList;
struct NecroDeclarationsInfo;
struct NecroTypeClassContext;

//=====================================================
// AST FunctionType
//=====================================================
typedef struct
{
    struct NecroAst* type;
    struct NecroAst* next_on_arrow;
} NecroAstFunctionType;

//=====================================================
// AST TypeClassInstance
//=====================================================
typedef struct
{
    struct NecroAst*              context;// optional, null_local_ptr if not present
    struct NecroAst*              qtycls;
    struct NecroAst*              inst;
    struct NecroAst*              declarations; // Points to the next in the list, null_local_ptr if the end
    struct NecroAst*              dictionary_instance; // Dictionary instance which is generated at compile time by the compiler
    struct NecroDeclarationGroup* declaration_group;
    NecroSymbol                   instance_name;
    NecroID                       instance_id;
} NecroAstTypeClassInstance;

//=====================================================
// AST TypeClassDeclaration
//=====================================================
typedef struct
{
    struct NecroAst*              context; // optional, null_local_ptr if not present
    struct NecroAst*              tycls;
    struct NecroAst*              tyvar;
    struct NecroAst*              declarations; // Points to the next in the list, null_local_ptr if the end
    struct NecroAst*              dictionary_data_declaration; // Dictionary data declaration which is generated at compile time by the compiler
    struct NecroDeclarationGroup* declaration_group;
} NecroAstTypeClassDeclaration;

//=====================================================
// AST TypeClassContext
//=====================================================
typedef struct
{
    struct NecroAst* conid;
    struct NecroAst* varid;
} NecroAstTypeClassContext;

//=====================================================
// AST TypeSignature
//=====================================================
typedef struct
{
    struct NecroAst*              var;
    struct NecroAst*              context; // optional, null_local_ptr if not present
    struct NecroAst*              type;
    NECRO_SIG_TYPE                sig_type;
    struct NecroDeclarationGroup* declaration_group;
} NecroAstTypeSignature;

//=====================================================
// AST DataDeclaration
//=====================================================
typedef struct
{
    struct NecroAst*              simpletype;
    struct NecroAst*              constructor_list; // Points to the next in the list, null_local_ptr if the end
    struct NecroDeclarationGroup* declaration_group;
    bool                          is_recursive;
} NecroAstDataDeclaration;

//=====================================================
// AST Constructor
//=====================================================
typedef struct
{
    struct NecroAst* conid;
    struct NecroAst* arg_list; // Points to the next in the list, null_local_ptr if the end
} NecroAstConstructor;

//=====================================================
// AST SimpleType
//=====================================================
typedef struct
{
    struct NecroAst* type_con;
    struct NecroAst* type_var_list; // Points to the next in the list, null_local_ptr if the end
} NecroAstSimpleType;

//=====================================================
// AST BinOpSym
//=====================================================
typedef struct
{
    struct NecroAst* left;
    struct NecroAst* op;
    struct NecroAst* right;
} NecroAstBinOpSym;

//=====================================================
// AST Operator Left Section
//=====================================================
typedef struct
{
    struct NecroAst*              left;
    NECRO_BIN_OP_TYPE             type;
    NecroSymbol                   symbol;
    NecroID                       id;
    struct NecroTypeClassContext* inst_context;
    struct NecroType*             op_necro_type;
} NecroAstOpLeftSection;

//=====================================================
// AST Operator Right Section
//=====================================================
typedef struct
{
    struct NecroAst*              right;
    NECRO_BIN_OP_TYPE             type;
    NecroSymbol                   symbol;
    NecroID                       id;
    struct NecroTypeClassContext* inst_context;
    struct NecroType*             op_necro_type;
} NecroAstOpRightSection;

//=====================================================
// AST Type App
//=====================================================
typedef struct
{
    struct NecroAst* ty;
    struct NecroAst* next_ty; // Points to the next in the list, null_local_ptr if the end
} NecroAstTypeApp;

//=====================================================
// AST ConID
//=====================================================
typedef struct
{
    NecroSymbol    symbol;
    NecroID        id;
    NECRO_CON_TYPE con_type;
} NecroAstConId;

//=====================================================
// AST Case
//=====================================================
typedef struct
{
    struct NecroAst* expression;
    struct NecroAst* alternatives;
} NecroAstCase;

//=====================================================
// AST CaseAlternative
//=====================================================
typedef struct
{
    struct NecroAst* pat;
    struct NecroAst* body;
} NecroAstCaseAlternative;

//=====================================================
// AST Module
//=====================================================


//=====================================================
// AST Undefined
//=====================================================
typedef struct
{
    uint8_t _pad;
} NecroAstUndefined;

//=====================================================
// AST Constant
//=====================================================
typedef struct
{
    union
    {
        double      double_literal;
        int64_t     int_literal;
        NecroSymbol symbol;
		bool        boolean_literal;
        uint32_t    char_literal;
    };
    NECRO_CONSTANT_TYPE type;
    struct NecroAst*    pat_from_ast;
    struct NecroAst*    pat_eq_ast;
} NecroAstConstant;

//=====================================================
// AST Unary Operation
//=====================================================

// typedef enum
// {
//     NECRO_UN_OP_NEG = 0,
//     NECRO_UN_OP_COUNT,
//     NECRO_UN_OP_UNDEFINED = NECRO_UN_OP_COUNT
// } NecroAST_UnaryOpType;
//
// typedef struct
// {
//     struct NecroAST_Node_Reified* rhs;
//     NecroAST_UnaryOpType type;
// } NecroAST_UnaryOp;

//=====================================================
// AST Binary Operation
//=====================================================
typedef struct
{
    struct NecroAst*              lhs;
    struct NecroAst*              rhs;
    NECRO_BIN_OP_TYPE             type;
    NecroSymbol                   symbol;
    NecroID                       id;
    struct NecroTypeClassContext* inst_context;
} NecroAstBinOp;

//=====================================================
// AST if then else
//=====================================================
typedef struct
{
    struct NecroAst* if_expr;
    struct NecroAst* then_expr;
    struct NecroAst* else_expr;
} NecroAstIfThenElse;

//=====================================================
// AST Right Hand Side
//=====================================================
typedef struct
{
    struct NecroAst* expression;
    struct NecroAst* declarations;
} NecroAstRightHandSide;

//=====================================================
// AST Let Expression
//=====================================================
typedef struct
{
    struct NecroAst* expression;
    struct NecroAst* declarations;
} NecroAstLetExpression;

//=====================================================
// AST Simple Assignment
//=====================================================
typedef struct
{
    NecroSymbol                   variable_name;
    struct NecroAst*              initializer;
    struct NecroAst*              rhs;
    NecroID                       id;
    struct NecroDeclarationGroup* declaration_group;
    bool                          is_recursive;
} NecroAstSimpleAssignment;

//=====================================================
// AST Bind Assignment
//=====================================================
typedef struct
{
    NecroSymbol      variable_name;
    struct NecroAst* expression;
    NecroID          id;
} NecroAstBindAssignment;

//=====================================================
// AST Bind Assignment
//=====================================================
typedef struct
{
    struct NecroAst* pat;
    struct NecroAst* expression;
} NecroAstPatBindAssignment;

//=====================================================
// AST apats
//=====================================================
typedef struct
{
    struct NecroAst* apat;
    struct NecroAst* next_apat;
} NecroAstApats;

//=====================================================
// AST Apats Assignment
//=====================================================
typedef struct
{
    NecroSymbol                   variable_name;
    struct NecroAst*              apats;
    struct NecroAst*              rhs;
    NecroID                       id;
    struct NecroDeclarationGroup* declaration_group;
} NecroAstApatsAssignment;

//=====================================================
// AST Pat Assignment
//=====================================================
typedef struct
{
    struct NecroAst*              pat;
    struct NecroAst*              rhs;
    struct NecroDeclarationGroup* declaration_group;
} NecroAstPatAssignment;

//=====================================================
// AST Lambda
//=====================================================
typedef struct
{
    struct NecroAst* apats;
    struct NecroAst* expression;
} NecroAstLambda;

//=====================================================
// AST List Node
//=====================================================
typedef struct
{
    struct NecroAst* item;
    struct NecroAst* next_item;
} NecroAstList;

//=====================================================
// AST Expression List
//=====================================================
typedef struct
{
    struct NecroAst* expressions; // NecroAST_ListNode of expressions
} NecroAstExpressionList;

//=====================================================
// AST Expression Array
//=====================================================
typedef struct
{
    struct NecroAst* expressions; // NecroAST_ListNode of expressions
} NecroAstExpressionArray;

//=====================================================
// AST Tuple
//=====================================================
typedef struct
{
    struct NecroAst* expressions; // NecroAST_ListNode of expressions
} NecroAstTuple;

//=====================================================
// AST Do
//=====================================================
typedef struct
{
    struct NecroAst*  statement_list; // NecroAST_ListNode of do statement items
    struct NecroType* monad_var;
} NecroAstDo;

//=====================================================
// AST Pat
//=====================================================
typedef struct
{
    struct NecroAst* expressions; // NecroAST_ListNode of expressions
} NecroAstPatExpression;

//=====================================================
// AST Variable
//=====================================================
typedef struct
{
    NecroSymbol                   symbol;
    NecroID                       id;
    NECRO_VAR_TYPE                var_type;
    struct NecroTypeClassContext* inst_context;
    struct NecroAst*              initializer;
    bool                          is_recursive;
} NecroAstVariable;

//=====================================================
// AST Function Expression
//=====================================================
typedef struct
{
    struct NecroAst* aexp;
    struct NecroAst* next_fexpression; // Points to the next in the list, null_local_ptr if the end
} NecroAstFunctionExpression;

//=====================================================
// AST Declarations
//=====================================================
typedef struct
{
    struct NecroAst*     declaration_impl;
    struct NecroAst*     next_declaration; // Points to the next in the list, null_local_ptr if the end
    struct NecroDeclarationGroupList* group_list;
} NecroAstDeclaration;

//=====================================================
// AST Top Declarations
//=====================================================
typedef struct
{
    struct NecroAst*     declaration;
    struct NecroAst*     next_top_decl; // Points to the next in the list, null_local_ptr if the end
    struct NecroDeclarationGroupList* group_list;
} NecroAstTopDeclaration;

//=====================================================
// AST Module
//=====================================================
// To Do: Define these!
// typedef struct
// {
//     struct NecroAST_Node_Reified* body;
// } NecroAST_SimpleModule;
//
// typedef struct
// {
//     struct NecroAST_Node_Reified* body;
// } NecroAST_ModuleWithExports;
//
// typedef union
// {
//     NecroAST_Body body;
//     NecroAST_SimpleModule simple_module;
//     NecroAST_ModuleWithExports module_with_exports;
// } NecroAST_Module;

//=====================================================
// AST Arithmetic Sequence
//=====================================================
typedef struct
{
    struct NecroAst* from;
    struct NecroAst* then;
    struct NecroAst* to;
    NECRO_ARITHMETIC_SEQUENCE_TYPE type;
} NecroAstArithmeticSequence;

//=====================================================
// AST Node
//=====================================================
struct NecroScope;
typedef struct NecroAst
{
    union
    {
        // NecroAST_UnaryOp unary_op; // Do we need this?
        NecroAstUndefined            undefined;
        NecroAstConstant             constant;
        NecroAstBinOp                bin_op;
        NecroAstIfThenElse           if_then_else;
        NecroAstTopDeclaration       top_declaration;
        NecroAstDeclaration          declaration;
        NecroAstSimpleAssignment     simple_assignment;
        NecroAstApats                apats;
        NecroAstApatsAssignment      apats_assignment;
        NecroAstPatAssignment        pat_assignment;
        NecroAstRightHandSide        right_hand_side;
        NecroAstLetExpression        let_expression;
        NecroAstFunctionExpression   fexpression;
        NecroAstVariable             variable;
        NecroAstLambda               lambda;
        NecroAstDo                   do_statement;
        NecroAstList                 list;
        NecroAstExpressionList       expression_list;
        NecroAstExpressionArray      expression_array;
        NecroAstTuple                tuple;
        NecroAstBindAssignment       bind_assignment;
        NecroAstPatBindAssignment    pat_bind_assignment;
        NecroAstArithmeticSequence   arithmetic_sequence;
        NecroAstCase                 case_expression;
        NecroAstCaseAlternative      case_alternative;
        NecroAstConId                conid;
        NecroAstBinOpSym             bin_op_sym;
        NecroAstOpLeftSection        op_left_section;
        NecroAstOpRightSection       op_right_section;
        NecroAstTypeApp              type_app;
        NecroAstSimpleType           simple_type;
        NecroAstConstructor          constructor;
        NecroAstDataDeclaration      data_declaration;
        NecroAstTypeClassContext     type_class_context;
        NecroAstTypeClassDeclaration type_class_declaration;
        NecroAstTypeClassInstance    type_class_instance;
        NecroAstTypeSignature        type_signature;
        NecroAstFunctionType         function_type;
        NecroAstPatExpression        pattern_expression;
    };
    NECRO_AST_TYPE     type;
    NecroSourceLoc     source_loc;
    struct NecroScope* scope;
    struct NecroType*  necro_type;
} NecroAst;

typedef struct
{
    NecroPagedArena arena;
    NecroAst*       root;
} NecroAstArena;

//NecroAstRef ?

NecroAstArena necro_ast_arena_empty();
NecroAstArena necro_ast_arena_create();
NecroAstArena necro_reify_ast(NecroParseAstArena* a_ast, NecroParseAstLocalPtr a_root, NecroIntern* intern);
void          necro_ast_arena_destroy(NecroAstArena* ast);
void          necro_print_reified_ast(NecroAstArena* ast, NecroIntern* intern);
void          necro_print_reified_ast_node(NecroAst* ast_node, NecroIntern* intern);

// Manual AST Creation
NecroAst* necro_create_conid_ast(NecroPagedArena* arena, NecroIntern* intern, const char* con_name, NECRO_CON_TYPE con_type);
NecroAst* necro_create_variable_ast(NecroPagedArena* arena, NecroIntern* intern, const char* variable_name, NECRO_VAR_TYPE var_type);
NecroAst* necro_create_ast_list(NecroPagedArena* arena, NecroAst* item, NecroAst* next);
NecroAst* necro_create_var_list_ast(NecroPagedArena* arena, NecroIntern* intern, size_t num_vars, NECRO_VAR_TYPE var_type);
NecroAst* necro_create_data_constructor_ast(NecroPagedArena* arena, NecroIntern* intern, const char* con_name, NecroAst* arg_list);
NecroAst* necro_create_simple_type_ast(NecroPagedArena* arena, NecroIntern* intern, const char* simple_type_name, NecroAst* ty_var_list);
NecroAst* necro_create_data_declaration_ast(NecroPagedArena* arena, NecroIntern* intern, NecroAst* simple_type, NecroAst* constructor_list);
NecroAst* necro_create_type_app_ast(NecroPagedArena* arena, NecroAst* type1, NecroAst* type2);
NecroAst* necro_create_fun_ast(NecroPagedArena* arena, NecroAst* type1, NecroAst* type2);
NecroAst* necro_create_fexpr_ast(NecroPagedArena* arena, NecroAst* f_ast, NecroAst* x_ast);
NecroAst* necro_create_fun_type_sig_ast(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroAst* context_ast, NecroAst* type_ast, NECRO_VAR_TYPE var_type, NECRO_SIG_TYPE sig_type);
NecroAst* necro_create_type_class_ast(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, const char* class_var, NecroAst* context_ast, NecroAst* declarations_ast);
NecroAst* necro_create_instance_ast(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, NecroAst* inst_ast, NecroAst* context_ast, NecroAst* declarations_ast);
NecroAst* necro_create_top_level_declaration_list(NecroPagedArena* arena, NecroAst* top_level_declaration, NecroAst* next);
NecroAst* necro_create_declaration_list(NecroPagedArena* arena, NecroAst* declaration, NecroAst* next);
NecroAst* necro_create_simple_assignment(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroAst* rhs_ast);
NecroAst* necro_create_apat_list_ast(NecroPagedArena* arena, NecroAst* apat_item, NecroAst* next_apat);
NecroAst* necro_create_apats_assignment_ast(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroAst* apats, NecroAst* rhs_ast);
NecroAst* necro_create_lambda_ast(NecroPagedArena* arena, NecroAst* apats, NecroAst* expr_ast);
NecroAst* necro_create_wild_card_ast(NecroPagedArena* arena);
NecroAst* necro_create_context(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, const char* var_name, NecroAst* next);
NecroAst* necro_create_rhs_ast(NecroPagedArena* arena, NecroAst* expression, NecroAst* declarations);
NecroAst* necro_create_bin_op_ast(NecroPagedArena* arena, NecroIntern* intern, const char* op_name, NecroAst* lhs, NecroAst* rhs);

// Dependency analysis
typedef struct NecroDependencyList
{
    struct NecroDeclarationGroup* dependency_group;
    struct NecroDependencyList*   next;
} NecroDependencyList;

typedef struct NecroDeclarationGroup
{
    NecroAst*                 declaration_ast;
    struct NecroDeclarationGroup* next;
    NecroDependencyList*          dependency_list;
    struct NecroDeclarationsInfo* info;
    bool                          type_checked;
    int32_t                       index;
    int32_t                       low_link;
    bool                          on_stack;
} NecroDeclarationGroup;

typedef struct NecroDeclarationGroupList
{
    NecroDeclarationGroup*            declaration_group;
    struct NecroDeclarationGroupList* next;
} NecroDeclarationGroupList;

NECRO_DECLARE_VECTOR(NecroDeclarationGroup*, NecroDeclarationGroup, declaration_group)

typedef struct NecroDeclarationsInfo
{
    int32_t                     index;
    NecroDeclarationGroupVector stack;
    NecroDeclarationGroupList*  group_lists;
    NecroDeclarationGroup*      current_group;
} NecroDeclarationsInfo;

NecroDependencyList*       necro_create_dependency_list(NecroPagedArena* arena, NecroDeclarationGroup* dependency_group, NecroDependencyList* head);
NecroDeclarationsInfo*     necro_create_declarations_info(NecroPagedArena* arena);
NecroDeclarationGroup*     necro_create_declaration_group(NecroPagedArena* arena, NecroAst* declaration_ast, NecroDeclarationGroup* prev);
NecroDeclarationGroup*     necro_append_declaration_group(NecroPagedArena* arena, NecroAst* declaration_ast, NecroDeclarationGroup* head);
void                       necro_append_declaration_group_to_group_in_group_list(NecroPagedArena* arena, NecroDeclarationGroupList* group_list, NecroDeclarationGroup* group_to_append);
void                       necro_prepend_declaration_group_to_group_in_group_list(NecroPagedArena* arena, NecroDeclarationGroupList* group_list, NecroDeclarationGroup* group_to_prepend);
NecroDeclarationGroupList* necro_prepend_declaration_group_list(NecroPagedArena* arena, NecroDeclarationGroup* declaration_group, NecroDeclarationGroupList* next);
NecroDeclarationGroupList* necro_create_declaration_group_list(NecroPagedArena* arena, NecroDeclarationGroup* declaration_group, NecroDeclarationGroupList* prev);
NecroDeclarationGroupList* necro_append_declaration_group_list(NecroPagedArena* arena, NecroDeclarationGroup* declaration_group, NecroDeclarationGroupList* head);
NecroDeclarationGroupList* necro_get_curr_group_list(NecroDeclarationGroupList* group_list);

#endif // NECRO_AST_H