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
std::string IRContext::NewLabel(const std::string &prefix) {
    return "%" + prefix + "_" + std::to_string(next_label++);
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
    if (!context.terminated) {
        out << "  ret 0\n";
        context.terminated = true;
    }
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
    if (op == "&&" || op == "||") {
        const bool is_and = (op == "&&");

        // 为整个逻辑表达式的结果安排临时存储。
        const auto result_address = context.NewTemp();
        out << "  " << result_address << " = alloc i32\n";

        // && 的默认结果为 0，|| 的默认结果为 1。
        out << "  store " << (is_and ? 0 : 1) << ", " << result_address << '\n';

        const auto left = lhs->GenerateIR(out, context);

        const auto rhs_label = context.NewLabel("logic_rhs");
        const auto end_label = context.NewLabel("logic_end");

        if (is_and) {
            // 左侧为真才需要计算右侧。
            out << "  br " << left << ", " << rhs_label << ", " << end_label << '\n';
        } else {
            // 左侧为假才需要计算右侧。
            out << "  br " << left << ", " << end_label << ", " << rhs_label << '\n';
        }
        context.terminated = true;

        // 右侧表达式的指令放在这个基本块中。
        out << '\n' << rhs_label << ":\n";
        context.terminated = false;

        const auto right = rhs->GenerateIR(out, context);

        // 将右侧结果规范化为 0 或 1。
        const auto right_bool = context.NewTemp();
        out << "  " << right_bool << " = ne " << right << ", 0\n";
        out << "  store " << right_bool << ", " << result_address << '\n';
        out << "  jump " << end_label << '\n';
        context.terminated = true;

        // 两条路径在这里汇合，读取最终结果。
        out << '\n' << end_label << ":\n";
        context.terminated = false;

        const auto result = context.NewTemp();
        out << "  " << result << " = load " << result_address << '\n';
        return result;
    }

    const auto left = lhs ->GenerateIR(out,context);
    const auto right = rhs ->GenerateIR(out,context);

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

std::string IfStmtAST::GenerateIR(std::ostream &out, IRContext &context) const {
    const auto cond = condition->GenerateIR(out, context);

    const auto then_label = context.NewLabel("then");
    const auto else_label = context.NewLabel("else");
    const auto end_label = context.NewLabel("end");

    // 没有 else 时，条件不成立就直接进入汇合块。
    out << "  br " << cond << ", " << then_label << ", " << (else_stmt ? else_label : end_label)<< '\n';
    context.terminated = true;

    // 没有 else 时，假分支一定需要汇合块。
    bool need_end = (else_stmt == nullptr);

    // 生成成立分支。
    out << '\n' << then_label << ":\n";
    context.terminated = false;
    then_stmt->GenerateIR(out, context);

    if (!context.terminated) {
        out << "  jump " << end_label << '\n';
        context.terminated = true;
        need_end = true;
    }

    // 生成不成立分支。
    if (else_stmt != nullptr) {
        out << '\n' << else_label << ":\n";
        context.terminated = false;
        else_stmt->GenerateIR(out, context);

        if (!context.terminated) {
            out << "  jump " << end_label << '\n';
            context.terminated = true;
            need_end = true;
        }
    }

    // 仅在确实存在后续路径时生成汇合块。
    if (need_end) {
        out << '\n' << end_label << ":\n";
        context.terminated = false;
    }

    return "";
}

std::string WhileStmtAST::GenerateIR(std::ostream &out, IRContext &context) const {
    const auto condition_label = context.NewLabel("while_cond");
    const auto body_label = context.NewLabel("while_body");
    const auto end_label = context.NewLabel("while_end");

    // 从当前块进入条件块。
    out << "  jump " << condition_label << '\n';
    context.terminated = true;

    // 每轮都从这里重新计算条件。
    out << '\n' << condition_label << ":\n";
    context.terminated = false;

    const auto cond = condition->GenerateIR(out, context);
    out << "  br " << cond << ", " << body_label << ", " << end_label << '\n';
    context.terminated = true;

    // 生成循环体，记录当前循环的跳转目标。
    out << '\n' << body_label << ":\n";
    context.terminated = false;

    context.loops.push_back({condition_label, end_label});
    body->GenerateIR(out, context);
    context.loops.pop_back();

    // 正常执行完循环体，回到条件块。
    if (!context.terminated) {
        out << "  jump " << condition_label << '\n';
        context.terminated = true;
    }

    // 循环结束后继续执行。
    out << '\n' << end_label << ":\n";
    context.terminated = false;

    return "";
}

std::string BreakStmtAST::GenerateIR(std::ostream &out, IRContext &context) const {
    if (context.loops.empty()) {
        throw std::runtime_error("break outside loop");
    }
    out << "  jump " << context.loops.back().break_target << '\n';
    context.terminated = true;
    return "";
}

std::string ContinueStmtAST::GenerateIR(std::ostream &out, IRContext &context) const {
    if (context.loops.empty()) {
        throw std::runtime_error("continue outside loop");
    }
    out << "  jump " << context.loops.back().continue_target << '\n';
    context.terminated = true;
    return "";
}