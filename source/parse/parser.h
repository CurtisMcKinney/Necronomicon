/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_PARSER_H
#define NECRO_PARSER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "lexer.h"
#include "arena.h"

/*
    TODO:
        * correct source_locs!!!!!
        * Consider removing OpPats
        * Better Type Class error messages
        * Better Class Instance error messages
*/

// Local offset into AST arena
typedef size_t NecroParseAstLocalPtr;
static const NecroParseAstLocalPtr null_local_ptr = (size_t) -1;

typedef enum
{
    NECRO_AST_UNDEFINED,
    NECRO_AST_CONSTANT,
    NECRO_AST_UN_OP,
    NECRO_AST_BIN_OP,
    NECRO_AST_IF_THEN_ELSE,
    NECRO_AST_TOP_DECL,
    NECRO_AST_DECL,
    NECRO_AST_SIMPLE_ASSIGNMENT,
    NECRO_AST_APATS_ASSIGNMENT,
    NECRO_AST_PAT_ASSIGNMENT,
    NECRO_AST_RIGHT_HAND_SIDE,
    NECRO_AST_LET_EXPRESSION,
    NECRO_AST_FUNCTION_EXPRESSION,
    // NECRO_AST_INFIX_EXPRESSION,
    NECRO_AST_VARIABLE,
    NECRO_AST_APATS,
    NECRO_AST_WILDCARD,
    NECRO_AST_LAMBDA,
    NECRO_AST_DO,
    NECRO_AST_SEQ_EXPRESSION,
    NECRO_AST_LIST_NODE,
    NECRO_AST_EXPRESSION_LIST,
    NECRO_AST_EXPRESSION_ARRAY,
    NECRO_AST_TUPLE,
    NECRO_BIND_ASSIGNMENT,
    NECRO_PAT_BIND_ASSIGNMENT,
    NECRO_AST_ARITHMETIC_SEQUENCE,
    NECRO_AST_CASE,
    NECRO_AST_CASE_ALTERNATIVE,
    NECRO_AST_CONID,
    NECRO_AST_TYPE_APP,
    NECRO_AST_BIN_OP_SYM,
    NECRO_AST_OP_LEFT_SECTION,
    NECRO_AST_OP_RIGHT_SECTION,
    NECRO_AST_CONSTRUCTOR,
    NECRO_AST_SIMPLE_TYPE,
    NECRO_AST_DATA_DECLARATION,
    NECRO_AST_TYPE_CLASS_CONTEXT,
    NECRO_AST_TYPE_CLASS_DECLARATION,
    NECRO_AST_TYPE_CLASS_INSTANCE,
    NECRO_AST_TYPE_SIGNATURE,
    NECRO_AST_FUNCTION_TYPE,
    NECRO_AST_DECLARATION_GROUP_LIST,
    NECRO_AST_TYPE_ATTRIBUTE,
    NECRO_AST_FOR_LOOP,
    NECRO_AST_WHILE_LOOP,
    // NECRO_AST_MODULE,
} NECRO_AST_TYPE;

//=====================================================
// AST FunctionType
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr type;
    NecroParseAstLocalPtr next_on_arrow;
} NecroParseAstFunctionType;

//=====================================================
// AST TypeClassInstance
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr context;// optional, null_local_ptr if not present
    NecroParseAstLocalPtr qtycls;
    NecroParseAstLocalPtr inst;
    NecroParseAstLocalPtr declarations; // Points to the next in the list, null_local_ptr if the end
} NecroParseAstTypeClassInstance;

//=====================================================
// AST TypeClassDeclaration
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr context; // optional, null_local_ptr if not present
    NecroParseAstLocalPtr tycls;
    NecroParseAstLocalPtr tyvar;
    NecroParseAstLocalPtr declarations; // Points to the next in the list, null_local_ptr if the end
} NecroParseAstTypeClassDeclaration;

//=====================================================
// AST TypeClassContext
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr conid;
    NecroParseAstLocalPtr varid;
} NecroParseAstTypeClassContext;

//=====================================================
// AST TypeSignature
//=====================================================
typedef enum
{
    NECRO_SIG_DECLARATION,
    NECRO_SIG_TYPE_CLASS,
    NECRO_SIG_TYPE_VAR,
} NECRO_SIG_TYPE;

typedef struct
{
    NecroParseAstLocalPtr var;
    NecroParseAstLocalPtr context; // optional, null_local_ptr if not present
    NecroParseAstLocalPtr type;
    NECRO_SIG_TYPE        sig_type;
} NecroParseAstTypeSignature;

//=====================================================
// AST DataDeclaration
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr simpletype;
    NecroParseAstLocalPtr constructor_list; // Points to the next in the list, null_local_ptr if the end
} NecroParseAstDataDeclaration;

//=====================================================
// AST TypeConstructor
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr conid;
    NecroParseAstLocalPtr arg_list; // Points to the next in the list, null_local_ptr if the end
} NecroParseAstConstructor;

//=====================================================
// AST SimpleType
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr type_con;
    NecroParseAstLocalPtr type_var_list; // Points to the next in the list, null_local_ptr if the end
} NecroParseAstSimpleType;

//=====================================================
// AST BinOpSym
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr left;
    NecroParseAstLocalPtr op;
    NecroParseAstLocalPtr right;
} NecroParseAstBinOpSym;

//=====================================================
// AST Type App
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr ty;
    NecroParseAstLocalPtr next_ty; // Points to the next in the list, null_local_ptr if the end
} NecroParseAstTypeApp;

//=====================================================
// AST ConID
//=====================================================
typedef enum
{
    NECRO_CON_VAR,
    NECRO_CON_TYPE_VAR,
    NECRO_CON_DATA_DECLARATION,
    NECRO_CON_TYPE_DECLARATION,
} NECRO_CON_TYPE;

typedef struct
{
    NecroSymbol    symbol;
    NECRO_CON_TYPE con_type;
} NecroParseAstConId;

//=====================================================
// AST Case
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr expression;
    NecroParseAstLocalPtr alternatives;
} NecroParseAstCase;

//=====================================================
// AST CaseAlternative
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr pat;
    NecroParseAstLocalPtr body;
} NecroParseAstCaseAlternative;

//=====================================================
// AST Module
//=====================================================

//=====================================================
// AST Undefined
//=====================================================
typedef struct
{
    uint8_t _pad;
} NecroParseAstUndefined;

//=====================================================
// AST Constant
//=====================================================
typedef enum
{
    NECRO_AST_CONSTANT_FLOAT,
    NECRO_AST_CONSTANT_INTEGER,
    NECRO_AST_CONSTANT_UNSIGNED_INTEGER,
    NECRO_AST_CONSTANT_STRING,
    NECRO_AST_CONSTANT_CHAR,

    NECRO_AST_CONSTANT_FLOAT_PATTERN,
    NECRO_AST_CONSTANT_INTEGER_PATTERN,
    NECRO_AST_CONSTANT_UNSIGNED_INTEGER_PATTERN,
    NECRO_AST_CONSTANT_CHAR_PATTERN,

    NECRO_AST_CONSTANT_TYPE_INT,
    NECRO_AST_CONSTANT_TYPE_STRING,

    NECRO_AST_CONSTANT_ARRAY,
} NECRO_CONSTANT_TYPE;

typedef struct
{
    union
    {
        double      double_literal;
        int64_t     int_literal;
        NecroSymbol symbol;
        uint32_t    char_literal;
    };
    NECRO_CONSTANT_TYPE type;
} NecroParseAstConstant;

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
//     NecroAST_LocalPtr rhs;
//     NecroAST_UnaryOpType type;
// } NecroAST_UnaryOp;

//=====================================================
// AST Binary Operation
//=====================================================

typedef enum
{
    NECRO_BIN_OP_ADD = 0,
    NECRO_BIN_OP_SUB,
    NECRO_BIN_OP_MUL,
    NECRO_BIN_OP_DIV,
    NECRO_BIN_OP_DOUBLE_DIV,
    NECRO_BIN_OP_MOD,
    NECRO_BIN_OP_GT,
    NECRO_BIN_OP_LT,
    NECRO_BIN_OP_GTE,
    NECRO_BIN_OP_LTE,
    NECRO_BIN_OP_COLON,
	NECRO_BIN_OP_DOUBLE_COLON,
	NECRO_BIN_OP_LEFT_SHIFT,
	NECRO_BIN_OP_RIGHT_SHIFT,
	NECRO_BIN_OP_PIPE,
	NECRO_BIN_OP_FORWARD_PIPE,
	NECRO_BIN_OP_BACK_PIPE,
    NECRO_BIN_OP_EQUALS,
    NECRO_BIN_OP_NOT_EQUALS,
	NECRO_BIN_OP_AND,
	NECRO_BIN_OP_OR,
    NECRO_BIN_OP_DOT,
    NECRO_BIN_OP_DOLLAR,
    NECRO_BIN_OP_BIND_RIGHT,
    NECRO_BIN_OP_BIND_LEFT,
    NECRO_BIN_OP_DOUBLE_EXCLAMATION,
    NECRO_BIN_OP_APPEND,
    NECRO_BIN_OP_ADD_ON_LEFT,
    NECRO_BIN_OP_ADD_ON_RIGHT,
    NECRO_BIN_OP_SUB_ON_LEFT,
    NECRO_BIN_OP_SUB_ON_RIGHT,
    NECRO_BIN_OP_MUL_ON_LEFT,
    NECRO_BIN_OP_MUL_ON_RIGHT,
    NECRO_BIN_OP_DIV_ON_LEFT,
    NECRO_BIN_OP_DIV_ON_RIGHT,
    NECRO_BIN_OP_CONST_LEFT_ON_LEFT,
    NECRO_BIN_OP_CONST_LEFT_ON_RIGHT,
    NECRO_BIN_OP_CONST_RIGHT_ON_LEFT,
    NECRO_BIN_OP_CONST_RIGHT_ON_RIGHT,
    NECRO_BIN_OP_CONST_LEFT_ON_BOTH,
    NECRO_BIN_OP_CONST_RIGHT_ON_BOTH,
    NECRO_BIN_OP_FBY,
    NECRO_BIN_OP_COUNT,
    NECRO_BIN_OP_UNDEFINED = NECRO_BIN_OP_COUNT
} NECRO_BIN_OP_TYPE;

typedef struct
{
    NecroParseAstLocalPtr lhs;
    NecroParseAstLocalPtr rhs;
    NECRO_BIN_OP_TYPE     type;
    NecroSymbol           symbol;
} NecroParseAstBinOp;

//=====================================================
// AST Op Left Section
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr left;
    NECRO_BIN_OP_TYPE     type;
    NecroSymbol           symbol;
} NecroParseAstOpLeftSection;

//=====================================================
// AST Op Right Section
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr right;
    NECRO_BIN_OP_TYPE     type;
    NecroSymbol           symbol;
} NecroParseAstOpRightSection;

//=====================================================
// AST if then else
//=====================================================

typedef struct
{
    NecroParseAstLocalPtr if_expr;
    NecroParseAstLocalPtr then_expr;
    NecroParseAstLocalPtr else_expr;
} NecroParseAstIfThenElse;

//=====================================================
// AST Right Hand Side
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr expression;
    NecroParseAstLocalPtr declarations;
} NecroParseAstRightHandSide;

//=====================================================
// AST Let Expression
//=====================================================

typedef struct
{
    NecroParseAstLocalPtr expression;
    NecroParseAstLocalPtr declarations;
} NecroParseAstLetExpression;

//=====================================================
// AST Simple Assignment
//=====================================================

typedef struct
{
    NecroSymbol           variable_name;
    NecroParseAstLocalPtr rhs;
    NecroParseAstLocalPtr initializer;
} NecroParseAstSimpleAssignment;

//=====================================================
// AST Bind Assignment
//=====================================================

typedef struct
{
    NecroSymbol           variable_name;
    NecroParseAstLocalPtr expression;
} NecroParseAstBindAssignment;

//=====================================================
// AST Pat Bind Assignment
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr pat;
    NecroParseAstLocalPtr expression;
} NecroParseAstPatBindAssignment;

//=====================================================
// AST apats
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr apat;
    NecroParseAstLocalPtr next_apat;
} NecroParseAstApats;

//=====================================================
// AST Apats Assignment
//=====================================================
typedef struct
{
    NecroSymbol           variable_name;
    NecroParseAstLocalPtr apats;
    NecroParseAstLocalPtr rhs;
} NecroParseAstApatsAssignment;

//=====================================================
// AST Pat Assignment
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr pat;
    NecroParseAstLocalPtr rhs;
} NecroParseAstPatAssignment;

//=====================================================
// AST Lambda
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr apats;
    NecroParseAstLocalPtr expression;
} NecroParseAstLambda;

//=====================================================
// AST List Node
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr item;
    NecroParseAstLocalPtr next_item;
} NecroParseAstList;

//=====================================================
// AST Expression List
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr expressions; // NecroAST_ListNode of expressions
} NecroParseAstExpressionList;

//=====================================================
// AST Expression Array
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr expressions; // NecroAST_ListNode of expressions
} NecroParseAstExpressionArray;

//=====================================================
// AST Tuple
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr expressions; // NecroAST_ListNode of expressions
    bool                  is_unboxed;
} NecroParseAstTuple;

//=====================================================
// AST Do
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr statement_list; // NecroAST_ListNode of do statement items
} NecroParseAstDo;

//=====================================================
// AST Pattern Expression
//=====================================================
typedef enum
{
    NECRO_SEQUENCE_SEQUENCE,
    NECRO_SEQUENCE_TUPLE,
    NECRO_SEQUENCE_INTERLEAVE,
} NECRO_SEQUENCE_TYPE;
typedef struct
{
    NecroParseAstLocalPtr expressions; // NecroAST_ListNode of expressions
    NECRO_SEQUENCE_TYPE   sequence_type;
} NecroParseAstSeqExpression;

//=====================================================
// AST ForLoop
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr range_init;
    NecroParseAstLocalPtr value_init;
    NecroParseAstLocalPtr index_apat;
    NecroParseAstLocalPtr value_apat;
    NecroParseAstLocalPtr expression;
} NecroParseAstForLoop;

//=====================================================
// AST WhileLoop
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr value_init;
    NecroParseAstLocalPtr value_apat;
    NecroParseAstLocalPtr while_expression;
    NecroParseAstLocalPtr do_expression;
} NecroParseAstWhileLoop;

//=====================================================
// AST Type Attribute
//=====================================================
typedef enum
{
    NECRO_TYPE_ATTRIBUTE_VOID,
    NECRO_TYPE_ATTRIBUTE_NONE,
    NECRO_TYPE_ATTRIBUTE_STAR,
    NECRO_TYPE_ATTRIBUTE_DOT,
    NECRO_TYPE_ATTRIBUTE_CONSTRUCTOR_DOT,
} NECRO_TYPE_ATTRIBUTE_TYPE;

typedef struct
{
    NecroParseAstLocalPtr     attributed_type;
    NECRO_TYPE_ATTRIBUTE_TYPE type;
} NecroParseAstTypeAttribute;

//=====================================================
// AST Variable
//=====================================================
typedef enum
{
    NECRO_VAR_VAR,
    NECRO_VAR_TYPE_FREE_VAR,
    NECRO_VAR_TYPE_VAR_DECLARATION,
    NECRO_VAR_DECLARATION,
    NECRO_VAR_SIG,
    NECRO_VAR_CLASS_SIG,
} NECRO_VAR_TYPE;
const char* necro_var_type_string(NECRO_VAR_TYPE symbol_type);

typedef enum
{
    NECRO_TYPE_ZERO_ORDER,
    NECRO_TYPE_HIGHER_ORDER,
    NECRO_TYPE_POLY_ORDER,
} NECRO_TYPE_ORDER;

typedef struct
{
    NecroSymbol           symbol;
    NECRO_VAR_TYPE        var_type;
    NecroParseAstLocalPtr initializer;
    NECRO_TYPE_ORDER      order;
} NecroParseAstVariable;

//=====================================================
// AST Function Expression
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr aexp;
    NecroParseAstLocalPtr next_fexpression; // Points to the next in the list, null_local_ptr if the end
} NecroParseAstFunctionExpression;

//=====================================================
// AST Declarations
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr declaration_impl;
    NecroParseAstLocalPtr next_declaration; // Points to the next in the list, null_local_ptr if the end
} NecroParseAstDeclaration;

//=====================================================
// AST Top Declarations
//=====================================================
typedef struct
{
    NecroParseAstLocalPtr declaration;
    NecroParseAstLocalPtr next_top_decl; // Points to the next in the list, null_local_ptr if the end
} NecroParseAstTopDeclaration;

//=====================================================
// AST Module
//=====================================================

// To Do: Define these!
// typedef struct
// {
//     NecroAST_LocalPtr body;
// } NecroAST_SimpleModule;
//
// typedef struct
// {
//     NecroAST_LocalPtr body;
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
typedef enum
{
    NECRO_ARITHMETIC_ENUM_FROM,
    NECRO_ARITHMETIC_ENUM_FROM_TO,
    NECRO_ARITHMETIC_ENUM_FROM_THEN_TO,
} NECRO_ARITHMETIC_SEQUENCE_TYPE;

typedef struct
{
    NecroParseAstLocalPtr          from;
    NecroParseAstLocalPtr          then;
    NecroParseAstLocalPtr          to;
    NECRO_ARITHMETIC_SEQUENCE_TYPE type;
} NecroParseAstArithmeticSequence;


//=====================================================
// AST Node
//=====================================================
typedef struct
{
    union
    {
        // NecroAST_UnaryOp unary_op; // Do we need this?
        NecroParseAstUndefined            undefined;
        NecroParseAstConstant             constant;
        NecroParseAstBinOp                bin_op;
        NecroParseAstIfThenElse           if_then_else;
        NecroParseAstTopDeclaration       top_declaration;
        NecroParseAstDeclaration          declaration;
        NecroParseAstSimpleAssignment     simple_assignment;
        NecroParseAstApats                apats;
        NecroParseAstApatsAssignment      apats_assignment;
        NecroParseAstPatAssignment        pat_assignment;
        NecroParseAstRightHandSide        right_hand_side;
        NecroParseAstLetExpression        let_expression;
        NecroParseAstFunctionExpression   fexpression;
        NecroParseAstVariable             variable;
        NecroParseAstLambda               lambda;
        NecroParseAstDo                   do_statement;
        NecroParseAstList                 list;
        NecroParseAstExpressionList       expression_list;
        NecroParseAstExpressionArray      expression_array;
        NecroParseAstTuple                tuple;
        NecroParseAstBindAssignment       bind_assignment;
        NecroParseAstPatBindAssignment    pat_bind_assignment;
        NecroParseAstArithmeticSequence   arithmetic_sequence;
        NecroParseAstCase                 case_expression;
        NecroParseAstCaseAlternative      case_alternative;
        NecroParseAstConId                conid;
        NecroParseAstBinOpSym             bin_op_sym;
        NecroParseAstOpLeftSection        op_left_section;
        NecroParseAstOpRightSection       op_right_section;
        NecroParseAstTypeApp              type_app;
        NecroParseAstSimpleType           simple_type;
        NecroParseAstConstructor          constructor;
        NecroParseAstDataDeclaration      data_declaration;
        NecroParseAstTypeClassContext     type_class_context;
        NecroParseAstTypeClassDeclaration type_class_declaration;
        NecroParseAstTypeClassInstance    type_class_instance;
        NecroParseAstTypeSignature        type_signature;
        NecroParseAstFunctionType         function_type;
        NecroParseAstSeqExpression        sequence_expression;
        NecroParseAstTypeAttribute        attribute;
        NecroParseAstForLoop              for_loop;
        NecroParseAstWhileLoop            while_loop;
    };
    NECRO_AST_TYPE type;
    NecroSourceLoc source_loc;
    NecroSourceLoc end_loc;
} NecroParseAst;

//=====================================================
// AST
//=====================================================
typedef struct
{
    NecroArena            arena;
    NecroParseAstLocalPtr root;
    NecroSymbol           module_name;
} NecroParseAstArena;
NecroParseAstArena  necro_parse_ast_arena_empty();
NecroParseAstArena  necro_parse_ast_arena_create(size_t capacity);
void                necro_parse_ast_arena_destroy(NecroParseAstArena* ast);
NecroParseAst*      necro_parse_ast_get_node(NecroParseAstArena* ast, NecroParseAstLocalPtr local_ptr);
NecroParseAst*      necro_parse_ast_get_root_node(NecroParseAstArena* ast);
NecroParseAst*      necro_parse_ast_alloc(NecroArena* arena, NecroParseAstLocalPtr* local_ptr);
void                necro_parse_ast_print(NecroParseAstArena* ast);

//=====================================================
// Parsing
//=====================================================
NecroResult(void) necro_parse(NecroCompileInfo info, NecroIntern* intern, NecroLexTokenVector* tokens, NecroSymbol module_name, NecroParseAstArena* out_ast);
const char*       necro_bin_op_name(NECRO_BIN_OP_TYPE type);
const char*       necro_con_type_string(NECRO_CON_TYPE symbol_type);

#endif // NECRO_PARSER_H
