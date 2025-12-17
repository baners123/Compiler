// parser.h
// Lille LL(1) recursive-descent parser (syntax only).
// Uses Scanner for tokens and Error_Handler for diagnostics.
// Each method implements one grammar production using single-token lookahead.

#ifndef PARSER_H_
#define PARSER_H_

#include <vector>
#include <string>
#include <stack>
#include "scanner.h"
#include "token.h"
#include "symbol.h"
#include "error_handler.h"
#include "semantics.h"
#include "lille_type.h"
#include "code_gen.h"

using namespace std;

// Parser Class Definition
class Parser
{
public:
    // Constructor
    // I take pointers to all the major compiler components:
    Parser(scanner* s, error_handler* e, semantics* sm, code_gen* c);

    // Main Entry Point
    // This is what compiler.cpp calls to start parsing the entire program.
    // I parse the whole lille program and generate code if no errors occurred.
    void parse_program();

private:
    // Core Collaborators - these are the other compiler components I work with
    scanner*       sc   = nullptr;   // I get tokens from the scanner
    error_handler* err  = nullptr;   // I send error messages here
    semantics*     sem  = nullptr;   // I call this for type/scope checking
    code_gen*      cg   = nullptr;   // I call this to generate PAL code (BONUS)

    // Lookahead State - I use 1-token lookahead for LL(1) parsing
    token* look = nullptr;           // Current token I'm examining
    token* prev = nullptr;           // Previous token (helps with error positions)

    // SCHEME 1 ERROR RECOVERY - This is the key part!
    // I use a simple boolean flag to track if I'm recovering from an error.
    // When recovering is true, I skip tokens without reporting new errors.
    // This prevents the "error cascade" problem where one mistake causes
    // dozens of confusing error messages.
    bool recovering = false;         // Am I currently recovering from an error?

    // Loop Context Stack: I need this to handle EXIT statements properly
    // When I'm inside a loop, I push its end label onto this stack.
    // EXIT statements need to know where to jump, so they pop from here.
    stack<string> loop_exit_labels;  // Stack of loop end labels for EXIT

    // Scope Level Tracking: I need this for code generation
    int current_level = 0;           // Current lexical scope level (0 = global)

    // Helper Methods: These make parsing code cleaner and easier to read    
    // S() returns the current token's symbol type 
    symbol::symbol_type S() const;
    
    // advance() moves to the next token and remembers the previous one
    void advance();
    
    // accept() checks if current token matches, consumes it if yes
    // Returns true if token matched, false otherwise
    bool accept(symbol::symbol_type s);
    
    // expect() is like accept() but with error recovery
    // If token doesn't match, I enter recovery mode and synchronize
    void expect(symbol::symbol_type s, int err_no);
    
    // SCHEME 1 SYNCHRONIZATION: This is where the magic happens!
    // When I detect an error, I call synchronize() with a set of "safe" tokens.
    // I skip tokens until I find one in this set, then resume normal parsing.
    // This prevents cascading errors from one mistake.
    void synchronize(const vector<symbol::symbol_type>& follow);

    // Error Reporting Helpers
    void flag_here(int code);   // Report error at current token position
    void flag_prev(int code);   // Report error at previous token position

    // Grammar Production Methods: One method per grammar rule
    // Each method implements one production from the lille grammar.
    // I use the naming convention that matches the grammar non-terminals.
    
    // Program structure
    void prog();                          // program id is decls begin stmts end id ;
    void decls();                         // Zero or more declarations
    bool starts_decl() const;             // Does current token start a declaration?
    bool starts_stmt() const;             // Does current token start a statement?

    // Declaration forms
    void decl_vars();                     // id {, id} : type [ := init_list ] ;
    void const_decl();                    // constant id : type := expr ;
    void proc_decl();                     // procedure declaration
    void func_decl();                     // function declaration
    void param_list();                    // formal parameter list
    void ident_list();                    // id { , id }
    void init_list();                     // expr { , expr }

    // Statements
    void stmt_part();                     // begin stmt_list end
    void stmt_list(const vector<symbol::symbol_type>& followers);
    void stmt();                          // Single statement
    void assign_or_call();                // id := expr | id ( args )
    void if_stmt();                       // if expr then ... end if
    void while_stmt();                    // while expr loop ... end loop
    void for_stmt();                      // for id in range loop ... end loop
    void loop_block();                    // loop ... end loop
    void write_stmt();                    // write ( args ) ;
    void writeln_stmt();                  // writeln [ ( args ) ] ;
    void exit_stmt();                     // exit [ when expr ] ;
    void read_stmt();                     // read ( id_list ) ;
    void return_stmt();                   // return [ expr ] ;

    // Expression Methods: These return types for semantic checking
    // Each expression method returns a lille_type so I can do type checking.
    // The code generator is called within these methods to emit PAL code.
    lille_type expr();                    // Full expression with comparisons
    lille_type simple_expr();             // Terms with + - or &
    lille_type term();                    // Factors with * / and
    lille_type factor();                  // Primaries with ** and unary ops
    lille_type primary();                 // Literals, identifiers, parenthesized
    lille_type parse_type();              // Parse and return a type keyword
};

#endif // PARSER_H_
