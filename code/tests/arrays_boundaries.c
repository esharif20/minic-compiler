extern int putint(int x);

// Global arrays
int g1[4];
int g2[2][3];
int g3[2][2][2];

int main() {
    int a[4];
    int b[2][3];
    int c[2][2][2];

    // Local boundaries
    a[0] = 1;
    a[3] = 2;

    b[0][0] = 3;
    b[1][2] = 4;

    c[0][0][0] = 5;
    c[1][1][1] = 6;

    // Global boundaries
    g1[0] = 10;
    g1[3] = 20;

    g2[0][0] = 100;
    g2[1][2] = 200;

    g3[0][0][0] = 1000;
    g3[1][1][1] = 2000;

    // Print local values
    putint(a[0]);        // 1
    putint(a[3]);        // 2

    putint(b[0][0]);     // 3
    putint(b[1][2]);     // 4

    putint(c[0][0][0]);  // 5
    putint(c[1][1][1]);  // 6

    // Print global values through array access
    putint(g1[0]);       // 10
    putint(g1[3]);       // 20

    putint(g2[0][0]);    // 100
    putint(g2[1][2]);    // 200

    putint(g3[0][0][0]); // 1000
    putint(g3[1][1][1]); // 2000

    return 0;
}
