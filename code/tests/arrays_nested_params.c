extern int putint(int x);

// Fill a[0..4] with 0,1,2,3,4
void fill1DSeq(int a[5]) {
    int i;
    i = 0;
    while (i < 5) {
        a[i] = i;
        i = i + 1;
    }
}

// Increment every element by delta
void inc1D(int a[5], int delta) {
    int i;
    i = 0;
    while (i < 5) {
        a[i] = a[i] + delta;
        i = i + 1;
    }
}

// Nested use, passes same array through two calls
void incTwice1D(int a[5], int d1, int d2) {
    inc1D(a, d1);
    inc1D(a, d2);
}

// Elementwise add: out[i] = x[i] + y[i]
void add1D(int x[5], int y[5], int out[5]) {
    int i;
    i = 0;
    while (i < 5) {
        out[i] = x[i] + y[i];
        i = i + 1;
    }
}

// Sum elements
int sum1D(int a[5]) {
    int i;
    int s;
    i = 0;
    s = 0;
    while (i < 5) {
        s = s + a[i];
        i = i + 1;
    }
    return s;
}

int main() {
    int a[5];
    int b[5];
    int s1;
    int s2;

    // a = 0,1,2,3,4
    fill1DSeq(a);
    // b = 0,1,2,3,4
    fill1DSeq(b);

    // incTwice1D calls inc1D twice through array parameter
    // Each element in a increases by 3
    incTwice1D(a, 1, 2);      // a = 3,4,5,6,7

    s1 = sum1D(a);            // expected 25
    add1D(a, b, b);           // b = a + original b = (3+0,4+1,5+2,6+3,7+4) = 3,5,7,9,11
    s2 = sum1D(b);            // expected 35

    putint(s1);
    putint(s2);

    // Return combined result for quick checking
    return s1 + s2;           // expected 60
}
