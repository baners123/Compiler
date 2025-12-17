

## PROJECT OVERVIEW

This is a complete compiler for the "lille" programming language that translates
lille source code into PAL (Pseudo Assembly Language) code that can be executed
by the PAL machine interpreter.

## ERROR RECOVERY IMPLEMENTATION

### Scheme Implemented: SCHEME 1 (Panic Mode with Recovery Flag)

I chose to implement Scheme 1 from Lecture 15 because it is effective at
preventing cascading errors while being straightforward to implement.

#### How Scheme 1 Works:

1. **Recovery Flag**: I maintain a boolean flag called `recovering` that tracks
   whether I am currently in error recovery mode.

2. **Error Detection**: When I detect an unexpected token:
   - If `recovering` is FALSE: I report the error and set `recovering = true`
   - If `recovering` is TRUE: I do NOT report the error (prevents cascading)

3. **Synchronization**: After detecting an error, I skip tokens until I find
   a "safe" token from a predefined set (like semicolons, END, ELSE, etc.)

4. **Resume Normal Parsing**: When I find a safe token, I set `recovering = false`
   and continue parsing normally.

#### Benefits of Scheme 1:
- Prevents the classic problem where one missing semicolon causes 50 error messages
- Simple to implement - just one boolean flag
- Effective at finding the next valid statement
- Does not generate excessive or unrelated errors

#### Implementation Location:
The Scheme 1 logic is primarily in `parser.cpp`:
- `expect()` method (lines 100-140): Main error recovery logic
- `synchronize()` method (lines 145-165): Skips to safe tokens
- `flag_here()` and `flag_prev()` methods: Error reporting with recovery flag check

## CODE GENERATION (BONUS)

### Implemented: YES

I implemented the bonus code generation phase. The compiler generates valid PAL
assembly code that can be executed by the PAL machine interpreter.

#### Generated PAL Instructions:
- LCI, LCR, LCS: Load constants (integer, real, string)
- LDV: Load variable value
- LDA: Load variable address
- STO: Store to variable
- STI: Store indirect
- OPR: Operations (arithmetic, comparisons, I/O, conversions)
- JMP: Unconditional jump
- JIF: Jump if false
- MST: Mark stack for procedure call
- CAL: Call procedure/function
- INC: Increment stack pointer
- RDI, RDR: Read integer/real

#### Code Generation Location:
- `code_gen.h` and `code_gen.cpp`: All PAL code generation logic

## FILES IN THIS SUBMISSION

### Source Files (.cpp and .h):
1. **compiler.cpp** - Main driver that ties all phases together
2. **scanner.cpp/h** - Lexical analyzer (tokenizer)
3. **parser.cpp/h** - Recursive descent parser with Scheme 1 error recovery
4. **semantics.cpp/h** - Type checking and scope management
5. **code_gen.cpp/h** - PAL code generation (BONUS)
6. **id_table.cpp/h** - Symbol table with scoped lookup
7. **error_handler.cpp/h** - Error reporting and listing generation
8. **symbol.cpp/h** - Token symbol types
9. **token.cpp/h** - Token representation
10. **lille_type.cpp/h** - Type system
11. **lille_kind.cpp/h** - Identifier kinds (variable, constant, etc.)
12. **lille_exception.cpp/h** - Exception handling

### Build Files:
- **makefile** - Builds the compiler with `make`

## HOW TO BUILD

```bash
# Build the compiler
make

# Clean and rebuild
make clean
make

# Run tests on all sample programs
make test
```

## HOW TO RUN

```bash
# Basic usage
./compiler program1

# With listing file
./compiler -l program1

# With custom output name
./compiler -o myoutput.pal program1

# Show help
./compiler -h
```

## SYMBOL TABLE OUTPUT

As required by the spec, the compiler prints the full symbol table every time
it exits a scope. This shows:
- Scope level
- Token name
- Type
- Kind
- Other attributes

## TEST RESULTS

All 9 sample programs compile with 0 errors:
- program1: 0 errors ✓
- program2: 0 errors ✓
- program3: 0 errors ✓
- program4: 0 errors ✓
- program5: 0 errors ✓
- program6: 0 errors ✓
- program7: 0 errors ✓
- program8: 0 errors ✓
- program9: 0 errors ✓

## DEBUGGING MODE

IMPORTANT: Debugging mode is DISABLED for submission.
The `DEBUGGING` constant in `compiler.cpp` is set to `false`.

## KEY FEATURES

1. **Zero Errors on Valid Programs**: All 9 sample programs compile without errors
2. **Scheme 1 Error Recovery**: Prevents cascading errors
3. **Symbol Table Dump**: Prints full symbol table when exiting each scope
4. **Code Generation**: Generates valid PAL assembly code (BONUS)
5. **Heavy Commenting**: All code is thoroughly commented in first person
6. **Proper Error Messages**: Uses the standard error codes from the kit

## SUMMARY

- Error Recovery: **Scheme 1 (Panic Mode with Recovery Flag)**
- Code Generation: **Yes (BONUS IMPLEMENTED)**
- Test Programs: **All 9 compile with 0 errors**
- Symbol Table: **Dumps on every scope exit**
- Comments: **Heavy first-person commenting throughout**
