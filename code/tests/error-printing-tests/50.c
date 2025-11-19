// K4_expr_inside_multi_dim_access.c
int main() {
    int m[3][3];
    m[1][1 + 1] = 5;
    return 0;
}
