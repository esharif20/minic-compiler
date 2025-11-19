// J3_complex_nested_calls.c
int f(int x) { return x + 1; }
int g(int y) { return y * 2; }

int main() {
    return f(g(f(2)));
}
