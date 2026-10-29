#include "riscv.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <array>
#include <stdexcept>
#include <vector>

namespace {

struct RiscvContext {
    // IR Value 的结果保存在哪个寄存器。
    std::unordered_map<koopa_raw_value_t, std::string> value_regs;

    // 下一个待分配寄存器在可用列表中的位置。
    std::size_t next_reg = 0;
};

std::string NewReg(RiscvContext &context) {
    static const std::array<const char *, 15> registers = {
        "t0", "t1", "t2", "t3", "t4", "t5", "t6",
        "a1", "a2", "a3", "a4", "a5", "a6", "a7", "a0"
    };

    if (context.next_reg >= registers.size()) {
        throw std::runtime_error("Register pool exhausted");
    }

    return registers[context.next_reg++];
}


std::string GetOperandReg(koopa_raw_value_t value, std::ostream &out, RiscvContext &context) {
    assert(value != nullptr);

    switch (value->kind.tag) {
        case KOOPA_RVT_INTEGER: {
            const auto number = value->kind.data.integer.value;

            if (number == 0) {
                return "zero";
            }
            const auto reg = NewReg(context);
            out << "  li " << reg << ", " << number << '\n';
            return reg;
        }

        case KOOPA_RVT_BINARY: {
            const auto it = context.value_regs.find(value);

            if (it == context.value_regs.end()) {
                throw std::logic_error(
                    "Operand result has not been generated");
            }

            return it->second;
        }

        default:
            throw std::logic_error("Unsupported operand");
    }
}

// 提前声明所有重载，供下面的 Visit 函数相互调用。
void Visit(const koopa_raw_program_t &program, std::ostream &out, RiscvContext &context);
void Visit(const koopa_raw_slice_t &slice, std::ostream &out, RiscvContext &context);
void Visit(const koopa_raw_function_t &func, std::ostream &out, RiscvContext &context);
void Visit(const koopa_raw_basic_block_t &bb, std::ostream &out, RiscvContext &context);
void Visit(const koopa_raw_value_t &value, std::ostream &out, RiscvContext &context);
void Visit(const koopa_raw_return_t &ret, std::ostream &out, RiscvContext &context);
void Visit(const koopa_raw_integer_t &integer, std::ostream &out, RiscvContext &context);
std::string Visit(const koopa_raw_binary_t &binary, std::ostream &out, RiscvContext &context);

// 访问 raw program。
void Visit(const koopa_raw_program_t &program, std::ostream &out, RiscvContext &context) {
    // 访问所有全局变量
    Visit(program.values, out, context);
    // 访问所有函数
    Visit(program.funcs, out, context);
}

// 访问 raw slice
void Visit(const koopa_raw_slice_t &slice, std::ostream &out, RiscvContext &context) {
    for (size_t i = 0; i < slice.len; ++i) {
        auto ptr = slice.buffer[i];
        // 根据slice.kind进行分类处理
        switch (slice.kind) {
            case KOOPA_RSIK_FUNCTION:
                Visit(reinterpret_cast<koopa_raw_function_t>(ptr), out, context);
                break;
            case KOOPA_RSIK_BASIC_BLOCK:
                Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr), out, context);
                break;
            case KOOPA_RSIK_VALUE:
                Visit(reinterpret_cast<koopa_raw_value_t>(ptr), out, context);
                break;
            default:
                assert(false);
        }
    }
}

// 访问函数。
void Visit(const koopa_raw_function_t &func, std::ostream &out, RiscvContext &context) {
    // 输出函数入口相关的汇编。
    if (func -> bbs.len == 0) return;

    context.value_regs.clear();
    context.next_reg = 0;

    const char *name = func -> name +1;
    
    out << "  .text \n";
    out << "  .globl " << name <<"\n";
    out << name << ":\n"; 
    Visit(func->bbs, out, context);
}

// 访问基本块。
void Visit(const koopa_raw_basic_block_t &bb, std::ostream &out, RiscvContext &context) {
    Visit(bb->insts, out, context);
}

// 访问 Value，根据具体种类分派处理。
void Visit(const koopa_raw_value_t &value, std::ostream &out, RiscvContext &context) {
    const auto &kind = value->kind;
    switch (kind.tag) {
        case KOOPA_RVT_RETURN:
            Visit(kind.data.ret, out, context);
            break;
        case KOOPA_RVT_INTEGER:
            Visit(kind.data.integer, out, context);
            break;
        case KOOPA_RVT_BINARY:
            context.value_regs[value] = Visit(kind.data.binary, out, context);
            break;
        default:
            assert(false);
    }
}

// 访问返回指令，继续访问其返回值（若存在）。
void Visit(const koopa_raw_return_t &ret, std::ostream &out, RiscvContext &context) {
    if (ret.value != nullptr) {
        if (ret.value->kind.tag == KOOPA_RVT_INTEGER) {
            out << "  li a0, " << ret.value->kind.data.integer.value << '\n';
        } 
        else {
            const auto reg = GetOperandReg(ret.value, out, context);
            out << "  mv a0, " << reg << '\n';
        }
    }
    out << "  ret\n";
}

// 访问整数常量。
void Visit(const koopa_raw_integer_t &integer, std::ostream &out, RiscvContext &context) {
    out << "  li a0, " << integer.value << '\n';
}

// 访问二元运算结果
std::string Visit(const koopa_raw_binary_t &binary, std::ostream &out, RiscvContext &context){
    const char *instruction;

    switch (binary.op) {
        case KOOPA_RBO_ADD:
            instruction = "add";
            break;
        case KOOPA_RBO_SUB:
            instruction = "sub";
            break;
        case KOOPA_RBO_MUL:
            instruction = "mul";
            break;
        case KOOPA_RBO_DIV:
            instruction = "div";
            break;
        case KOOPA_RBO_MOD:
            instruction = "rem";
            break;
        case KOOPA_RBO_AND:
            instruction = "and";
            break;
        case KOOPA_RBO_OR:
            instruction = "or";
            break;
        // 相等、不等：先计算两个操作数的异或。
        case KOOPA_RBO_EQ:
        case KOOPA_RBO_NOT_EQ:
            instruction = "xor";
            break;
        // 小于直接使用 slt；大于等于先计算“小于”，之后取反。
        case KOOPA_RBO_LT:
        case KOOPA_RBO_GE:
            instruction = "slt";
            break;
        // 大于使用 sgt；小于等于先计算“大于”，之后取反。
        case KOOPA_RBO_GT:
        case KOOPA_RBO_LE:
            instruction = "sgt";
            break;            
        default:
            throw std::logic_error(
                "Unsupported binary operation");
    }    

    const auto lhs_reg = GetOperandReg(binary.lhs, out, context);
    const auto rhs_reg = GetOperandReg(binary.rhs, out, context);
    std::string result_reg;

    if (lhs_reg != "zero")      result_reg = lhs_reg;
    else if (rhs_reg != "zero") result_reg = rhs_reg;
    else                        result_reg = NewReg(context);
    
    out << "  " << instruction << " " << result_reg << ", " << lhs_reg << ", " << rhs_reg << '\n';
    if (binary.op == KOOPA_RBO_EQ || binary.op == KOOPA_RBO_GE || binary.op == KOOPA_RBO_LE) {
        out << "  seqz " << result_reg << ", " << result_reg << '\n';
    } 
    else if (binary.op == KOOPA_RBO_NOT_EQ) {
        out << "  snez " << result_reg << ", " << result_reg << '\n';
    }

    return result_reg;
}

}  // namespace

void GenerateRiscv(const koopa_raw_program_t &raw, std::ostream &out) {
    RiscvContext context;
    Visit(raw, out, context);
}
