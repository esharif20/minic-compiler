// F1_scalar_to_array_param.c
void foo(int a[2]) { a[0] = 3; }
int main() {
    int x;
    foo(x);
    return 0;
}
