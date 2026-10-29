#pragma once

#include <ostream>
#include <string>
#include <memory>
#include <cstddef>

// 同一次 IR 生成共用临时值编号；每次 GenerateIR 都会创建新的上下文。
struct IRContext {
    std::size_t next_temp = 0;

    std::string NewTemp();
};

// 所有 AST 的基类
class BaseAST {
    public:
    virtual ~BaseAST() = default;
    virtual void Dump() const = 0;
    void GenerateIR(std::ostream &out) const;

    // 表达式返回结果操作数（如 "6"、"%0"），语句等节点返回空字符串。
    // 类型节点返回类型名称（当前为 "i32"）。
    virtual std::string GenerateIR(std::ostream &out, IRContext &context) const = 0;
};

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
    public:
    using BaseAST::GenerateIR;
    // 用智能指针管理对象
    std::unique_ptr<BaseAST> func_def;

    void Dump() const override;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;

};

// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST {
    public:
    using BaseAST::GenerateIR;
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;

    void Dump() const override;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;

};

// FuncType
class FuncTypeAST : public BaseAST {
    public:
    using BaseAST::GenerateIR;

    void Dump() const override;

    std::string GenerateIR(std::ostream &, IRContext &) const override;

};

// Block
class BlockAST  : public BaseAST{
    public:
    using BaseAST::GenerateIR;
    std::unique_ptr<BaseAST> stmt;

    void Dump() const override;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;

};

class StmtAST : public BaseAST{
    public :
    using BaseAST::GenerateIR;
    std::unique_ptr<BaseAST> exp;

    void Dump() const override;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;

};

// 一元表达式：op 为 '+'、'-' 或 '!'，operand 可继续嵌套表达式。
class ExpAST : public BaseAST{
    public:
    using BaseAST::GenerateIR;
    char op ;
    std::unique_ptr<BaseAST> operand;

    void Dump() const override;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;
};

class NumAST : public BaseAST{
    public:
    using BaseAST::GenerateIR;
    int num;

    void Dump() const override;
    std::string GenerateIR(std::ostream &, IRContext &) const override;
};

class BinaryAST : public BaseAST{
    public:
    using BaseAST::GenerateIR;

    std::string op;
    std::unique_ptr<BaseAST> lhs;
    std::unique_ptr<BaseAST> rhs;

    void Dump()const override;

    std::string GenerateIR(std::ostream &out, IRContext &context) const override;
};
