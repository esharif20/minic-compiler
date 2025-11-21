// invalid_scope_arrays.c
// Mixed scope, array and typing errors for MiniC semantic checks

extern int ext_int(int x);
extern float ext_mul(float a, float b);

int g;
int arr1[4];
float mat[2][3];

int bad_scope_arrays(int n) {
    int i;
    int i;                 // ERROR: redeclaration of local 'i'
    g = 1;                 // OK: global
    {
        int g;             // OK: shadows global
        g = 2;
        j = 3;             // ERROR: 'j' undeclared in inner block
    }
    return h;              // ERROR: 'h' undeclared in function
}

void bad_array_usage() {
    int i;
    float f;
    bool b;

    arr1[0] = 1;           // OK
    arr1[1] = 2;           // OK

    f = arr1[0] * mat[0][0]; // OK: int * float, int widened to float

    f = arr1;              // ERROR: array to scalar assignment
    arr1[1.5] = 3;         // ERROR: non int index
    mat[0][0] = 1.0;       // OK

    f = mat[0];            // ERROR: too few indices for 2D array
    mat[0][0][1] = 2.0;    // ERROR: too many indices

    b = mat[0][0];         // ERROR: narrowing float to bool
    i = mat[0][0];         // ERROR: narrowing float to int
}

float bad_calls_and_returns() {
    int i;
    float f;
    bool b;

    i = ext_int(1.5);      // ERROR: float to int parameter narrows
    f = ext_mul(2.0, g);   // OK: int -> float widening
    b = ext_int(1);        // ERROR: int to bool assignment narrows

    if (g) {
        return 1;          // OK: int -> float widening
    }
    return g % 2.0;        // ERROR: '%' on float
}

int main() {
    int x;
    float y;

    x = bad_scope_arrays(3);
    bad_array_usage();
    y = bad_calls_and_returns();

    arr1[y] = 5;           // ERROR: float index
    z = 10;                // ERROR: 'z' undeclared at global scope

    return 0;
}
