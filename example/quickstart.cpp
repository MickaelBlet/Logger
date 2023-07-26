#include "blet/logger.h"

#define NB_THREAD 5

unsigned int nbThread = 0;

void *threadLog(void*e) {
    (void)e;
    for (int i = 0; i < 1000000; ++i) {
        LOGGER_DEBUG("test" << ':' << i);
    }
    return NULL;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    blet::Logger::getMain().setAllFormat("{decimal}: {message}");
    // blet::Logger::getMain().disableLevel(::blet::Logger::DEBUG);
    pthread_t threadId[NB_THREAD];
    for (int i = 0; i < NB_THREAD; ++i) {
        pthread_create(&threadId[i], NULL, &threadLog, &i);
    }

    LOGGER_EMERG("x");
    LOGGER_ALERT("x");
    LOGGER_CRIT("x");
    LOGGER_ERROR("x");
    LOGGER_WARN("x");
    LOGGER_NOTICE("x");
    LOGGER_INFO("x");
    LOGGER_DEBUG("x");

    LOGGER_DEBUG_P(("%s", "test"));

    LOGGER_FLUSH();
    // for (int i = 0; i < 1000000; ++i) {
    //     blet::Logger::getMain().asyncLog(blet::Logger::DEBUG, "woot?");
    // }

    for (int i = 0; i < NB_THREAD; ++i) {
        pthread_join(threadId[i], NULL);
    }

    LOGGER_FLUSH();
    LOGGER_DEBUG("test");

    return 0;
}