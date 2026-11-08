#include "stack_frame.h"

#include <cstddef>

namespace {

bool FitsImm12(int value) {
    return value >= -2048 && value <= 2047;
}

}

void EmitStackAdjust(int delta, std::ostream &out) {
    if (delta == 0) {
        return;
    }
    if (FitsImm12(delta)) {
        out << "  addi sp, sp, " << delta << '\n';
    } else {
        out << "  li t6, " << delta << '\n';
        out << "  add sp, sp, t6\n";
    }
}

void EmitStackLoad(const std::string &reg, int offset, std::ostream &out) {
    if (FitsImm12(offset)) {
        out << "  lw " << reg << ", " << offset << "(sp)\n";
    } else {
        out << "  li t6, " << offset << '\n';
        out << "  add t6, sp, t6\n";
        out << "  lw " << reg << ", 0(t6)\n";
    }
}

void EmitStackStore(const std::string &reg, int offset, std::ostream &out) {
    if (FitsImm12(offset)) {
        out << "  sw " << reg << ", " << offset << "(sp)\n";
    } else {
        out << "  li t6, " << offset << '\n';
        out << "  add t6, sp, t6\n";
        out << "  sw " << reg << ", 0(t6)\n";
    }
}

void StackFrame::Analyze(const koopa_raw_function_t &func) {
    offsets.clear();
    stack_size = 0;
    int next_offset = 0;

    for (std::size_t i = 0; i < func->bbs.len; ++i) {
        auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
        for (std::size_t j = 0; j < bb->insts.len; ++j) {
            auto value = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);
            // store、return 等没有结果，不分配位置。
            if (value->ty->tag == KOOPA_RTT_UNIT) {
                continue;
            }
            offsets.emplace(value, next_offset);
            next_offset += 4;
        }
    }
    // 向上对齐到 16 字节。
    stack_size = (next_offset + 15) / 16 * 16;
}

int StackFrame::GetOffset(koopa_raw_value_t value) const {
    return offsets.at(value);
}

int StackFrame::GetSize() const {
    return stack_size;
}
