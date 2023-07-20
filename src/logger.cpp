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

#include <iostream>
#include <list>
#include <map>
#include <string>

// printf(FORMAT, level, name, path, file, line, func, pid, time, message)

#define LOGGER_OPEN_BRACE  static_cast<char>(-41)
#define LOGGER_SEPARATOR   static_cast<char>(-42)
#define LOGGER_CLOSE_BRACE static_cast<char>(-43)

namespace blet {

Logger::Logger() :
    name(""),
    _isStarted(true),
    _currentMessageId(0),
    _messages(new Message[LOGGER_QUEUE_SIZE]),
    _messagesSwap(new Message[LOGGER_QUEUE_SIZE]) {
    // default file
    _pfile = stdout;
    // default format
    setAllFormat(LOGGER_DEFAULT_FORMAT);
    // init thread
    if (pthread_mutex_init(&_logMutex, NULL)) {
        throw Exception("pthread_mutex_init: ", strerror(errno));
    }
    if (pthread_cond_init(&_condLog, NULL)) {
        throw Exception("pthread_cond_init: ", strerror(errno));
    }
    if (sem_init(&_queueSemaphore, 0, 0)) {
        throw Exception("sem_init: ", strerror(errno));
    }
    if (pthread_create(&_threadLogId, NULL, &_threadLogger, this)) {
        throw Exception("pthread_create: ", strerror(errno));
    }

#ifdef LOGGER_PERF_DEBUG
    clock_gettime(CLOCK_MONOTONIC, &_startTs);
    _messageCount = 0;
    _messagePrinted = 0;
#endif
}

Logger::~Logger() {
    _isStarted = false;
    // unlock thread
    sem_post(&_queueSemaphore);
    pthread_join(_threadLogId, NULL);
    // delete messageQueue
    delete[] _messages;
    delete[] _messagesSwap;

#ifdef LOGGER_PERF_DEBUG
    timespec endTs;
    clock_gettime(CLOCK_MONOTONIC, &endTs);
    if ((endTs.tv_nsec - _startTs.tv_nsec) < 0) {
        endTs.tv_nsec = 1000000000 + (endTs.tv_nsec - _startTs.tv_nsec);
        endTs.tv_sec -= 1;
    }
    else {
        endTs.tv_nsec = endTs.tv_nsec - _startTs.tv_nsec;
    }
    fprintf(stderr, "LOGGER_PERF %s:\n", name.c_str());
    fprintf(stderr, "- Time: %ld.%09ld\n", (endTs.tv_sec - _startTs.tv_sec), endTs.tv_nsec);
    fprintf(stderr, "- Message counted: %u\n", _messageCount);
    fprintf(stderr, "- Message printed: %u\n", _messagePrinted);
    fprintf(stderr, "- Message rate: %f\n", _messagePrinted / ((endTs.tv_sec - _startTs.tv_sec) * 1000000000.0 + endTs.tv_nsec) * 1000000000);
    fflush(stderr);
#endif
}

void Logger::flush() {
    int semValue = 1;
    while (_isStarted && semValue > 0) {
        pthread_mutex_lock(&_logMutex);
        sem_getvalue(&_queueSemaphore, &semValue);
        if (semValue == 0) {
            sem_post(&_queueSemaphore);
        }
        pthread_cond_wait(&_condLog, &_logMutex);
        pthread_mutex_unlock(&_logMutex);
    }
    fflush(_pfile);
}

void* Logger::_threadLogger(void* e) {
    Logger* loggin = static_cast<Logger*>(e);
    loggin->_threadLog();
    return NULL;
}

void Logger::printMessage(Logger::Message& message) const {
    static char ftime[128];

    const char* strLevel = NULL;
    const Format* format = &_format;
    switch (message.level) {
        case EMERGENCY:
            strLevel = "EMERG";
            format = &_emergencyFormat;
            break;
        case ALERT:
            strLevel = "ALERT";
            format = &_alertFormat;
            break;
        case CRITICAL:
            strLevel = "CRIT";
            format = &_criticalFormat;
            break;
        case ERROR:
            strLevel = "ERROR";
            format = &_errorFormat;
            break;
        case WARNING:
            strLevel = "WARN";
            format = &_warningFormat;
            break;
        case NOTICE:
            strLevel = "NOTICE";
            format = &_noticeFormat;
            break;
        case INFO:
            strLevel = "INFO";
            format = &_infoFormat;
            break;
        case DEBUG:
            strLevel = "DEBUG";
            format = &_debugFormat;
            break;
    }

    if (format->hasTime) {
        struct tm t;
        localtime_r(&(message.ts.tv_sec), &t);
        strftime(ftime, 128, format->time.c_str(), &t);
    }
    else {
        ftime[0] = '\0';
    }

    fprintf(_pfile, format->str.c_str(),
            name.c_str(),
            strLevel,
            message.file,
            message.filename,
            message.line,
            message.function,
            format->pid,
            ftime,
            message.message,
            message.ts.tv_nsec / format->nsecDivisor);
}

void Logger::_threadLog() {
    unsigned int lastMessageId;
    int semValue = 1;
    while (_isStarted || semValue > 0) {
        sem_wait(&_queueSemaphore);
        pthread_mutex_lock(&_logMutex);
        if (_currentMessageId == 0) {
            sem_getvalue(&_queueSemaphore, &semValue);
            pthread_mutex_unlock(&_logMutex);
            pthread_cond_signal(&_condLog);
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
        pthread_mutex_unlock(&_logMutex);

        // call print function
        for (unsigned int i = 0; i < lastMessageId; ++i) {
            printMessage(_messagesSwap[i]);
#ifdef LOGGER_PERF_DEBUG
            ++_messagePrinted;
#endif
        }

        pthread_cond_broadcast(&_condLog);
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

Logger::Format Logger::_formatContructor(const char* str) {
    // initialize default format
    std::map<std::string, std::string> emptyFormats;
    emptyFormats["name"] = "%1$.0s";
    emptyFormats["level"] = "%2$.0s";
    emptyFormats["path"] = "%3$.0s";
    emptyFormats["file"] = "%4$.0s";
    emptyFormats["line"] = "%5$.0s";
    emptyFormats["func"] = "%6$.0s";
    emptyFormats["pid"] = "%7$.0s";
    emptyFormats["time"] = "%8$.0s";
    emptyFormats["message"] = "%9$.0s";
    emptyFormats["decimal"] = "%10$.0s";
    std::map<std::string, std::string> defaultFormats;
    defaultFormats["name"] = "%1$s";
    defaultFormats["level"] = "%2$s";
    defaultFormats["path"] = "%3$s";
    defaultFormats["file"] = "%4$s";
    defaultFormats["line"] = "%5$d";
    defaultFormats["func"] = "%6$s";
    defaultFormats["pid"] = "%7$d";
    defaultFormats["time"] = "%8$s";
    defaultFormats["message"] = "%9$s";
    defaultFormats["microsec"] = "%10$d";
    defaultFormats["millisec"] = "%10$d";
    defaultFormats["nanosec"] = "%10$d";
    std::map<std::string, std::string> keyToid;
    keyToid["name"] = "1$";
    keyToid["level"] = "2$";
    keyToid["path"] = "3$";
    keyToid["file"] = "4$";
    keyToid["line"] = "5$";
    keyToid["func"] = "6$";
    keyToid["pid"] = "7$";
    keyToid["time"] = "8$";
    keyToid["message"] = "9$";
    keyToid["microsec"] = "10$";
    keyToid["millisec"] = "10$";
    keyToid["nanosec"] = "10$";

    Format ret;
    ret.origin = str;
    // transform "{:}" non escape characters
    std::string format(str);
    s_formatSerialize(format);
    std::list<std::string> formats;
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
        }
        lastIndexStart = indexEnd + 1;
        // search first occurrence of ':' after indexStart
        indexFormat = format.find(LOGGER_SEPARATOR, indexStart);
        // if ':' not found or ':' is not between '{' and '}'
        if (indexFormat == std::string::npos || indexFormat > indexEnd) {
            // get name of key
            std::string key = format.substr(indexStart + 1, indexEnd - indexStart - 1);
            if (key == "name") {
                formats.push_back(defaultFormats.at("name"));
                emptyFormats.erase("name");
            }
            else if (key == "level") {
                formats.push_back(defaultFormats.at("level"));
                emptyFormats.erase("level");
            }
            else if (key == "path") {
                formats.push_back(defaultFormats.at("path"));
                emptyFormats.erase("path");
            }
            else if (key == "file") {
                formats.push_back(defaultFormats.at("file"));
                emptyFormats.erase("file");
            }
            else if (key == "line") {
                ret.hasLine = true;
                formats.push_back(defaultFormats.at("line"));
                emptyFormats.erase("line");
            }
            else if (key == "func") {
                formats.push_back(defaultFormats.at("func"));
                emptyFormats.erase("func");
            }
            else if (key == "pid") {
                ret.hasPid = true;
                ret.pid = ::getpid();
                formats.push_back(defaultFormats.at("pid"));
                emptyFormats.erase("pid");
            }
            else if (key == "time") {
                ret.hasTime = true;
                ret.time = "%x %X";
                formats.push_back(defaultFormats.at("time"));
                emptyFormats.erase("time");
            }
            else if (key == "message") {
                formats.push_back(defaultFormats.at("message"));
                emptyFormats.erase("message");
            }
            else if (key == "microsec") {
                formats.push_back(defaultFormats.at("microsec"));
                emptyFormats.erase("decimal");
                ret.nsecDivisor = 1000000;
            }
            else if (key == "millisec") {
                formats.push_back(defaultFormats.at("millisec"));
                emptyFormats.erase("decimal");
                ret.nsecDivisor = 1000;
            }
            else if (key == "nanosec") {
                formats.push_back(defaultFormats.at("nanosec"));
                emptyFormats.erase("decimal");
                ret.nsecDivisor = 1;
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
            if (key == "name") {
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("name"));
                formats.push_back(formatKey);
                emptyFormats.erase("name");
            }
            else if (key == "level") {
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("level"));
                formats.push_back(formatKey);
                emptyFormats.erase("level");
            }
            else if (key == "path") {
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("path"));
                formats.push_back(formatKey);
                emptyFormats.erase("path");
            }
            else if (key == "file") {
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("file"));
                formats.push_back(formatKey);
                emptyFormats.erase("file");
            }
            else if (key == "line") {
                ret.hasLine = true;
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("line"));
                formats.push_back(formatKey);
                emptyFormats.erase("line");
            }
            else if (key == "func") {
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("func"));
                formats.push_back(formatKey);
                emptyFormats.erase("func");
            }
            else if (key == "pid") {
                ret.hasPid = true;
                ret.pid = ::getpid();
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("pid"));
                formats.push_back(formatKey);
                emptyFormats.erase("pid");
            }
            else if (key == "time") {
                ret.hasTime = true;
                ret.time = formatKey;
                formats.push_back("%" + keyToid.at("time") + "s");
                emptyFormats.erase("time");
            }
            else if (key == "message") {
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("message"));
                formats.push_back(formatKey);
                emptyFormats.erase("message");
            }
            else if (key == "microsec") {
                ret.hasMicroSec = true;
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("microsec"));
                formats.push_back(formatKey);
                emptyFormats.erase("decimal");
                ret.nsecDivisor = 1000000;
            }
            else if (key == "millisec") {
                ret.hasMilliSec = true;
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("millisec"));
                formats.push_back(formatKey);
                emptyFormats.erase("decimal");
                ret.nsecDivisor = 1000;
            }
            else if (key == "nanosec") {
                ret.hasNanoSec = true;
                formatKey.insert(formatKey.find('%') + 1, keyToid.at("nanosec"));
                formats.push_back(formatKey);
                emptyFormats.erase("decimal");
                ret.nsecDivisor = 1;
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
    }

    {
        std::map<std::string, std::string>::const_iterator cit;
        for (cit = emptyFormats.begin(); cit != emptyFormats.end(); ++cit) {
            ret.str.append(cit->second);
        }
    }
    {
        std::list<std::string>::const_iterator cit;
        for (cit = formats.begin(); cit != formats.end(); ++cit) {
            ret.str.append(*cit);
        }
    }
    ret.str.append("\n");
    return ret;
}

void Logger::setTypeFormat(const eLevel& level, const char* format) {
    switch (level) {
        case EMERGENCY:
            _emergencyFormat = _formatContructor(format);
            break;
        case ALERT:
            _alertFormat = _formatContructor(format);
            break;
        case CRITICAL:
            _criticalFormat = _formatContructor(format);
            break;
        case ERROR:
            _errorFormat = _formatContructor(format);
            break;
        case WARNING:
            _warningFormat = _formatContructor(format);
            break;
        case NOTICE:
            _noticeFormat = _formatContructor(format);
            break;
        case INFO:
            _infoFormat = _formatContructor(format);
            break;
        case DEBUG:
            _debugFormat = _formatContructor(format);
            break;
    }
}

void Logger::setAllFormat(const char* format) {
    _format = _formatContructor(format);
    _emergencyFormat = _format;
    _alertFormat = _format;
    _criticalFormat = _format;
    _errorFormat = _format;
    _warningFormat = _format;
    _noticeFormat = _format;
    _infoFormat = _format;
    _debugFormat = _format;
    std::cerr << _format.str << std::endl;
}

void Logger::setFILE(FILE* file) {
    _pfile = file;
}

void Logger::asyncLog(eLevel level, const char* file, const char* filename, int line, const char* function,
                    const char* format, ...) {
    pthread_mutex_lock(&_logMutex);
#ifdef LOGGER_ASYNC_WAIT_PRINT
    if (_currentMessageId >= LOGGER_QUEUE_SIZE - LOGGER_MAX_LOG_THREAD_NB) {
        // wait end of print
        pthread_cond_wait(&_condLog, &_logMutex);
    }
#endif

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

    sem_post(&_queueSemaphore);

#ifdef LOGGER_PERF_DEBUG
    ++_messageCount;
#endif

    pthread_mutex_unlock(&_logMutex);
}

void Logger::log(eLevel level, const char* file, const char* filename, int line, const char* function,
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
    ++_messageCount;
    ++_messagePrinted;
#endif
    printMessage(message);
}

} // namespace blet

#undef LOGGER_CLOSE_BRACE
#undef LOGGER_SEPARATOR
#undef LOGGER_OPEN_BRACE