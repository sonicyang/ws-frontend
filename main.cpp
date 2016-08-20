#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"

#include "llvm/IR/Verifier.h"

#include <cstdlib>
#include <iostream>

enum {POS, NEG};

char* code;
char* token;
llvm::LLVMContext context;
llvm::Module* module;
llvm::IRBuilder<>* builder;
llvm::Function* mainFunction;
llvm::GlobalVariable* stack;
llvm::Value* stackIndex;

void init(){
    // Create Module
    module = new llvm::Module("MainModule", context);
    // Create main function
    mainFunction = llvm::cast<llvm::Function>(
            module->getOrInsertFunction(
                "main", llvm::Type::getVoidTy(context), nullptr));
    // Build first basic block in main function
    auto bb = llvm::BasicBlock::Create(context, "Entry", mainFunction);
    builder = new llvm::IRBuilder<>(bb);

    // Create global stack
    auto arrayType = llvm::ArrayType::get(builder->getInt16Ty(), 200);
    auto constArray = llvm::ConstantAggregateZero::get(arrayType);
    stack = new llvm::GlobalVariable(
            *module,
            arrayType,
            false,
            llvm::GlobalValue::CommonLinkage,
            0,
            "ProgramStack");
    stack->setAlignment(16);
    stack->setInitializer(constArray);

    // Create stack index
    stackIndex = builder->CreateAlloca(
            builder->getInt16Ty(),
            nullptr,
            "StackPointer");
    builder->CreateStore(builder->getInt16(-1), stackIndex);
}

void finish() {
    auto bb = llvm::BasicBlock::Create(context, "mainReturn", mainFunction);
    llvm::ReturnInst::Create(context, bb);
    builder->CreateBr(bb);
    module->dump();
}

void popStack() {
    builder->CreateStore(
            builder->CreateSub(
                builder->CreateLoad(stackIndex),
                builder->getInt16(1)),
            stackIndex);
}

void pushStack(llvm::Value* v) {
    builder->CreateStore(
            builder->CreateAdd(
                builder->CreateLoad(stackIndex),
                builder->getInt16(1)),
            stackIndex);
    llvm::Value* addr = builder->CreateGEP(
            stack,
            {builder->getInt16(1), builder->CreateLoad(stackIndex)});
    builder->CreateStore(v, addr);
}

llvm::Value* topStack() {
    llvm::Value* addr = builder->CreateGEP(
            stack,
            {builder->getInt16(1), builder->CreateLoad(stackIndex)});
    return builder->CreateLoad(addr);
}

llvm::Value* intToLLVMValue(int v) {
    return builder->getInt16(v);
}

void putChar(llvm::Value* v)
{
    // Trunc is the output value. So it should be 8 bits.
    auto trunc = builder->CreateTrunc(v, builder->getInt8Ty());

    std::vector<llvm::Type*> inputTypes(2, llvm::Type::getInt8Ty(context));
    std::vector<llvm::Value*> inputs(2);
    inputs[0] = llvm::ConstantInt::get(context, llvm::APInt(8, 2, false));
    inputs[1] = trunc;

    auto functionType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context),
            inputTypes,
            false);
    auto inlineasm = llvm::InlineAsm::get(
            functionType,
            "call 5h", "{C}, {E}", true, false,
            llvm::InlineAsm::AD_ATT);
    auto call = builder->CreateCall(inlineasm, inputs);
    call->setTailCall(false);
}

void putInt(llvm::Value* v) {
    llvm::Value* intToChar = builder->CreateAdd(
            v,
            builder->getInt16('0'));
    putChar(intToChar);
}

llvm::Value* getChar()
{
    auto input = llvm::ConstantInt::get(
            context,
            llvm::APInt(32, 1, false));

    auto functionType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context),
            llvm::Type::getInt32Ty(context),
            false);
    auto inlineasm = llvm::InlineAsm::get(
            functionType,
            "call 5h", "={A}, {C}", true, false,
            llvm::InlineAsm::AD_ATT);

    auto call = builder->CreateCall(inlineasm, input);
    call->setTailCall(false);
    return builder->CreateSExt(call, builder->getInt16Ty());
}

llvm::Value* getInt() {
    llvm::Value* charToInt = builder->CreateSub(
            getChar(),
            builder->getInt16('0'));
    return charToInt;
}

void strip(){
    char* head;
    char* tail;
    tail = head = code;
    while(*head != '\0'){
        if(*head == ' ' || *head == '\t' || *head == '\n'){
            *tail++ = *head;
        }
        head++;
    }
    *tail = 0;
    return;
}

//Get Parameter value, using token pointed position as start
short get_parameter(){
    short value = 0;
    int pos_neg;

    switch(*token){
        case ' ':
            pos_neg = POS;
            break;
        case '\t':
            pos_neg = NEG;
            break;
        case '\n':
            perror("Invalid Parameter Sign Format");
            break;
        default:
            break;
    }

    while(*token != '\n'){
        switch(*token){
            case ' ':
                value <<= 1;
                break;
            case '\t':
                value <<= 1;
                value |= 1;
                break;
            default:
                break;
        }

        token++;
    }

    if(pos_neg == NEG){
        value = -value;
    }

    return value;
}

int exe_io(){
    llvm::Value* v;
    token++;
    switch(*token){
        case ' ':
            token++;
            switch(*token){
                case ' ':
                    v = topStack();
                    popStack();
                    putChar(v);
                    break;
                case '\t':
                    v = topStack();
                    popStack();
                    putInt(v);
                    break;
                case '\n':
                    perror("Wrong I/O command after 1 Space");
                    break;
            }
            break;
        case '\t':
            token++;
            switch(*token){
                case ' ':
                    pushStack(getChar());
                    break;
                case '\t':
                    pushStack(getInt());
                    break;
                case '\n':
                    perror("Wrong I/O command after 1 Tab");
                    break;
            }
            break;
        case '\n':
            token++;
            if(*token == '\n'){
                return 1;
            }else{
                perror("Wrong flow command after 1 LF");
            }
            break;
    }

    return 0;
}

int exe_flow(){
    short value;

    token++;
    switch(*token){
        case ' ':
            token++;
            switch(*token){
                case ' ':
                    // TODO
                    token++;
                    //value = get_parameter();
                    //symbols[value] = token;
                    break;
                case '\t':
                    // TODO
                    token++;
                    //value = get_parameter();
                    //flow_stack.push_back(token);
                    //token = symbols[value];
                    break;
                case '\n':
                    // TODO
                    token++;
                    //value = get_parameter();
                    //token = symbols[value];
                    break;
            }
            break;
        case '\t':
            token++;
            switch(*token){
                case ' ':
                    // TODO
                    token++;
                    //value = get_parameter();
                    //if(stack.back() == 0){
                        //token = symbols[value];
                    //}
                    break;
                case '\t':
                    // TODO
                    token++;
                    //value = get_parameter();
                    //if(stack.back() < 0){
                        //token = symbols[value];
                    //}
                    break;
                case '\n':
                    // TODO
                    //token = flow_stack.back();
                    //flow_stack.pop_back();
                    break;
            }
            break;
        case '\n':
            token++;
            if(*token == '\n'){
                return 1;
            }else{
                perror("Wrong flow command after 1 LF;");
            }
            break;
    }

    return 0;
}

int exe_heap(){
    short address, value;

    token++;
    if(*token == ' '){
        // TODO
        //value = stack.back();
        //stack.pop_back();
        //address = stack.back();
        //stack.pop_back();

        //heap[address] = value;
    }else if(*token == '\t'){
        // TODO
        //address = stack.back();
        //stack.pop_back();
        //stack.push_back(heap[address]);
    }else{
        perror("Wrong Heap command");
    }

    return 0;
}

int exe_arithmetic(){
    llvm::Value* result;
    auto rhs = topStack();
    popStack();
    auto lhs = topStack();
    popStack();

    token++;
    if(*token == ' '){
        token++;
        switch(*token){
            case ' ':
                    result = builder->CreateAdd(lhs, rhs, "AddStatement");
                break;
            case '\t':
                    result = builder->CreateSub(lhs, rhs, "SubStatement");
                break;
            case '\n':
                    result = builder->CreateMul(lhs, rhs, "MulStatement");
                break;
        }
    }else if(*token == '\t'){
        token++;
        switch(*token){
            case ' ':
                    result = builder->CreateExactSDiv(lhs, rhs, "DivStatement");
                break;
            case '\t':
                    result = builder->CreateSRem(lhs, rhs, "ModStatement");
                break;
            case '\n':
                perror("Wrong Arithmetic command after 1 TAB");
                break;
        }
    }else{
        perror("Wrong Arithmetic command");
    }

    pushStack(result);

    return 0;
}

int exe_stack(){
    int state = 0;
    short value;

    do{
        token++;
        switch(*token){
            case ' ':
                if(!state){
                    token++;
                    value = get_parameter();
                    pushStack(intToLLVMValue(value));
                }else{
                    pushStack(topStack());
                }
                break;
            case '\t':
                if(!state){
                    perror("Invalid Tab command for Stack IMP");
                }else{
                    llvm::Value* tmp1 = topStack();
                    popStack();
                    llvm::Value* tmp2 = topStack();
                    popStack();
                    pushStack(tmp1);
                    pushStack(tmp2);
                }
                break;
            case '\n':
                if(!state){
                    state = 1;
                }else{
                    popStack();
                    state = 0;
                }
                break;
        }
    }while(state == 1);

    return 0;
}

//Scan the input tokens decode imp.
//token pointer will stay at eh end of the imp entering exe_ functions;
int exe_imps(){
    int state = 1;

    while(*token != '\0'){
        switch(*token){
            case ' ':
                state ? exe_stack() : exe_arithmetic();
                state = 1;
                break;
            case '\t':
                state = state ? 0 : 1;

                if(state)
                    exe_heap();
                break;
            case '\n':
                state ? exe_flow() : exe_io();
                state = 1;
                break;
            default:
                break;
        }

        token++;
    }

    return 0;
}

int main(int argc, char* argv[]){
    if (argc < 2){
        std::cout << "Usage: ./wsc input.ws > output.ll 2>&1" << std::endl;
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    int fsize;
    if (f == NULL) {
        std::cout << "Cannot open file\n";
        return 1;
    }

    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    code = (char*)malloc(fsize + 1);
    fread(code, fsize, 1, f);
    code[fsize] = 0;

    fclose(f);

    strip();

    token = code;
    init();
    exe_imps();
    finish();

    return 0;
}
