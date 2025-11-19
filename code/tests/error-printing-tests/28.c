// J1_nested_calls_arrays.c
int foo(int x) { return x; }

int main() {
    int a[3];
    return foo(a[foo(1)]);
}
