/*
 * code_gen.h
 *
 * Written:            December 2025
 * Last modified:      December 2025
 *      Author: Jimi Adegoroye
 *
 * This is the code generator for the lille compiler.
 * It translates lille source code into PAL (Pseudo Assembly Language).
 * 
 * PAL is a simple stack-based assembly language with these key instructions:
 * - PUSH: push a value onto the stack
 * - POP: remove top of stack
 * - ADD, SUB, MUL, DIV: arithmetic operations
 * - JMP: unconditional jump
 * - JPF: jump if false (conditional)
 * - CALL: call a procedure/function
 * - RET: return from procedure/function
 * - HALT: stop program execution
 */

#ifndef CODE_GEN_H_
#define CODE_GEN_H_

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include "lille_type.h"
#include "symbol.h"

using namespace std;

class code_gen {
private:
    // Output file management
    ofstream code_file;           // The .pal file I write to
    string output_filename;        // Name of the output file
    
    // Instruction counter
    // PAL instructions are numbered starting from 1.
    // I track this so I can generate comments like "(15) Load value"
    int instruction_count;         // Current instruction number
    
    // Label management
    // I use labels for control flow (if, while, for).
    // Labels are converted to instruction addresses later.
    int label_counter;             // For generating unique labels
    map<string, int> label_addresses;  // Maps label name to instruction number
    vector<pair<int, string>> forward_refs;  // Instructions that need label resolution
    
    // Variable offset tracking
    // Each variable gets an offset within its scope's stack frame.
    // I track this per-level for nested scopes.
    map<string, pair<int, int>> var_info;  // name -> (level, offset)
    vector<int> level_offsets;             // Current offset at each level
    
    // Stored instructions for later output
    // I store instructions in a vector so I can resolve forward references
    // before writing the final output.
    struct Instruction {
        string opcode;     // PAL opcode (JMP, LDV, OPR, etc.)
        int operand1;      // Level difference or 0
        string operand2;   // Address, value, or label
        string comment;    // Helpful comment
    };
    vector<Instruction> instructions;
    
    // Helper methods
    void emit(const string& op, int l, int a, const string& comment);
    void emit(const string& op, int l, const string& label, const string& comment);
    void emit_string(const string& op, int l, const string& str, const string& comment);
    void emit_real(const string& op, int l, float val, const string& comment);
    
public:
    // Constructor and destructor
    code_gen(const string& filename);
    ~code_gen();
    
    // Label management
    string gen_new_label();              // Generate a unique label like "L0", "L1"
    void gen_label(const string& label); // Place a label at current position
    
    // Program structure
    void gen_program_start(const string& name);
    void gen_program_body_start();
    void gen_program_end();
    
    // Procedure/function structure
    string gen_procedure_start(const string& name);
    void gen_procedure_body_start();
    void gen_procedure_end(const string& end_label);
    void gen_procedure_return();
    
    string gen_function_start(const string& name);
    void gen_function_body_start();
    void gen_function_end(const string& end_label);
    void gen_function_return();
    
    // Calls
    void gen_call_start();
    void gen_call(const string& name, int arg_count, int current_level);
    void gen_function_call(const string& name, int arg_count, int current_level);
    
    // Variable management
    void gen_variable(const string& name, int level);
    void gen_load(const string& name, int current_level);
    void gen_store(const string& name, int current_level);
    void gen_load_address(const string& name, int current_level);
    
    // Literal loading
    void gen_load_int(int value);
    void gen_load_real(float value);
    void gen_load_string(const string& value);
    void gen_load_bool(bool value);
    
    // Arithmetic operations (OPR instructions)
    void gen_add();
    void gen_subtract();
    void gen_multiply();
    void gen_divide();
    void gen_power();
    void gen_negate();
    void gen_odd();
    
    // Comparison operations
    void gen_comparison(symbol::symbol_type op);
    
    // Logical operations
    void gen_and();
    void gen_or();
    void gen_not();
    
    // String operations
    void gen_concat();
    
    // Control flow
    void gen_jump(const string& label);
    void gen_jump_false(const string& label);
    void gen_jump_true(const string& label);
    
    // For loop helpers
    void gen_for_init(const string& var, bool reverse, int level);
    void gen_for_test(const string& var, bool reverse, const string& end_label, int level);
    void gen_for_step(const string& var, bool reverse, int level);
    
    // I/O operations
    void gen_read(const string& var, const lille_type& type, int level);
    void gen_write(const lille_type& type);
    void gen_writeln();
    
    // Finalization
    void finalize();  // Resolve forward references and write output
};

#endif /* CODE_GEN_H_ */