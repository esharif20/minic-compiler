// G3_unknown_function.c
int main() {
    foo(1);
    return 0;
}
EOF~cat > 21.c << 'EOF'
// H1_redecl_local.c
int main() {
    int x;
    int x;
    return 0;
}
