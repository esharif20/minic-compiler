// invalid_typing_complex.c
// Rich mixture of typing and semantic errors, plus some valid code

// External functions (assume correct)
extern int ext_add_int(int a, int b);
extern float ext_scale(float x, float y);
extern bool ext_flag(bool f);

// Global variables and arrays
int gi;
float gf;
bool gb;

int iarr[3];              // 1D int array
float fmat[2][3];         // 2D float array
int cube[2][2][2];        // 3D int array

int bad_globals() {
    gi = 3;               // OK: int -> int
    gf = gi;              // OK: int -> float (widening)
    gb = true;            // OK: bool -> bool

    gi = gf;              // ERROR: narrowing float -> int in assignment
    gb = gi;              // ERROR: narrowing int -> bool in assignment
    gb = gf;              // ERROR: narrowing float -> bool in assignment

    gf = gf % 2.0;        // ERROR: '%' not allowed for float operands

    // Use scalar as array
    gf[0] = 1.0;          // ERROR: 'gf' is not an array or pointer in access

    return 0;
}

// Test array indexing rules and element typing
void array_mistakes() {
    int i;
    float x;

    i = 0;
    while (i < 3) {
        iarr[i] = i;      // OK
        i = i + 1;
    }

    // Too many indices on 1D array
    iarr[1][2] = 5;       // ERROR: too many indices supplied for array 'iarr'

    // Index type errors
    fmat[1.5][2] = 1.0;   // ERROR: array index for 'fmat' must be int
    fmat[1][2.0] = 2.0;   // ERROR: array index for 'fmat' must be int

    // Too few indices on multi-dimensional array
    x = fmat[1];          // ERROR: too few indices supplied for array 'fmat'

    // Access 3D array with wrong index count
    cube[0][1][1] = 7;    // OK: 3 indices for 3D array
    cube[0] = 1;          // ERROR: too few indices supplied for array 'cube'
    // cube[0][1][1][0] = 9; // ERROR: too many indices supplied for array 'cube'

    // Array element type vs assignment
    x = iarr[0];          // OK: int -> float (widening)
    i = fmat[0][0];       // ERROR: narrowing float -> int in assignment
}

// Array parameter tests (pointer-style semantics)
int sum_first_row(int n, int mat[3][4]) {
    int s;
    int j;

    s = 0;
    j = 0;
    while (j < 4) {
        s = s + mat[0][j];   // OK
        j = j + 1;
    }

    // Too many indices on 2D parameter
    s = mat[1][2][0];        // ERROR: too many indices for pointer-to-array 'mat'

    // Non-int index on parameter
    s = mat[1.5][2];         // ERROR: array index for 'mat' must be int

    return s;
}

void fill_cube(int c[2][2][2]) {
    int i;
    int j;
    int k;

    i = 0;
    while (i < 2) {
        j = 0;
        while (j < 2) {
            k = 0;
            while (k < 2) {
                c[i][j][k] = i + j + k;  // OK
                k = k + 1;
            }
            j = j + 1;
        }
        i = i + 1;
    }

    // Too few indices for pointer-to-array
    c[1] = 5;                   // ERROR: too few indices for pointer-to-array 'c'
}

// Typing in function calls and returns
float fun_ret_errors(int x, float y, bool b) {
    int k;
    float z;
    bool flag;

    k = x;                   // OK
    z = y;                   // OK
    flag = b;                // OK

    k = y;                   // ERROR: narrowing float -> int in assignment
    flag = x;                // ERROR: narrowing int -> bool in assignment

    z = z % 2.0;             // ERROR: '%' not allowed for float operands

    if (z) {
        return 1;            // OK: int -> float (widening)
    }

    return b * y;            // OK: bool promoted to int, then to float
}

int ret_narrow_errors(float x, bool b) {
    if (b) {
        return 3.5;          // ERROR: narrowing float -> int in return
    }

    return x;                // ERROR: narrowing float -> int in return
}

bool ret_bool_errors(int x, float y) {
    if (x > 0) {
        return y;            // ERROR: narrowing float -> bool in return
    }
    return x;                // ERROR: narrowing int -> bool in return
}

int call_errors() {
    int i;
    float f;
    bool b;

    i = ext_add_int(1, 2);   // OK
    f = ext_scale(2.0, 3.0); // OK
    b = ext_flag(true);      // OK

    // Argument narrowing in calls
    i = ext_add_int(1.5, 2);   // ERROR: passing 'float' to 'int' parameter narrows
    i = ext_add_int(1, 2.5);   // ERROR: passing 'float' to 'int' parameter narrows

    f = ext_scale(1, 2.5);     // OK: int -> float widening
    f = ext_scale(1, true);    // OK: bool -> float widening through int

    b = ext_flag(1);           // ERROR: passing 'int' to 'bool' parameter narrows
    b = ext_flag(2.5);         // ERROR: passing 'float' to 'bool' parameter narrows

    // Call to user functions with wrong argument types
    f = fun_ret_errors(1.5, 2.0, true);
                               // ERROR: passing 'float' to first 'int' parameter narrows

    f = fun_ret_errors(1, 2, 0.0);
                               // ERROR: passing 'int' to second 'float' parameter is OK
                               // ERROR: passing 'float' to third 'bool' parameter narrows

    i = ret_narrow_errors(3, true);   // OK: float literal, bool literal types match
    i = ret_narrow_errors(true, false);
                               // ERROR: passing 'bool' to 'float' parameter narrows

    b = ret_bool_errors(1, 2.5);      // OK for call, errors in returns above

    return 0;
}

int main() {
    int s;
    float idx;    // move here

    bad_globals();
    array_mistakes();

    s = sum_first_row(3, cube);
    fill_cube(cube);
    call_errors();

    idx = 1.5;
    iarr[idx] = 10;  // should trigger the index-type error in codegen

    return 0;
}
