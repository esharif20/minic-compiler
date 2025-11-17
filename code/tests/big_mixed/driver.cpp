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

// MiniC function under test
extern "C" {
    int big_mixed();
}

int main() {
    int result = big_mixed();
    if (result == 4) {
        std::cout << "PASSED Result: " << result << std::endl;
    } else {
        std::cout << "FAILED Result: " << result << std::endl;
    }
}
