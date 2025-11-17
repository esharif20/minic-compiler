// tests/ret42/ret42.c

int forward_test(int n) {
  return bar(n);      // call function defined later
}

int bar(int x) {
  return x + 1;
}
