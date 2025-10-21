cat > README.md << 'EOF'
# MiniC Compiler

CS325 Coursework - A compiler for the MiniC language

## Project Structure
```
minic-compiler/
├── code/
│   ├── mccomp.cpp      # Main compiler source
│   ├── Makefile        # Build configuration
│   └── tests/          # Test programs in MiniC
└── tasks/
    ├── task_1_2_1.txt  # Grammar transformations
    ├── task_1_2_2.txt  # AST nodes documentation
    └── task_1_2_3.txt  # Parser implementation notes
```

## Building
```bash
cd code
make
```

## Running
```bash
cd code
./mccomp tests/addition/addition.c
```

## Testing
```bash
cd code
./tests/tests.sh -compile_only
```

## Progress

- ✅ Task 1.2.1: Grammar transformations complete
- ✅ Task 1.2.2: AST nodes added
- 🔵 Task 1.2.3: Parser implementation in progress

