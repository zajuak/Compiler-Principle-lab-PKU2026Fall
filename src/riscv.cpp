#include "riscv.h"

#include <cassert>

namespace {

// 提前声明所有重载，供下面的 Visit 函数相互调用。
void Visit(const koopa_raw_program_t &program, std::ostream &out);
void Visit(const koopa_raw_slice_t &slice, std::ostream &out);
void Visit(const koopa_raw_function_t &func, std::ostream &out);
void Visit(const koopa_raw_basic_block_t &bb, std::ostream &out);
void Visit(const koopa_raw_value_t &value, std::ostream &out);
void Visit(const koopa_raw_return_t &ret, std::ostream &out);
void Visit(const koopa_raw_integer_t &integer, std::ostream &out);

// 访问 raw program。
void Visit(const koopa_raw_program_t &program, std::ostream &out) {
    // 访问所有全局变量
    Visit(program.values, out);
    // 访问所有函数
    Visit(program.funcs, out);
}

// 访问 raw slice
void Visit(const koopa_raw_slice_t &slice, std::ostream &out) {
    for (size_t i = 0; i < slice.len; ++i) {
        auto ptr = slice.buffer[i];
        // 根据slice.kind进行分类处理
        switch (slice.kind) {
            case KOOPA_RSIK_FUNCTION:
                Visit(reinterpret_cast<koopa_raw_function_t>(ptr), out);
                break;
            case KOOPA_RSIK_BASIC_BLOCK:
                Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr), out);
                break;
            case KOOPA_RSIK_VALUE:
                Visit(reinterpret_cast<koopa_raw_value_t>(ptr), out);
                break;
            default:
                assert(false);
        }
    }
}

// 访问函数。
void Visit(const koopa_raw_function_t &func, std::ostream &out) {
    // 输出函数入口相关的汇编。
    if (func -> bbs.len == 0) return;
    const char *name = func -> name +1;
    
    out << "  .text \n";
    out << "  .globl " << name <<"\n";
    out << name << ":\n"; 
    Visit(func->bbs, out);
}

// 访问基本块。
void Visit(const koopa_raw_basic_block_t &bb, std::ostream &out) {
    Visit(bb->insts, out);
}

// 访问 Value，根据具体种类分派处理。
void Visit(const koopa_raw_value_t &value, std::ostream &out) {
    const auto &kind = value->kind;
    switch (kind.tag) {
        case KOOPA_RVT_RETURN:
            Visit(kind.data.ret, out);
            break;
        case KOOPA_RVT_INTEGER:
            Visit(kind.data.integer, out);
            break;
        default:
            assert(false);
    }
}

// 访问返回指令，继续访问其返回值（若存在）。
void Visit(const koopa_raw_return_t &ret, std::ostream &out) {
    if (ret.value != nullptr) {
        Visit(ret.value, out);
    }
    out << "  ret\n";
}

// 访问整数常量。
void Visit(const koopa_raw_integer_t &integer, std::ostream &out) {
    out << "  li a0, " << integer.value << '\n';
}

}  // namespace

void GenerateRiscv(const koopa_raw_program_t &raw, std::ostream &out) {
    Visit(raw, out);
}
