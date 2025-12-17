/*
 * scanner.cpp
 *
 * Written:            1 June 2020
 * Last modified:      11/25/2025
 *      Author: Jimi Adegoroye
 *
 * My notes on behavior:
 * - I treat whitespace and “-- …end of line” as separators.
 * - Numbers: I differentiate 1.23 vs. the range operator “..” (so “.” alone is not a decimal).
 * - Strings: must close on the same line; "" inside a string counts as a single quote char.
 * - I uppercase identifier text for reserved-word checks, but I still store the original
 *   lexeme (uppercased) into the token’s identifier value (per kit convention).
 * - (line, col) points at the first character of the token.
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <cctype>
#include <cmath>

#include "symbol.h"
#include "error_handler.h"
#include "token.h"
#include "scanner.h"
#include "lille_exception.h"

using namespace std;

// -------------------- ctor wiring --------------------

scanner::scanner()
{
    pos_on_line = -1;                 // nothing read yet
    line_number = 0;
    eoln_flag   = true;               // treat as “between lines”
    eof_flag    = false;
    debugging   = false;

    input_buffer    = "";
    next_char       = end_marker;

    current_symbol  = new symbol();
    current_token   = new token();

    current_line_number   = 0;
    current_pos_on_line   = 0;
    current_integer_value = 0;
    current_real_value    = 0.0f;
    current_string_value  = "";
    current_identifier_name = "";

    error  = nullptr;
    id_tab = nullptr;
}

scanner::scanner(string source_filename, id_table* id_t, error_handler* e)
    : scanner::scanner() // initialize common fields above first
{
    id_tab = id_t;
    error  = e;

    if (std::filesystem::exists(source_filename)) {
        source_file.open(source_filename);
    } else {
        cerr << "Source code file not found." << endl;
        throw lille_exception("Source code file not found.");
    }

    get_line(); // prime input_buffer + line_number
}

// -------------------- error code mapping (shared with parser) --------------------

int error_message(symbol::symbol_type s)
{
    switch (s) {
    case symbol::identifier:          return 0;
    case symbol::strng:               return 1;
    case symbol::real_num:            return 2;
    case symbol::integer:             return 3;
    case symbol::end_of_program:      return 4;
    case symbol::semicolon_sym:       return 5;
    case symbol::colon_sym:           return 6;
    case symbol::comma_sym:           return 7;
    case symbol::equals_sym:          return 8;
    case symbol::not_equals_sym:      return 9;
    case symbol::less_than_sym:       return 10;
    case symbol::greater_than_sym:    return 11;
    case symbol::less_or_equal_sym:   return 12;
    case symbol::greater_or_equal_sym:return 13;
    case symbol::plus_sym:            return 14;
    case symbol::minus_sym:           return 15;
    case symbol::slash_sym:           return 16;
    case symbol::asterisk_sym:        return 17;
    case symbol::power_sym:           return 18;
    case symbol::ampersand_sym:       return 19;
    case symbol::left_paren_sym:      return 20;
    case symbol::right_paren_sym:     return 21;
    case symbol::range_sym:           return 22;
    case symbol::becomes_sym:         return 23;
    case symbol::and_sym:             return 24;
    case symbol::begin_sym:           return 25;
    case symbol::boolean_sym:         return 26;
    case symbol::constant_sym:        return 27;
    case symbol::else_sym:            return 28;
    case symbol::elsif_sym:           return 29;
    case symbol::end_sym:             return 30;
    case symbol::eof_sym:             return 31;
    case symbol::exit_sym:            return 32;
    case symbol::false_sym:           return 33;
    case symbol::for_sym:             return 34;
    case symbol::function_sym:        return 35;
    case symbol::if_sym:              return 36;
    case symbol::in_sym:              return 37;
    case symbol::integer_sym:         return 38;
    case symbol::is_sym:              return 39;
    case symbol::loop_sym:            return 40;
    case symbol::not_sym:             return 41;
    case symbol::null_sym:            return 42;
    case symbol::odd_sym:             return 43;
    case symbol::or_sym:              return 44;
    case symbol::pragma_sym:          return 45;
    case symbol::procedure_sym:       return 46;
    case symbol::program_sym:         return 47;
    case symbol::read_sym:            return 48;
    case symbol::real_sym:            return 49;
    case symbol::ref_sym:             return 50;
    case symbol::return_sym:          return 51;
    case symbol::reverse_sym:         return 52;
    case symbol::string_sym:          return 53;
    case symbol::then_sym:            return 54;
    case symbol::true_sym:            return 55;
    case symbol::value_sym:           return 56;
    case symbol::when_sym:            return 57;
    case symbol::while_sym:           return 58;
    case symbol::write_sym:           return 59;
    case symbol::writeln_sym:         return 60;
    default:
        throw lille_exception("Unexpected symbol passed to error_message (scanner).");
    }
}

// -------------------- buffer + character plumbing --------------------

void scanner::get_line()
{
    if (!source_file.eof()) {
        std::getline(source_file, input_buffer);
        line_number++;
    } else {
        eof_flag = true;
        input_buffer = "";
    }

    if (debugging) {
        cout << "GET_LINE " << line_number << ": >" << input_buffer << "<\n";
    }
}

void scanner::fill_buffer()
{
    // Move to next line and reset cursor
    pos_on_line = -1;
    get_line();
    eoln_flag = true;          // we just crossed a line boundary
    next_char = end_marker;    // sentinel (ensures whitespace loop calls get_char)
}

void scanner::get_char()
{
    // advance one character (or trigger next line)
    if ((pos_on_line + 1) < (int)input_buffer.length()) {
        pos_on_line++;
        next_char = input_buffer.at(pos_on_line);
        eoln_flag = false;
    } else {
        // ran off end of line -> move to next line
        fill_buffer();
    }
}

char scanner::following_char()
{
    // peek char after next_char (or end_marker if beyond)
    if ((!eof_flag) && ((pos_on_line + 1) < (int)input_buffer.length()))
        return input_buffer.at(pos_on_line + 1);
    else
        return end_marker;
}

// -------------------- main token fetch --------------------

token* scanner::get_token()
{
    // 1) Skip whitespace and “-- …end of line” comments
    while ((!eof_flag) && ((next_char <= ' ') || ((next_char == '-') && (following_char() == '-')))) {

        // consume any whitespace on this line
        while ((!eof_flag) && (next_char <= ' ')) {
            get_char();
        }

        // if we are at comment start, drop the rest of the line
        while ((!eof_flag) && (next_char == '-') && (following_char() == '-')) {
            fill_buffer(); // move to next line
        }
    }

    // 2) Initialize default token as EndOfProgram (overwritten below if we’re not at EOF)
    current_symbol      = new symbol(symbol::end_of_program);
    current_line_number = line_number;
    current_pos_on_line = pos_on_line;

    // 3) Dispatch by first character class
    if (!eof_flag) {
        if (isalpha((unsigned char)next_char)) {
            scan_alpha();
        } else if (isdigit((unsigned char)next_char)) {
            scan_digit();
        } else {
            scan_special_symbol();
        }

        // 4) Build the token object for the parser
        switch (current_symbol->get_sym()) {
        case symbol::identifier:
            current_token = new token(new symbol(symbol::identifier),
                                      current_line_number, current_pos_on_line);
            current_token->set_identifier_value(current_identifier_name);
            break;

        case symbol::strng:
            current_token = new token(new symbol(symbol::strng),
                                      current_line_number, current_pos_on_line);
            current_token->set_string_value(current_string_value);
            break;

        case symbol::integer:
            current_token = new token(new symbol(symbol::integer),
                                      current_line_number, current_pos_on_line);
            current_token->set_integer_value(current_integer_value);
            break;

        case symbol::real_num:
            current_token = new token(new symbol(symbol::real_num),
                                      current_line_number, current_pos_on_line);
            current_token->set_real_value(current_real_value);
            break;

        case symbol::pragma_sym:
            // scanner “eats” pragmas by itself (per kit design)
            parse_pragma();
            // after parse_pragma returns, the *current token* should already be updated
            break;

        default:
            // single punctuation token, keywords, etc.
            current_token = new token(current_symbol, current_line_number, current_pos_on_line);
            break;
        }
    } else {
        // 5) At EOF: return EndOfProgram token positioned at the end
        current_token = new token(new symbol(symbol::end_of_program), line_number, pos_on_line);
    }

    if (debugging) {
        cout << "GET_TOKEN returning: ";
        current_token->print_token();
    }

    return current_token;
}

// -------------------- scanners by first character --------------------

// Strings: "hello", with "" allowed inside to mean a single quote in the string.
// Must terminate on the SAME line. If not, I flag with the kit’s string error code.
void scanner::scan_string()
{
    current_string_value.clear();

    // we are on the opening quote; move past it
    get_char();

    bool terminated = false;
    while (!eof_flag && !eoln_flag) {
        if (next_char == '"') {
            // could be "" (escaped quote) or end of string
            if (following_char() == '"') {
                // "" -> append one quote char and skip both
                get_char(); // this moves onto the second "
                get_char(); // now move beyond second "
                current_string_value += '"';
                continue;
            } else {
                // end of string: consume the closing quote and finish
                get_char();
                terminated = true;
                break;
            }
        } else {
            current_string_value += next_char;
            get_char();
        }
    }

    if (!terminated && error) {
        // Unterminated string literal
        error->flag(current_line_number, current_pos_on_line, error_message(symbol::strng));
    }

    current_symbol = new symbol(symbol::strng);
}

// Identifiers/reserved words.
// I uppercase for comparison so “Begin”, “begin”, “BEGIN” all map to BEGIN.
void scanner::scan_alpha()
{
    bool malformed_ident { false };
    current_identifier_name.clear();

    // first char (we’re already sitting on it)
    current_identifier_name += char(toupper((unsigned char)next_char));
    get_char();

    // Continue through [A-Za-z0-9_]* (track illegal '__' or trailing '_')
    while (isalpha((unsigned char)next_char) ||
           isdigit((unsigned char)next_char) ||
           (next_char == '_'))
    {
        if ((next_char == '_') && (following_char() == '_'))
            malformed_ident = true;

        current_identifier_name += char(toupper((unsigned char)next_char));
        get_char();
    }

    // no trailing underscore allowed
    if (!current_identifier_name.empty() &&
        current_identifier_name.back() == '_')
        malformed_ident = true;

    if (malformed_ident && error) {
        // “Illegal underscore in identifier.”
        error->flag(current_line_number, current_pos_on_line, 61);
    }

    // Reserved word mapping (complete set used by parser for this phase)
    if      (current_identifier_name == "AND")       current_symbol = new symbol(symbol::and_sym);
    else if (current_identifier_name == "BEGIN")     current_symbol = new symbol(symbol::begin_sym);
    else if (current_identifier_name == "BOOLEAN")   current_symbol = new symbol(symbol::boolean_sym);
    else if (current_identifier_name == "CONSTANT")  current_symbol = new symbol(symbol::constant_sym);
    else if (current_identifier_name == "ELSE")      current_symbol = new symbol(symbol::else_sym);
    else if (current_identifier_name == "ELSIF")     current_symbol = new symbol(symbol::elsif_sym);
    else if (current_identifier_name == "END")       current_symbol = new symbol(symbol::end_sym);
    else if (current_identifier_name == "EOF")       current_symbol = new symbol(symbol::eof_sym);
    else if (current_identifier_name == "EXIT")      current_symbol = new symbol(symbol::exit_sym);
    else if (current_identifier_name == "FALSE")     current_symbol = new symbol(symbol::false_sym);
    else if (current_identifier_name == "FOR")       current_symbol = new symbol(symbol::for_sym);
    else if (current_identifier_name == "FUNCTION")  current_symbol = new symbol(symbol::function_sym);
    else if (current_identifier_name == "IF")        current_symbol = new symbol(symbol::if_sym);
    else if (current_identifier_name == "IN")        current_symbol = new symbol(symbol::in_sym);
    else if (current_identifier_name == "INTEGER")   current_symbol = new symbol(symbol::integer_sym);
    else if (current_identifier_name == "IS")        current_symbol = new symbol(symbol::is_sym);
    else if (current_identifier_name == "LOOP")      current_symbol = new symbol(symbol::loop_sym);
    else if (current_identifier_name == "NOT")       current_symbol = new symbol(symbol::not_sym);
    else if (current_identifier_name == "NULL")      current_symbol = new symbol(symbol::null_sym);
    else if (current_identifier_name == "ODD")       current_symbol = new symbol(symbol::odd_sym);
    else if (current_identifier_name == "OR")        current_symbol = new symbol(symbol::or_sym);
    else if (current_identifier_name == "PRAGMA")    current_symbol = new symbol(symbol::pragma_sym);
    else if (current_identifier_name == "PROCEDURE") current_symbol = new symbol(symbol::procedure_sym);
    else if (current_identifier_name == "PROGRAM")   current_symbol = new symbol(symbol::program_sym);
    else if (current_identifier_name == "READ")      current_symbol = new symbol(symbol::read_sym);
    else if (current_identifier_name == "REAL")      current_symbol = new symbol(symbol::real_sym);
    else if (current_identifier_name == "REF")       current_symbol = new symbol(symbol::ref_sym);
    else if (current_identifier_name == "RETURN")    current_symbol = new symbol(symbol::return_sym);
    else if (current_identifier_name == "REVERSE")   current_symbol = new symbol(symbol::reverse_sym);
    else if (current_identifier_name == "STRING")    current_symbol = new symbol(symbol::string_sym);
    else if (current_identifier_name == "THEN")      current_symbol = new symbol(symbol::then_sym);
    else if (current_identifier_name == "TRUE")      current_symbol = new symbol(symbol::true_sym);
    else if (current_identifier_name == "VALUE")     current_symbol = new symbol(symbol::value_sym);
    else if (current_identifier_name == "WHEN")      current_symbol = new symbol(symbol::when_sym);
    else if (current_identifier_name == "WRITE")     current_symbol = new symbol(symbol::write_sym);
    else if (current_identifier_name == "WRITELN")   current_symbol = new symbol(symbol::writeln_sym);
    else if (current_identifier_name == "WHILE")     current_symbol = new symbol(symbol::while_sym);
    else                                             current_symbol = new symbol(symbol::identifier);
}

// Numbers: integer or real (with optional fractional part and optional exponent).
// Very important: a lone dot followed by dot (“..”) is the RANGE token, not a decimal.
// I only accept a decimal point if the following character is NOT a dot.
void scanner::scan_digit()
{
    string number_text;
    bool is_real = false;

    // integer part
    while (isdigit((unsigned char)next_char)) {
        number_text += next_char;
        get_char();
    }

    // optional fractional part (but not if it starts a “..” range)
    if (next_char == '.' && following_char() != '.') {
        is_real = true;
        number_text += next_char;
        get_char();

        // at least one digit should follow the decimal point
        if (!isdigit((unsigned char)next_char) && error) {
            error->flag(current_line_number, current_pos_on_line, 77); // numeric format error
        }
        while (isdigit((unsigned char)next_char)) {
            number_text += next_char;
            get_char();
        }
    }

    // optional exponent (E or e, with optional + / -)
    if (next_char == 'E' || next_char == 'e') {
        is_real = true;
        number_text += next_char;
        get_char();

        if (next_char == '+' || next_char == '-') {
            number_text += next_char;
            get_char();
        }

        if (!isdigit((unsigned char)next_char) && error) {
            error->flag(current_line_number, current_pos_on_line, 77);
        }
        while (isdigit((unsigned char)next_char)) {
            number_text += next_char;
            get_char();
        }
    }

    // If a letter immediately follows the number, that’s illegal (e.g., 12A)
    if (isalpha((unsigned char)next_char) && error) {
        error->flag(current_line_number, current_pos_on_line, 77);
    }

    // try to parse text into value (guard with try in case of overflow)
    try {
        if (is_real) {
            current_real_value = static_cast<float>(std::stod(number_text));
            current_symbol = new symbol(symbol::real_num);
        } else {
            current_integer_value = std::stoi(number_text);
            current_symbol = new symbol(symbol::integer);
        }
    } catch (...) {
        if (error) error->flag(current_line_number, current_pos_on_line, 77);
        if (is_real) {
            current_real_value = 0.0f;
            current_symbol = new symbol(symbol::real_num);
        } else {
            current_integer_value = 0;
            current_symbol = new symbol(symbol::integer);
        }
    }
}

// Punctuation and multi-char operators. I always consume the right number of chars.
// For multi-char tokens I do an internal get_char() and the final get_char() at the
// bottom eats the first char; together they consume exactly both.
void scanner::scan_special_symbol()
{
    switch (next_char)
    {
    case ':':   // ':=' or ':'
        if (following_char() == '=') {
            current_symbol = new symbol(symbol::becomes_sym);
            get_char(); // move onto '=' (second char of ':=')
        } else {
            current_symbol = new symbol(symbol::colon_sym);
        }
        break;

    case '<':   // '<', '<=', '<>'
        if (following_char() == '=') {
            current_symbol = new symbol(symbol::less_or_equal_sym);
            get_char();
        } else if (following_char() == '>') {
            current_symbol = new symbol(symbol::not_equals_sym);
            get_char();
        } else {
            current_symbol = new symbol(symbol::less_than_sym);
        }
        break;

    case '>':   // '>' or '>='
        if (following_char() == '=') {
            current_symbol = new symbol(symbol::greater_or_equal_sym);
            get_char();
        } else {
            current_symbol = new symbol(symbol::greater_than_sym);
        }
        break;

    case '*':   // '*' or '**'
        if (following_char() == '*') {
            current_symbol = new symbol(symbol::power_sym);
            get_char();
        } else {
            current_symbol = new symbol(symbol::asterisk_sym);
        }
        break;

    case '.':   // '..' (range)
        if (following_char() == '.') {
            current_symbol = new symbol(symbol::range_sym);
            get_char(); // step onto second '.'
        } else {
            // a bare '.' is not legal in this grammar (decimals handled in scan_digit)
            current_symbol = new symbol(symbol::nul);
            if (error) error->flag(current_line_number, current_pos_on_line,
                                   error_message(symbol::range_sym));
        }
        break;

    case '"':   // start of string literal
        scan_string();
        return; // scan_string() moved next_char already; don’t do the final get_char() here

    case '&':
        current_symbol = new symbol(symbol::ampersand_sym);    break;
    case '/':
        current_symbol = new symbol(symbol::slash_sym);        break;
    case ';':
        current_symbol = new symbol(symbol::semicolon_sym);    break;
    case '(':
        current_symbol = new symbol(symbol::left_paren_sym);   break;
    case ')':
        current_symbol = new symbol(symbol::right_paren_sym);  break;
    case ',':
        current_symbol = new symbol(symbol::comma_sym);        break;
    case '+':
        current_symbol = new symbol(symbol::plus_sym);         break;
    case '-':
        // single '-' is a token; '--' comment was already handled in the skipper
        current_symbol = new symbol(symbol::minus_sym);        break;
    case '=':
        current_symbol = new symbol(symbol::equals_sym);       break;

    default:
        // unknown character
        current_symbol = new symbol(symbol::nul);
        if (error) error->flag(current_line_number, current_pos_on_line, 74); // illegal character
        break;
    }

    // Consume the “current” character for all the single-char branches
    // (for multi-char branches we already ate the second char above)
    get_char();
}

// -------------------- pragma (stubbed to be safe) --------------------
//
// The project defers pragma functionality until later. I parse the shell here so
// pragmas don’t break token flow, but I don’t do symbol-table actions yet.
// I also don’t spam errors if the pragma is malformed—I emit the kit’s codes.
void scanner::parse_pragma()
{
    // Just chew tokens in a safe, minimal way so parser continues.
    // PRAGMA <IDENT> '(' <stuff> ')' ';'
    // I’ll reuse get_token() so positions and values remain consistent.

    // consume "PRAGMA"
    get_token();

    // name (IDENT) or error 69/70 (per kit skeleton)
    if (current_symbol->get_sym() != symbol::identifier) {
        if (error) error->flag(current_line_number, current_pos_on_line, 69);
    } else {
        string name = current_identifier_name; // e.g., DEBUG / TRACE / ERROR_LIMIT
        // (deferred actions happen in later phases)
    }

    // left paren?
    get_token();
    if (current_symbol->get_sym() != symbol::left_paren_sym) {
        if (error) error->flag(current_line_number, current_pos_on_line, error_message(symbol::left_paren_sym));
    }

    // skip up to ')' in a very forgiving way (no nested parens in pragma args)
    do {
        get_token();
        if (current_symbol->get_sym() == symbol::end_of_program) break;
    } while (current_symbol->get_sym() != symbol::right_paren_sym);

    // require ';'
    if (current_symbol->get_sym() != symbol::semicolon_sym) {
        if (error) error->flag(current_line_number, current_pos_on_line, error_message(symbol::semicolon_sym));
    } else {
        // advance past ';' so parser sees the next real token
        get_token();
    }
}

// -------------------- compatibility helpers (kept) --------------------

bool scanner::have(symbol::symbol_type s)
{
    return current_token && (current_token->get_sym() == s);
}

void scanner::must_be(symbol::symbol_type s)
{
    if (current_token && (current_token->get_sym() == s)) {
        get_token();
    } else {
        if (error) error->flag(current_token, error_message(s));
    }
}

token* scanner::this_token()
{
    return current_token;
}
