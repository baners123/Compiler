# === Minimal fix Makefile (keep the current structure) ===

CXX := /usr/bin/clang++          
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra \
  -isysroot $(shell xcrun --show-sdk-path) -stdlib=libc++

all: compiler
	@echo Compilation complete.

# Final link: include ALL objects compiled
compiler: compiler.o error_handler.o lille_exception.o scanner.o id_table.o symbol.o token.o \
           lille_type.o lille_kind.o parser.o semantics.o code_gen.o
	$(CXX) $(CXXFLAGS) -o $@ $^

compiler.o: compiler.cpp id_table.o error_handler.o lille_exception.o scanner.o symbol.o token.o
	$(CXX) $(CXXFLAGS) -c $<

error_handler.o: error_handler.cpp error_handler.h token.o lille_exception.o
	$(CXX) $(CXXFLAGS) -c $<

id_table.o: id_table.cpp id_table.h
	$(CXX) $(CXXFLAGS) -c $<

lille_exception.o: lille_exception.cpp lille_exception.h
	$(CXX) $(CXXFLAGS) -c $<

scanner.o: scanner.cpp scanner.h error_handler.o lille_exception.o token.o symbol.o id_table.o
	$(CXX) $(CXXFLAGS) -c $<

symbol.o: symbol.cpp symbol.h
	$(CXX) $(CXXFLAGS) -c $<

token.o: token.cpp token.h lille_exception.o symbol.o
	$(CXX) $(CXXFLAGS) -c $<

lille_type.o: lille_type.cpp lille_type.h
	$(CXX) $(CXXFLAGS) -c $<

lille_kind.o: lille_kind.cpp lille_kind.h
	$(CXX) $(CXXFLAGS) -c $<

parser.o: parser.cpp parser.h scanner.h token.h symbol.h error_handler.h semantics.h lille_type.h lille_kind.h id_table.h code_gen.h
	$(CXX) $(CXXFLAGS) -c $<

semantics.o: semantics.cpp semantics.h id_table.h lille_type.h lille_kind.h error_handler.h token.h symbol.h
	$(CXX) $(CXXFLAGS) -c $<

code_gen.o: code_gen.cpp code_gen.h lille_exception.h lille_type.h symbol.h
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f compiler *.o *.lst
	@echo Clean complete.

