// F4_call_with_array_element_wrong_dimension.c
void foo(int a[2][2]) { }

int main() {
    int x[2];
    foo(x); // wrong dimension
    return 0;
}
