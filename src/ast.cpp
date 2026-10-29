#include "ast.h"

#include <iostream>
#include <stdexcept>

std::string IRContext::NewTemp() {
    return "%" + std::to_string(next_temp++);
}

void BaseAST::GenerateIR(std::ostream &out) const {
    IRContext context;
    GenerateIR(out, context);
}

void CompUnitAST::Dump() const {
    std::cout << "CompUnitAST { ";
    func_def->Dump();
    std::cout << " }";
}

std::string CompUnitAST::GenerateIR(std::ostream &out, IRContext &context) const {
    return func_def->GenerateIR(out, context);
}

void FuncDefAST::Dump() const {
    std::cout << "FuncDefAST { ";
    func_type->Dump();
    std::cout << ", " << ident << ", ";
    block->Dump();
    std::cout << " }";
}

std::string FuncDefAST::GenerateIR(std::ostream &out, IRContext &context) const {
    context.next_temp = 0;
    const auto type = func_type->GenerateIR(out, context);
    out << "fun @" << ident << "(): " << type << " {\n";

    block->GenerateIR(out, context);

    out << "}\n";
    return "";
}

void FuncTypeAST::Dump() const {
    std::cout << "FuncTypeAST {int} ";
}

std::string FuncTypeAST::GenerateIR(std::ostream &, IRContext &) const {
    return "i32";
}

void BlockAST::Dump() const {
    std::cout << "BlockAST { ";
    stmt -> Dump();
    std::cout<< "}";
}

std::string BlockAST::GenerateIR(std::ostream &out, IRContext &context) const {
    out << "%entry:\n";
    stmt->GenerateIR(out, context);
    return "";
}

void StmtAST::Dump() const {
    std::cout << "StmtAST { ";
    exp -> Dump();
    std::cout <<"}";
}

std::string StmtAST::GenerateIR(std::ostream &out, IRContext &context) const {
    const auto value = exp->GenerateIR(out, context);
    out << "  ret " << value << "\n";
    return "";
}

void ExpAST::Dump() const {
    std::cout << "ExpAST { " << op << ", ";
    operand -> Dump();
    std::cout << " }";
}

std::string ExpAST::GenerateIR(std::ostream &out, IRContext &context) const {
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

void NumAST::Dump() const {std::cout << "NumAST {"<< num <<"}";}

std::string NumAST::GenerateIR(std::ostream &, IRContext &) const {
    return std::to_string(num);
}

void BinaryAST::Dump() const {
    std::cout << "BinaryExpAST { " << op << ", ";
    lhs->Dump();
    std::cout << ", ";
    rhs->Dump();
    std::cout << " }";
}

std::string BinaryAST::GenerateIR(std::ostream &out, IRContext &context) const {
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
