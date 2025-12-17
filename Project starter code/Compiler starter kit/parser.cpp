#include "parser.h"
#include "semantics.h"
#include "code_gen.h"
#include <iostream>
#include <algorithm>

using namespace std;

// I use ST as a shorthand for symbol::symbol_type because I type it so often
using ST = symbol::symbol_type;

// External function from scanner - I need this for error message codes

// The scanner defines this function that maps symbol types to error codes.
// For example, if I expect a semicolon but don't find one, this tells me
// which error message number to use.

extern int error_message(symbol::symbol_type s);


// CONSTRUCTOR

// I initialize all my pointers and get the first token ready.
// The lookahead token (look) is primed here so I can start parsing immediately.

Parser::Parser(scanner* s, error_handler* e, semantics* sm, code_gen* c)
{
    // I store pointers to all the compiler components I'll need
    sc  = s;    // Scanner gives me tokens
    err = e;    // Error handler prints error messages
    sem = sm;   // Semantics checker validates types and scopes
    cg  = c;    // Code generator produces PAL code (can be null if not generating)
    
    // I initialize my error recovery flag to false - not recovering yet!
    recovering = false;
    
    // I start at scope level 0 (the global/program level)
    current_level = 0;
    
    // I get the first token so I have something to look at
    // This "primes the pump" for parsing
    look = sc->get_token();
    
    // prev starts as null since there's no previous token yet
    prev = nullptr;
}


// S() - Get Current Symbol Type

// This is my most-used helper. It returns what kind of token I'm looking at.
// I call this constantly to decide what to parse next.

symbol::symbol_type Parser::S() const 
{ 
    // I just return the symbol type from the current lookahead token
    return look->get_sym(); 
}


// advance() - Move to Next Token

// I call this to consume the current token and get the next one.
// I save the old token in 'prev' because sometimes I need to report errors
// at the position of the token I just consumed.

void Parser::advance() 
{
    // I remember the current token before moving on
    // This is useful for error messages like "expected ';' after this"
    prev = look;
    
    // I get the next token from the scanner
    look = sc->get_token();
}


// accept() - Try to Match a Token

// I use this when a token is OPTIONAL or when I'm checking alternatives.
// If the current token matches what I want, I consume it and return true.
// Otherwise, I leave it alone and return false.

bool Parser::accept(ST s) 
{
    // I check if the current token is what I'm looking for
    if (S() == s) {
        // Yes! I consume it and say "success"
        advance();
        return true;
    }
    // No match - I leave the token alone and say "not found"
    return false;
}


// expect() - Require a Token (with Scheme 1 Error Recovery)

// This is where my Scheme 1 error recovery really shows.
// If the expected token is there, great - I consume it.
// If not, I enter recovery mode and skip until I find something safe.

void Parser::expect(ST expected, int err_no) 
{
    // ========================================================================
    // SCHEME 1 RECOVERY LOGIC
    // ========================================================================
    // If I'm already recovering, I need to skip tokens until I find
    // the expected one (or hit EOF). This prevents error cascades.
    // ========================================================================
    
    if (recovering) {
        // I'm in recovery mode - skip tokens until I find what I need
        // I also stop at EOF to avoid infinite loops
        while (S() != expected && S() != ST::end_of_program) {
            advance();  // Skip this token and try the next
        }
        
        // Did I find what I was looking for?
        if (S() == expected) {
            advance();           // Consume it
            recovering = false;  // Exit recovery mode - back to normal!
        }
        // If I hit EOF, I stay in recovery mode but stop skipping
        return;
    }
    
    // ========================================================================
    // Normal case - not recovering
    // ========================================================================
    
    if (S() == expected) {
        // Perfect - the token I want is here
        advance();  // Consume it and move on
    } else {
        // Uh oh - unexpected token!
        // I report the error (only because recovering is false)
        err->flag(look, err_no);
        
        // Now I enter recovery mode
        recovering = true;
        
        // I don't advance here - synchronize() will handle skipping
    }
}


// synchronize() - Skip to a Safe Token

// This is my panic-mode recovery helper. When I'm lost in bad input,
// I call this with a list of "safe" tokens to look for.
// I skip everything until I find one of them.

void Parser::synchronize(const vector<ST>& follow) 
{
    // I keep skipping tokens until I find one in my follow set
    // or until I hit end-of-program
    while (S() != ST::end_of_program) {
        // Check if current token is in the follow set
        for (auto f : follow) {
            if (S() == f) {
                // Found a safe token! I can stop skipping now
                recovering = false;  // Exit recovery mode
                return;
            }
        }
        // Not safe yet - skip this token
        advance();
    }
    // I hit EOF - stop skipping anyway
    recovering = false;
}


// flag_here() - Report Error at Current Token

// I use this when the error is AT the current token.
// Example: "unexpected keyword 'begin'" - the error is on 'begin'

void Parser::flag_here(int code) 
{
    // Only report if I'm not in recovery mode (Scheme 1 rule!)
    if (!recovering) {
        err->flag(look, code);
        recovering = true;  // Enter recovery mode after reporting
    }
}


// flag_prev() - Report Error at Previous Token

// I use this when the error is AFTER the previous token.
// Example: "expected ';' after 'x'" - the error is after 'x', not on the next token
// This makes error messages more intuitive for the programmer.

void Parser::flag_prev(int code) 
{
    // Only report if I'm not in recovery mode
    if (!recovering) {
        if (prev) {
            // I point the error just past the previous token
            // The +1 to column makes it look like "expected something here"
            err->flag(prev->get_line_number(), prev->get_pos_on_line() + 1, code);
        } else {
            // No previous token (shouldn't happen, but be safe)
            err->flag(look, code);
        }
        recovering = true;  // Enter recovery mode
    }
}


// MAIN ENTRY POINT

void Parser::parse_program() 
{
    // I just call prog() which handles the entire program grammar
    prog();
}


// starts_decl() - Does Current Token Start a Declaration?

// I use this to decide whether to parse another declaration or move on.
// Declarations start with: identifier, CONSTANT, PROCEDURE, or FUNCTION

bool Parser::starts_decl() const 
{
    // I check if the current token can begin a declaration
    switch (S()) {
        case ST::identifier:      // Variable declaration: x, y : integer;
        case ST::constant_sym:    // Constant: constant PI : real := 3.14;
        case ST::procedure_sym:   // Procedure: procedure foo is ...
        case ST::function_sym:    // Function: function bar return integer is ...
            return true;
        default:
            return false;
    }
}


// starts_stmt() - Does Current Token Start a Statement?

// I use this to decide whether to parse another statement or stop.
// This list includes all the statement starters in lille.

bool Parser::starts_stmt() const 
{
    switch (S()) {
        case ST::identifier:      // Assignment or procedure call: x := 5; or foo();
        case ST::null_sym:        // Null statement: null;
        case ST::if_sym:          // If statement: if x > 0 then ...
        case ST::while_sym:       // While loop: while x > 0 loop ...
        case ST::for_sym:         // For loop: for i in 1..10 loop ...
        case ST::loop_sym:        // Bare loop: loop ... exit when ...; end loop;
        case ST::begin_sym:       // Nested block: begin ... end;
        case ST::exit_sym:        // Exit: exit; or exit when condition;
        case ST::write_sym:       // Write: write("hello");
        case ST::writeln_sym:     // Writeln: writeln; or writeln("hello");
        case ST::read_sym:        // Read: read(x, y);
        case ST::return_sym:      // Return: return; or return expr;
            return true;
        default:
            return false;
    }
}


// prog() - Parse Entire Program

// Grammar: program <id> is <decls> begin <stmts> end <id> ;
// This is the top-level production that parses everything.

void Parser::prog() 
{
    // I expect "program" keyword first
    expect(ST::program_sym, error_message(ST::program_sym));
    
    // I need to save the program name for later (end must match)
    string prog_name = "";
    if (S() == ST::identifier) {
        prog_name = look->get_identifier_value();
    }
    
    // I expect the program name (an identifier)
    expect(ST::identifier, error_message(ST::identifier));
    
    // I expect "is" keyword
    if (!accept(ST::is_sym)) {
        // Missing "is" - I flag and try to recover
        flag_here(error_message(ST::is_sym));
        synchronize({ ST::identifier, ST::constant_sym, ST::procedure_sym, 
                      ST::function_sym, ST::begin_sym });
    }
    
    // ========================================================================
    // Code Generation: Program header
    // ========================================================================
    // I tell the code generator we're starting a program.
    // It will emit the initial jump over built-in functions.
    // ========================================================================
    if (cg) {
        cg->gen_program_start(prog_name);
    }
    
    // I open a new scope for the program body
    sem->open_scope();
    current_level = 1;  // Program body is level 1
    
    // I parse all declarations (variables, constants, procedures, functions)
    decls();
    
    // ========================================================================
    // Code Generation: After declarations, before BEGIN
    // ========================================================================
    if (cg) {
        cg->gen_program_body_start();
    }
    
    // I expect "begin" keyword
    expect(ST::begin_sym, error_message(ST::begin_sym));
    
    // I parse the statement list (the program body)
    // Statements end at "end" keyword
    stmt_list({ ST::end_sym });
    
    // I expect "end" keyword
    expect(ST::end_sym, error_message(ST::end_sym));
    
    // The ending identifier is OPTIONAL in lille
    // Some programs use "end prog1;" others just use "end;"
    if (S() == ST::identifier) {
        string end_name = look->get_identifier_value();
        if (end_name != prog_name) {
            // Names don't match - I flag error 75
            flag_here(75);  // "Identifier name must match program name"
        }
        advance();  // Consume the identifier
    }
    
    // I expect the final semicolon
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
    
    // Allow trailing semicolons (some test files have them)
    while (accept(ST::semicolon_sym)) { }
    
    // I should be at end of file now
    if (S() != ST::end_of_program) {
        // There's garbage after the program - flag it
        flag_here(77);  // "End of program expected"
    }
    
    // ========================================================================
    // Code Generation: Program end
    // ========================================================================
    if (cg) {
        cg->gen_program_end();
    }
    
    // I print the symbol table and close the scope
    // This is required: "print the full symbol table every time your program exits a scope"
    sem->close_scope_and_dump();
    current_level = 0;
}


// decls() - Parse Zero or More Declarations

// I keep parsing declarations until I see something that isn't one.
// Order: variables, constants, procedures, functions all intermixed.

void Parser::decls() 
{
    // I loop while the current token can start a declaration
    while (starts_decl()) {
        if (S() == ST::constant_sym) {
            // It's a constant declaration
            const_decl();
        } else if (S() == ST::procedure_sym) {
            // It's a procedure declaration
            proc_decl();
        } else if (S() == ST::function_sym) {
            // It's a function declaration
            func_decl();
        } else {
            // It's a variable declaration (starts with identifier)
            decl_vars();
        }
    }
}


// decl_vars() - Parse Variable Declaration

// Grammar: id {, id} : type [ := init_expr {, init_expr} ] ;
// Example: x, y, z : integer := 1, 2, 3;

void Parser::decl_vars() 
{
    // I collect all the variable names first
    // I store both the name and the token for error reporting
    struct NameTok { string name; token* tok; };
    vector<NameTok> names;
    
    // I expect at least one identifier
    if (S() == ST::identifier) {
        names.push_back({ look->get_identifier_value(), look });
    }
    expect(ST::identifier, error_message(ST::identifier));
    
    // I parse more identifiers if there are commas
    while (accept(ST::comma_sym)) {
        if (S() == ST::identifier) {
            names.push_back({ look->get_identifier_value(), look });
        }
        expect(ST::identifier, error_message(ST::identifier));
    }
    
    // I expect a colon before the type
    expect(ST::colon_sym, error_message(ST::colon_sym));
    
    // I parse the type (integer, real, string, boolean)
    lille_type var_type = parse_type();
    
    // I declare all the variables in the symbol table
    // The semantics checker will flag if any are duplicates
    for (auto& nt : names) {
        if (!nt.name.empty()) {
            sem->declare_var(nt.name, var_type, nt.tok);
            
            // ================================================================
            // Code Generation: Reserve space for variable
            // ================================================================
            if (cg) {
                cg->gen_variable(nt.name, current_level);
            }
        }
    }
    
    // I check for optional initialization
    if (accept(ST::becomes_sym)) {
        // There's an initializer - I parse expressions
        lille_type init_type = expr();
        
        // I should check that the type matches (basic check)
        if (!names.empty()) {
            sem->check_assignment(names[0].name, init_type, prev);
        }
        
        // ====================================================================
        // Code Generation: Store initial value
        // ====================================================================
        if (cg && !names.empty()) {
            cg->gen_store(names[0].name, current_level);
        }
        
        // I parse more initializers if there are commas
        int idx = 1;
        while (accept(ST::comma_sym)) {
            lille_type next_init = expr();
            if (idx < (int)names.size()) {
                sem->check_assignment(names[idx].name, next_init, prev);
                if (cg) {
                    cg->gen_store(names[idx].name, current_level);
                }
            }
            idx++;
        }
    }
    
    // I expect a semicolon to end the declaration
    if (!accept(ST::semicolon_sym)) {
        flag_prev(error_message(ST::semicolon_sym));
        synchronize({ ST::identifier, ST::constant_sym, ST::procedure_sym,
                      ST::function_sym, ST::begin_sym });
    }
}


// const_decl() - Parse Constant Declaration

// Grammar: constant id {, id} : type := expr {, expr} ;
// Example: constant PI : real := 3.14159;

void Parser::const_decl() 
{
    // I expect "constant" keyword
    expect(ST::constant_sym, error_message(ST::constant_sym));
    
    // I collect constant names
    struct NameTok { string name; token* tok; };
    vector<NameTok> names;
    
    if (S() == ST::identifier) {
        names.push_back({ look->get_identifier_value(), look });
    }
    expect(ST::identifier, error_message(ST::identifier));
    
    while (accept(ST::comma_sym)) {
        if (S() == ST::identifier) {
            names.push_back({ look->get_identifier_value(), look });
        }
        expect(ST::identifier, error_message(ST::identifier));
    }
    
    // I check for the type declaration
    lille_type const_type(lille_type::type_unknown);
    
    if (accept(ST::colon_sym)) {
        // Typed form: constant x : integer := 5;
        const_type = parse_type();
        expect(ST::becomes_sym, error_message(ST::becomes_sym));
    } else if (accept(ST::becomes_sym)) {
        // Untyped form: constant x := 5; (type inferred)
        // I'll infer the type from the expression
    } else if (accept(ST::is_sym)) {
        // Alternate form: constant x is 5;
    } else {
        flag_here(error_message(ST::becomes_sym));
    }
    
    // I parse the initializer expression(s)
    vector<lille_type> init_types;
    if (S() != ST::semicolon_sym) {
        init_types.push_back(expr());
        while (accept(ST::comma_sym)) {
            init_types.push_back(expr());
        }
    }
    
    // If type was not specified, I infer it from the first expression
    if (const_type.get_type() == lille_type::type_unknown && !init_types.empty()) {
        const_type = init_types[0];
    }
    
    // I declare the constants
    for (size_t i = 0; i < names.size(); i++) {
        if (!names[i].name.empty()) {
            sem->declare_const(names[i].name, const_type, names[i].tok);
        }
    }
    
    // I expect semicolon
    if (!accept(ST::semicolon_sym)) {
        flag_prev(error_message(ST::semicolon_sym));
        synchronize({ ST::identifier, ST::constant_sym, ST::procedure_sym,
                      ST::function_sym, ST::begin_sym });
    }
}


// proc_decl() - Parse Procedure Declaration

// Grammar: procedure id [ ( params ) ] is decls begin stmts end [ id ] ;
// Example: procedure greet(name : value string) is begin writeln(name); end;

void Parser::proc_decl() 
{
    expect(ST::procedure_sym, error_message(ST::procedure_sym));
    
    string proc_name = "";
    if (S() == ST::identifier) {
        proc_name = look->get_identifier_value();
    }
    expect(ST::identifier, error_message(ST::identifier));
    
    // ========================================================================
    // Code Generation: Procedure header
    // ========================================================================
    string proc_end_label = "";
    if (cg) {
        proc_end_label = cg->gen_procedure_start(proc_name);
    }
    
    // I open a new scope for the procedure
    sem->open_scope();
    current_level++;
    
    // I check for parameters
    if (accept(ST::left_paren_sym)) {
        param_list();
        expect(ST::right_paren_sym, error_message(ST::right_paren_sym));
    }
    
    expect(ST::is_sym, error_message(ST::is_sym));
    
    // I parse local declarations
    decls();
    
    // ========================================================================
    // Code Generation: After procedure declarations
    // ========================================================================
    if (cg) {
        cg->gen_procedure_body_start();
    }
    
    expect(ST::begin_sym, error_message(ST::begin_sym));
    
    stmt_list({ ST::end_sym });
    
    expect(ST::end_sym, error_message(ST::end_sym));
    
    // Optional procedure name at end
    if (S() == ST::identifier) {
        advance();  // I just consume it without strict checking
    }
    
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
    
    // ========================================================================
    // Code Generation: Procedure end
    // ========================================================================
    if (cg) {
        cg->gen_procedure_end(proc_end_label);
    }
    
    // I print symbol table and close scope (required!)
    sem->close_scope_and_dump();
    current_level--;
}


// func_decl() - Parse Function Declaration

// Grammar: function id [ ( params ) ] return type is decls begin stmts end [ id ] ;

void Parser::func_decl() 
{
    expect(ST::function_sym, error_message(ST::function_sym));
    
    string func_name = "";
    if (S() == ST::identifier) {
        func_name = look->get_identifier_value();
    }
    expect(ST::identifier, error_message(ST::identifier));
    
    // ========================================================================
    // Code Generation: Function header  
    // ========================================================================
    string func_end_label = "";
    if (cg) {
        func_end_label = cg->gen_function_start(func_name);
    }
    
    sem->open_scope();
    current_level++;
    
    // I check for parameters
    if (accept(ST::left_paren_sym)) {
        param_list();
        expect(ST::right_paren_sym, error_message(ST::right_paren_sym));
    }
    
    // I expect "return" and a return type
    expect(ST::return_sym, error_message(ST::return_sym));
    lille_type ret_type = parse_type();
    
    expect(ST::is_sym, error_message(ST::is_sym));
    
    decls();
    
    if (cg) {
        cg->gen_function_body_start();
    }
    
    expect(ST::begin_sym, error_message(ST::begin_sym));
    
    stmt_list({ ST::end_sym });
    
    expect(ST::end_sym, error_message(ST::end_sym));
    
    if (S() == ST::identifier) {
        advance();
    }
    
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
    
    if (cg) {
        cg->gen_function_end(func_end_label);
    }
    
    sem->close_scope_and_dump();
    current_level--;
}


// param_list() - Parse Formal Parameter List

// Grammar: param {; param}
// param: id {, id} : mode type
// mode: value | ref

void Parser::param_list() 
{
    // I parse parameters until I hit right paren
    while (S() != ST::right_paren_sym && S() != ST::end_of_program) {
        // I collect parameter names
        vector<string> param_names;
        
        if (S() == ST::identifier) {
            param_names.push_back(look->get_identifier_value());
        }
        expect(ST::identifier, error_message(ST::identifier));
        
        while (accept(ST::comma_sym)) {
            if (S() == ST::identifier) {
                param_names.push_back(look->get_identifier_value());
            }
            expect(ST::identifier, error_message(ST::identifier));
        }
        
        expect(ST::colon_sym, error_message(ST::colon_sym));
        
        // I parse the mode (value or ref)
        bool is_ref = false;
        if (accept(ST::value_sym)) {
            is_ref = false;
        } else if (accept(ST::ref_sym)) {
            is_ref = true;
        } else {
            flag_here(94);  // "Parameter mode expected"
        }
        
        // I parse the type
        lille_type param_type = parse_type();
        
        // I declare parameters in current scope
        for (const auto& name : param_names) {
            sem->declare_var(name, param_type, look);
        }
        
        // I check for more parameters
        if (!accept(ST::semicolon_sym)) {
            break;  // No more parameters
        }
    }
}


// stmt_part() - Parse Statement Part (begin ... end)

// I use this for nested blocks within procedures/functions.

void Parser::stmt_part() 
{
    expect(ST::begin_sym, error_message(ST::begin_sym));
    
    sem->open_scope();
    current_level++;
    
    stmt_list({ ST::end_sym });
    
    // I print and close scope (required per spec!)
    sem->close_scope_and_dump();
    current_level--;
    
    expect(ST::end_sym, error_message(ST::end_sym));
}


// stmt_list() - Parse a List of Statements

// I keep parsing statements until I hit a token in the followers set.
// The followers tell me when to stop (e.g., END, ELSE, ELSIF).

void Parser::stmt_list(const vector<ST>& followers) 
{
    // I create a helper to check if current token is a follower
    auto isFollower = [&](ST s) {
        for (auto f : followers) {
            if (s == f) return true;
        }
        return false;
    };
    
    // I loop parsing statements
    while (true) {
        // I check if I should stop
        if (isFollower(S())) break;
        
        // I skip extra semicolons (lille allows them)
        while (S() == ST::semicolon_sym) {
            advance();
            if (isFollower(S())) break;
        }
        if (isFollower(S())) break;
        
        // I check if this looks like a statement
        if (!starts_stmt()) break;
        
        // I parse one statement
        stmt();
        
        // I accept an optional semicolon between statements
        accept(ST::semicolon_sym);
    }
    
    // I drain any trailing semicolons before the follower
    while (S() == ST::semicolon_sym) {
        advance();
        if (isFollower(S())) break;
        if (starts_stmt()) break;
    }
}


// stmt() - Parse a Single Statement

// I look at the current token to decide which kind of statement this is.

void Parser::stmt() 
{
    switch (S()) {
        case ST::identifier:
            // Could be assignment or procedure call
            assign_or_call();
            break;
            
        case ST::null_sym:
            // Null statement - does nothing
            advance();
            expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
            break;
            
        case ST::if_sym:
            if_stmt();
            break;
            
        case ST::while_sym:
            while_stmt();
            break;
            
        case ST::for_sym:
            for_stmt();
            break;
            
        case ST::loop_sym:
            loop_block();
            break;
            
        case ST::begin_sym:
            // Nested block
            stmt_part();
            expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
            break;
            
        case ST::exit_sym:
            exit_stmt();
            break;
            
        case ST::write_sym:
            write_stmt();
            break;
            
        case ST::writeln_sym:
            writeln_stmt();
            break;
            
        case ST::read_sym:
            read_stmt();
            break;
            
        case ST::return_sym:
            return_stmt();
            break;
            
        default:
            // Unknown statement - I flag and try to recover
            flag_here(79);  // "Error in statement"
            synchronize({ ST::semicolon_sym, ST::end_sym, ST::else_sym, 
                          ST::elsif_sym, ST::loop_sym });
            break;
    }
}


// assign_or_call() - Parse Assignment or Procedure Call

// Grammar: id := expr ; OR id ( args ) ;
// I look ahead to see if there's := or ( to decide which one.

void Parser::assign_or_call() 
{
    // I save the identifier info
    string var_name = look->get_identifier_value();
    token* id_tok = look;
    
    expect(ST::identifier, error_message(ST::identifier));
    
    if (S() == ST::becomes_sym) {
        // ====================================================================
        // Assignment: id := expr
        // ====================================================================
        token* becomes_tok = look;  // I save for error reporting
        advance();  // Consume :=
        
        // I parse the right-hand side expression
        lille_type rhs_type = expr();
        
        // I check that the assignment is valid (type compatibility)
        sem->check_assignment(var_name, rhs_type, becomes_tok);
        
        // ====================================================================
        // Code Generation: Assignment
        // ====================================================================
        // The expression already pushed its value onto the stack.
        // Now I just need to store it in the variable.
        // ====================================================================
        if (cg) {
            cg->gen_store(var_name, current_level);
        }
        
        expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
        
    } else if (S() == ST::left_paren_sym) {
        // ====================================================================
        // Procedure Call: id ( args )
        // ====================================================================
        advance();  // Consume (
        
        // ====================================================================
        // Code Generation: Mark stack for procedure call
        // ====================================================================
        if (cg) {
            cg->gen_call_start();
        }
        
        // I parse arguments
        int arg_count = 0;
        if (S() != ST::right_paren_sym) {
            expr();  // First argument
            arg_count++;
            while (accept(ST::comma_sym)) {
                expr();
                arg_count++;
            }
        }
        
        expect(ST::right_paren_sym, error_message(ST::right_paren_sym));
        
        // ====================================================================
        // Code Generation: Call the procedure
        // ====================================================================
        if (cg) {
            cg->gen_call(var_name, arg_count, current_level);
        }
        
        expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
        
    } else {
        // ====================================================================
        // Bare identifier as statement (procedure call with no args)
        // ====================================================================
        if (cg) {
            cg->gen_call_start();
            cg->gen_call(var_name, 0, current_level);
        }
        
        expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
    }
}


// if_stmt() - Parse If Statement

// Grammar: if expr then stmts {elsif expr then stmts} [else stmts] end if ;

void Parser::if_stmt() 
{
    expect(ST::if_sym, error_message(ST::if_sym));
    
    // ========================================================================
    // Code Generation: Labels for if statement
    // ========================================================================
    string else_label = "";
    string end_label = "";
    if (cg) {
        else_label = cg->gen_new_label();  // Jump here if condition false
        end_label = cg->gen_new_label();   // Jump here after any branch
    }
    
    // I parse and type-check the condition
    token* cond_tok = look;
    lille_type cond_type = expr();
    sem->require_boolean(cond_type, cond_tok);
    
    // ========================================================================
    // Code Generation: Jump if false to else/end
    // ========================================================================
    if (cg) {
        cg->gen_jump_false(else_label);
    }
    
    expect(ST::then_sym, error_message(ST::then_sym));
    
    // I define what tokens can end the THEN part
    vector<ST> then_followers = { ST::elsif_sym, ST::else_sym, ST::end_sym };
    stmt_list(then_followers);
    
    // ========================================================================
    // Code Generation: Jump over else part
    // ========================================================================
    if (cg) {
        cg->gen_jump(end_label);
        cg->gen_label(else_label);
    }
    
    // I handle ELSIF chains
    while (accept(ST::elsif_sym)) {
        string next_else = "";
        if (cg) {
            next_else = cg->gen_new_label();
        }
        
        token* econd_tok = look;
        lille_type econd_type = expr();
        sem->require_boolean(econd_type, econd_tok);
        
        if (cg) {
            cg->gen_jump_false(next_else);
        }
        
        expect(ST::then_sym, error_message(ST::then_sym));
        stmt_list(then_followers);
        
        if (cg) {
            cg->gen_jump(end_label);
            cg->gen_label(next_else);
        }
    }
    
    // I handle optional ELSE
    if (accept(ST::else_sym)) {
        stmt_list({ ST::end_sym });
    }
    
    // I drain extra semicolons before END
    while (accept(ST::semicolon_sym)) { }
    
    // ========================================================================
    // Code Generation: End label
    // ========================================================================
    if (cg) {
        cg->gen_label(end_label);
    }
    
    expect(ST::end_sym, error_message(ST::end_sym));
    if (S() == ST::if_sym) advance();  // Optional "IF" after END
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
}


// while_stmt() - Parse While Loop

// Grammar: while expr loop stmts end loop ;

void Parser::while_stmt() 
{
    expect(ST::while_sym, error_message(ST::while_sym));
    
    // ========================================================================
    // Code Generation: Labels for while loop
    // ========================================================================
    string loop_start = "";
    string loop_end = "";
    if (cg) {
        loop_start = cg->gen_new_label();
        loop_end = cg->gen_new_label();
        cg->gen_label(loop_start);  // Loop starts here
    }
    
    // I push the exit label for EXIT statements
    loop_exit_labels.push(loop_end);
    
    // I parse the condition
    token* cond_tok = look;
    lille_type cond_type = expr();
    sem->require_boolean(cond_type, cond_tok);
    
    // ========================================================================
    // Code Generation: Exit loop if condition is false
    // ========================================================================
    if (cg) {
        cg->gen_jump_false(loop_end);
    }
    
    expect(ST::loop_sym, error_message(ST::loop_sym));
    
    stmt_list({ ST::end_sym });
    
    // ========================================================================
    // Code Generation: Jump back to loop start
    // ========================================================================
    if (cg) {
        cg->gen_jump(loop_start);
        cg->gen_label(loop_end);  // Loop ends here
    }
    
    // I pop the exit label
    loop_exit_labels.pop();
    
    while (accept(ST::semicolon_sym)) { }
    
    expect(ST::end_sym, error_message(ST::end_sym));
    if (S() == ST::loop_sym) advance();
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
}


// for_stmt() - Parse For Loop

// Grammar: for id in [reverse] expr .. expr loop stmts end loop ;
// The loop variable is implicitly declared as integer.
// Each for loop creates its own scope for the loop variable.

void Parser::for_stmt() 
{
    expect(ST::for_sym, error_message(ST::for_sym));
    
    // I get the loop variable name
    string loop_var = "";
    token* var_tok = look;
    if (S() == ST::identifier) {
        loop_var = look->get_identifier_value();
    }
    expect(ST::identifier, error_message(ST::identifier));
    
    // I open a new scope for the for loop
    // This way the loop variable doesn't conflict with other for loops
    sem->open_scope();
    current_level++;
    
    // I declare the loop variable in this new scope
    sem->declare_var(loop_var, lille_type(lille_type::type_integer), var_tok);
    
    expect(ST::in_sym, error_message(ST::in_sym));
    
    // I check for optional REVERSE
    bool is_reverse = accept(ST::reverse_sym);
    
    // I parse the range: expr .. expr
    lille_type low_type = simple_expr();
    expect(ST::range_sym, error_message(ST::range_sym));
    lille_type high_type = simple_expr();
    
    // ========================================================================
    // Code Generation: For loop setup
    // ========================================================================
    string loop_start = "";
    string loop_end = "";
    if (cg) {
        loop_start = cg->gen_new_label();
        loop_end = cg->gen_new_label();
        
        // I initialize the loop variable
        // For normal: start at low
        // For reverse: start at high
        cg->gen_for_init(loop_var, is_reverse, current_level);
        cg->gen_label(loop_start);
        cg->gen_for_test(loop_var, is_reverse, loop_end, current_level);
    }
    
    loop_exit_labels.push(loop_end);
    
    expect(ST::loop_sym, error_message(ST::loop_sym));
    
    stmt_list({ ST::end_sym });
    
    // ========================================================================
    // Code Generation: Increment/decrement and loop back
    // ========================================================================
    if (cg) {
        cg->gen_for_step(loop_var, is_reverse, current_level);
        cg->gen_jump(loop_start);
        cg->gen_label(loop_end);
    }
    
    loop_exit_labels.pop();
    
    // I close the for-loop scope (this also dumps the symbol table as required)
    sem->close_scope_and_dump();
    current_level--;
    
    while (accept(ST::semicolon_sym)) { }
    
    expect(ST::end_sym, error_message(ST::end_sym));
    if (S() == ST::loop_sym) advance();
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
}


// loop_block() - Parse Bare Loop

// Grammar: loop stmts end loop ;
// This is an infinite loop - must use EXIT to break out.

void Parser::loop_block() 
{
    expect(ST::loop_sym, error_message(ST::loop_sym));
    
    // ========================================================================
    // Code Generation: Labels for loop
    // ========================================================================
    string loop_start = "";
    string loop_end = "";
    if (cg) {
        loop_start = cg->gen_new_label();
        loop_end = cg->gen_new_label();
        cg->gen_label(loop_start);
    }
    
    loop_exit_labels.push(loop_end);
    
    stmt_list({ ST::end_sym });
    
    if (cg) {
        cg->gen_jump(loop_start);  // Loop back forever
        cg->gen_label(loop_end);   // EXIT jumps here
    }
    
    loop_exit_labels.pop();
    
    while (accept(ST::semicolon_sym)) { }
    
    expect(ST::end_sym, error_message(ST::end_sym));
    if (S() == ST::loop_sym) advance();
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
}


// exit_stmt() - Parse Exit Statement

// Grammar: exit [when expr] ;

void Parser::exit_stmt() 
{
    expect(ST::exit_sym, error_message(ST::exit_sym));
    
    // I check if we're inside a loop
    if (loop_exit_labels.empty()) {
        flag_here(89);  // "Exit statement only valid inside a loop"
    }
    
    string exit_label = loop_exit_labels.empty() ? "" : loop_exit_labels.top();
    
    if (accept(ST::when_sym)) {
        // Conditional exit: exit when condition
        token* cond_tok = look;
        lille_type cond_type = expr();
        sem->require_boolean(cond_type, cond_tok);
        
        // ====================================================================
        // Code Generation: Jump if condition is TRUE
        // ====================================================================
        if (cg && !exit_label.empty()) {
            cg->gen_jump_true(exit_label);
        }
    } else {
        // Unconditional exit
        if (cg && !exit_label.empty()) {
            cg->gen_jump(exit_label);
        }
    }
    
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
}


// write_stmt() - Parse Write Statement

// Grammar: write ( expr {, expr} ) ;

void Parser::write_stmt() 
{
    expect(ST::write_sym, error_message(ST::write_sym));
    
    bool had_paren = accept(ST::left_paren_sym);
    
    // I parse expressions to write
    if (S() != ST::right_paren_sym && S() != ST::semicolon_sym) {
        lille_type t = expr();
        
        // ====================================================================
        // Code Generation: Write the value
        // ====================================================================
        if (cg) {
            cg->gen_write(t);
        }
        
        while (accept(ST::comma_sym)) {
            t = expr();
            if (cg) {
                cg->gen_write(t);
            }
        }
    }
    
    if (had_paren) {
        expect(ST::right_paren_sym, error_message(ST::right_paren_sym));
    }
    
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
}


// writeln_stmt() - Parse Writeln Statement

// Grammar: writeln [ ( expr {, expr} ) | expr {, expr} ] ;
// I handle two forms:
// 1. writeln;                          -- just newline
// 2. writeln(expr, expr);              -- with parentheses
// 3. writeln expr, expr;               -- without parentheses

void Parser::writeln_stmt() 
{
    expect(ST::writeln_sym, error_message(ST::writeln_sym));
    
    // I check if there's anything to output
    if (S() == ST::semicolon_sym) {
        // Just a newline - writeln;
        if (cg) {
            cg->gen_writeln();
        }
        expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
        return;
    }
    
    // I check for parentheses form
    bool had_paren = accept(ST::left_paren_sym);
    
    // I parse expressions to output
    if (S() != ST::right_paren_sym && S() != ST::semicolon_sym) {
        lille_type t = expr();
        if (cg) {
            cg->gen_write(t);
        }
        
        while (accept(ST::comma_sym)) {
            t = expr();
            if (cg) {
                cg->gen_write(t);
            }
        }
    }
    
    if (had_paren) {
        expect(ST::right_paren_sym, error_message(ST::right_paren_sym));
    }
    
    // ========================================================================
    // Code Generation: End the line
    // ========================================================================
    if (cg) {
        cg->gen_writeln();
    }
    
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
}


// read_stmt() - Parse Read Statement

// Grammar: read ( id {, id} ) ;

void Parser::read_stmt() 
{
    expect(ST::read_sym, error_message(ST::read_sym));
    
    bool had_paren = accept(ST::left_paren_sym);
    
    // I need at least one identifier
    if (S() == ST::identifier) {
        string var_name = look->get_identifier_value();
        lille_type var_type = sem->lookup_type(var_name, look);
        
        // ====================================================================
        // Code Generation: Read into variable
        // ====================================================================
        if (cg) {
            cg->gen_read(var_name, var_type, current_level);
        }
    }
    expect(ST::identifier, error_message(ST::identifier));
    
    while (accept(ST::comma_sym)) {
        if (S() == ST::identifier) {
            string var_name = look->get_identifier_value();
            lille_type var_type = sem->lookup_type(var_name, look);
            if (cg) {
                cg->gen_read(var_name, var_type, current_level);
            }
        }
        expect(ST::identifier, error_message(ST::identifier));
    }
    
    if (had_paren) {
        expect(ST::right_paren_sym, error_message(ST::right_paren_sym));
    }
    
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
}


// return_stmt() - Parse Return Statement

// Grammar: return [expr] ;

void Parser::return_stmt() 
{
    expect(ST::return_sym, error_message(ST::return_sym));
    
    // I check if there's a return value
    if (S() != ST::semicolon_sym) {
        lille_type ret_type = expr();
        
        // ====================================================================
        // Code Generation: Function return with value
        // ====================================================================
        if (cg) {
            cg->gen_function_return();
        }
    } else {
        // Procedure return (no value)
        if (cg) {
            cg->gen_procedure_return();
        }
    }
    
    expect(ST::semicolon_sym, error_message(ST::semicolon_sym));
}


// parse_type() - Parse a Type Keyword

// Returns the lille_type for integer, real, string, or boolean.

lille_type Parser::parse_type() 
{
    if (accept(ST::integer_sym)) {
        return lille_type(lille_type::type_integer);
    } else if (accept(ST::real_sym)) {
        return lille_type(lille_type::type_real);
    } else if (accept(ST::string_sym)) {
        return lille_type(lille_type::type_string);
    } else if (accept(ST::boolean_sym)) {
        return lille_type(lille_type::type_boolean);
    }
    
    // Not a valid type keyword
    flag_here(96);  // "Type name expected"
    advance();  // Skip the bad token to avoid infinite loop
    return lille_type(lille_type::type_unknown);
}


// EXPRESSION PARSING

// I use the standard precedence-based recursive descent approach:
// expr -> simple_expr [relop simple_expr]
// simple_expr -> [+|-] term {(+|-|or|&) term}
// term -> factor {(*|/|and) factor}
// factor -> [+|-|not|odd] primary [** primary]
// primary -> id | literal | ( expr ) | function_call



// expr() - Parse Full Expression

// Handles comparisons at the top level.

lille_type Parser::expr() 
{
    lille_type left = simple_expr();
    
    // I check for relational operators
    switch (S()) {
        case ST::equals_sym:
        case ST::not_equals_sym:
        case ST::less_than_sym:
        case ST::less_or_equal_sym:
        case ST::greater_than_sym:
        case ST::greater_or_equal_sym:
        {
            token* op_tok = look;
            ST op = S();
            advance();
            
            lille_type right = simple_expr();
            
            // ================================================================
            // Code Generation: Comparison operation
            // ================================================================
            if (cg) {
                cg->gen_comparison(op);
            }
            
            // I check types and return boolean (comparisons always return bool)
            return sem->check_binary(left, op, right, op_tok);
        }
        default:
            return left;  // No comparison - just return what I have
    }
}


// simple_expr() - Parse Additive Expression

// Handles +, -, OR, and & (string concatenation).

lille_type Parser::simple_expr() 
{
    // I check for leading unary + or -
    bool had_unary = false;
    ST unary_op = ST::nul;
    if (S() == ST::plus_sym || S() == ST::minus_sym) {
        had_unary = true;
        unary_op = S();
        advance();
    }
    
    lille_type result = term();
    
    // I apply unary operator if present
    if (had_unary) {
        result = sem->check_unary(unary_op, result, prev);
        if (cg && unary_op == ST::minus_sym) {
            cg->gen_negate();
        }
    }
    
    // I parse additional terms
    while (S() == ST::plus_sym || S() == ST::minus_sym ||
           S() == ST::or_sym || S() == ST::ampersand_sym) 
    {
        token* op_tok = look;
        ST op = S();
        advance();
        
        lille_type right = term();
        
        // ====================================================================
        // Code Generation: Arithmetic/logical operation
        // ====================================================================
        if (cg) {
            switch (op) {
                case ST::plus_sym:      cg->gen_add(); break;
                case ST::minus_sym:     cg->gen_subtract(); break;
                case ST::or_sym:        cg->gen_or(); break;
                case ST::ampersand_sym: cg->gen_concat(); break;
                default: break;
            }
        }
        
        result = sem->check_binary(result, op, right, op_tok);
    }
    
    return result;
}


// term() - Parse Multiplicative Expression

// Handles *, /, and AND.

lille_type Parser::term() 
{
    lille_type result = factor();
    
    while (S() == ST::asterisk_sym || S() == ST::slash_sym || S() == ST::and_sym) {
        token* op_tok = look;
        ST op = S();
        advance();
        
        lille_type right = factor();
        
        // ====================================================================
        // Code Generation: Multiplication/division operation
        // ====================================================================
        if (cg) {
            switch (op) {
                case ST::asterisk_sym: cg->gen_multiply(); break;
                case ST::slash_sym:    cg->gen_divide(); break;
                case ST::and_sym:      cg->gen_and(); break;
                default: break;
            }
        }
        
        result = sem->check_binary(result, op, right, op_tok);
    }
    
    return result;
}


// factor() - Parse Factor with Unary Operators and Exponentiation

// Handles unary +, -, NOT, ODD, and the ** power operator.

lille_type Parser::factor() 
{
    // I check for unary operators
    if (S() == ST::plus_sym || S() == ST::minus_sym || 
        S() == ST::not_sym || S() == ST::odd_sym) 
    {
        ST op = S();
        advance();
        
        lille_type base = primary();
        
        // I handle ODD specially - it returns boolean
        if (op == ST::odd_sym) {
            if (cg) {
                cg->gen_odd();
            }
            return lille_type(lille_type::type_boolean);
        }
        
        // I handle NOT
        if (op == ST::not_sym) {
            if (cg) {
                cg->gen_not();
            }
            return sem->check_unary(op, base, prev);
        }
        
        // I handle unary minus
        if (op == ST::minus_sym) {
            if (cg) {
                cg->gen_negate();
            }
        }
        
        return sem->check_unary(op, base, prev);
    }
    
    lille_type base = primary();
    
    // I check for ** (exponentiation)
    if (S() == ST::power_sym) {
        token* op_tok = look;
        advance();
        
        lille_type exp = primary();
        
        if (cg) {
            cg->gen_power();
        }
        
        return sem->check_binary(base, ST::power_sym, exp, op_tok);
    }
    
    return base;
}


// primary() - Parse Primary Expression

// Handles identifiers, literals, parenthesized expressions, and function calls.
lille_type Parser::primary() 
{
    switch (S()) {
        case ST::identifier:
        {
            // Could be variable, constant, or function call
            string name = look->get_identifier_value();
            token* id_tok = look;
            advance();
            
            // I check for function call syntax
            if (accept(ST::left_paren_sym)) {
                // Function call
                if (cg) {
                    cg->gen_call_start();
                }
                
                int arg_count = 0;
                if (S() != ST::right_paren_sym) {
                    expr();
                    arg_count++;
                    while (accept(ST::comma_sym)) {
                        expr();
                        arg_count++;
                    }
                }
                
                expect(ST::right_paren_sym, error_message(ST::right_paren_sym));
                
                if (cg) {
                    cg->gen_function_call(name, arg_count, current_level);
                }
                
                return sem->lookup_type(name, id_tok);
            }
            
            // Just a variable or constant reference
            if (cg) {
                cg->gen_load(name, current_level);
            }
            
            return sem->lookup_type(name, id_tok);
        }
        
        case ST::integer:
        {
            // Integer literal
            int val = look->get_integer_value();
            advance();
            
            if (cg) {
                cg->gen_load_int(val);
            }
            
            return lille_type(lille_type::type_integer);
        }
        
        case ST::real_num:
        {
            // Real literal
            float val = look->get_real_value();
            advance();
            
            if (cg) {
                cg->gen_load_real(val);
            }
            
            return lille_type(lille_type::type_real);
        }
        
        case ST::strng:
        {
            // String literal
            string val = look->get_string_value();
            advance();
            
            if (cg) {
                cg->gen_load_string(val);
            }
            
            return lille_type(lille_type::type_string);
        }
        
        case ST::true_sym:
        {
            advance();
            if (cg) {
                cg->gen_load_bool(true);
            }
            return lille_type(lille_type::type_boolean);
        }
        
        case ST::false_sym:
        {
            advance();
            if (cg) {
                cg->gen_load_bool(false);
            }
            return lille_type(lille_type::type_boolean);
        }
        
        case ST::left_paren_sym:
        {
            // Parenthesized expression
            advance();
            lille_type result = expr();
            expect(ST::right_paren_sym, error_message(ST::right_paren_sym));
            return result;
        }
        
        default:
        {
            // I try to recover gracefully at statement boundaries
            switch (S()) {
                case ST::semicolon_sym:
                case ST::right_paren_sym:
                case ST::end_sym:
                case ST::else_sym:
                case ST::elsif_sym:
                case ST::then_sym:
                case ST::end_of_program:
                    // These are natural boundaries - don't report error
                    return lille_type(lille_type::type_unknown);
                default:
                    flag_here(error_message(ST::identifier));
                    advance();
                    return lille_type(lille_type::type_unknown);
            }
        }
    }
}
