extern int print_int(int x);

int scope_shadow() {
    int x;
    x = 1;

    {
        int x;
        x = 2;
        print_int(x);   // prints 2
    }

    print_int(x);       // prints 1
    return x;           // returns 1
}
