#if 0
#include <cstdio>
#include "../test_generator.h"

#ifndef NO_MAIN
int main(int argc, char** argv) {
    if (argc < 2) {
        return -1;
    }

    if (!Teakra::Test::GenerateTestCasesToFile(argv[1])) {
        std::fprintf(stderr, "Unable to successfully generate all tests.\n");
        return -2;
    }

    return 0;
}

#endif // NO_MAIN

#endif
