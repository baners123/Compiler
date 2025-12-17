#include <iostream>
#include "semantics.h"
#include "lille_type.h"
#include "token.h"
#include "error_handler.h"

using namespace std;

// I use this alias for the nested enum type - saves typing
using LTKind = lille_type::lille_ty;

// CONSTRUCTOR
// I just store the pointers to the symbol table and error handler.
// I'll use these throughout my semantic checking.
semantics::semantics(id_table* t, error_handler* e)
    : id_tab(t), err(e) 
{
    // Nothing else to initialize - the symbol table handles its own setup
}

// DECLARATION METHODS

// declare_var() - Declare a variable in the current scope
// I check if the name already exists in the CURRENT scope (not outer scopes).
// If it does, I flag error 82 (duplicate declaration).
// Otherwise, I add it to the symbol table.
void semantics::declare_var(const string& name, const lille_type& ty, token* where) 
{
    // I only check the local scope - outer scopes can have same name (shadowing)
    if (id_tab->lookup_local(name) != nullptr) {
        // Oops! This name is already declared in this block
        // Error 82: Identifier declared multiple times in same block
        err->flag(where, 82);
        return;  // I don't add it again
    }
    
    // I add the variable to the symbol table
    auto* rec = id_tab->enter(name);
    rec->set_kind(lille_kind(lille_kind::variable));
    rec->set_type(ty);
}

// declare_const() - Declare a constant in the current scope
// Same as declare_var() but I mark it as a constant.
// Constants can't be assigned to after declaration.
void semantics::declare_const(const string& name, const lille_type& ty, token* where) 
{
    if (id_tab->lookup_local(name) != nullptr) {
        // Error 82: Identifier declared multiple times in same block
        err->flag(where, 82);
        return;
    }
    
    auto* rec = id_tab->enter(name);
    rec->set_kind(lille_kind(lille_kind::constant));  // Mark as constant!
    rec->set_type(ty);
}

// LOOKUP METHODS

// lookup_type() - Find an identifier and return its type
// I search all scopes (inner to outer) for the name.
// If not found, I flag error 81 and return unknown type.
// I also handle built-in functions specially.
lille_type semantics::lookup_type(const string& name, token* where) 
{
    // I first check for built-in functions
    // These have known return types regardless of symbol table
    if (name == "INT2REAL") {
        return lille_type(lille_type::type_real);
    }
    if (name == "REAL2INT") {
        return lille_type(lille_type::type_integer);
    }
    if (name == "INT2STRING") {
        return lille_type(lille_type::type_string);
    }
    if (name == "REAL2STRING") {
        return lille_type(lille_type::type_string);
    }
    
    // I search all scopes, starting from innermost
    auto* rec = id_tab->lookup(name);
    
    if (!rec) {
        // Error 81: Identifier not previously declared
        err->flag(where, 81);
        return lille_type(lille_type::type_unknown);
    }
    
    return rec->get_type();
}

// ASSIGNMENT CHECKING

// check_assignment() - Verify an assignment is valid
// I check two things:
// 1. The LHS must be assignable (not a constant)
// 2. The types must be compatible (or coercible)
void semantics::check_assignment(const string& lhs_name, const lille_type& rhs, token* where) 
{
    // I first look up the LHS
    auto* rec = id_tab->lookup(lhs_name);
    
    if (!rec) {
        // Error 81: Not declared - I already flagged this in lookup_type
        err->flag(where, 81);
        return;
    }
    
    // I check if the LHS is assignable
    // Constants and for-loop indices cannot be assigned to
    lille_kind kind = rec->get_kind();
    if (kind.is_kind(lille_kind::constant)) {
        // Error 85: Identifier is not assignable
        err->flag(where, 85);
        return;
    }
    
    // Now I check type compatibility
    lille_type lhs = rec->get_type();
    LTKind LT = Ty(lhs);
    LTKind RT = Ty(rhs);
    
    // Same types are always compatible
    if (LT == RT) {
        return;  // All good!
    }
    
    // I allow widening: assigning integer to real is OK
    // (The value gets promoted automatically)
    if (LT == lille_type::type_real && RT == lille_type::type_integer) {
        return;  // Widening is OK
    }
    
    // I allow unknown types to pass (error already reported elsewhere)
    if (LT == lille_type::type_unknown || RT == lille_type::type_unknown) {
        return;  // Don't report additional errors
    }
    
    // Anything else is a type mismatch
    // Error 93: LHS and RHS of assignment are not type compatible
    err->flag(where, 93);
}

// SCOPE MANAGEMENT

// open_scope() - Start a new scope
// I delegate to the symbol table. This is called when entering:
// - The program body
// - A procedure or function body
// - A begin/end block
void semantics::open_scope() 
{
    id_tab->open_scope();
}

// close_scope_and_dump() - Print symbol table and close scope
// This is REQUIRED by the project spec:
// "Also print the full symbol table every time your program exits a scope"
void semantics::close_scope_and_dump() 
{
    // I print all scopes (the spec says "full symbol table")
    dump_all_scopes(cout);
    
    // Then I close the current scope
    id_tab->close_scope();
}

// dump_all_scopes() - Print the entire symbol table
void semantics::dump_all_scopes(ostream& out) 
{
    // I delegate to the symbol table's dump method
    id_tab->dump(out);
}

// BUILT-IN FUNCTIONS

// install_builtins() - Add standard library functions
// lille has four built-in conversion functions:
// - int2real: integer -> real
// - real2int: real -> integer
// - int2string: integer -> string
// - real2string: real -> string
void semantics::install_builtins() 
{
    // I add each built-in function to the global scope
    // They're treated as function-typed identifiers
    
    auto* rec = id_tab->enter("INT2REAL");
    rec->set_type(lille_type(lille_type::type_func));
    
    rec = id_tab->enter("INT2STRING");
    rec->set_type(lille_type(lille_type::type_func));
    
    rec = id_tab->enter("REAL2INT");
    rec->set_type(lille_type(lille_type::type_func));
    
    rec = id_tab->enter("REAL2STRING");
    rec->set_type(lille_type(lille_type::type_func));
}

// EXPRESSION CHECKING

// check_binary() - Check a binary operation and return result type
// I handle all the binary operators:
// - Arithmetic: + - * / ** (need numeric operands)
// - Boolean: and or (need boolean operands)
// - String: & (concatenation - needs strings or promotable types)
// - Comparison: = <> < <= > >= (return boolean)
lille_type semantics::check_binary(const lille_type& L, symbol::symbol_type op,
                                   const lille_type& R, token* where)
{
    // ARITHMETIC OPERATORS: + - * / **
    if (op == symbol::plus_sym || op == symbol::minus_sym ||
        op == symbol::asterisk_sym || op == symbol::slash_sym || 
        op == symbol::power_sym)
    {
        // Both operands must be numeric
        if (!(is_num(L) && is_num(R))) {
            // Error 116: Arithmetic expression expected
            err->flag(where, 116);
            return lille_type(lille_type::type_unknown);
        }
        
        // If either operand is real, result is real
        // (This includes division - lille uses integer division when both are integers)
        if (Ty(L) == lille_type::type_real || Ty(R) == lille_type::type_real) {
            return lille_type(lille_type::type_real);
        }
        
        // Both integers -> integer result (including integer division)
        return lille_type(lille_type::type_integer);
    }

    // ========================================================================
    // BOOLEAN OPERATORS: and or
    // ========================================================================
    if (op == symbol::and_sym || op == symbol::or_sym) {
        if (is_bool(L) && is_bool(R)) {
            return lille_type(lille_type::type_boolean);
        }
        // Error 120: Boolean expression expected
        err->flag(where, 120);
        return lille_type(lille_type::type_unknown);
    }

    // STRING CONCATENATION: &
    if (op == symbol::ampersand_sym) {
        // I make copies because get_type() might not be const
        lille_type Lc = L;
        lille_type Rc = R;
        
        LTKind lt = Lc.get_type();
        LTKind rt = Rc.get_type();
        
        // I'm lenient with unknown types (error recovery)
        if (lt == lille_type::type_unknown || rt == lille_type::type_unknown) {
            return lille_type(lille_type::type_string);
        }
        
        // String & string is always OK
        if (lt == lille_type::type_string && rt == lille_type::type_string) {
            return lille_type(lille_type::type_string);
        }
        
        // I allow numeric and boolean types to be promoted to string
        // This is a lille convenience feature
        auto is_promotable = [](LTKind t) {
            return t == lille_type::type_integer
                || t == lille_type::type_real
                || t == lille_type::type_boolean;
        };
        
        if ((lt == lille_type::type_string && is_promotable(rt)) ||
            (rt == lille_type::type_string && is_promotable(lt)) ||
            (is_promotable(lt) && is_promotable(rt))) {
            return lille_type(lille_type::type_string);
        }
        
        // Not concatenable
        // Error 115: Both expressions must be strings
        err->flag(where, 115);
        return lille_type(lille_type::type_unknown);
    }

    // COMPARISON OPERATORS: = <> < <= > >=
    if (op == symbol::equals_sym || op == symbol::not_equals_sym ||
        op == symbol::less_than_sym || op == symbol::less_or_equal_sym ||
        op == symbol::greater_than_sym || op == symbol::greater_or_equal_sym)
    {
        bool ok = false;
        
        // Numeric vs numeric is OK (I promote integer to real internally)
        if (is_num(L) && is_num(R)) {
            ok = true;
        }
        
        // String vs string is OK for = and <>
        if (is_str(L) && is_str(R) &&
            (op == symbol::equals_sym || op == symbol::not_equals_sym)) {
            ok = true;
        }
        
        // Boolean vs boolean is OK for = and <>
        if (is_bool(L) && is_bool(R) &&
            (op == symbol::equals_sym || op == symbol::not_equals_sym)) {
            ok = true;
        }
        
        if (ok) {
            // Comparisons always return boolean
            return lille_type(lille_type::type_boolean);
        }
        
        // Error 114: Types of expressions must match
        err->flag(where, 114);
        return lille_type(lille_type::type_unknown);
    }

    // Default case - shouldn't happen with valid operators
    return lille_type(lille_type::type_unknown);
}

// check_unary() - Check a unary operation and return result type
// I handle:
// - not: requires boolean, returns boolean
// - + -: requires numeric, returns same numeric type
lille_type semantics::check_unary(symbol::symbol_type op, const lille_type& T, token* where) 
{
    // NOT requires boolean
    if (op == symbol::not_sym) {
        if (is_bool(T)) {
            return lille_type(lille_type::type_boolean);
        }
        // Error 120: Boolean expression expected
        err->flag(where, 120);
        return lille_type(lille_type::type_unknown);
    }
    
    // Unary + and - require numeric
    if (op == symbol::plus_sym || op == symbol::minus_sym) {
        if (is_num(T)) {
            return T;  // Result has same type as operand
        }
        // Error 116: Arithmetic expression expected
        err->flag(where, 116);
        return lille_type(lille_type::type_unknown);
    }
    
    return lille_type(lille_type::type_unknown);
}

// require_boolean() - Ensure an expression is boolean
// I use this for if/while conditions that MUST be boolean.
void semantics::require_boolean(const lille_type& T, token* where) 
{
    if (!is_bool(T)) {
        // I give unknown types a pass (error already reported)
        if (Ty(T) != lille_type::type_unknown) {
            // Error 120: Boolean expression expected
            err->flag(where, 120);
        }
    }
}
