#pragma once

#include <ostream>
#include <string>
#include <memory>
#include <cstddef>
#include <vector>
#include <unordered_map>

// 同一次 IR 生成共用临时值编号；每次 GenerateIR 都会创建新的上下文。
struct IRContext {
    std::size_t next_temp = 0;
    std::unordered_map<std::string, int> constants;
    std::string NewTemp();
};

// 所有 AST 的基类
class BaseAST {
    public:
    virtual ~BaseAST() = default;
    void GenerateIR(std::ostream &out) const;
    virtual std::string GenerateIR(std::ostream &out, IRContext &context) const = 0;
    virtual int Calculate(const IRContext &context) const;
};

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
    public:
    using BaseAST::GenerateIR;
    // 用智能指针管理对象
    std::unique_ptr<BaseAST> func_def;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;

};

// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST {
    public:
    using BaseAST::GenerateIR;
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;

};

// FuncType
class FuncTypeAST : public BaseAST {
    public:
    using BaseAST::GenerateIR;

    std::string GenerateIR(std::ostream &, IRContext &) const override;

};

// Block
class BlockAST  : public BaseAST{
    public:
    using BaseAST::GenerateIR;
    std::vector<std::unique_ptr<BaseAST>> items;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;

};

class StmtAST : public BaseAST{
    public :
    using BaseAST::GenerateIR;
    std::unique_ptr<BaseAST> exp;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;

};

class ConstDeclAST : public BaseAST {
public:
    using BaseAST::GenerateIR;
    std::vector<std::unique_ptr<BaseAST>> defs;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;
};

class ConstDefAST : public BaseAST {
public:
    using BaseAST::GenerateIR;
    std::string ident;
    std::unique_ptr<BaseAST> init_val;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;
};

class LValAST : public BaseAST {
public:
    using BaseAST::GenerateIR;
    std::string ident;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;
    int Calculate(const IRContext &context) const override;
};

class NumAST : public BaseAST{
    public:
    using BaseAST::GenerateIR;
    int num;

    std::string GenerateIR(std::ostream &, IRContext &) const override;
    int Calculate(const IRContext &context) const override;
};

// 一元表达式：op 为 '+'、'-' 或 '!'，operand 可继续嵌套表达式。
class ExpAST : public BaseAST{
    public:
    using BaseAST::GenerateIR;
    char op ;
    std::unique_ptr<BaseAST> operand;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;
    int Calculate(const IRContext &context) const override;
};

class BinaryAST : public BaseAST{
    public:
    using BaseAST::GenerateIR;
    std::string op;
    std::unique_ptr<BaseAST> lhs;
    std::unique_ptr<BaseAST> rhs;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;
    int Calculate(const IRContext &context) const override;   
};
