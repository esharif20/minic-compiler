int foo(int x, float y) {
    return x;
}

int bad_call_args() {
    float a;
    a = 2.0;

    // First argument: float -> int is narrowing, should be rejected
    // Second argument: int -> float is widening, ok
    return foo(a, 3);
}
