#pragma once

#include <iostream>
#include <string>
#include <memory>

// 所有 AST 的基类
class BaseAST {
    public:
    virtual ~BaseAST() = default;
    virtual void Dump() const = 0; 
    virtual void GenerateIR(std::ostream &out) const = 0;
};

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
    public:
    // 用智能指针管理对象
    std::unique_ptr<BaseAST> func_def;

    void Dump() const override {
        std::cout << "CompUnitAST { ";
        func_def->Dump();
        std::cout << " }";
    }

    void GenerateIR(std::ostream &out) const override{
        func_def->GenerateIR(out);
    }
    
};

// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST {
    public:
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

    void GenerateIR(std::ostream &out) const override{
        out << "fun @" << ident << "(): ";
        func_type->GenerateIR(out);
        out << " {\n";

        block->GenerateIR(out);

        out << "}\n";        
    }
    
};

// FuncType
class FuncTypeAST : public BaseAST {

    void Dump() const override {
        std::cout << "FuncTypeAST {int} ";
    }

    void GenerateIR(std::ostream &out) const override{
        out << "i32";
    }
    
};

// Block
class BlockAST  : public BaseAST{
    public:
    std::unique_ptr<BaseAST> stmt;

    void Dump() const override {
        std::cout << "BlockAST { ";
        stmt -> Dump();
        std::cout<< "}";
    }

    void GenerateIR(std::ostream &out) const override{
        out << "%entry:\n";
        stmt->GenerateIR(out);
    }
    
};

class StmtAST : public BaseAST{
    public :
    int number;
    void Dump() const override {
        std::cout << "StmtAST {"<< number <<"}";
    }

    void GenerateIR(std::ostream &out) const override{
        out << "  ret " << number << "\n";       
    }
    
};
