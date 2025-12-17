/*
 * code_gen.cpp
 *
 * Written:            December 2025
 * Last modified:      December 2025
 *      Author: Jimi Adegoroye
 *
 * Implementation of the code generator for lille -> PAL translation.
 * 
 * This file contains all the functions that generate PAL assembly instructions.
 * Each function corresponds to a lille language construct.
 * 
 * IMPORTANT NOTES:
 * - PAL is a simple stack-based language
 * - Most operations pop operands from stack and push results
 * - We use labels for control flow (if, while, for)
 * - Comments in PAL code (starting with ';') help with debugging
 */

#include "code_gen.h"
#include "lille_exception.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace std;

// CONSTRUCTOR
code_gen::code_gen(const string& filename)
{
    output_filename = filename;
    instruction_count = 1;
    label_counter = 0;
    
    // I initialize level offsets for scope tracking
    level_offsets.push_back(0);  // Level 0
    level_offsets.push_back(0);  // Level 1 (program)
}

// DESTRUCTOR
code_gen::~code_gen()
{
    // I finalize and close the file if still open
    if (code_file.is_open()) {
        code_file.close();
    }
}

// emit() - Basic instruction emission
// I store the instruction in my vector for later output.
void code_gen::emit(const string& op, int l, int a, const string& comment)
{
    Instruction instr;
    instr.opcode = op;
    instr.operand1 = l;
    instr.operand2 = to_string(a);
    instr.comment = comment;
    instructions.push_back(instr);
    instruction_count++;
}

// emit() - Instruction with label reference
void code_gen::emit(const string& op, int l, const string& label, const string& comment)
{
    Instruction instr;
    instr.opcode = op;
    instr.operand1 = l;
    instr.operand2 = label;  // This will be resolved later
    instr.comment = comment;
    instructions.push_back(instr);
    instruction_count++;
}

// emit_string(): Instruction with string operand
void code_gen::emit_string(const string& op, int l, const string& str, const string& comment)
{
    Instruction instr;
    instr.opcode = op;
    instr.operand1 = l;
    instr.operand2 = "'" + str + "'";  // PAL uses single quotes for strings
    instr.comment = comment;
    instructions.push_back(instr);
    instruction_count++;
}

// emit_real(): Instruction with real operand
void code_gen::emit_real(const string& op, int l, float val, const string& comment)
{
    Instruction instr;
    instr.opcode = op;
    instr.operand1 = l;
    
    // I format the real number properly
    stringstream ss;
    ss << val;
    instr.operand2 = ss.str();
    instr.comment = comment;
    instructions.push_back(instr);
    instruction_count++;
}

// gen_new_label() - Generate unique label
string code_gen::gen_new_label()
{
    string label = "L" + to_string(label_counter++);
    return label;
}

// gen_label() - Place a label at current position
void code_gen::gen_label(const string& label)
{
    // I record where this label points to
    label_addresses[label] = instruction_count;
}

// PROGRAM STRUCTURE

void code_gen::gen_program_start(const string& name)
{
    // I generate the jump over built-in functions
    // Built-in functions occupy addresses 2-13:
    // - int2real (2-4)
    // - real2int (5-7) 
    // - int2string (8-10)
    // - real2string (11-13)
    
    emit("JMP", 0, 14, "Jump over the predefined functions.");
    
    // int2real function (addresses 2-4)
    emit("LDV", 0, 0, "Load argument.");
    emit("OPR", 0, 25, "Convert an integer to a real.");
    emit("OPR", 0, 1, "Function value return.");
    
    // real2int function (addresses 5-7)
    emit("LDV", 0, 0, "Load argument.");
    emit("OPR", 0, 26, "Convert a real to an integer.");
    emit("OPR", 0, 1, "Function value return.");
    
    // int2string function (addresses 8-10)
    emit("LDV", 0, 0, "Load argument.");
    emit("OPR", 0, 27, "Convert an integer to a string.");
    emit("OPR", 0, 1, "Function value return.");
    
    // real2string function (addresses 11-13)
    emit("LDV", 0, 0, "Load argument.");
    emit("OPR", 0, 28, "Convert a real to a string.");
    emit("OPR", 0, 1, "Function value return.");
}

void code_gen::gen_program_body_start()
{
    // I emit INC to reserve space for program-level variables
    int var_count = level_offsets.size() > 1 ? level_offsets[1] : 0;
    if (var_count > 0) {
        emit("INC", 0, var_count, "Reserve space for local variables");
    }
    
    // I generate a jump label for the start of statements
    string body_label = gen_new_label();
    emit("JMP", 0, body_label, "Jump to start of statements or block.");
    
    // The INC for declared variables comes here
    emit("INC", 0, var_count, "Reserve space for declared variables and constants.");
    
    gen_label(body_label);
}

void code_gen::gen_program_end()
{
    // Program ends with JMP 0 0 (halt)
    emit("JMP", 0, 0, "Halt program.");
}

// PROCEDURE STRUCTURE

string code_gen::gen_procedure_start(const string& name)
{
    string end_label = gen_new_label();
    
    // I ensure we have space for this level
    while (level_offsets.size() <= 2) {
        level_offsets.push_back(0);
    }
    
    return end_label;
}

void code_gen::gen_procedure_body_start()
{
    // Reserve space for local variables
    int var_count = level_offsets.back();
    emit("INC", 0, var_count, "Reserve space for local variables");
}

void code_gen::gen_procedure_end(const string& end_label)
{
    emit("OPR", 0, 0, "Procedure return.");
    gen_label(end_label);
}

void code_gen::gen_procedure_return()
{
    emit("OPR", 0, 0, "Procedure return.");
}

// FUNCTION STRUCTURE

string code_gen::gen_function_start(const string& name)
{
    string end_label = gen_new_label();
    return end_label;
}

void code_gen::gen_function_body_start()
{
    int var_count = level_offsets.back();
    emit("INC", 0, var_count, "Reserve space for local variables");
}

void code_gen::gen_function_end(const string& end_label)
{
    emit("OPR", 0, 1, "Function value return.");
    gen_label(end_label);
}

void code_gen::gen_function_return()
{
    emit("OPR", 0, 1, "Function value return.");
}

// CALLS

void code_gen::gen_call_start()
{
    // I mark the stack for the upcoming call
    emit("MST", 1, 0, "Mark stack.");
}

void code_gen::gen_call(const string& name, int arg_count, int current_level)
{
    // For now, I just emit a placeholder call
    // In a real implementation, I'd look up the procedure's address
    emit("CAL", 1, 0, "Call procedure: " + name);
}

void code_gen::gen_function_call(const string& name, int arg_count, int current_level)
{
    // I check for built-in functions
    if (name == "INT2REAL") {
        emit("CAL", 1, 2, "Function call: int2real");
    } else if (name == "REAL2INT") {
        emit("CAL", 1, 5, "Function call: real2int");
    } else if (name == "INT2STRING") {
        emit("CAL", 1, 8, "Function call: int2string");
    } else if (name == "REAL2STRING") {
        emit("CAL", 1, 11, "Function call: real2string");
    } else {
        // User-defined function
        emit("CAL", 1, 0, "Function call: " + name);
    }
}

// VARIABLE MANAGEMENT

void code_gen::gen_variable(const string& name, int level)
{
    // I assign an offset to this variable
    while (level_offsets.size() <= (size_t)level) {
        level_offsets.push_back(0);
    }
    
    int offset = level_offsets[level];
    var_info[name] = make_pair(level, offset);
    level_offsets[level]++;
}

void code_gen::gen_load(const string& name, int current_level)
{
    // I look up the variable's location
    auto it = var_info.find(name);
    if (it != var_info.end()) {
        int var_level = it->second.first;
        int offset = it->second.second;
        int level_diff = current_level - var_level;
        emit("LDV", level_diff, offset, "Load variable or constant.");
    } else {
        // Variable not found - might be a built-in or parameter
        emit("LDV", 0, 0, "Load variable: " + name);
    }
}

void code_gen::gen_store(const string& name, int current_level)
{
    auto it = var_info.find(name);
    if (it != var_info.end()) {
        int var_level = it->second.first;
        int offset = it->second.second;
        int level_diff = current_level - var_level;
        emit("STO", level_diff, offset, "Store result.");
    } else {
        emit("STO", 0, 0, "Store to: " + name);
    }
}

void code_gen::gen_load_address(const string& name, int current_level)
{
    auto it = var_info.find(name);
    if (it != var_info.end()) {
        int var_level = it->second.first;
        int offset = it->second.second;
        int level_diff = current_level - var_level;
        emit("LDA", level_diff, offset, "Load address of variable.");
    } else {
        emit("LDA", 0, 0, "Load address: " + name);
    }
}

// LITERAL LOADING

void code_gen::gen_load_int(int value)
{
    emit("LCI", 0, value, "Load integer constant.");
}

void code_gen::gen_load_real(float value)
{
    emit_real("LCR", 0, value, "Load real constant.");
}

void code_gen::gen_load_string(const string& value)
{
    emit_string("LCS", 0, value, "Load string value.");
}

void code_gen::gen_load_bool(bool value)
{
    if (value) {
        emit("OPR", 0, 17, "Load true.");
    } else {
        emit("OPR", 0, 18, "Load false.");
    }
}

// ARITHMETIC OPERATIONS

void code_gen::gen_add()
{
    emit("OPR", 0, 3, "Add arithmetic expressions together.");
}

void code_gen::gen_subtract()
{
    emit("OPR", 0, 4, "Subtract arithmetic expressions.");
}

void code_gen::gen_multiply()
{
    emit("OPR", 0, 5, "Multiply arithmetic expressions.");
}

void code_gen::gen_divide()
{
    emit("OPR", 0, 6, "Divide arithmetic expressions.");
}

void code_gen::gen_power()
{
    emit("OPR", 0, 7, "Exponentiation.");
}

void code_gen::gen_negate()
{
    emit("OPR", 0, 2, "Negate.");
}

void code_gen::gen_odd()
{
    emit("OPR", 0, 9, "Test if odd.");
}

// COMPARISON OPERATIONS

void code_gen::gen_comparison(symbol::symbol_type op)
{
    switch (op) {
        case symbol::equals_sym:
            emit("OPR", 0, 10, "Test for equality.");
            break;
        case symbol::not_equals_sym:
            emit("OPR", 0, 11, "Test for inequality.");
            break;
        case symbol::less_than_sym:
            emit("OPR", 0, 12, "Test less than.");
            break;
        case symbol::greater_or_equal_sym:
            emit("OPR", 0, 13, "Test greater than or equal.");
            break;
        case symbol::greater_than_sym:
            emit("OPR", 0, 14, "Test greater than.");
            break;
        case symbol::less_or_equal_sym:
            emit("OPR", 0, 15, "Test less than or equal.");
            break;
        default:
            break;
    }
}

// LOGICAL OPERATIONS

void code_gen::gen_and()
{
    emit("OPR", 0, 29, "Logical and.");
}

void code_gen::gen_or()
{
    emit("OPR", 0, 30, "Logical or.");
}

void code_gen::gen_not()
{
    emit("OPR", 0, 16, "Logical complement (not).");
}

// STRING OPERATIONS

void code_gen::gen_concat()
{
    emit("OPR", 0, 8, "String concatenation.");
}

// CONTROL FLOW

void code_gen::gen_jump(const string& label)
{
    emit("JMP", 0, label, "Jump.");
}

void code_gen::gen_jump_false(const string& label)
{
    emit("JIF", 0, label, "Jump if false.");
}

void code_gen::gen_jump_true(const string& label)
{
    // PAL doesn't have a direct "jump if true", so I negate and jump if false
    emit("OPR", 0, 16, "Logical complement (not).");
    emit("JIF", 0, label, "Jump if true (via not + jif).");
}

// FOR LOOP HELPERS

void code_gen::gen_for_init(const string& var, bool reverse, int level)
{
    // The range values should already be on the stack (low, high)
    // I need to initialize the loop variable
    if (reverse) {
        // For reverse, I start at the high value
        emit("OPR", 0, 22, "Swap (get high value on top).");
    }
    // Store the starting value in the loop variable
    gen_store(var, level);
}

void code_gen::gen_for_test(const string& var, bool reverse, const string& end_label, int level)
{
    // I load the loop variable and compare to the limit
    gen_load(var, level);
    
    // The limit should still be accessible - this is simplified
    // A real implementation would save the limit somewhere
    if (reverse) {
        // Test if loop_var >= low (for reverse)
        emit("OPR", 0, 13, "Test greater than or equal.");
    } else {
        // Test if loop_var <= high (for normal)
        emit("OPR", 0, 15, "Test less than or equal.");
    }
    
    emit("JIF", 0, end_label, "Exit loop if test fails.");
}

void code_gen::gen_for_step(const string& var, bool reverse, int level)
{
    // I load, increment/decrement, and store back
    gen_load(var, level);
    emit("LCI", 0, 1, "Load 1 for increment/decrement.");
    
    if (reverse) {
        emit("OPR", 0, 4, "Subtract (decrement).");
    } else {
        emit("OPR", 0, 3, "Add (increment).");
    }
    
    gen_store(var, level);
}

// I/O OPERATIONS
void code_gen::gen_read(const string& var, const lille_type& type, int level)
{
    auto it = var_info.find(var);
    int var_level = 0;
    int offset = 0;
    
    if (it != var_info.end()) {
        var_level = it->second.first;
        offset = it->second.second;
    }
    
    int level_diff = level - var_level;
    
    // I make a copy to work around const issue with get_type()
    lille_type type_copy = type;
    if (type_copy.get_type() == lille_type::type_real) {
        emit("RDR", level_diff, offset, "Read real value.");
    } else {
        emit("RDI", level_diff, offset, "Read integer value.");
    }
}

void code_gen::gen_write(const lille_type& type)
{
    // OPR 0 20 writes the value on top of stack
    emit("OPR", 0, 20, "Write value.");
}

void code_gen::gen_writeln()
{
    // OPR 0 21 terminates the current output line
    emit("OPR", 0, 21, "Terminate output to the current line.");
}

// FINALIZE - Write output file

void code_gen::finalize()
{
    // I open the output file
    code_file.open(output_filename);
    if (!code_file) {
        throw lille_exception("Unable to open code file: " + output_filename);
    }
    
    // I write each instruction, resolving labels as I go
    int instr_num = 1;
    for (auto& instr : instructions) {
        // I try to resolve the operand2 if it's a label
        string op2 = instr.operand2;
        
        // Check if it's a label (starts with 'L' and rest is digits)
        if (!op2.empty() && op2[0] == 'L') {
            bool is_label = true;
            for (size_t i = 1; i < op2.size(); i++) {
                if (!isdigit(op2[i])) {
                    is_label = false;
                    break;
                }
            }
            
            if (is_label) {
                // I look up the label address
                auto it = label_addresses.find(op2);
                if (it != label_addresses.end()) {
                    op2 = to_string(it->second);
                }
            }
        }
        
        // I format and write the instruction
        // Format: OPCODE  LEVEL  ADDRESS  (NUM) COMMENT
        code_file << left << setw(5) << instr.opcode 
                  << setw(6) << instr.operand1
                  << setw(13) << op2
                  << "(" << instr_num << ") " << instr.comment << endl;
        
        instr_num++;
    }
    
    code_file.close();
}