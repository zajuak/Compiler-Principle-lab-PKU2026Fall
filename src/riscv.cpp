#include "riscv.h"
#include "stack_frame.h"

#include <cstddef>
#include <string>
#include <stdexcept>

namespace {

struct RiscvContext {
    StackFrame stack;
};

//读取操作数
void LoadValue(koopa_raw_value_t value, const std::string &reg, std::ostream &out, RiscvContext &context) {
    switch (value->kind.tag) {
        case KOOPA_RVT_INTEGER:
            out << "  li " << reg << ", " << value->kind.data.integer.value << '\n';
            break;

        case KOOPA_RVT_BINARY:
        case KOOPA_RVT_LOAD:
            EmitStackLoad(reg, context.stack.GetOffset(value), out);
            break;
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
void Visit(const koopa_raw_binary_t &binary, std::ostream &out, RiscvContext &context);
void Visit(const koopa_raw_load_t &load, std::ostream &out, RiscvContext &context);
void Visit(const koopa_raw_store_t &store, std::ostream &out, RiscvContext &context);

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
                break;
        }
    }
}

// 访问函数。
void Visit(const koopa_raw_function_t &func, std::ostream &out, RiscvContext &context) {
    // 输出函数入口相关的汇编。
    if (func -> bbs.len == 0) return;

    context = RiscvContext{};
    context.stack.Analyze(func);
    const char *name = func -> name +1;
    
    out << "  .text \n";
    out << "  .globl " << name <<"\n";
    out << name << ":\n"; 
    EmitStackAdjust(-context.stack.GetSize(), out);
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
        case KOOPA_RVT_ALLOC:
            // Analyze 已经为对象安排了空间。
            break;
        case KOOPA_RVT_LOAD:
            Visit(kind.data.load, out, context);
            EmitStackStore("t0", context.stack.GetOffset(value), out);
            break;
        case KOOPA_RVT_STORE:
            Visit(kind.data.store, out, context);
            break;
        case KOOPA_RVT_RETURN:
            Visit(kind.data.ret, out, context);
            break;
        case KOOPA_RVT_BINARY:
            Visit(kind.data.binary, out, context);
            EmitStackStore("t0", context.stack.GetOffset(value), out);
            break;
        default:
            break;
    }
}

// 访问返回指令
void Visit(const koopa_raw_return_t &ret, std::ostream &out, RiscvContext &context) {
    if (ret.value != nullptr) {
        LoadValue(ret.value, "a0", out, context);
    }
    EmitStackAdjust(context.stack.GetSize(), out);
    out << "  ret\n";
}

// 访问整数常量。
void Visit(const koopa_raw_integer_t &integer, std::ostream &out, RiscvContext &context) {
    out << "  li a0, " << integer.value << '\n';
}

// 访问二元运算结果
void Visit(const koopa_raw_binary_t &binary, std::ostream &out, RiscvContext &context){
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
    }    

    LoadValue(binary.lhs, "t0", out, context);
    LoadValue(binary.rhs, "t1", out, context);

    out << "  " << instruction << " t0, t0, t1\n";

    if (binary.op == KOOPA_RBO_EQ ||
        binary.op == KOOPA_RBO_GE ||
        binary.op == KOOPA_RBO_LE) {
        out << "  seqz t0, t0\n";
    } else if (binary.op == KOOPA_RBO_NOT_EQ) {
        out << "  snez t0, t0\n";
    }
}

void Visit(const koopa_raw_load_t &load, std::ostream &out, RiscvContext &context) {
    // 读取变量当前的值，暂存在 t0。
    EmitStackLoad(
        "t0", context.stack.GetOffset(load.src), out);
}

void Visit(const koopa_raw_store_t &store, std::ostream &out, RiscvContext &context) {
    // 读取要写入的值。
    LoadValue(store.value, "t0", out, context);
    // 写入目标变量的位置。
    EmitStackStore("t0", context.stack.GetOffset(store.dest), out);
}

}  // namespace

void GenerateRiscv(const koopa_raw_program_t &raw, std::ostream &out) {
    RiscvContext context;
    Visit(raw, out, context);
}
