/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef AST_H
#define AST_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "lexer.h"
#include "intern.h"
#include "arena.h"
#include "parser.h"

struct NecroAST_Node_Reified;
struct NecroDeclarationGroup;
struct NecroDeclarationGroupList;
struct NecroDeclarationsInfo;
struct NecroTypeClassContext;

//=====================================================
// AST FunctionType
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* type;
    struct NecroAST_Node_Reified* next_on_arrow;
} NecroAST_FunctionType_Reified;

//=====================================================
// AST TypeClassInstance
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* context;// optional, null_local_ptr if not present
    struct NecroAST_Node_Reified* qtycls;
    struct NecroAST_Node_Reified* inst;
    struct NecroAST_Node_Reified* declarations; // Points to the next in the list, null_local_ptr if the end
    struct NecroAST_Node_Reified* dictionary_instance; // Dictionary instance which is generated at compile time by the compiler
    struct NecroDeclarationGroup* declaration_group;
    NecroSymbol                   instance_name;
    NecroID                       instance_id;
} NecroAST_TypeClassInstance_Reified;

//=====================================================
// AST TypeClassDeclaration
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* context; // optional, null_local_ptr if not present
    struct NecroAST_Node_Reified* tycls;
    struct NecroAST_Node_Reified* tyvar;
    struct NecroAST_Node_Reified* declarations; // Points to the next in the list, null_local_ptr if the end
    struct NecroAST_Node_Reified* dictionary_data_declaration; // Dictionary data declaration which is generated at compile time by the compiler
    struct NecroDeclarationGroup* declaration_group;
} NecroAST_TypeClassDeclaration_Reified;

//=====================================================
// AST TypeClassContext
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* conid;
    struct NecroAST_Node_Reified* varid;
} NecroAST_TypeClassContext_Reified;

//=====================================================
// AST TypeSignature
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* var;
    struct NecroAST_Node_Reified* context; // optional, null_local_ptr if not present
    struct NecroAST_Node_Reified* type;
    NECRO_SIG_TYPE                sig_type;
    struct NecroDeclarationGroup* declaration_group;
} NecroAST_TypeSignature_Reified;

//=====================================================
// AST DataDeclaration
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* simpletype;
    struct NecroAST_Node_Reified* constructor_list; // Points to the next in the list, null_local_ptr if the end
    struct NecroDeclarationGroup* declaration_group;
    bool                          is_recursive;
} NecroAST_DataDeclaration_Reified;

//=====================================================
// AST Constructor
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* conid;
    struct NecroAST_Node_Reified* arg_list; // Points to the next in the list, null_local_ptr if the end
} NecroAST_Constructor_Reified;

//=====================================================
// AST SimpleType
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* type_con;
    struct NecroAST_Node_Reified* type_var_list; // Points to the next in the list, null_local_ptr if the end
} NecroAST_SimpleType_Reified;

//=====================================================
// AST BinOpSym
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* left;
    struct NecroAST_Node_Reified* op;
    struct NecroAST_Node_Reified* right;
} NecroAST_BinOpSym_Reified;

//=====================================================
// AST Operator Left Section
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* left;
    NecroAST_BinOpType            type;
    NecroSymbol                   symbol;
    NecroID                       id;
    struct NecroTypeClassContext* inst_context;
    struct NecroType*             op_necro_type;
} NecroAST_OpLeftSection_Reified;

//=====================================================
// AST Operator Right Section
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* right;
    NecroAST_BinOpType            type;
    NecroSymbol                   symbol;
    NecroID                       id;
    struct NecroTypeClassContext* inst_context;
    struct NecroType*             op_necro_type;
} NecroAST_OpRightSection_Reified;

//=====================================================
// AST Type App
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* ty;
    struct NecroAST_Node_Reified* next_ty; // Points to the next in the list, null_local_ptr if the end
} NecroAST_TypeApp_Reified;

//=====================================================
// AST ConID
//=====================================================
typedef struct
{
    NecroSymbol    symbol;
    NecroID        id;
    NECRO_CON_TYPE con_type;
} NecroAST_ConID_Reified;

//=====================================================
// AST Case
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* expression;
    struct NecroAST_Node_Reified* alternatives;
} NecroAST_Case_Reified;

//=====================================================
// AST CaseAlternative
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* pat;
    struct NecroAST_Node_Reified* body;
} NecroAST_CaseAlternative_Reified;

//=====================================================
// AST Module
//=====================================================


//=====================================================
// AST Undefined
//=====================================================
typedef struct
{
    uint8_t _pad;
} NecroAST_Undefined_Reified;

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
        char        char_literal;
    };
    NecroAST_ConstantType type;
    struct NecroAST_Node_Reified* pat_from_ast;
    struct NecroAST_Node_Reified* pat_eq_ast;
} NecroAST_Constant_Reified;

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
    struct NecroAST_Node_Reified* lhs;
    struct NecroAST_Node_Reified* rhs;
    NecroAST_BinOpType            type;
    NecroSymbol                   symbol;
    NecroID                       id;
    struct NecroTypeClassContext* inst_context;
} NecroAST_BinOp_Reified;

//=====================================================
// AST if then else
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* if_expr;
    struct NecroAST_Node_Reified* then_expr;
    struct NecroAST_Node_Reified* else_expr;
} NecroAST_IfThenElse_Reified;

//=====================================================
// AST Right Hand Side
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* expression;
    struct NecroAST_Node_Reified* declarations;
} NecroAST_RightHandSide_Reified;

//=====================================================
// AST Let Expression
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* expression;
    struct NecroAST_Node_Reified* declarations;
} NecroAST_LetExpression_Reified;

//=====================================================
// AST Simple Assignment
//=====================================================
typedef struct
{
    NecroSymbol                   variable_name;
    struct NecroAST_Node_Reified* initializer;
    struct NecroAST_Node_Reified* rhs;
    NecroID                       id;
    struct NecroDeclarationGroup* declaration_group;
    bool                          is_recursive;
} NecroAST_SimpleAssignment_Reified;

//=====================================================
// AST Bind Assignment
//=====================================================
typedef struct
{
    NecroSymbol                   variable_name;
    struct NecroAST_Node_Reified* expression;
    NecroID                       id;
} NecroAST_BindAssignment_Reified;

//=====================================================
// AST Bind Assignment
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* pat;
    struct NecroAST_Node_Reified* expression;
} NecroAST_PatBindAssignment_Reified;

//=====================================================
// AST apats
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* apat;
    struct NecroAST_Node_Reified* next_apat;
} NecroAST_Apats_Reified;

//=====================================================
// AST Apats Assignment
//=====================================================
typedef struct
{
    NecroSymbol                   variable_name;
    struct NecroAST_Node_Reified* apats;
    struct NecroAST_Node_Reified* rhs;
    NecroID                       id;
    struct NecroDeclarationGroup* declaration_group;
} NecroAST_ApatsAssignment_Reified;

//=====================================================
// AST Pat Assignment
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* pat;
    struct NecroAST_Node_Reified* rhs;
    struct NecroDeclarationGroup* declaration_group;
} NecroAST_PatAssignment_Reified;

//=====================================================
// AST Lambda
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* apats;
    struct NecroAST_Node_Reified* expression;
} NecroAST_Lambda_Reified;

//=====================================================
// AST List Node
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* item;
    struct NecroAST_Node_Reified* next_item;
} NecroAST_ListNode_Reified;

//=====================================================
// AST Expression List
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* expressions; // NecroAST_ListNode of expressions
} NecroAST_ExpressionList_Reified;

//=====================================================
// AST Tuple
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* expressions; // NecroAST_ListNode of expressions
} NecroAST_Tuple_Reified;

//=====================================================
// AST Do
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* statement_list; // NecroAST_ListNode of do statement items
    struct NecroType*             monad_var;
} NecroAST_Do_Reified;

//=====================================================
// AST Pat
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* expressions; // NecroAST_ListNode of expressions
} NecroAST_PatternExpression_Reified;

//=====================================================
// AST Variable
//=====================================================
typedef struct
{
    NecroSymbol                   symbol;
    NecroID                       id;
    NECRO_VAR_TYPE                var_type;
    struct NecroTypeClassContext* inst_context;
    struct NecroAST_Node_Reified* initializer;
    bool                          is_recursive;
} NecroAST_Variable_Reified;

//=====================================================
// AST Function Expression
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* aexp;
    struct NecroAST_Node_Reified* next_fexpression; // Points to the next in the list, null_local_ptr if the end
} NecroAST_FunctionExpression_Reified;

//=====================================================
// AST Declarations
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified*     declaration_impl;
    struct NecroAST_Node_Reified*     next_declaration; // Points to the next in the list, null_local_ptr if the end
    struct NecroDeclarationGroupList* group_list;
} NecroAST_Declaration_Reified;

//=====================================================
// AST Top Declarations
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified*     declaration;
    struct NecroAST_Node_Reified*     next_top_decl; // Points to the next in the list, null_local_ptr if the end
    struct NecroDeclarationGroupList* group_list;
} NecroAST_TopDeclaration_Reified;


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
    struct NecroAST_Node_Reified* from;
    struct NecroAST_Node_Reified* then;
    struct NecroAST_Node_Reified* to;
    NecroAST_ArithmeticSeqType type;
} NecroAST_ArithmeticSequence_Reified;

//=====================================================
// AST Node
//=====================================================
struct NecroScope;
typedef struct NecroAST_Node_Reified
{
    union
    {
        // NecroAST_UnaryOp unary_op; // Do we need this?
        NecroAST_Undefined_Reified            undefined;
        NecroAST_Constant_Reified             constant;
        NecroAST_BinOp_Reified                bin_op;
        NecroAST_IfThenElse_Reified           if_then_else;
        NecroAST_TopDeclaration_Reified       top_declaration;
        NecroAST_Declaration_Reified          declaration;
        NecroAST_SimpleAssignment_Reified     simple_assignment;
        NecroAST_Apats_Reified                apats;
        NecroAST_ApatsAssignment_Reified      apats_assignment;
        NecroAST_PatAssignment_Reified        pat_assignment;
        NecroAST_RightHandSide_Reified        right_hand_side;
        NecroAST_LetExpression_Reified        let_expression;
        NecroAST_FunctionExpression_Reified   fexpression;
        NecroAST_Variable_Reified             variable;
        NecroAST_Lambda_Reified               lambda;
        NecroAST_Do_Reified                   do_statement;
        NecroAST_ListNode_Reified             list;
        NecroAST_ExpressionList_Reified       expression_list;
        NecroAST_Tuple_Reified                tuple;
        NecroAST_BindAssignment_Reified       bind_assignment;
        NecroAST_PatBindAssignment_Reified    pat_bind_assignment;
        NecroAST_ArithmeticSequence_Reified   arithmetic_sequence;
        NecroAST_Case_Reified                 case_expression;
        NecroAST_CaseAlternative_Reified      case_alternative;
        NecroAST_ConID_Reified                conid;
        NecroAST_BinOpSym_Reified             bin_op_sym;
        NecroAST_OpLeftSection_Reified        op_left_section;
        NecroAST_OpRightSection_Reified       op_right_section;
        NecroAST_TypeApp_Reified              type_app;
        NecroAST_SimpleType_Reified           simple_type;
        NecroAST_Constructor_Reified          constructor;
        NecroAST_DataDeclaration_Reified      data_declaration;
        NecroAST_TypeClassContext_Reified     type_class_context;
        NecroAST_TypeClassDeclaration_Reified type_class_declaration;
        NecroAST_TypeClassInstance_Reified    type_class_instance;
        NecroAST_TypeSignature_Reified        type_signature;
        NecroAST_FunctionType_Reified         function_type;
        NecroAST_PatternExpression_Reified    pattern_expression;
    };
    NecroAST_NodeType       type;
    NecroSourceLoc          source_loc;
    struct NecroScope*      scope;
    struct NecroType*       necro_type;
} NecroAST_Node_Reified;

typedef struct
{
    NecroPagedArena        arena;
    NecroAST_Node_Reified* root;
} NecroAST_Reified;

NecroAST_Reified necro_create_reified_ast();
NecroAST_Reified necro_reify_ast(NecroAST* a_ast, NecroAST_LocalPtr a_root, NecroIntern* intern);
void             necro_destroy_reified_ast(NecroAST_Reified* ast);
void             necro_print_reified_ast(NecroAST_Reified* ast, NecroIntern* intern);
void             necro_print_reified_ast_node(NecroAST_Node_Reified* ast_node, NecroIntern* intern);

// Manual AST Creation
typedef NecroAST_Node_Reified NecroASTNode;
NecroASTNode* necro_create_conid_ast(NecroPagedArena* arena, NecroIntern* intern, const char* con_name, NECRO_CON_TYPE con_type);
NecroASTNode* necro_create_variable_ast(NecroPagedArena* arena, NecroIntern* intern, const char* variable_name, NECRO_VAR_TYPE var_type);
NecroASTNode* necro_create_ast_list(NecroPagedArena* arena, NecroASTNode* item, NecroASTNode* next);
NecroASTNode* necro_create_var_list_ast(NecroPagedArena* arena, NecroIntern* intern, size_t num_vars, NECRO_VAR_TYPE var_type);
NecroASTNode* necro_create_data_constructor_ast(NecroPagedArena* arena, NecroIntern* intern, const char* con_name, NecroASTNode* arg_list);
NecroASTNode* necro_create_simple_type_ast(NecroPagedArena* arena, NecroIntern* intern, const char* simple_type_name, NecroASTNode* ty_var_list);
NecroASTNode* necro_create_data_declaration_ast(NecroPagedArena* arena, NecroIntern* intern, NecroASTNode* simple_type, NecroASTNode* constructor_list);
NecroASTNode* necro_create_type_app_ast(NecroPagedArena* arena, NecroASTNode* type1, NecroASTNode* type2);
NecroASTNode* necro_create_fun_ast(NecroPagedArena* arena, NecroASTNode* type1, NecroASTNode* type2);
NecroASTNode* necro_create_fexpr_ast(NecroPagedArena* arena, NecroASTNode* f_ast, NecroASTNode* x_ast);
NecroASTNode* necro_create_fun_type_sig_ast(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroASTNode* context_ast, NecroASTNode* type_ast, NECRO_VAR_TYPE var_type, NECRO_SIG_TYPE sig_type);
NecroASTNode* necro_create_type_class_ast(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, const char* class_var, NecroASTNode* context_ast, NecroASTNode* declarations_ast);
NecroASTNode* necro_create_instance_ast(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, NecroASTNode* inst_ast, NecroASTNode* context_ast, NecroASTNode* declarations_ast);
NecroASTNode* necro_create_top_level_declaration_list(NecroPagedArena* arena, NecroASTNode* top_level_declaration, NecroASTNode* next);
NecroASTNode* necro_create_declaration_list(NecroPagedArena* arena, NecroASTNode* declaration, NecroASTNode* next);
NecroASTNode* necro_create_simple_assignment(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroASTNode* rhs_ast);
NecroASTNode* necro_create_apat_list_ast(NecroPagedArena* arena, NecroASTNode* apat_item, NecroASTNode* next_apat);
NecroASTNode* necro_create_apats_assignment_ast(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroASTNode* apats, NecroASTNode* rhs_ast);
NecroASTNode* necro_create_lambda_ast(NecroPagedArena* arena, NecroASTNode* apats, NecroASTNode* expr_ast);
NecroASTNode* necro_create_wild_card_ast(NecroPagedArena* arena);
NecroASTNode* necro_create_context(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, const char* var_name, NecroASTNode* next);
NecroASTNode* necro_create_rhs_ast(NecroPagedArena* arena, NecroASTNode* expression, NecroASTNode* declarations);
NecroASTNode* necro_create_bin_op_ast(NecroPagedArena* arena, NecroIntern* intern, const char* op_name, NecroASTNode* lhs, NecroASTNode* rhs);

// Dependency analysis
typedef struct NecroDependencyList
{
    struct NecroDeclarationGroup* dependency_group;
    struct NecroDependencyList*   next;
} NecroDependencyList;

typedef struct NecroDeclarationGroup
{
    NecroASTNode*                 declaration_ast;
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
NecroDeclarationGroup*     necro_create_declaration_group(NecroPagedArena* arena, NecroASTNode* declaration_ast, NecroDeclarationGroup* prev);
NecroDeclarationGroup*     necro_append_declaration_group(NecroPagedArena* arena, NecroASTNode* declaration_ast, NecroDeclarationGroup* head);
void                       necro_append_declaration_group_to_group_in_group_list(NecroPagedArena* arena, NecroDeclarationGroupList* group_list, NecroDeclarationGroup* group_to_append);
void                       necro_prepend_declaration_group_to_group_in_group_list(NecroPagedArena* arena, NecroDeclarationGroupList* group_list, NecroDeclarationGroup* group_to_prepend);
NecroDeclarationGroupList* necro_prepend_declaration_group_list(NecroPagedArena* arena, NecroDeclarationGroup* declaration_group, NecroDeclarationGroupList* next);
NecroDeclarationGroupList* necro_create_declaration_group_list(NecroPagedArena* arena, NecroDeclarationGroup* declaration_group, NecroDeclarationGroupList* prev);
NecroDeclarationGroupList* necro_append_declaration_group_list(NecroPagedArena* arena, NecroDeclarationGroup* declaration_group, NecroDeclarationGroupList* head);
NecroDeclarationGroupList* necro_get_curr_group_list(NecroDeclarationGroupList* group_list);

#endif // AST_H