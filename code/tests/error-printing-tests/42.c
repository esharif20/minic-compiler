// F3_wrong_type_array_element.c
void foo(float z) { }

int main() {
    int a[4];
    foo(a[2]);
    return 0;
}
