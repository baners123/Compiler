 /* By Jimi Adegoroye
 * Programming
 * October 24th
 
 /*************************************************************************************************
 *
 * lille Compiler
 *
 * Author:     Michael Oudshoorn
 *             Webb School of Engineering
 *             High Point University
 *             High Point
 *             NC   27265
 *             USA
 *
 * Written:            1 June 2020
 * Last modified:      1 June 2023
 *
 * Open Source - free to distribute and modify. May not be used for profit.
 *
 * Written using C++20.
 *         g++ -std=c++20 -o lille compiler.ccp
 *
 *
 * Usage
 *        lille [flags] filename
 * where filename contains the source code to be compiler.
 *
 * Flags are:
 *		-l 				Generate a listing file
 *		-o filename  	Generate code file with the specified name
 *		-h	Help		Generate help instructions
 *
 **************************************************************************************************/


#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <map>
#include <iterator>

#include "lille_exception.h"
#include "scanner.h"
#include "parser.h"
#include "symbol.h"
#include "error_handler.h"
#include "code_gen.h"
#include "id_table.h"
#include "semantics.h" 

using namespace std;
using namespace std::chrono;

const string default_listing_file_name = "LISTING";	// Default listing file name
const string default_source_file_name = "SOURCE";	// Default source code file name
const string default_code_filename = "CODE";	// Default code file name if one not specified on command line

bool listing_required = false;      
string source_filename;             // Input: lille source code
string code_filename;               // Output: PAL assembly code
string listing_filename;            // Output: listing with errors

// DEBUGGING FLAG
const bool DEBUGGING = false;   

// Compiler component pointers
error_handler* err = nullptr;
scanner* scan = nullptr;
id_table* id_tab = nullptr;

// process_command_line() - Parse command-line arguments
// I handle the flags and filenames from the command line.
// Returns true if everything is OK, false if there's a problem.
bool process_command_line(int argc, char *argv[]) 
{
    bool hflag = false;     // Help flag
    bool sflag = false;     // Source file provided
    bool cflag = false;     // Code file specified
    string root_filename;

    listing_required = false;
    code_filename = default_code_filename;

    if (argc < 2) {
        // No arguments - I show usage and return false
        cerr << "Usage: " << argv[0] << " filename" << endl;
        return false;
    }

    // I process each command-line argument
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "-h") {
            // Help flag - I print usage information
            if (!hflag) {
                hflag = true;
                cout << "Usage: " << argv[0] << " [flags] filename" << endl;
                cout << "    where filename is the name of the file to be compiled." << endl;
                cout << endl;
                cout << "    Valid flags are:" << endl;
                cout << "        -h              Print this help message." << endl;
                cout << "        -l              Create a listing file with errors." << endl;
                cout << "        -o filename     Name the output PAL file." << endl;
                cout << endl;
                cout << "    ==================================================" << endl;
                cout << "    ERROR RECOVERY: Scheme 1 (Panic Mode)" << endl;
                cout << "    CODE GENERATION: Yes (BONUS)" << endl;
                cout << "    ==================================================" << endl;
            }
        }
        else if (arg == "-l") {
            // Listing flag - I'll generate a listing file
            listing_required = true;
        }
        else if (arg == "-o") {
            // Output file flag - next argument is the filename
            if (i + 1 < argc) {
                code_filename = argv[++i];
                cflag = true;
            } else {
                cerr << "Output filename expected after -o." << endl;
                return false;
            }
        }
        else if (arg[0] == '-') {
            // Unknown flag
            cerr << "Illegal flag: " << arg << endl;
            return false;
        }
        else {
            // Must be the source filename
            sflag = true;
            source_filename = arg;
        }
    }

    // I set up the output filenames based on the source filename
    if (!sflag) {
        cout << "No source file provided. Using defaults." << endl;
        source_filename = default_source_file_name;
        listing_filename = default_listing_file_name;
        if (!cflag) {
            code_filename = default_code_filename;
        }
    } else {
        // I derive filenames from the source file
        // e.g., "program1" -> "program1.lis" and "program1.pal"
        size_t dot_pos = source_filename.find(".");
        if (dot_pos != string::npos) {
            root_filename = source_filename.substr(0, dot_pos);
        } else {
            root_filename = source_filename;
        }
        
        listing_filename = root_filename + ".lis";
        if (!cflag) {
            code_filename = root_filename + ".pal";
        }
    }

    return true;
}

// MAIN FUNCTION
// This is where everything comes together.
int main(int argc, char *argv[])
{
    // I use these to measure compilation time
    high_resolution_clock::time_point start;
    high_resolution_clock::time_point stop;
    milliseconds time_span;

    // I start the timer
    start = high_resolution_clock::now();

    // I process the command line
    bool status = process_command_line(argc, argv);

    if (status) {
        try {
            // STEP 1: Create the error handler
            // The error handler collects and reports errors.
            // If listing_required is true, it will also generate a listing.
            if (!listing_required) {
                err = new error_handler(source_filename);
            } else {
                err = new error_handler(source_filename, listing_filename);
            }

            // STEP 2: Create the symbol table
            // The id_table holds all declared identifiers and their info.
            // It supports nested scopes for blocks and procedures.
            id_tab = new id_table(err);

            // STEP 3: Create the scanner
            // The scanner breaks the source code into tokens.
            // It handles comments, strings, numbers, identifiers, etc.
            scan = new scanner(source_filename, id_tab, err);

            // This checks types, scopes, and usage rules.
            // I also install the built-in functions here.
            semantics sem(id_tab, err);
            sem.install_builtins();  // Add int2real, int2string, etc.

            // I only create this if we're going to generate code.
            // I'll check for errors before actually writing the file.
            code_gen* code = new code_gen(code_filename);

            // This is where all the magic happens. The parser:
            // - Reads tokens from the scanner
            // - Checks syntax (recursive descent)
            // - Calls semantics for type/scope checking
            // - Calls code_gen to emit PAL instructions
            // - Implements Scheme 1 error recovery
            Parser p(scan, err, &sem, code);
            
            // I parse the entire program
            p.parse_program();

            // STEP 7: Handle results
            
            // I generate the listing if requested
            if (listing_required) {
                err->generate_listing();
            }

            // I finalize code generation only if there were NO errors
            if (err->error_count() == 0) {
                code->finalize();
                cout << "Code generation successful: " << code_filename << endl;
            } else {
                // I don't generate code if there were errors
                cout << "Code generation skipped due to errors." << endl;
            }

            // I clean up
            delete code;

            // STEP 8: Report timing
            stop = high_resolution_clock::now();
            time_span = duration_cast<milliseconds>(stop - start);

            cout << endl;
            cout << "==================================================" << endl;
            cout << "Compilation completed in " << time_span.count() 
                 << " milliseconds with " << err->error_count() 
                 << " error(s) found." << endl;
            cout << "==================================================" << endl;
            cout << endl;
            cout << "Error Recovery: Scheme 1 (Panic Mode with Recovery Flag)" << endl;
            cout << "Code Generation: Yes (BONUS PHASE IMPLEMENTED)" << endl;
            cout << "==================================================" << endl;

        }
        catch (lille_exception& e) {
            cerr << "Lille Exception: " << e.what() << endl;
            return 1;
        }
        catch (exception& e) {
            cerr << "Exception: " << e.what() << endl;
            return 1;
        }
    }

    return 0;
}