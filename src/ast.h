#pragma once

#include <iostream>
#include <string>
#include <memory>
#include <cstddef>
#include <stdexcept>

// 同一次 IR 生成共用临时值编号；每次 GenerateIR 都会创建新的上下文。
struct IRContext {
    std::size_t next_temp = 0;

    std::string NewTemp() {
        return "%" + std::to_string(next_temp++);
    }
};

// 所有 AST 的基类
class BaseAST {
    public:
    virtual ~BaseAST() = default;
    virtual void Dump() const = 0; 
    void GenerateIR(std::ostream &out) const {
        IRContext context;
        GenerateIR(out, context);
    }

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

    void Dump() const override {
        std::cout << "CompUnitAST { ";
        func_def->Dump();
        std::cout << " }";
    }

    std::string GenerateIR(std::ostream &out, IRContext &context) const override {
        return func_def->GenerateIR(out, context);
    }
    
};

// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST {
    public:
    using BaseAST::GenerateIR;
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;

    void Dump() const override {
        std::cout << "FuncDefAST { ";
        func_type->Dump();
        std::cout << ", " << ident << ", ";
        block->Dump();
        std::cout << " }";
    }

    std::string GenerateIR(std::ostream &out, IRContext &context) const override {
        context.next_temp = 0;
        const auto type = func_type->GenerateIR(out, context);
        out << "fun @" << ident << "(): " << type << " {\n";

        block->GenerateIR(out, context);

        out << "}\n";
        return "";
    }
    
};

// FuncType
class FuncTypeAST : public BaseAST {
    public:
    using BaseAST::GenerateIR;

    void Dump() const override {
        std::cout << "FuncTypeAST {int} ";
    }

    std::string GenerateIR(std::ostream &, IRContext &) const override {
        return "i32";
    }
    
};

// Block
class BlockAST  : public BaseAST{
    public:
    using BaseAST::GenerateIR;
    std::unique_ptr<BaseAST> stmt;

    void Dump() const override {
        std::cout << "BlockAST { ";
        stmt -> Dump();
        std::cout<< "}";
    }

    std::string GenerateIR(std::ostream &out, IRContext &context) const override {
        out << "%entry:\n";
        stmt->GenerateIR(out, context);
        return "";
    }
    
};

class StmtAST : public BaseAST{
    public :
    using BaseAST::GenerateIR;
    std::unique_ptr<BaseAST> exp;

    void Dump() const override {
        std::cout << "StmtAST { ";
        exp -> Dump();
        std::cout <<"}";
    }

    std::string GenerateIR(std::ostream &out, IRContext &context) const override {
        const auto value = exp->GenerateIR(out, context);
        out << "  ret " << value << "\n";
        return "";
    }
    
};

// 一元表达式：op 为 '+'、'-' 或 '!'，operand 可继续嵌套表达式。
class ExpAST : public BaseAST{
    public:
    using BaseAST::GenerateIR;
    char op ;
    std::unique_ptr<BaseAST> operand;

    void Dump() const override {
        std::cout << "ExpAST { " << op << ", ";
        operand -> Dump();
        std::cout << " }";
    }

    std::string GenerateIR(std::ostream &out, IRContext &context) const override {
        // 先生成子表达式，再使用其返回的立即数或临时值。
        const auto value = operand->GenerateIR(out, context);
        if (op == '+') {
            return value;
        }

        const auto result = context.NewTemp();
        switch (op) {
            case '-':
                out << "  " << result << " = sub 0, " << value << '\n';
                break;
            case '!':
                out << "  " << result << " = eq " << value << ", 0\n";
                break;
            default:
                throw std::logic_error("Unsupported unary operator");
        }
        return result;
    }
};

class NumAST : public BaseAST{
    public:
    using BaseAST::GenerateIR;
    int num;

    void Dump() const override {std::cout << "NumAST {"<< num <<"}";}
    std::string GenerateIR(std::ostream &, IRContext &) const override {
        return std::to_string(num);
    }
};

class BinaryAST : public BaseAST{
    public:
    using BaseAST::GenerateIR;

    std::string op;
    std::unique_ptr<BaseAST> lhs;
    std::unique_ptr<BaseAST> rhs;

    void Dump()const override {
        std::cout << "BinaryExpAST { " << op << ", ";
        lhs->Dump();
        std::cout << ", ";
        rhs->Dump();
        std::cout << " }";
    }

    std::string GenerateIR(std::ostream &out, IRContext &context) const override{
        const auto left = lhs ->GenerateIR(out,context);
        const auto right = rhs ->GenerateIR(out,context);

        if (op == "&&" || op == "||") {
            const auto left_bool = context.NewTemp();
            out << "  " << left_bool << " = ne " << left << ", 0\n";
            const auto right_bool = context.NewTemp();
            out << "  " << right_bool << " = ne " << right << ", 0\n";

            const auto result = context.NewTemp();
            const char *instruction = op == "&&" ? "and" : "or";
            out << "  " << result << " = " << instruction << " " << left_bool << ", " << right_bool << '\n';
            return result;
        }

        const char *instruction;
        if (op == "+") {
            instruction = "add";
        } else if (op == "-") {
            instruction = "sub";
        } else if (op == "*") {
            instruction = "mul";
        } else if (op == "/") {
            instruction = "div";
        } else if (op == "%") {
            instruction = "mod";
        } else if (op == "<") {
            instruction = "lt";
        } else if (op == ">") {
            instruction = "gt";
        } else if (op == "<=") {
            instruction = "le";
        } else if (op == ">=") {
            instruction = "ge";
        } else if (op == "==") {
            instruction = "eq";
        } else if (op == "!=") {
            instruction = "ne";
        } else {
            throw std::logic_error("Unsupported binary operator: " + op);
        }

        const auto result = context.NewTemp();
        out << "  " << result << " = " << instruction<< " " << left << ", " << right << '\n';

        return result;
    }
};
