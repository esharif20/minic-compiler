#include <iostream>
#include <cstdio>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" DLLEXPORT int print_int(int X) {
  std::fprintf(stderr, "%d\n", X);
  return 0;
}

extern "C" DLLEXPORT float print_float(float X) {
  std::fprintf(stderr, "%f\n", X);
  return 0;
}

// Match the MiniC function name
extern "C" {
    int forward_test(int n);
}

int main() {
    int result = forward_test(41);  // expect 42

    if (result == 42) {
        std::cout << "PASSED Result: " << result << std::endl;
        return 0;
    } else {
        std::cout << "FAILED Result: " << result << std::endl;
        return 1;
    }
}
