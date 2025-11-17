int bad_assign_narrow() {
    float f;
    int x;

    f = 3.5;
    x = f;      // should fail: float -> int is narrowing

    return x;
}
