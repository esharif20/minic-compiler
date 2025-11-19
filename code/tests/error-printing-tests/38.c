// A6_array_param_ok.c
void foo(int a[3]) {
    a[1] = 9;
}

int main() {
    int x[3];
    foo(x);
    return 0;
}
