#include <iostream>

#include "blet/logger.h"
#include "test/already_include.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    blet::Logger logger("test");
    logger.setAllFormat("{message}");
    logger.log(blet::Logger::ALERT, "%s", "woot?");
    test(logger);

    for (int i = 0; i < 10000; ++i) {
        LOGGER_TO_DEBUG(logger, "test: " << i);
    }
    return 0;
}