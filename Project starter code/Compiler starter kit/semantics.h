#ifndef SEMANTICS_H
#define SEMANTICS_H

#include <string>
#include <iostream>
#include "symbol.h"
#include "lille_type.h"
#include "lille_kind.h"
#include "id_table.h"
#include "token.h"
#include "error_handler.h"

using namespace std;

class semantics {
private:
    // My connections to other compiler components
    id_table*      id_tab;  // The symbol table - I use this for all lookups
    error_handler* err;     // For reporting semantic errors

    // Helper function to unwrap lille_type
    // I use this to get the actual type enum from a lille_type object.
    // It makes my code cleaner.
    static inline lille_type::lille_ty Ty(lille_type t) { 
        return t.get_type(); 
    }

    // Type predicates - these make my code more readable
    // Instead of writing long type checks everywhere, I use these helpers.
    static inline bool is_num(const lille_type& t) { 
        auto k = Ty(t); 
        return k == lille_type::type_integer || k == lille_type::type_real; 
    }
    
    static inline bool is_bool(const lille_type& t) { 
        return Ty(t) == lille_type::type_boolean; 
    }
    
    static inline bool is_str(const lille_type& t) { 
        return Ty(t) == lille_type::type_string; 
    }

public:
    // Constructor
    // I need the symbol table and error handler to do my job.
    semantics(id_table* t, error_handler* e);

    // DECLARATION METHODS
    // I call these when the parser sees a new declaration.
    // I check for duplicates and add to the symbol table.
    
    // Declare a variable - I flag error 82 if it's already declared locally
    void declare_var(const string& name, const lille_type& ty, token* where);
    
    // Declare a constant - same duplicate check as variables
    void declare_const(const string& name, const lille_type& ty, token* where);

    // LOOKUP METHODS
    // I call these when the parser sees an identifier being used.
    // I return the type (or type_unknown if not found).
    
    // Look up an identifier and return its type
    // I flag error 81 if it's not declared
    lille_type lookup_type(const string& name, token* where);

    // ASSIGNMENT CHECKING
    // I verify that assignments are valid:
    // - LHS must be assignable (not a constant or for-loop index)
    // - Types must be compatible
    void check_assignment(const string& lhs_name, const lille_type& rhs, token* where);

    // SCOPE MANAGEMENT
    // I delegate scope operations to the symbol table.
    void open_scope();
    void close_scope_and_dump();  // Also prints the symbol table!
    void dump_all_scopes(std::ostream& out);

    // BUILT-IN FUNCTIONS
    // I install the standard library functions at startup.
    // These are: int2real, real2int, int2string, real2string
    void install_builtins();

    // EXPRESSION CHECKING
    // I verify that operators are used with compatible types.
    // I return the resulting type of the expression.    
    // Check a binary operation (like +, -, *, <, and, etc.)
    lille_type check_binary(const lille_type& L, symbol::symbol_type op,
                            const lille_type& R, token* where);
    
    // Check a unary operation (like -, not)
    lille_type check_unary(symbol::symbol_type op, const lille_type& T, token* where);
    
    // Require that an expression is boolean (for if/while conditions)
    // I flag error 120 if it's not
    void require_boolean(const lille_type& T, token* where);
};

#endif // SEMANTICS_H

