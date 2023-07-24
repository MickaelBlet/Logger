#include <gtest/gtest.h>

#include "blet/logger.h"

GTEST_TEST(logger, littleflush) {
    LOGGER_MAIN().setAllFormat("{name} - {message} - {name}");
    for (int i = 0; i < 1000; ++i) {
        testing::internal::CaptureStdout();
        LOGGER_DEBUG(("test"));
        LOGGER_FLUSH();
        EXPECT_EQ(testing::internal::GetCapturedStdout(), "main - test - main\n");
    }
}

GTEST_TEST(logger, bigflush) {
    LOGGER_MAIN().setAllFormat("{message}");
    testing::internal::CaptureStdout();
    std::ostringstream oss("");
    for (int i = 0; i < 1000; ++i) {
        LOGGER_DEBUG(("test"));
        oss << "test\n";
    }
    LOGGER_FLUSH();
    EXPECT_EQ(testing::internal::GetCapturedStdout(), oss.str());
}