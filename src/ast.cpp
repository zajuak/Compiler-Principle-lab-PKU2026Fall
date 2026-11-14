#include "ast.h"

#include <cstdint>
#include <stdexcept>

std::string IRContext::NewTemp() {
    return "%" + std::to_string(next_temp++);
}
void IRContext::EnterScope() {
    scopes.emplace_back();
}
void IRContext::ExitScope() {
    scopes.pop_back();
}
void IRContext::Declare(const std::string &name, const Symbol &symbol) {
    auto &current = scopes.back();
    if (!current.emplace(name, symbol).second) {
        throw std::runtime_error("Duplicate definition: " + name);
    }
}
const Symbol &IRContext::Lookup(const std::string &name) const {
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        const auto it = scope->find(name);
        if (it != scope->end()) {
            return it->second;
        }
    }
    throw std::runtime_error("Undefined identifier: " + name);
}

void BaseAST::GenerateIR(std::ostream &out) const {
    IRContext context;
    GenerateIR(out, context);
}
int BaseAST::Calculate(const IRContext &) const {
    return 0;
}

std::string CompUnitAST::GenerateIR(std::ostream &out, IRContext &context) const {
    return func_def->GenerateIR(out, context);
}

std::string FuncDefAST::GenerateIR(std::ostream &out, IRContext &context) const {
    context = IRContext{};
    const auto type = func_type->GenerateIR(out, context);
    out << "fun @" << ident << "(): " << type << " {\n";
    out << "%entry:\n";    
    block->GenerateIR(out, context);

    out << "}\n";
    return "";
}

std::string FuncTypeAST::GenerateIR(std::ostream &, IRContext &) const {
    return "i32";
}

std::string BlockAST::GenerateIR(std::ostream &out, IRContext &context) const {
    context.EnterScope();
    for (const auto &item : items){
        if (context.terminated) break;
        item->GenerateIR(out, context);
    }
    context.ExitScope();
    return "";
}

std::string StmtAST::GenerateIR(std::ostream &out, IRContext &context) const {
    const auto value = exp->GenerateIR(out, context);
    out << "  ret " << value << "\n";
    context.terminated = true;
    return "";
}

std::string ExprStmtAST::GenerateIR(std::ostream &out, IRContext &context) const {
    if (exp != nullptr) {
        exp->GenerateIR(out, context);
    }
    return "";
}

std::string VarDeclAST::GenerateIR(std::ostream &out, IRContext &context) const {
    for (const auto &def : defs) {
        def->GenerateIR(out, context);
    }
    return "";
}

std::string VarDefAST::GenerateIR(std::ostream &out, IRContext &context) const {
    // 为变量的存储位置生成唯一的 IR 名称。
    const auto address = context.NewTemp();
    out << "  " << address << " = alloc i32\n";

    // 将变量名与存储地址关联。
    context.Declare(ident, Symbol{Symbol::Kind::Variable, 0, address});

    // 有初始化表达式时，计算并写入。
    if (init_val != nullptr) {
        const auto value = init_val->GenerateIR(out, context);
        out << "  store " << value << ", " << address << '\n';
    }

    return "";
}

std::string ConstDeclAST::GenerateIR(std::ostream &out, IRContext &context) const {
    for (const auto &def : defs) {
        def->GenerateIR(out, context);
    }
    return "";
}

std::string ConstDefAST::GenerateIR(std::ostream &, IRContext &context) const {
    const auto value = init_val->Calculate(context);
    context.Declare(ident, Symbol{Symbol::Kind::Constant, value, ""});
    return "";
}

std::string LValAST::GenerateIR(std::ostream &out, IRContext &context) const {
    const auto &symbol = context.Lookup(ident);
    if (symbol.kind == Symbol::Kind::Constant) {
        return std::to_string(symbol.value);
    }
    const auto result = context.NewTemp();
    out << "  " << result << " = load " << symbol.address << '\n';
    return result;
}
int LValAST::Calculate(const IRContext &context) const {
    const auto &it = context.Lookup(ident);
    return it.value;
}
std::string LValAST::GetAddress(const IRContext &context) const {
    const auto &symbol = context.Lookup(ident);
    return symbol.address;
}

std::string AssignStmtAST::GenerateIR(std::ostream &out, IRContext &context) const {
    const auto address = lval->GetAddress(context);
    const auto value = exp->GenerateIR(out, context);
    out << "  store " << value << ", " << address << '\n';
    return "";
}

std::string NumAST::GenerateIR(std::ostream &, IRContext &) const {
    return std::to_string(num);
}
int NumAST::Calculate(const IRContext &) const {
    return num;
}

std::string ExpAST::GenerateIR(std::ostream &out, IRContext &context) const {
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
            return "";
    }
    return result;
}

int ExpAST::Calculate(const IRContext &context) const {
    const auto value = operand->Calculate(context);
    switch (op) {
        case '+':
            return value;
        case '-':
            return static_cast<int>(-static_cast<std::int64_t>(value));
        case '!':
            return value == 0;
        default:
            return 0;
    }
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
        return "";
    }

    const auto result = context.NewTemp();
    out << "  " << result << " = " << instruction<< " " << left << ", " << right << '\n';

    return result;
}
int BinaryAST::Calculate(const IRContext &context) const {
    const std::int64_t left = lhs->Calculate(context);

    if (op == "&&") {
        return left != 0 && rhs->Calculate(context) != 0;
    }
    if (op == "||") {
        return left != 0 || rhs->Calculate(context) != 0;
    }

    const std::int64_t right = rhs->Calculate(context);

    if (op == "+") {
        return static_cast<int>(left + right);
    }
    if (op == "-") {
        return static_cast<int>(left - right);
    }
    if (op == "*") {
        return static_cast<int>(left * right);
    }

    if (op == "/") {
        return static_cast<int>(left / right);
    }
    if (op == "%") {
        return static_cast<int>(left % right);
    }

    if (op == "<")  return left < right;
    if (op == ">")  return left > right;
    if (op == "<=") return left <= right;
    if (op == ">=") return left >= right;
    if (op == "==") return left == right;
    if (op == "!=") return left != right;

    return 0;
}
