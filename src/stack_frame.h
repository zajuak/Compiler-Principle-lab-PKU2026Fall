#pragma once

#include <unordered_map>
#include <ostream>
#include <string>
#include "koopa.h"

//修改栈指针
void EmitStackAdjust(int delta, std::ostream &out);
//load
void EmitStackLoad(const std::string &reg, int offset, std::ostream &out);
//store
void EmitStackStore(const std::string &reg, int offset, std::ostream &out);

class StackFrame {
public:
    // 扫描函数，安排各个对象的栈位置。
    void Analyze(const koopa_raw_function_t &func);

    // 查询对象相对于 sp 的偏移量。
    int GetOffset(koopa_raw_value_t value) const;

    // 获取对齐后的栈帧大小。
    int GetSize() const;

private:
    std::unordered_map<koopa_raw_value_t, int> offsets;
    int stack_size = 0;
};
