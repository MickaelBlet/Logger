#include <gtest/gtest.h>

#include "blet/logger.h"

GTEST_TEST(logger, littleflush) {
    fprintf(stdout, "%10$.0s%4$.0s%6$.0s%2$.0s%5$.0s%3$.0s%7$.0s%8$.0s%1$s - %9$s - %1$s\n", "main", "toto", "", "", 10, "", 10, "", "test", 100);
    fflush(stdout);
    LOGGER_MAIN().setAllFormat("{name} - {message} - {name}");
    for (int i = 0; i < 1000; ++i) {
        testing::internal::CaptureStdout();
        LOGGER_DEBUG("main - test - main");
        LOGGER_DEBUG("main - test - main");
        LOGGER_FLUSH();
        std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(output, "main - test - main\n");
    }
}

GTEST_TEST(logger, bigflush) {
    LOGGER_MAIN().setAllFormat("{message}");
    testing::internal::CaptureStdout();
    std::ostringstream oss("");
    for (int i = 0; i < 1000; ++i) {
        LOGGER_DEBUG("test");
        oss << "test\n";
    }
    LOGGER_FLUSH();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, oss.str());
}