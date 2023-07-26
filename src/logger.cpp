/**
 * logger.cpp
 *
 * Licensed under the MIT License <http://opensource.org/licenses/MIT>.
 * Copyright (c) 2023 BLET Mickaël.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "blet/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include <iostream>
#include <list>
#include <map>
#include <string>

// printf(FORMAT, level, name, path, file, line, func, pid, time, message)

#define LOGGER_OPEN_BRACE  static_cast<char>(-41)
#define LOGGER_SEPARATOR   static_cast<char>(-42)
#define LOGGER_CLOSE_BRACE static_cast<char>(-43)

// #ifdef LOGGER_COLOR_LEVELS
#ifdef LOGGER_DEFAULT_FORMAT
#undef LOGGER_DEFAULT_FORMAT
#endif
#define LOGGER_COLOR_RESET "\033[0m"
#define LOGGER_COLOR_BLACK_FG "\033[30m"
#define LOGGER_COLOR_RED_FG "\033[31m"
#define LOGGER_COLOR_GREEN_FG "\033[32m"
#define LOGGER_COLOR_YELLOW_FG "\033[33m"
#define LOGGER_COLOR_BLUE_FG "\033[34m"
#define LOGGER_COLOR_MAGENTA_FG "\033[35m"
#define LOGGER_COLOR_CYAN_FG "\033[36m"
#define LOGGER_COLOR_GRAY_FG "\033[37m"
#define LOGGER_COLOR_RED_BG "\033[41m"
#define LOGGER_COLOR_GREEN_BG "\033[42m"
#define LOGGER_COLOR_YELLOW_BG "\033[43m"
#define LOGGER_COLOR_BLUE_BG "\033[44m"
#define LOGGER_COLOR_MAGENTA_BG "\033[45m"
#define LOGGER_COLOR_CYAN_BG "\033[46m"
#define LOGGER_COLOR_GRAY_BG "\033[47m"

#define LOGGER_DEFAULT_FORMAT " [{pid}:{tid}] {name:%10s}: {time}.{decimal:%03d}:{file: %25s:}{line:%-3d} {message}"
#define LOGGER_EMERGENCY_FORMAT LOGGER_COLOR_MAGENTA_BG LOGGER_COLOR_BLACK_FG "{level:%-6s}" LOGGER_DEFAULT_FORMAT LOGGER_COLOR_RESET
#define LOGGER_ALERT_FORMAT LOGGER_COLOR_MAGENTA_FG "{level:%-6s}" LOGGER_DEFAULT_FORMAT LOGGER_COLOR_RESET
#define LOGGER_CRITICAL_FORMAT LOGGER_COLOR_RED_BG LOGGER_COLOR_BLACK_FG "{level:%-6s}" LOGGER_DEFAULT_FORMAT LOGGER_COLOR_RESET
#define LOGGER_ERROR_FORMAT LOGGER_COLOR_RED_FG "{level:%-6s}" LOGGER_DEFAULT_FORMAT LOGGER_COLOR_RESET
#define LOGGER_WARNING_FORMAT LOGGER_COLOR_YELLOW_FG "{level:%-6s}" LOGGER_DEFAULT_FORMAT LOGGER_COLOR_RESET
#define LOGGER_NOTICE_FORMAT LOGGER_COLOR_CYAN_FG "{level:%-6s}" LOGGER_DEFAULT_FORMAT LOGGER_COLOR_RESET
#define LOGGER_INFO_FORMAT LOGGER_COLOR_BLUE_FG "{level:%-6s}" LOGGER_DEFAULT_FORMAT LOGGER_COLOR_RESET
#define LOGGER_DEBUG_FORMAT LOGGER_COLOR_GREEN_FG "{level:%-6s}" LOGGER_DEFAULT_FORMAT LOGGER_COLOR_RESET
// #endif

namespace blet {

struct Logger::DebugPerf {
    DebugPerf(const char* name_) :
    name(name_),
    messageCount(0),
    messagePrinted(0) {
        clock_gettime(CLOCK_MONOTONIC, &startTime);
    }
    ~DebugPerf() {
        ::timespec endTs;
        clock_gettime(CLOCK_MONOTONIC, &endTs);
        if ((endTs.tv_nsec - startTime.tv_nsec) < 0) {
            endTs.tv_nsec = 1000000000 + (endTs.tv_nsec - startTime.tv_nsec);
            endTs.tv_sec -= 1;
        }
        else {
            endTs.tv_nsec = endTs.tv_nsec - startTime.tv_nsec;
        }
        fprintf(stderr, "LOGGER_PERF %s:\n", name.c_str());
        fprintf(stderr, "- Time: %ld.%09ld\n", (endTs.tv_sec - startTime.tv_sec), endTs.tv_nsec);
        fprintf(stderr, "- Message counted: %u\n", messageCount);
        fprintf(stderr, "- Message printed: %u\n", messagePrinted);
        fprintf(stderr, "- Message lost: %u\n", messageCount - messagePrinted);
        fprintf(stderr, "- Message rate: %f\n", messagePrinted / ((endTs.tv_sec - startTime.tv_sec) * 1000000000.0 + endTs.tv_nsec) * 1000000000);
        fflush(stderr);
    }
    std::string name;
    ::timespec startTime;
    volatile unsigned int messageCount;
    volatile unsigned int messagePrinted;
};

Logger::Logger(const char* name_) :
    name(name_),
    _isStarted(true),
    _currentMessageId(0),
    _levelFilter((1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<6)|(1<<7)),
    _messages(new Message[LOGGER_QUEUE_SIZE]),
    _messagesSwap(new Message[LOGGER_QUEUE_SIZE]),
    _formats(new Format[DEBUG + 1]),
    _perf(NULL) {
    // default file
    _pfile = stdout;
    // default format
    // setAllFormat(LOGGER_DEFAULT_FORMAT);
    setTypeFormat(EMERGENCY, LOGGER_EMERGENCY_FORMAT);
    setTypeFormat(ALERT, LOGGER_ALERT_FORMAT);
    setTypeFormat(CRITICAL, LOGGER_CRITICAL_FORMAT);
    setTypeFormat(ERROR, LOGGER_ERROR_FORMAT);
    setTypeFormat(WARNING, LOGGER_WARNING_FORMAT);
    setTypeFormat(NOTICE, LOGGER_NOTICE_FORMAT);
    setTypeFormat(INFO, LOGGER_INFO_FORMAT);
    setTypeFormat(DEBUG, LOGGER_DEBUG_FORMAT);
    // init thread
    if (pthread_mutex_init(&_logMutex, NULL)) {
        throw Exception("pthread_mutex_init: ", strerror(errno));
    }
    if (pthread_mutex_init(&_logQueue, NULL)) {
        throw Exception("pthread_mutex_init: ", strerror(errno));
    }
    if (pthread_cond_init(&_condLog, NULL)) {
        throw Exception("pthread_cond_init: ", strerror(errno));
    }
    if (pthread_cond_init(&_condFlush, NULL)) {
        throw Exception("pthread_cond_init: ", strerror(errno));
    }
    if (sem_init(&_queueSemaphore, 0, 0)) {
        throw Exception("sem_init: ", strerror(errno));
    }
    if (pthread_create(&_threadLogId, NULL, &_threadLogger, this)) {
        throw Exception("pthread_create: ", strerror(errno));
    }

#ifdef LOGGER_PERF_DEBUG
    _perf = new DebugPerf(name.c_str());
#endif
}

Logger::~Logger() {
    _isStarted = false;
    // unlock thread
    sem_post(&_queueSemaphore);
    pthread_join(_threadLogId, NULL);
    sem_close(&_queueSemaphore);
    pthread_mutex_destroy(&_logMutex);
    // delete messageQueue
    delete[] _messages;
    delete[] _messagesSwap;
    delete[] _formats;

#ifdef LOGGER_PERF_DEBUG
    delete _perf;
#endif
}

void Logger::flush() {
    int semValue = 1;
    pthread_mutex_lock(&_logMutex);
    while (_isStarted && semValue > 0) {
        sem_getvalue(&_queueSemaphore, &semValue);
        if (semValue == 0) {
            sem_post(&_queueSemaphore);
        }
        // std::cerr << "Flush Wait" << semValue << std::endl;
        pthread_cond_wait(&_condFlush, &_logMutex);
        // std::cerr << "Flush UnWait" << std::endl;
    }
    pthread_mutex_unlock(&_logMutex);
    fflush(_pfile);
}

void* Logger::_threadLogger(void* e) {
    Logger* loggin = static_cast<Logger*>(e);
    loggin->_threadLog();
    return NULL;
}

void Logger::printMessage(Logger::Message& message) const {
    static const char* levelToStr[] = {
        "EMERG",
        "ALERT",
        "CRIT",
        "ERROR",
        "WARN",
        "NOTICE",
        "INFO",
        "DEBUG"
    };
    char ftime[128];

    const Format* format = &_formats[message.level];

    if (format->time.empty()) {
        ftime[0] = '\0';
    }
    else {
        struct tm t;
        localtime_r(&(message.ts.tv_sec), &t);
        strftime(ftime, 128, format->time.c_str(), &t);
    }

    const char* formatStr = NULL;
    if (message.file) {
        formatStr = format->str.c_str();
    }
    else {
        formatStr = format->strWOInfo.c_str();
    }
    fprintf(_pfile, formatStr,
            name.c_str(),
            levelToStr[message.level],
            message.file,
            message.filename,
            message.line,
            message.function,
            format->pid,
            ftime,
            message.ts.tv_nsec / format->nsecDivisor,
            message.message,
            format->threadId);
}

void Logger::_threadLog() {
    unsigned int lastMessageId;
    int semValue = 1;
    while (_isStarted || semValue > 0) {
        sem_wait(&_queueSemaphore);
        pthread_mutex_lock(&_logMutex);
        // std::cerr << "Locked: " << _currentMessageId << std::endl;
        if (_currentMessageId == 0) {
            sem_getvalue(&_queueSemaphore, &semValue);
            pthread_cond_broadcast(&_condLog);
            pthread_cond_broadcast(&_condFlush);
            pthread_mutex_unlock(&_logMutex);
            continue;
        }
        // swap messages
        Message* tmp = _messages;
        _messages = _messagesSwap;
        _messagesSwap = tmp;
        // save last message id
        lastMessageId = _currentMessageId;
        // reset current message id
        _currentMessageId = 0;
        pthread_cond_broadcast(&_condLog);
        // std::cerr << "Broadcasted: " << _currentMessageId << std::endl;
        pthread_mutex_unlock(&_logMutex);

        // std::cerr << "lastId:" << lastMessageId << std::endl;

        // call print function
        for (unsigned int i = 0; i < lastMessageId; ++i) {
            printMessage(_messagesSwap[i]);
#ifdef LOGGER_PERF_DEBUG
            ++_perf->messagePrinted;
#endif
        }

        pthread_cond_broadcast(&_condFlush);
    }
}

static void s_formatSerialize(std::string& str) {
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (i > 0 && str[i - 1] == '\\') {
            if (str[i] == '\\') {
                str.erase(i - 1, 1);
            }
            else {
                --i;
                str.erase(i, 1);
            }
        }
        else if (str[i] == '{') {
            str[i] = LOGGER_OPEN_BRACE;
        }
        else if (str[i] == ':') {
            str[i] = LOGGER_SEPARATOR;
        }
        else if (str[i] == '}') {
            str[i] = LOGGER_CLOSE_BRACE;
        }
    }
}

static void s_formatDeserialize(std::string& str) {
    for (std::size_t i = 0; i < str.size(); ++i) {
        switch (str[i]) {
            case LOGGER_OPEN_BRACE:
                str[i] = '{';
                break;
            case LOGGER_SEPARATOR:
                str[i] = ':';
                break;
            case LOGGER_CLOSE_BRACE:
                str[i] = '}';
                break;
        }
    }
}

static void s_formatInsertId(std::string& format, int formatId) {
    static const char* idToStr[] = {
        "",
        "1$",
        "2$",
        "3$",
        "4$",
        "5$",
        "6$",
        "7$",
        "8$",
        "9$",
        "10$",
        "11$"
    };
    std::size_t percentPos = format.find('%');
    if (percentPos != format.npos) {
        format.insert(percentPos + 1, idToStr[formatId]);
    }
}

static int s_getDecimalDivisor(const std::string& format) {
    static const std::pair<std::string, int> keyToDivisorPairs[] = {
        std::pair<std::string, int>("%d", 1),
        std::pair<std::string, int>("%i", 1),
        std::pair<std::string, int>("%1d", 100000000),
        std::pair<std::string, int>("%1i", 100000000),
        std::pair<std::string, int>("%01d", 100000000),
        std::pair<std::string, int>("%01i", 100000000),
        std::pair<std::string, int>("%2d", 10000000),
        std::pair<std::string, int>("%2i", 10000000),
        std::pair<std::string, int>("%02d", 10000000),
        std::pair<std::string, int>("%02i", 10000000),
        std::pair<std::string, int>("%3d", 1000000),
        std::pair<std::string, int>("%3i", 1000000),
        std::pair<std::string, int>("%03d", 1000000),
        std::pair<std::string, int>("%03i", 1000000),
        std::pair<std::string, int>("%4d", 100000),
        std::pair<std::string, int>("%4i", 100000),
        std::pair<std::string, int>("%04d", 100000),
        std::pair<std::string, int>("%04i", 100000),
        std::pair<std::string, int>("%5d", 10000),
        std::pair<std::string, int>("%5i", 10000),
        std::pair<std::string, int>("%05d", 10000),
        std::pair<std::string, int>("%05i", 10000),
        std::pair<std::string, int>("%6d", 1000),
        std::pair<std::string, int>("%6i", 1000),
        std::pair<std::string, int>("%06d", 1000),
        std::pair<std::string, int>("%06i", 1000),
        std::pair<std::string, int>("%7d", 100),
        std::pair<std::string, int>("%7i", 100),
        std::pair<std::string, int>("%07d", 100),
        std::pair<std::string, int>("%07i", 100),
        std::pair<std::string, int>("%8d", 10),
        std::pair<std::string, int>("%8i", 10),
        std::pair<std::string, int>("%08d", 10),
        std::pair<std::string, int>("%08i", 10),
        std::pair<std::string, int>("%9d", 1),
        std::pair<std::string, int>("%9i", 1),
        std::pair<std::string, int>("%09d", 1),
        std::pair<std::string, int>("%09i", 1)
    };
    static const std::map<std::string, int> keyToDivisor(
        keyToDivisorPairs, keyToDivisorPairs + sizeof(keyToDivisorPairs) / sizeof(*keyToDivisorPairs));

    std::map<std::string, int>::const_iterator cit = keyToDivisor.find(format);
    if (cit == keyToDivisor.end()) {
        return 1;
    }
    return cit->second;
}

Logger::Format Logger::_formatContructor(const char* str) {
    static const std::pair<eFormat, const char*> idToEmptyPairs[] = {
        std::pair<eFormat, const char*>(NAME_FORMAT, "%1$.0s"),
        std::pair<eFormat, const char*>(LEVEL_FORMAT, "%2$.0s"),
        std::pair<eFormat, const char*>(PATH_FORMAT, "%3$.0s"),
        std::pair<eFormat, const char*>(FILE_FORMAT, "%4$.0s"),
        std::pair<eFormat, const char*>(LINE_FORMAT, "%5$.0s"),
        std::pair<eFormat, const char*>(FUNC_FORMAT, "%6$.0s"),
        std::pair<eFormat, const char*>(PID_FORMAT, "%7$.0s"),
        std::pair<eFormat, const char*>(TIME_FORMAT, "%8$.0s"),
        std::pair<eFormat, const char*>(DECIMAL_FORMAT, "%9$.0s"),
        std::pair<eFormat, const char*>(MESSAGE_FORMAT, "%10$.0s"),
        std::pair<eFormat, const char*>(TID_FORMAT, "%11$.0s")
    };
    // initialize empty format
    std::map<eFormat, const char*> idToEmptyFormat(
        idToEmptyPairs, idToEmptyPairs + sizeof(idToEmptyPairs) / sizeof(*idToEmptyPairs));
    std::map<eFormat, const char*> idToEmptyFormatWOInfo(
        idToEmptyPairs, idToEmptyPairs + sizeof(idToEmptyPairs) / sizeof(*idToEmptyPairs));
    Format ret;
    ret.origin = str;
    // transform "{:}" non escape characters
    std::string format(str);
    s_formatSerialize(format);
    std::list<std::string> formats;
    std::list<std::string> formatsWOInfo;
    // search first occurence of '{'
    std::size_t lastIndexStart = 0;
    std::size_t indexStart = format.find(LOGGER_OPEN_BRACE);
    std::size_t indexEnd;
    std::size_t indexFormat;
    while (indexStart != std::string::npos) {
        // search first occurrence of '}' after indexStart
        indexEnd = format.find(LOGGER_CLOSE_BRACE, indexStart);
        if (indexEnd == std::string::npos) {
            break;
        }
        // substr the part before key
        if (lastIndexStart - indexStart > 0) {
            std::string beforeKey = format.substr(lastIndexStart, indexStart - lastIndexStart);
            s_formatDeserialize(beforeKey);
            formats.push_back(beforeKey);
            formatsWOInfo.push_back(beforeKey);
        }
        lastIndexStart = indexEnd + 1;
        // search first occurrence of ':' after indexStart
        indexFormat = format.find(LOGGER_SEPARATOR, indexStart);
        // if ':' not found or ':' is not between '{' and '}'
        if (indexFormat == std::string::npos || indexFormat > indexEnd) {
            // get name of key {[...]}
            std::string key = format.substr(indexStart + 1, indexEnd - indexStart - 1);
            // get the id of key
            eFormat formatId = _nameToEnumFormat(key);
            if (formatId != UNKNOWN_FORMAT) {
                switch (formatId) {
                    case PID_FORMAT:
                        ret.pid = ::getpid();
                        break;
                    case TIME_FORMAT:
                        ret.time = "%x %X";
                        break;
                    case TID_FORMAT:
                        ret.threadId = ::pthread_self();
                        break;
                    default:
                        break;
                }
                formats.push_back(_idToDefaultFormat(formatId));
                if (formatId != PATH_FORMAT && formatId != FILE_FORMAT &&
                    formatId != LINE_FORMAT && formatId != FUNC_FORMAT) {
                    formatsWOInfo.push_back(_idToDefaultFormat(formatId));
                    idToEmptyFormatWOInfo.erase(formatId);
                }
                idToEmptyFormat.erase(formatId);
            }
            indexStart = format.find(LOGGER_OPEN_BRACE, indexStart + 1);
            // find other key
            continue;
        }
        else {
            // get name of key {[...]:...}
            std::string key = format.substr(indexStart + 1, indexFormat - indexStart - 1);
            // get format of key {...:[...]}
            std::string formatKey = format.substr(indexFormat + 1, indexEnd - indexFormat - 1);
            // replace no print character by real
            s_formatDeserialize(formatKey);
            // get the id of key
            eFormat formatId = _nameToEnumFormat(key);
            if (formatId != UNKNOWN_FORMAT) {
                switch (formatId) {
                    case PID_FORMAT:
                        ret.pid = ::getpid();
                        break;
                    case TID_FORMAT:
                        ret.threadId = ::pthread_self();
                        break;
                    case TIME_FORMAT:
                        ret.time = formatKey;
                        formatKey = "%s";
                        break;
                    case DECIMAL_FORMAT:
                        ret.nsecDivisor = s_getDecimalDivisor(formatKey);
                        break;
                    default:
                        break;
                }
                s_formatInsertId(formatKey, formatId);
                formats.push_back(formatKey);
                if (formatId != PATH_FORMAT && formatId != FILE_FORMAT &&
                    formatId != LINE_FORMAT && formatId != FUNC_FORMAT) {
                    formatsWOInfo.push_back(formatKey);
                    idToEmptyFormatWOInfo.erase(formatId);
                }
                idToEmptyFormat.erase(formatId);
            }
            // parse other key
            indexStart = format.find(LOGGER_OPEN_BRACE, indexStart + 1);
            continue;
        }
    }
    // get last characters in format
    if (lastIndexStart - format.size() > 0) {
        std::string lastFormat = format.substr(lastIndexStart, format.size() - lastIndexStart);
        s_formatDeserialize(lastFormat);
        formats.push_back(lastFormat);
        formatsWOInfo.push_back(lastFormat);
    }

    {
        std::list<std::string>::const_iterator cit;
        for (cit = formats.begin(); cit != formats.end(); ++cit) {
            ret.str.append(*cit);
        }
    }
    {
        std::map<eFormat, const char*>::const_iterator cit;
        for (cit = idToEmptyFormat.begin(); cit != idToEmptyFormat.end(); ++cit) {
            ret.str.append(cit->second);
        }
    }
    {
        std::list<std::string>::const_iterator cit;
        for (cit = formatsWOInfo.begin(); cit != formatsWOInfo.end(); ++cit) {
            ret.strWOInfo.append(*cit);
        }
    }
    {
        std::map<eFormat, const char*>::const_iterator cit;
        for (cit = idToEmptyFormatWOInfo.begin(); cit != idToEmptyFormatWOInfo.end(); ++cit) {
            ret.strWOInfo.append(cit->second);
        }
    }
    ret.str.append("\n");
    ret.strWOInfo.append("\n");
    return ret;
}

void Logger::setTypeFormat(const eLevel& level, const char* format) {
    pthread_mutex_lock(&_logMutex);
    _formats[level] = _formatContructor(format);
    pthread_mutex_unlock(&_logMutex);
}

void Logger::setAllFormat(const char* format) {
    pthread_mutex_lock(&_logMutex);
    for (int i = 0; i < DEBUG + 1; ++i) {
        _formats[i] = _formatContructor(format);
    }
    pthread_mutex_unlock(&_logMutex);
}

void Logger::setFILE(FILE* file) {
    _pfile = file;
}

void Logger::macroAsyncLog(eLevel level, const char* file, const char* filename, int line, const char* function,
                    const char* format, ...) {
    pthread_mutex_lock(&_logMutex);
#if LOGGER_ASYNC_WAIT_PRINT == 1
    if (_currentMessageId >= LOGGER_QUEUE_SIZE) {
        // wait end of print
        pthread_cond_wait(&_condLog, &_logMutex);
        // // std::cerr << _currentMessageId << std::endl;
        // int semValue = LOGGER_QUEUE_SIZE - LOGGER_MAX_LOG_CONCURRENCY_NB;
        // while (_isStarted && semValue >= LOGGER_QUEUE_SIZE - LOGGER_MAX_LOG_CONCURRENCY_NB) {
        //     pthread_mutex_lock(&_logMutex);
        //     sem_getvalue(&_queueSemaphore, &semValue);
        //     if (semValue == 0) {
        //         sem_post(&_queueSemaphore);
        //     }
        //     pthread_cond_wait(&_condLog, &_logMutex);
        //     pthread_mutex_unlock(&_logMutex);
        // }
    }
#endif

    if (_currentMessageId == 0) {
        sem_post(&_queueSemaphore);
    }

    // create a new message
    _messages[_currentMessageId].level = level;
    _messages[_currentMessageId].file = file;
    _messages[_currentMessageId].filename = filename;
    _messages[_currentMessageId].line = line;
    _messages[_currentMessageId].function = function;

    clock_gettime(CLOCK_REALTIME, &_messages[_currentMessageId].ts);

    // copy formated message
    va_list vargs;
    va_start(vargs, format);
    ::vsnprintf(_messages[_currentMessageId].message, LOGGER_MESSAGE_MAX_SIZE, format, vargs);
    va_end(vargs);
    // move index
    ++_currentMessageId;
#ifndef LOGGER_ASYNC_WAIT_PRINT
    _currentMessageId = _currentMessageId % LOGGER_QUEUE_SIZE;
#endif

#ifdef LOGGER_PERF_DEBUG
    ++_perf->messageCount;
#endif

    pthread_mutex_unlock(&_logMutex);
}

void Logger::asyncLog(eLevel level, const char* format, ...) {
    pthread_mutex_lock(&_logMutex);
#ifdef LOGGER_ASYNC_WAIT_PRINT
    if (_currentMessageId >= LOGGER_QUEUE_SIZE) {
        // wait end of print
        pthread_cond_wait(&_condLog, &_logMutex);
    }
#endif

    if (_currentMessageId == 0) {
        sem_post(&_queueSemaphore);
    }

    // create a new message
    _messages[_currentMessageId].level = level;
    _messages[_currentMessageId].file = NULL;
    _messages[_currentMessageId].filename = NULL;
    _messages[_currentMessageId].line = 0;
    _messages[_currentMessageId].function = NULL;

    clock_gettime(CLOCK_REALTIME, &_messages[_currentMessageId].ts);

    // copy formated message
    va_list vargs;
    va_start(vargs, format);
    ::vsnprintf(_messages[_currentMessageId].message, LOGGER_MESSAGE_MAX_SIZE, format, vargs);
    va_end(vargs);

    // move index
    ++_currentMessageId;
#ifndef LOGGER_ASYNC_WAIT_PRINT
    _currentMessageId = _currentMessageId % LOGGER_QUEUE_SIZE;
#endif

#ifdef LOGGER_PERF_DEBUG
    ++_perf->messageCount;
#endif

    pthread_mutex_unlock(&_logMutex);
}

void Logger::macroLog(eLevel level, const char* file, const char* filename, int line, const char* function,
               const char* format, ...) {
    Message message;

    // create a new message
    message.level = level;
    message.file = file;
    message.filename = filename;
    message.line = line;
    message.function = function;
    clock_gettime(CLOCK_REALTIME, &message.ts);

    // copy formated message
    va_list vargs;
    va_start(vargs, format);
    ::vsnprintf(message.message, LOGGER_MESSAGE_MAX_SIZE, format, vargs);
    va_end(vargs);

#ifdef LOGGER_PERF_DEBUG
    ++_perf->messageCount;
    ++_perf->messagePrinted;
#endif
    printMessage(message);
}

void Logger::log(eLevel level, const char* format, ...) {
    Message message;

    // create a new message
    message.level = level;
    message.file = NULL;
    message.filename = NULL;
    message.line = 0;
    message.function = NULL;
    clock_gettime(CLOCK_REALTIME, &message.ts);

    // copy formated message
    va_list vargs;
    va_start(vargs, format);
    ::vsnprintf(message.message, LOGGER_MESSAGE_MAX_SIZE, format, vargs);
    va_end(vargs);

#ifdef LOGGER_PERF_DEBUG
    ++_perf->messageCount;
    ++_perf->messagePrinted;
#endif
    printMessage(message);
}

void Logger::format(const char* format, ...) {
    pthread_mutex_lock(&_logQueue);
#ifdef LOGGER_ASYNC_WAIT_PRINT
    if (_currentMessageId >= LOGGER_QUEUE_SIZE - 1) {
        sem_post(&_queueSemaphore);
        // std::cerr << "Log Wait" << std::endl;
        while (_currentMessageId >= LOGGER_QUEUE_SIZE - 1) {
            // wait end of print
            pthread_cond_wait(&_condLog, &_logQueue);
        }
        // std::cerr << "Log UnWait" << std::endl;
        // std::cerr << "Log CurrentId:" << _currentMessageId << std::endl;
    }
#endif
    // std::cerr << "Log Wait to Lock" <<  std::endl;
    pthread_mutex_lock(&_logMutex);
    // std::cerr << "Log Locked: " << _currentMessageId << std::endl;

    if (_currentMessageId == 0) {
        sem_post(&_queueSemaphore);
    }

    clock_gettime(CLOCK_REALTIME, &_messages[_currentMessageId].ts);

    // copy formated message
    va_list vargs;
    va_start(vargs, format);
    ::vsnprintf(_messages[_currentMessageId].message, LOGGER_MESSAGE_MAX_SIZE, format, vargs);
    va_end(vargs);
}

void Logger::logFmt(eLevel level, const char* file, const char* filename, int line, const char* function) {

    // create a new message
    _messages[_currentMessageId].level = level;
    _messages[_currentMessageId].file = file;
    _messages[_currentMessageId].filename = filename;
    _messages[_currentMessageId].line = line;
    _messages[_currentMessageId].function = function;

    // move index
    ++_currentMessageId;
#ifndef LOGGER_ASYNC_WAIT_PRINT
    _currentMessageId = _currentMessageId % LOGGER_QUEUE_SIZE;
#endif

#ifdef LOGGER_PERF_DEBUG
    ++_perf->messageCount;
#endif

    pthread_mutex_unlock(&_logMutex);
    pthread_mutex_unlock(&_logQueue);
}

void Logger::logStream(eLevel level, const char* file, const char* filename, int line, const char* function, const ::std::ostringstream& oss) {
    pthread_mutex_lock(&_logQueue);
#ifdef LOGGER_ASYNC_WAIT_PRINT
    if (_currentMessageId >= LOGGER_QUEUE_SIZE - 1) {
        sem_post(&_queueSemaphore);
        // std::cerr << "Log Wait" << std::endl;
        while (_currentMessageId >= LOGGER_QUEUE_SIZE - 1) {
            // wait end of print
            pthread_cond_wait(&_condLog, &_logQueue);
        }
        // std::cerr << "Log UnWait" << std::endl;
        // std::cerr << "Log CurrentId:" << _currentMessageId << std::endl;
    }
#endif
    // std::cerr << "Log Wait to Lock" <<  std::endl;
    pthread_mutex_lock(&_logMutex);
    // std::cerr << "Log Locked: " << _currentMessageId << std::endl;

    if (_currentMessageId == 0) {
        sem_post(&_queueSemaphore);
    }

    clock_gettime(CLOCK_REALTIME, &_messages[_currentMessageId].ts);

    std::string strMessage = oss.str();
    unsigned int cpySize = LOGGER_MESSAGE_MAX_SIZE < strMessage.size() ? LOGGER_MESSAGE_MAX_SIZE : strMessage.size() + 1;
    ::memcpy(_messages[_currentMessageId].message, strMessage.c_str(), cpySize);
    if (cpySize == LOGGER_MESSAGE_MAX_SIZE) {
        _messages[_currentMessageId].message[LOGGER_MESSAGE_MAX_SIZE - 1] = '\0';
    }

    // create a new message
    _messages[_currentMessageId].level = level;
    _messages[_currentMessageId].file = file;
    _messages[_currentMessageId].filename = filename;
    _messages[_currentMessageId].line = line;
    _messages[_currentMessageId].function = function;

    // move index
    ++_currentMessageId;
#ifndef LOGGER_ASYNC_WAIT_PRINT
    _currentMessageId = _currentMessageId % LOGGER_QUEUE_SIZE;
#endif

#ifdef LOGGER_PERF_DEBUG
    ++_perf->messageCount;
#endif

    pthread_mutex_unlock(&_logMutex);
    pthread_mutex_unlock(&_logQueue);
}

Logger::eFormat Logger::_nameToEnumFormat(const std::string& name) {
    static const std::pair<std::string, eFormat> nameToFormatPairs[] = {
        std::pair<std::string, eFormat>("name", NAME_FORMAT),
        std::pair<std::string, eFormat>("level", LEVEL_FORMAT),
        std::pair<std::string, eFormat>("path", PATH_FORMAT),
        std::pair<std::string, eFormat>("file", FILE_FORMAT),
        std::pair<std::string, eFormat>("line", LINE_FORMAT),
        std::pair<std::string, eFormat>("func", FUNC_FORMAT),
        std::pair<std::string, eFormat>("pid", PID_FORMAT),
        std::pair<std::string, eFormat>("time", TIME_FORMAT),
        std::pair<std::string, eFormat>("decimal", DECIMAL_FORMAT),
        std::pair<std::string, eFormat>("message", MESSAGE_FORMAT),
        std::pair<std::string, eFormat>("tid", TID_FORMAT)
    };
    static const std::map<std::string, eFormat> nameToEnumFormat(
        nameToFormatPairs, nameToFormatPairs + sizeof(nameToFormatPairs) / sizeof(*nameToFormatPairs));

    std::map<std::string, eFormat>::const_iterator cit = nameToEnumFormat.find(name);
    if (cit == nameToEnumFormat.end()) {
        return UNKNOWN_FORMAT;
    }
    return cit->second;
}

const char* Logger::_idToDefaultFormat(const eFormat& id) {
    static const char* idToDefaultFormat[] = {
        "unknown",
        "%1$s", // name
        "%2$s", // level
        "%3$s", // path
        "%4$s", // file
        "%5$d", // line
        "%6$s", // func
        "%7$d", // pid
        "%8$s", // time
        "%9$d", // decimal
        "%10$s", // message
        "%11$d" // threadid
    };
    return idToDefaultFormat[id];
}

} // namespace blet

#undef LOGGER_CLOSE_BRACE
#undef LOGGER_SEPARATOR
#undef LOGGER_OPEN_BRACE

// "%3$.0s%6$.0s[%7$d] %8$s.%9$03d: %1$-10s:%2$-6s: %4$25s:%5$3d %10$s\n"