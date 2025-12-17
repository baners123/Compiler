/*
 * idtable.cpp
 *
 * Written:            1 June 2020
 * Last modified:      1 June 2023
 *      Author: Michael Oudshoorn
 */

#include <iomanip>
#include "id_table.h"

using namespace std;
using std::setw;
using std::left;

static const char* ty_name(lille_type::lille_ty t) {
    switch (t) {
        case lille_type::type_integer: return "INTEGER";
        case lille_type::type_real:    return "REAL";
        case lille_type::type_string:  return "STRING";
        case lille_type::type_boolean: return "BOOLEAN";
        case lille_type::type_prog:    return "PROG";     // << your enum name
        default:                       return "UNKNOWN";  // includes type_unknown
    }
}

// If your record::get_kind() returns a string already, you can skip this and print it.
// Otherwise, map your internal kind enum/int to a label here:
static const char* kind_name(int k) {
    // adjust these numbers to your enum if different
    // 1=VARIABLE 2=CONSTANT 3=PROCEDURE 4=FUNCTION 5=PROG
    switch (k) {
        case 1: return "VARIABLE";
        case 2: return "CONSTANT";
        case 3: return "PROCEDURE";
        case 4: return "FUNCTION";
        case 5: return "PROG";
        default: return "UNKNOWN";
    }
}

id_table::id_table(error_handler* err) : error(err) {
    // start with one global scope
    scopes.clear();
    scopes.emplace_back();
}

id_table::~id_table() {
    // free all records
    for (auto& m : scopes) {
        for (auto& kv : m) delete kv.second;
    }
}

void id_table::open_scope() {
    scopes.emplace_back();
}

void id_table::close_scope() {
    // keep at least the global scope
    if (scopes.size() <= 1) return;

    // delete all records in the current scope
    for (auto& kv : scopes.back()) delete kv.second;
    scopes.pop_back();
}

id_table::record* id_table::enter(const string& name) {
    auto& cur = scopes.back();
    auto it = cur.find(name);
    if (it != cur.end()) return it->second; // caller decides whether to flag redeclare
    record* r = new record(name);
    cur[name] = r;
    return r;
}

id_table::record* id_table::lookup_local(const string& name) const {
    auto const& cur = scopes.back();
    auto it = cur.find(name);
    return (it == cur.end()) ? nullptr : it->second;
}

id_table::record* id_table::lookup(const string& name) const {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) return it->second;
    }
    return nullptr;
}

void id_table::dump_id_table(bool) const {
    cout << "---- id table (inner to outer) ----\n";
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        cout << "scope[" << i << "]:\n";
        for (auto& kv : scopes[i]) {
            auto* r = kv.second;
            cout << "  " << r->get_name()
                 << "  type=" << r->get_type().to_string()
                 << "  kind=" << r->get_kind().to_string()
                 << "\n";
        }
    }
    cout << "-----------------------------------\n";
}

void id_table::dump(std::ostream& out) const {
    // Print from outer (level 0) to inner (top), to match the sample.
    for (size_t lvl = 0; lvl < scopes.size(); ++lvl) {
        out << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        out << "scope level " << lvl << "\n";
        out << "---------------------\n";

        const auto& scope = scopes[lvl];

        // NOTE: your 'scopes' is a vector<map<string, record*>>.
        // Iterate each entry in this level’s map:
        for (const auto& kv : scope) {
            const record* r = kv.second;

            // If you don’t track these yet, we print zeros like the sample.
            int line = 0, pos = 0;
            int offset = 0;
            int trace  = 0;
            int params = 0;

            // Kind label: if you have a string already, use that.
            // Otherwise, change 'r->kind()' or whatever accessor you use.
            std::string kindLabel;
            #if 1
            // If you have a string kind:
            // kindLabel = r->get_kind().to_string();
            // If you have an int/enum kind:
            kindLabel = kind_name(/* put your numeric kind here, e.g. r->get_kind_code() */ 0);
            #endif

            // Type label:
            // If you have lille_type with .get_type(), we can map via ty_name(...)
            std::string typeLabel;
            #if 1
            // If lille_type has to_string():
            // typeLabel = r->get_type().to_string();
            // Or use the enum mapper if you have get_type():
            typeLabel = ty_name(r->get_type().get_type());
            #endif

            out << "Token Name: " << r->get_name()
                << " Line No: "   << line
                << " Position: "  << pos
                << "  Type: "     << typeLabel
                << "  Kind: "     << kindLabel
                << "  Level: "    << static_cast<int>(lvl)
                << "  Offset: "   << offset
                << "  Trace?: "   << trace
                << "  #params: "  << params;

            // If this entry represents a function and you store a return type,
            // print it like the sample:
            // out << "  Return ty: " << ty_name(r->get_return_type().get_type());

            out << "\n";
        }
    }
}