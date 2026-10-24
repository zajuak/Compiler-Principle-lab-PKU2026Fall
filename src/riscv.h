#pragma once

#include <ostream>
#include "koopa.h"

// 遍历 raw program，后续在此基础上向 out 输出 RISC-V 汇编。
void GenerateRiscv(const koopa_raw_program_t &raw, std::ostream &out);
