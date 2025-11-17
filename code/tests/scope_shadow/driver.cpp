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

extern "C" {
    int scope_shadow();
}

int main() {
    int result = scope_shadow();

    if (result == 1) {
        std::cout << "PASSED Result: " << result << std::endl;
    } else {
        std::cout << "FAILED Result: " << result << std::endl;
    }

    return 0;
}
