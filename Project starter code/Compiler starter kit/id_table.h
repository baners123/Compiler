/*
 * idtable.h
 *
 * Written:            1 June 2020
 * Last modified:      1 June 2023
 *      Author: Michael Oudshoorn
 */

#ifndef ID_TABLE_H_
#define ID_TABLE_H_

#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>

#include "token.h"
#include "error_handler.h"
#include "lille_type.h"
#include "lille_kind.h"

using namespace std;

class id_table {
public:
    // a tiny record object so i can store type/kind with the name
    class record {
        string     name;
        lille_type ty;
        lille_kind kd;
        enum { KIND_VARIABLE = 1, KIND_CONSTANT = 2, KIND_PROCEDURE = 3, KIND_FUNCTION = 4, KIND_PROG = 5 };
    public:
        record(const string& n)
        : name(n),
          ty(lille_type(lille_type::type_unknown)),
          kd(lille_kind(lille_kind::unknown)) { }
        const string& get_name() const { return name; }

        void set_type(const lille_type& t) { ty = t; }
        lille_type get_type() const { return ty; }

        void set_kind(const lille_kind& k) { kd = k; }
        lille_kind get_kind() const { return kd; }
    };

private:
    error_handler* error;
    // scopes.back() is current (innermost) scope
    vector<unordered_map<string, record*>> scopes;

public:
    explicit id_table(error_handler* err);
    ~id_table();

    // basic scope ops (open when entering block, close when leaving)
    void open_scope();
    void close_scope();

    void dump(std::ostream& out) const;   // print all scopes (outer -> inner)

    // symbol ops
    record* enter(const string& name);          // insert in current scope (no flag here)
    record* lookup(const string& name) const;   // search inner -> outer
    record* lookup_local(const string& name) const; // only current scope

    // debug dump (not required for grading but nice to have)
    void dump_id_table(bool dump_all = true) const;
};

#endif /* ID_TABLE_H_ */

