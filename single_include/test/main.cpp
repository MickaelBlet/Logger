#include <iostream>

#include "blet/logger.h"
#include "test/already_include.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    blet::Logger logger;
    logger.setName("foobar");
    test(logger);
    return 0;
}