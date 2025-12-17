/*
 * scanner.h
 *
 * Written:            1 June 2020
 * Last modified:      11/25/2025
 *      Author: Jimi Adegoroye
 *
 * Notes (my own):
 * - I keep a single lookahead character (next_char) and refill line-by-line.
 * - I record the token’s starting (line, col) right before scanning it.
 * - All reserved words are uppercased for comparison so source can be mixed case.
 * - I left the public API (have/must_be/this_token) for compatibility,
 *   but the parser now drives things with its own look token.
 */

#ifndef SCANNER_H_
#define SCANNER_H_

#include <iostream>
#include <fstream>
#include <string>

#include "symbol.h"
#include "token.h"
#include "error_handler.h"
#include "id_table.h"

using namespace std;

class scanner {
private:
    bool debugging { false };             // flip to true only if I want scanner traces

    const char end_marker = char(7);      // BELL (not likely to appear in source)
    token* current_token { nullptr };
    ifstream source_file;                 // source program
    error_handler* error { nullptr };     // error sink
    id_table* id_tab { nullptr };         // symbol-table (for future pragmas)

    // line/column + buffer management
    int pos_on_line { -1 };               // index in current line buffer
    int line_number { 0 };                // 1-based line number
    bool eoln_flag { true };              // true => previous line ended
    string input_buffer;                  // current line text
    char next_char { char(7) };           // next character to process

    // token fields I fill while scanning
    symbol* current_symbol { nullptr };
    int current_line_number { 0 };        // token start line
    int current_pos_on_line { 0 };        // token start column (0-based)
    int current_integer_value { 0 };
    float current_real_value { 0.0f };
    string current_string_value;
    string current_identifier_name;

    // low-level scanners (by first char class)
    void scan_string();                   // "hello" style (supports "" -> embedded quote)
    void scan_alpha();                    // identifiers + reserved words
    void scan_digit();                    // integers and reals (handles E/exponent)
    void scan_special_symbol();           // punctuation and multi-char ops
    void parse_pragma();                  // #pragma-like thing (kit specific)

    // lifecycle
    scanner();                            // default is private; use public ctor below
    void get_line();                      // read next source line into input_buffer
    void get_char();                      // advance one character within input_buffer
    char following_char();                // peek char after next_char (or end_marker)
    void fill_buffer();                   // advance to next line and reset char state

public:
    bool eof_flag { false };              // true only after I go past the last line

    // Open file, wire dependencies. I’ll throw if file not found.
    scanner(string source_filename, id_table* id_t, error_handler* e);

    // Main “gimme a token”.
    token* get_token();

    // Compatibility helpers (parser shouldn’t rely on these anymore, but I keep them)
    bool have(symbol::symbol_type s);
    void must_be(symbol::symbol_type s);
    token* this_token();
};

// I export this so the parser can reuse the exact codes the kit expects.
int error_message(symbol::symbol_type s);

#endif /* SCANNER_H_ */
