#include <iostream>

#include "blet/logger.h"
#include "test/already_include.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    blet::Logger logger("test");
    logger.log(blet::Logger::ALERT, "%s", "woot?");
    test(logger);
    return 0;
}