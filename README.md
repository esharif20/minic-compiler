# MiniC Compiler

A compiler implementation for the MiniC programming language, developed as coursework for CS325. This compiler translates MiniC source code through lexical analysis, parsing, semantic analysis, and intermediate representation generation.

## Overview

MiniC is a subset of the C programming language designed for educational purposes. This compiler implements a complete compilation pipeline from source code to intermediate representation, suitable for further optimization or code generation.

## Project Structure

```
minic-compiler/
├── code/
│   ├── mccomp.cpp           # Main compiler implementation
│   ├── Makefile             # Build configuration
│   └── tests/               # Test suite with sample programs
│       ├── addition/
│       ├── fibonacci/
│       ├── factorial/
│       └── tests.sh         # Test runner script
└── grammar/
    └── task_1_2_1.txt       # Grammar transformations and analysis
```

## Prerequisites

Before building the compiler, ensure you have the following installed:

- C++ compiler with C++11 support (GCC 4.8+ or Clang 3.4+)
- GNU Make
- Standard C++ libraries

## Building the Compiler

Navigate to the source directory and compile:

```bash
cd code
make
```

This generates the `mccomp` executable in the current directory.

### Build Options

```bash
make clean          # Remove all compiled objects and executables
make rebuild        # Clean and rebuild from scratch
```

## Usage

### Basic Compilation

Compile a MiniC source file:

```bash
./mccomp <input-file.c>
```

### Example

```bash
./mccomp tests/addition/addition.c
```

### Output

The compiler produces:
- Parse tree visualization (if enabled)
- Intermediate representation (IR) code
- Error messages for syntax or semantic errors

## Testing

### Running the Test Suite

Execute all test cases to verify compiler correctness:

```bash
cd code
./tests/tests.sh
```

### Compile-Only Tests

Run tests without executing generated code:

```bash
./tests/tests.sh -compile_only
```

### IR Generation Tests

Test intermediate representation generation:

```bash
./tests/tests.sh -ir
```

### Running Individual Tests

Test a specific program:

```bash
./mccomp tests/fibonacci/fibonacci.c
```

## MiniC Language Features

The MiniC language supports:

- Integer and floating-point arithmetic
- Variable declarations and assignments
- Control flow structures (if/else, while loops)
- Function definitions and calls
- Expressions with standard operators (+, -, *, /, %, ==, !=, <, >, <=, >=)
- Block scoping

### Example Program

```c
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    int result;
    result = factorial(5);
    return result;
}
```

## Compilation Stages

The compiler implements the following phases:

1. **Lexical Analysis** - Tokenizes the input source code
2. **Syntax Analysis** - Builds a parse tree based on the MiniC grammar
3. **Semantic Analysis** - Performs type checking and scope resolution
4. **IR Generation** - Produces intermediate representation code

## IR Validation with LLVM Tools (Task 2.1.3)

After `mccomp` generates `output.ll`, use the LLVM tools to check that the IR is valid and that the program behaves as expected.

### Running the LLVM Interpreter

Execute the generated LLVM IR directly:

```bash
lli output.ll
```

- `lli` runs the LLVM IR with the LLVM interpreter
- It executes the `main` function in `output.ll`
- The process exit status holds the return value of `main`

Check the return value:

```bash
echo $?
```

For example, if `main` returns 42, `echo $?` prints 42.

### Generating Native Assembly and Executable

Compile LLVM IR to native assembly:

```bash
llc output.ll
```

This generates `output.s`, a native assembly file.

Assemble and link to create an executable:

```bash
clang output.s -o prog
```

Run the executable and verify the return code:

```bash
./prog
echo $?
```

This confirms that the compiled program returns the expected value.

### Comparing with Clang-Generated IR

Generate LLVM IR for the same MiniC program using Clang as a reference:

```bash
clang -cc1 tests/your_test/your_test.c -emit-llvm -o your_test.clang.ll
```

- `clang -cc1` runs the Clang front end and emits LLVM IR
- `your_test.clang.ll` holds Clang's version of the IR for the same source
- Compare `output.ll` from `mccomp` with `your_test.clang.ll` to debug or cross-check the IR

This reference IR is useful for:
- Verifying correct IR structure
- Debugging code generation issues
- Understanding optimal IR patterns

## Grammar

The formal grammar specification and transformations are documented in:

```
grammar/task_1_2_1.txt
```

This includes:
- Original grammar definition
- Left-recursion elimination
- Left-factoring transformations
- FIRST and FOLLOW set calculations

## Development

### Adding New Test Cases

1. Create a new directory under `tests/`
2. Add your MiniC source file with a `.c` extension
3. Include expected output or error messages
4. Update the test runner script if needed

### Debugging

Enable verbose output during compilation:

```bash
./mccomp -v tests/your-test/test.c
```

## Known Limitations

- Limited standard library support
- No preprocessor directives
- No pointer arithmetic
- No struct or union types
- Single-file compilation only

## Troubleshooting

### Common Issues

**Compilation fails with "command not found":**
- Ensure you're in the `code/` directory
- Verify the executable has correct permissions: `chmod +x mccomp`

**Makefile errors:**
- Check that GNU Make is installed: `make --version`
- Verify C++ compiler is in your PATH

**Test failures:**
- Ensure all test input files are present in `tests/`
- Check file permissions on `tests.sh`: `chmod +x tests/tests.sh`


