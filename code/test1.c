// invalid_typing_multi.c
// Several separate typing errors in one file

int f(int x, float y, bool b) {
    int k;
    float z;
    bool flag;

    k = y;           // ERROR: narrowing float -> int in assignment
    flag = x;        // ERROR: narrowing int -> bool in assignment

    z = z % 2.0;     // ERROR: '%' not allowed for float operands

    if (z) {
        return 1.5;  // ERROR: narrowing float -> int in return
    }

    return b * y;    // ERROR: result is float, returned from int function
}

bool g(bool x) {
    return 5;        // ERROR: narrowing int -> bool in return
}

int main() {
    float farr[4];
    int i;

    i = 0;
    while (i < 4) {
        farr[i] = 1.0;
        i = i + 1;
    }

    i = farr[0];        // ERROR: narrowing float -> int in assignment

    i = f(1, 2.5, true);  // ERROR: float argument to int parameter in f (narrowing)
    g(1);                 // ERROR: int argument to bool parameter in g (narrowing)

    farr[1.5] = 3.0;   // ERROR: array index must be int, not float

    return 0;
}
