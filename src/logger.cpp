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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <list>
#include <map>
#include <string>
#include <vector>

#ifdef LOGGER_COLOR_LEVELS
#define LOGGER_EMERGENCY_FORMAT "{bg_magenta}{fg_black}" LOGGER_DEFAULT_FORMAT "{color_reset}"
#define LOGGER_ALERT_FORMAT "{fg_magenta}" LOGGER_DEFAULT_FORMAT "{color_reset}"
#define LOGGER_CRITICAL_FORMAT "{bg_red}{fg_black}" LOGGER_DEFAULT_FORMAT "{color_reset}"
#define LOGGER_ERROR_FORMAT "{fg_red}" LOGGER_DEFAULT_FORMAT "{color_reset}"
#define LOGGER_WARNING_FORMAT "{fg_yellow}" LOGGER_DEFAULT_FORMAT "{color_reset}"
#define LOGGER_NOTICE_FORMAT "{fg_cyan}" LOGGER_DEFAULT_FORMAT "{color_reset}"
#define LOGGER_INFO_FORMAT "{fg_blue}" LOGGER_DEFAULT_FORMAT "{color_reset}"
#define LOGGER_DEBUG_FORMAT "{fg_green}" LOGGER_DEFAULT_FORMAT "{color_reset}"
#endif

namespace blet {

Logger::Logger(const char* name_, unsigned int queueMaxSize, unsigned int messageMaxSize) :
    name(name_),
    _isStarted(true),
    _currentMessageId(0),
    _droppedMessageNb(0),
    _levelFilter((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7)),
    _pfile(stdout),
    _messageMaxSize(messageMaxSize),
    _queueMaxSize(queueMaxSize),
    _bufferMessages(new char[queueMaxSize * messageMaxSize]),
    _messages(new Message[queueMaxSize]),
    _messagesSwap(new Message[queueMaxSize]),
    _formats(new logger::Format[DEBUG + 1]),
    _perf(
#ifdef LOGGER_PERF_DEBUG
    new DebugPerf(name_)
#else
    NULL
#endif
    ) {
    std::cout.sync_with_stdio(false);
    for (unsigned int i = 0; i < _queueMaxSize; ++i) {
        _messages[i].message = _bufferMessages + i * _messageMaxSize;
        _messagesSwap[i].message = _bufferMessages + i * _messageMaxSize;
    }
    ::memset(&_queueMutex, 0, sizeof(_queueMutex));
    ::memset(&_logMutex, 0, sizeof(_logMutex));
    ::memset(&_condLog, 0, sizeof(_condLog));
    ::memset(&_condFlush, 0, sizeof(_condFlush));
#ifdef LOGGER_COLOR_LEVELS
    setTypeFormat(EMERGENCY, LOGGER_EMERGENCY_FORMAT);
    setTypeFormat(ALERT, LOGGER_ALERT_FORMAT);
    setTypeFormat(CRITICAL, LOGGER_CRITICAL_FORMAT);
    setTypeFormat(ERROR, LOGGER_ERROR_FORMAT);
    setTypeFormat(WARNING, LOGGER_WARNING_FORMAT);
    setTypeFormat(NOTICE, LOGGER_NOTICE_FORMAT);
    setTypeFormat(INFO, LOGGER_INFO_FORMAT);
    setTypeFormat(DEBUG, LOGGER_DEBUG_FORMAT);
#else
    // default format
    setAllFormat(LOGGER_DEFAULT_FORMAT);
#endif
    // init thread
    if (pthread_mutex_init(&_queueMutex, NULL)) {
        throw Exception("pthread_mutex_init: ", strerror(errno));
    }
    if (pthread_mutex_init(&_logMutex, NULL)) {
        throw Exception("pthread_mutex_init: ", strerror(errno));
    }
    if (pthread_mutex_init(&_droppedMutex, NULL)) {
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
    if (sem_init(&_droppedMessageSemaphore, 0, 0)) {
        throw Exception("sem_init: ", strerror(errno));
    }
    if (pthread_create(&_threadLogId, NULL, &_threadLogger, this)) {
        throw Exception("pthread_create: ", strerror(errno));
    }
}

Logger::~Logger() {
    _isStarted = false;
    // unlock thread
    sem_post(&_queueSemaphore);
    pthread_join(_threadLogId, NULL);
    sem_close(&_queueSemaphore);
    sem_destroy(&_queueSemaphore);
    sem_close(&_droppedMessageSemaphore);
    sem_destroy(&_droppedMessageSemaphore);
    pthread_cond_destroy(&_condLog);
    pthread_cond_destroy(&_condFlush);
    pthread_mutex_destroy(&_queueMutex);
    pthread_mutex_destroy(&_logMutex);
    // delete messageQueue
    delete[] _bufferMessages;
    delete[] _messages;
    delete[] _messagesSwap;
    delete[] _formats;

#ifdef LOGGER_PERF_DEBUG
    delete _perf;
#endif
}

void Logger::flush() {
    int semValue = 1;
    pthread_mutex_lock(&_queueMutex);
    while (_isStarted && semValue > 0) {
        sem_getvalue(&_queueSemaphore, &semValue);
        if (semValue == 0) {
            sem_post(&_queueSemaphore);
        }
        pthread_cond_wait(&_condFlush, &_queueMutex);
    }
    pthread_mutex_unlock(&_queueMutex);
    fflush(_pfile);
}

void* Logger::_threadLogger(void* e) {
    Logger* loggin = static_cast<Logger*>(e);
    loggin->_threadLog();
    return NULL;
}

void inline Logger::printMessage(const Logger::Message& message) {
    static const char* levelToStr[] = {"EMERG", "ALERT", "CRIT", "ERROR", "WARN", "NOTICE", "INFO", "DEBUG"};
    // static char *buffer = new char[_messageMaxSize];

    const logger::Format* format = &_formats[message.level];

    // int index = 0;

    if (message.file) {
        std::list<logger::Format::Action>::const_iterator cit;
        for (cit = format->actions.begin(); cit != format->actions.end(); ++cit) {
            switch (cit->action) {
                case logger::Format::PRINT_ACTION:
                    fprintf(_pfile, cit->format.c_str(), 0);
                    break;
                case logger::Format::NAME_ACTION:
                    fprintf(_pfile, cit->format.c_str(), name.c_str());
                    break;
                case logger::Format::LEVEL_ACTION:
                    fprintf(_pfile, cit->format.c_str(), levelToStr[message.level]);
                    break;
                case logger::Format::PATH_ACTION:
                    fprintf(_pfile, cit->format.c_str(), message.file);
                    break;
                case logger::Format::FILE_ACTION:
                    fprintf(_pfile, cit->format.c_str(), message.filename);
                    break;
                case logger::Format::LINE_ACTION:
                    fprintf(_pfile, cit->format.c_str(), message.line);
                    break;
                case logger::Format::FUNC_ACTION:
                    fprintf(_pfile, cit->format.c_str(), message.function);
                    break;
                case logger::Format::PID_ACTION:
                    fprintf(_pfile, cit->format.c_str(), format->pid);
                    break;
                case logger::Format::TIME_ACTION:
                    char ftime[128];
                    struct tm t;
                    localtime_r(&(message.ts.tv_sec), &t);
                    strftime(ftime, 128, format->time.c_str(), &t);
                    fprintf(_pfile, cit->format.c_str(), ftime);
                    break;
                case logger::Format::DECIMAL_ACTION:
                    fprintf(_pfile, cit->format.c_str(), message.ts.tv_nsec / format->nsecDivisor);
                    break;
                case logger::Format::MESSAGE_ACTION:
                    fprintf(_pfile, cit->format.c_str(), message.message);
                    break;
                case logger::Format::TID_ACTION:
                    fprintf(_pfile, cit->format.c_str(), format->threadId);
                    break;
            }
        }
    }
    else {
        std::list<logger::Format::Action>::const_iterator cit;
        for (cit = format->actions.begin(); cit != format->actions.end(); ++cit) {
            switch (cit->action) {
                case logger::Format::PRINT_ACTION:
                    fprintf(_pfile, cit->format.c_str(), 0);
                    break;
                case logger::Format::NAME_ACTION:
                    fprintf(_pfile, cit->format.c_str(), name.c_str());
                    break;
                case logger::Format::LEVEL_ACTION:
                    fprintf(_pfile, cit->format.c_str(), levelToStr[message.level]);
                    break;
                case logger::Format::PID_ACTION:
                    fprintf(_pfile, cit->format.c_str(), format->pid);
                    break;
                case logger::Format::TIME_ACTION:
                    char ftime[128];
                    struct tm t;
                    localtime_r(&(message.ts.tv_sec), &t);
                    strftime(ftime, 128, format->time.c_str(), &t);
                    fprintf(_pfile, cit->format.c_str(), ftime);
                    break;
                case logger::Format::DECIMAL_ACTION:
                    fprintf(_pfile, cit->format.c_str(), message.ts.tv_nsec / format->nsecDivisor);
                    break;
                case logger::Format::MESSAGE_ACTION:
                    fprintf(_pfile, cit->format.c_str(), message.message);
                    break;
                case logger::Format::TID_ACTION:
                    fprintf(_pfile, cit->format.c_str(), format->threadId);
                    break;
                default:
                    break;
            }
        }
    }

    // fwrite(buffer, index, 1, _pfile);
}

void Logger::_threadLog() {
    unsigned int lastMessageId;
#ifndef LOGGER_ASYNC_WAIT_PRINT
    int lastDroppedNb = 0;
    timespec tsDropped;
    ::clock_gettime(CLOCK_REALTIME, &tsDropped);
#endif
    int semQueueValue = 1;
    while (_isStarted || semQueueValue > 0) {
        sem_wait(&_queueSemaphore);
        pthread_mutex_lock(&_queueMutex);
        if (_currentMessageId == 0) {
            sem_getvalue(&_queueSemaphore, &semQueueValue);
#ifdef LOGGER_ASYNC_WAIT_PRINT
            pthread_cond_signal(&_condLog);
#endif
            pthread_cond_broadcast(&_condFlush);
            pthread_mutex_unlock(&_queueMutex);
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
#ifdef LOGGER_ASYNC_WAIT_PRINT
        pthread_cond_signal(&_condLog);
#else
        timespec currentTime;
        ::clock_gettime(CLOCK_REALTIME, &currentTime);
        if (currentTime.tv_sec > tsDropped.tv_sec) {
            tsDropped.tv_sec = currentTime.tv_sec;
            unsigned int messageDroppedCount = _droppedMessageNb - lastDroppedNb;
            if (messageDroppedCount > 0) {
                #ifdef LOGGER_PERF_DEBUG
                    _perf->messageCount += messageDroppedCount;
                #endif
                _M_logFormat("Message dropped: %u", messageDroppedCount);
                _M_logInfos(WARNING, _LOGGER_FILE_INFOS);
                // move index
                ++_currentMessageId;
                lastDroppedNb += messageDroppedCount;
            }
        }

        // sem_getvalue(&_droppedMessageSemaphore, &semDroppedValue);
        // if (semDroppedValue - lastDroppedCount > 0) {
        //     #ifdef LOGGER_PERF_DEBUG
        //         _perf->messageCount += semDroppedValue - lastDroppedCount;
        //     #endif
        //     _M_logFormat("Message dropped: %i", semDroppedValue - lastDroppedCount);
        //     _M_logInfos(WARNING, _LOGGER_FILE_INFOS);
        //     // move index
        //     ++_currentMessageId;
        //     lastDroppedCount = semDroppedValue;
        // }
#endif
        pthread_mutex_unlock(&_queueMutex);

        // call print function
        for (unsigned int i = 0; i < lastMessageId; ++i) {
            printMessage(_messagesSwap[i]);
#ifdef LOGGER_PERF_DEBUG
            ++_perf->messagePrinted;
#endif
        }

        pthread_cond_broadcast(&_condFlush);
    }

#ifndef LOGGER_ASYNC_WAIT_PRINT
    unsigned int messageDroppedCount = _droppedMessageNb - lastDroppedNb;
    if (messageDroppedCount > 0) {
        #ifdef LOGGER_PERF_DEBUG
            _perf->messageCount += messageDroppedCount;
        #endif
        _M_logFormat("Message dropped: %u", messageDroppedCount);
        _M_logInfos(WARNING, _LOGGER_FILE_INFOS);
        printMessage(_messages[0]);
    }
#endif
}

void Logger::setTypeFormat(const eLevel& level, const char* format) {
    pthread_mutex_lock(&_queueMutex);
    _formats[level] = logger::Format(name, format);
    pthread_mutex_unlock(&_queueMutex);
}

void Logger::setAllFormat(const char* format) {
    pthread_mutex_lock(&_queueMutex);
    logger::Format fmt = logger::Format(name, format);
    for (int i = 0; i < DEBUG + 1; ++i) {
        _formats[i] = fmt;
    }
    pthread_mutex_unlock(&_queueMutex);
}

void Logger::setFILE(FILE* file) {
    _pfile = file;
}

void Logger::logSync(eLevel level, const char* format, ...) {
    ::pthread_mutex_lock(&_logMutex);
    // char message[LOGGER_MESSAGE_MAX_SIZE];
    Message msg;
    msg.message = reinterpret_cast<char*>(const_cast<char*>(format));

    ::clock_gettime(CLOCK_REALTIME, &msg.ts);

    // va_list vargs;
    // va_start(vargs, format);
    // ::vsnprintf(msg.message, _messageMaxSize, format, vargs);
    // va_end(vargs);

    // create a new message
    msg.level = level;
    msg.file = NULL;
    msg.filename = NULL;
    msg.line = 0;
    msg.function = NULL;

    printMessage(msg);
    ::pthread_mutex_unlock(&_logMutex);
}

void Logger::vlog(eLevel level, const char* format, va_list& vargs) {
    ::pthread_mutex_lock(&_logMutex);
    ::pthread_mutex_lock(&_queueMutex);
#ifdef LOGGER_ASYNC_WAIT_PRINT
    if (_currentMessageId >= _queueMaxSize - 1) {
        ::pthread_cond_wait(&_condLog, &_queueMutex);
    }
#endif

    if (_currentMessageId == 0) {
        ::sem_post(&_queueSemaphore);
    }

    ::clock_gettime(CLOCK_REALTIME, &_messages[_currentMessageId].ts);

    // copy formated message
    ::vsnprintf(_messages[_currentMessageId].message, _messageMaxSize, format, vargs);

    // create a new message
    _messages[_currentMessageId].level = level;
    _messages[_currentMessageId].file = NULL;
    _messages[_currentMessageId].filename = NULL;
    _messages[_currentMessageId].line = 0;
    _messages[_currentMessageId].function = NULL;

    // move index
    ++_currentMessageId;
#ifndef LOGGER_ASYNC_WAIT_PRINT
    if (_currentMessageId == _queueMaxSize) {
        _currentMessageId = 0;
    }
#endif

#ifdef LOGGER_PERF_DEBUG
    ++_perf->messageCount;
#endif
    ::pthread_mutex_unlock(&_queueMutex);
    ::pthread_mutex_unlock(&_logMutex);
}

void Logger::log(eLevel level, const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    vlog(level, format, vargs);
    va_end(vargs);
}

void Logger::emergency(const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    vlog(EMERGENCY, format, vargs);
    va_end(vargs);
}

void Logger::alert(const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    vlog(ALERT, format, vargs);
    va_end(vargs);
}

void Logger::critical(const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    vlog(CRITICAL, format, vargs);
    va_end(vargs);
}

void Logger::error(const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    vlog(ERROR, format, vargs);
    va_end(vargs);
}

void Logger::warning(const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    vlog(WARNING, format, vargs);
    va_end(vargs);
}

void Logger::notice(const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    vlog(NOTICE, format, vargs);
    va_end(vargs);
}

void Logger::info(const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    vlog(INFO, format, vargs);
    va_end(vargs);
}

void Logger::debug(const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    vlog(DEBUG, format, vargs);
    va_end(vargs);
}

void Logger::_M_logFormat(const char* format, ...) {
    // copy formated message
    va_list vargs;
    va_start(vargs, format);
    ::vsnprintf(_messages[_currentMessageId].message, _messageMaxSize, format, vargs);
    va_end(vargs);
}

void Logger::_M_logStream(const ::std::ostringstream& oss) {
    std::string str = oss.str();
    unsigned int cpySize = _messageMaxSize < str.size() ? _messageMaxSize : str.size() + 1;
    ::memcpy(_messages[_currentMessageId].message, str.c_str(), cpySize);
    if (cpySize == _messageMaxSize) {
        _messages[_currentMessageId].message[_messageMaxSize - 1] = '\0';
    }
}

void Logger::_M_logInfos(eLevel level, const char* file, const char* filename, int line, const char* function) {
#ifdef LOGGER_PERF_DEBUG
    ++_perf->messageCount;
#endif

    ::clock_gettime(CLOCK_REALTIME, &_messages[_currentMessageId].ts);

    // create a new message
    _messages[_currentMessageId].level = level;
    _messages[_currentMessageId].file = file;
    _messages[_currentMessageId].filename = filename;
    _messages[_currentMessageId].line = line;
    _messages[_currentMessageId].function = function;

    if (_currentMessageId == 0) {
        ::sem_post(&_queueSemaphore);
    }
}

} // namespace blet