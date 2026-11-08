%code requires {
  #include <memory>
  #include <string>
  #include <vector>
  #include "ast.h"
}

%{

#include <memory>
#include <string>
#include "ast.h"
#include <utility>

// 声明 lexer 函数和错误处理函数
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

// 定义 parser 函数和错误处理函数的附加参数
// 我们需要返回一个字符串作为 AST, 所以我们把附加参数定义成字符串的智能指针
// 解析完成后, 我们要手动修改这个参数, 把它设置成解析得到的字符串
%parse-param { std::unique_ptr<BaseAST> &ast }

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// 请自行 STFW 在 union 里写一个带析构函数的类会出现什么情况
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
  std::vector<std::unique_ptr<BaseAST>> *ast_list;
}

// lexer 返回的所有 token 种类的声明
// 注意 IDENT 和 INT_CONST 会返回 token 的值, 分别对应 str_val 和 int_val
%token INT RETURN
%token <str_val> IDENT
%token <int_val> INT_CONST
%token LE GE EQ NE LAND LOR
%token CONST

// 非终结符的类型定义
%type <ast_val> FuncDef FuncType Block Stmt 
%type <ast_val> Exp PrimaryExp Number UnaryExp MulExp AddExp LOrExp RelExp EqExp LAndExp
%type <ast_val> Decl ConstDecl ConstDef ConstInitVal ConstExp
%type <ast_val> BlockItem LVal
%type <ast_list> BlockItems ConstDefList
%type <int_val> UnaryOp MulOp AddOp 
%type <str_val> EqOp RelOp
%type <ast_val> VarDecl VarDef InitVal
%type <ast_list> VarDefList

%%

CompUnit
  : FuncDef {
    auto comp_unit = make_unique<CompUnitAST>();
    comp_unit->func_def = unique_ptr<BaseAST>($1);
    ast = std::move(comp_unit);
  }
  ;

FuncDef
  : FuncType IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  ;

FuncType
  : INT {
    $$ = new FuncTypeAST();
  }
  ;

Block
  : '{' BlockItems '}' {
    auto items = unique_ptr<vector<unique_ptr<BaseAST>>>($2);
    auto node = new BlockAST();
    node->items = std::move(*items);
    $$ = node;
  }
  ;

BlockItems
  : %empty {
    $$ = new vector<unique_ptr<BaseAST>>();
  }
  | BlockItems BlockItem {
    $1->emplace_back($2);
    $$ = $1;
  }
  ;

BlockItem
  : Decl {
    $$ = $1;
  }
  | Stmt {
    $$ = $1;
  }
  ;

Stmt
  : RETURN Exp ';' {
    auto ast = new StmtAST();
    ast -> exp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | LVal '=' Exp ';' {
    auto node = new AssignStmtAST();
    node->lval = unique_ptr<LValAST>(static_cast<LValAST*>($1));
    node->exp = unique_ptr<BaseAST>($3);
    $$ = node;
  }  
  ;

//常量和变量
Decl
  : ConstDecl {
    $$ = $1;
  }
  | VarDecl{
    $$ = $1;
  }
  ;

  VarDecl
  : BType VarDefList ';' {
    auto defs = unique_ptr<vector<unique_ptr<BaseAST>>>($2);
    auto node = new VarDeclAST();
    node->defs = std::move(*defs);
    $$ = node;
  }
  ;

VarDefList
  : VarDef {
    auto defs = new vector<unique_ptr<BaseAST>>();
    defs->emplace_back($1);
    $$ = defs;
  }
  | VarDefList ',' VarDef {
    $1->emplace_back($3);
    $$ = $1;
  }
  ;
  
VarDef
  : IDENT {
    auto ident = unique_ptr<string>($1);
    auto node = new VarDefAST();
    node->ident = *ident;
    node->init_val = nullptr;
    $$ = node;
  }
  | IDENT '=' InitVal {
    auto ident = unique_ptr<string>($1);
    auto node = new VarDefAST();
    node->ident = *ident;
    node->init_val = unique_ptr<BaseAST>($3);
    $$ = node;
  }
  ;

InitVal
  : Exp {
    $$ = $1;
  }
  ;

ConstDecl
  : CONST BType ConstDefList ';' {
    auto defs = unique_ptr<vector<unique_ptr<BaseAST>>>($3);
    auto node = new ConstDeclAST();
    node->defs = std::move(*defs);
    $$ = node;
  }
  ;
BType
  : INT {
  }
  ;

ConstDefList
  : ConstDef {
    auto defs = new vector<unique_ptr<BaseAST>>();
    defs->emplace_back($1);
    $$ = defs;
  }
  | ConstDefList ',' ConstDef {
    $1->emplace_back($3);
    $$ = $1;
  }
  ;

ConstDef
  : IDENT '=' ConstInitVal {
    auto ident = unique_ptr<string>($1);

    auto node = new ConstDefAST();
    node->ident = *ident;
    node->init_val = unique_ptr<BaseAST>($3);
    $$ = node;
  }
  ;

ConstInitVal
  : ConstExp {
    $$ = $1;
  }
  ;

ConstExp
  : Exp {
    $$ = $1;
  }
  ;
//表达式
Exp
  : LOrExp {
    $$ = $1;
  }
  ;

PrimaryExp
  : '(' Exp ')'{
    $$ = $2;
  }  
  | LVal {
    $$ = $1;
  }
  | Number {
    $$ = $1;
  }
  ;

LVal
  : IDENT {
    auto ident = unique_ptr<string>($1);
    auto node = new LValAST();
    node->ident = *ident;
    $$ = node;
  }
  ;

Number 
  : INT_CONST {
    auto ast = new NumAST();
    ast -> num = $1;
    $$ = ast;
  }
  ;

UnaryOp 
  : '+'{
    $$ = '+';
  }
  | '-'{ 
    $$ = '-';
  }
  | '!'{
    $$ = '!';
  }
  ;

UnaryExp
  : PrimaryExp{
    $$ = $1;
  }
  | UnaryOp UnaryExp{
    auto ast = new ExpAST();
    ast -> op = static_cast<char>($1);
    ast -> operand = unique_ptr <BaseAST>($2);
    $$ = ast;
  }
  ;

MulExp 
  : UnaryExp {
    $$ = $1;
  }
  | MulExp MulOp UnaryExp { 
    auto ast = new BinaryAST();
    ast->op = static_cast<char>($2);
    ast->lhs = unique_ptr<BaseAST>($1);
    ast->rhs = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

MulOp
  : '*' { $$ = '*'; }
  | '/' { $$ = '/'; }
  | '%' { $$ = '%'; }
  ;

AddExp
  : MulExp {
    $$ = $1;
  }
  | AddExp AddOp MulExp {
    auto ast = new BinaryAST();
    ast->op = static_cast<char>($2);
    ast->lhs = unique_ptr<BaseAST>($1);
    ast->rhs = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

AddOp
  : '+' { $$ = '+'; }
  | '-' { $$ = '-'; }
  ;

RelExp
  : AddExp{  
    $$ = $1;
  }
  | RelExp RelOp AddExp { 
    auto ast = new BinaryAST();
    ast -> op = *unique_ptr <string> ($2);
    ast->lhs = unique_ptr<BaseAST>($1);
    ast->rhs = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

RelOp 
  : '<' { $$ = new string("<"); }
  | '>' { $$ = new string(">"); }
  | LE  { $$ = new string("<="); }
  | GE  { $$ = new string(">="); }
  ;

EqExp 
  : RelExp {$$ = $1;}
  | EqExp EqOp RelExp{
    auto ast = new BinaryAST();
    ast->op = *unique_ptr<string>($2);
    ast->lhs = unique_ptr<BaseAST>($1);
    ast->rhs = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

EqOp 
  :  EQ { $$ = new string("=="); }
  | NE { $$ = new string("!="); }
  ;

LAndExp
  : EqExp {
    $$ = $1;
  }
  | LAndExp LAND EqExp {
    auto ast = new BinaryAST();
    ast->op = "&&";
    ast->lhs = unique_ptr<BaseAST>($1);
    ast->rhs = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

LOrExp
  : LAndExp {
    $$ = $1;
  }
  | LOrExp LOR LAndExp {
    auto ast = new BinaryAST();
    ast->op = "||";
    ast->lhs = unique_ptr<BaseAST>($1);
    ast->rhs = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;
%%

// Bison 所需的错误回调。
void yyerror(unique_ptr<BaseAST> &, const char *) {}
