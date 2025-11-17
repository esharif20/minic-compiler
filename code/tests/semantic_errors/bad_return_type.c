int bad_return_type() {
    float f;
    f = 1.5;
    return f;   // should fail: float -> int is narrowing
}
