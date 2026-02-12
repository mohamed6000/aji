#include "general.h"

//
// This file is only here for the purpose of testing the engine API.
// In a real project, the entrypoint should be platform specific...
//

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

    print("Hello friend!\n");
    return 0;
}


#define NB_IMPLEMENTATION
#include "general.h"