#include <unistd.h>

#include <iostream>

#include "blet/logger.h"

#define NB_THREAD 10

int nbLogs = 100000;
unsigned int nbThread = 0;

void* threadLog(void* e) {
    (void)e;
    for (int i = 0; i < nbLogs; ++i) {
        LOGGER_DEBUG_FMT_P(("test"));
        // usleep(1000);
    }
    return NULL;
}

int main(int argc, char** argv) {
    // std::cerr << ::pthread_self() << std::endl;
    // std::cerr << ::pthread_self() % 101 << std::endl;
    (void)argc;
    (void)argv;
    // blet::Logger::getMain().setAllFormat("{decimal} {message}");
    // blet::Logger::getMain().disableLevel(blet::Logger::DEBUG);
    pthread_t* threadId = NULL;
    if (NB_THREAD > 0) {
        threadId = new pthread_t[NB_THREAD];
    }
    for (int i = 0; i < NB_THREAD; ++i) {
        pthread_create(&threadId[i], NULL, &threadLog, &i);
    }

    timespec startTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    for (int i = 0; i < nbLogs; ++i) {
        LOGGER_DEBUG_FMT_P(("test"));
        // usleep(1000);
    }

    timespec endTime;
    clock_gettime(CLOCK_MONOTONIC, &endTime);

    if ((endTime.tv_nsec - startTime.tv_nsec) < 0) {
        endTime.tv_nsec = 1000000000 + (endTime.tv_nsec - startTime.tv_nsec);
        endTime.tv_sec -= 1;
    }
    else {
        endTime.tv_nsec = endTime.tv_nsec - startTime.tv_nsec;
    }
    // fprintf(stderr, "***Time: %ld.%09ld\n", (endTime.tv_sec - startTime.tv_sec), endTime.tv_nsec);

    for (int i = 0; i < NB_THREAD; ++i) {
        pthread_join(threadId[i], NULL);
    }

    if (NB_THREAD > 0) {
        delete[] threadId;
    }

    return 0;
}