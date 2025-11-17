int bad_redeclare_local() {
    int x;
    int x;      // redeclaration in same block, should fail
    x = 3;
    return x;
}
