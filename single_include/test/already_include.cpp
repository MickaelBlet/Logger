#include "blet/logger.h"

void test(blet::Logger& logger) {
    logger.log(blet::Logger::ALERT, "%s", "woot?");
}