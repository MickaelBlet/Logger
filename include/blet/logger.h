/**
 * logger.h
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

#ifndef _BLET_LOGGER_H_
#define _BLET_LOGGER_H_

#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <stdio.h>

#include <sstream>
#include <exception>
#include <string>
#include <map>

#if defined _WIN32 || defined _WIN64 || defined __CYGWIN__
#define _LOGGER_SEPARATOR_PATH '\\'
#else
#define _LOGGER_SEPARATOR_PATH '/'
#endif

#if defined _WIN32 || defined _WIN64
#define __FUNCTION_NAME__ __FUNCTION__
#else
#define __FUNCTION_NAME__ __func__
#endif

#define _LOGGER_FILENAME (const char*)(::strrchr(__FILE__, _LOGGER_SEPARATOR_PATH) + 1)
#define _LOGGER_FILE_INFOS __FILE__, _LOGGER_FILENAME, __LINE__, __FUNCTION_NAME__

#define LOGGER_MAIN() ::blet::Logger::getMain()

#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(X) /* do nothing */
#endif
#endif

#ifdef LOGGER_VARIDIC_MACRO

#define _LOGGER_LOG_FMT(logger, level, ...) \
    logger.format(##__VA_ARGS__); \
    logger.logFmt(level, _LOGGER_FILE_INFOS)

#define LOGGER_EMERG_FMT(...) _LOGGER_LOG_FMT(LOGGER_MAIN(), blet::Logger::EMERGENCY, __VA_ARGS__)
#define LOGGER_ALERT_FMT(...) _LOGGER_LOG_FMT(LOGGER_MAIN(), blet::Logger::ALERT, __VA_ARGS__)
#define LOGGER_CRIT_FMT(...) _LOGGER_LOG_FMT(LOGGER_MAIN(), blet::Logger::CRITICAL, __VA_ARGS__)
#define LOGGER_ERROR_FMT(...) _LOGGER_LOG_FMT(LOGGER_MAIN(), blet::Logger::ERROR, __VA_ARGS__)
#define LOGGER_WARN_FMT(...) _LOGGER_LOG_FMT(LOGGER_MAIN(), blet::Logger::WARNING, __VA_ARGS__)
#define LOGGER_NOTICE_FMT(...) _LOGGER_LOG_FMT(LOGGER_MAIN(), blet::Logger::NOTICE, __VA_ARGS__)
#define LOGGER_INFO_FMT(...) _LOGGER_LOG_FMT(LOGGER_MAIN(), blet::Logger::INFO, __VA_ARGS__)
#define LOGGER_DEBUG_FMT(...) _LOGGER_LOG_FMT(LOGGER_MAIN(), blet::Logger::DEBUG, __VA_ARGS__)

#endif

/**
 * @brief Use format method for get the custom message
 */
#define _LOGGER_LOG_P(logger, level, msg) \
    logger.format msg; \
    logger.logFmt(level, _LOGGER_FILE_INFOS)

#define LOGGER_EMERG_P(parenthesis_msg) _LOGGER_LOG_P(LOGGER_MAIN(), ::blet::Logger::EMERGENCY, parenthesis_msg)
#define LOGGER_ALERT_P(parenthesis_msg) _LOGGER_LOG_P(LOGGER_MAIN(), ::blet::Logger::ALERT, parenthesis_msg)
#define LOGGER_CRIT_P(parenthesis_msg) _LOGGER_LOG_P(LOGGER_MAIN(), ::blet::Logger::CRITICAL, parenthesis_msg)
#define LOGGER_ERROR_P(parenthesis_msg) _LOGGER_LOG_P(LOGGER_MAIN(), ::blet::Logger::ERROR, parenthesis_msg)
#define LOGGER_WARN_P(parenthesis_msg) _LOGGER_LOG_P(LOGGER_MAIN(), ::blet::Logger::WARNING, parenthesis_msg)
#define LOGGER_NOTICE_P(parenthesis_msg) _LOGGER_LOG_P(LOGGER_MAIN(), ::blet::Logger::NOTICE, parenthesis_msg)
#define LOGGER_INFO_P(parenthesis_msg) _LOGGER_LOG_P(LOGGER_MAIN(), ::blet::Logger::INFO, parenthesis_msg)
#define LOGGER_DEBUG_P(parenthesis_msg) _LOGGER_LOG_P(LOGGER_MAIN(), ::blet::Logger::DEBUG, parenthesis_msg)

/**
 * @brief Use format method for get the custom message
 */
#define _LOGGER_LOG(logger, level, stream) \
    do { \
        if (logger.isPrintable(level)) { \
        ::std::ostringstream __loggerOss(""); \
        __loggerOss << stream; \
        logger.logStream(level, _LOGGER_FILE_INFOS, __loggerOss); \
    }} while(0)

#define LOGGER_EMERG(stream) _LOGGER_LOG(LOGGER_MAIN(), ::blet::Logger::EMERGENCY, stream)
#define LOGGER_ALERT(stream) _LOGGER_LOG(LOGGER_MAIN(), ::blet::Logger::ALERT, stream)
#define LOGGER_CRIT(stream) _LOGGER_LOG(LOGGER_MAIN(), ::blet::Logger::CRITICAL, stream)
#define LOGGER_ERROR(stream) _LOGGER_LOG(LOGGER_MAIN(), ::blet::Logger::ERROR, stream)
#define LOGGER_WARN(stream) _LOGGER_LOG(LOGGER_MAIN(), ::blet::Logger::WARNING, stream)
#define LOGGER_NOTICE(stream) _LOGGER_LOG(LOGGER_MAIN(), ::blet::Logger::NOTICE, stream)
#define LOGGER_INFO(stream) _LOGGER_LOG(LOGGER_MAIN(), ::blet::Logger::INFO, stream)
#define LOGGER_DEBUG(stream) _LOGGER_LOG(LOGGER_MAIN(), ::blet::Logger::DEBUG, stream)

#define LOGGER_FLUSH() LOGGER_MAIN().flush()
#define LOGGER_TO_FLUSH(logger) logger.flush()

// OPTIONS

#ifndef LOGGER_DROP_OVERFLOW
#define LOGGER_ASYNC_WAIT_PRINT 1
#else
#define LOGGER_ASYNC_WAIT_PRINT 0
#endif

#ifndef LOGGER_QUEUE_SIZE
#define LOGGER_QUEUE_SIZE 256
#endif

#ifndef LOGGER_MESSAGE_MAX_SIZE
#define LOGGER_MESSAGE_MAX_SIZE 2048
#endif

#ifndef LOGGER_DEFAULT_FORMAT
#define LOGGER_DEFAULT_FORMAT "{level:%-6s} [{pid}:{tid}] {name:%10s}: {time}.{decimal:%03d}:{file: %25s:}{line:%-3d} {message}"
#endif

// name, level, path, file, line, func, pid, time, message, microsec, millisec, nanosec

namespace blet {

class Logger {
  public:
    class Exception: public std::exception {
        public:
            Exception(const char* s1, const char* s2 = "", const char* s3 = "") {
                _str = s1;
                _str += s2;
                _str += s3;
            }
            virtual ~Exception() throw() {}
            const char* what() const throw() {
                return _str.c_str();
            }

        protected:
            std::string _str;
    };

    enum eLevel {
        EMERGENCY = LOG_EMERG,
        ALERT = LOG_ALERT,
        CRITICAL = LOG_CRIT,
        ERROR = LOG_ERR,
        WARNING = LOG_WARNING,
        NOTICE = LOG_NOTICE,
        INFO = LOG_INFO,
        DEBUG = LOG_DEBUG
    };

    struct Message {
        char message[LOGGER_MESSAGE_MAX_SIZE];
        eLevel level;
        struct timespec ts;
        const char* file;
        const char* filename;
        const char* function;
        int line;
    };

    Logger(const char* name);
    ~Logger();

    static Logger& getMain() {
        static Logger logger("main");
        return logger;
    }

    bool isPrintable(const eLevel& level) {
        return (1 << level) & _levelFilter;
    }

    void enableLevel(const eLevel& level) {
        _levelFilter |= (1 << level);
    }
    void disableLevel(const eLevel& level) {
        _levelFilter &= ~(1 << level);
    }

    void flush();

    /**
     * @brief Set format of type
     * {asctime}
     * {filename}
     * {funcname}
     * {levelname}
     * {lineno}
     * {process}
     * {message}
     * {name}
     * {pathname}
     * {thread}
     *
     * @param level
     * @param format
     */
    void setTypeFormat(const eLevel& level, const char* format);

    /**
     * @brief Set all format type.
     * keywords:
     * - name: name of logger
     * - level: level of log
     * - path: __FILE__ of log
     * - file: filename of log
     * - line: __LINE__ of log
     * - func: __func__ of log
     * - pid: process id
     * - time: datetime of log
     * - message: format message of log
     * - microsec: micro seconds
     * - millisec: milli seconds
     * - nanosec: nano seconds
     *
     * @param format C string of format.
     */
    void setAllFormat(const char* format);

    void setFILE(FILE* file);

    __attribute__((__format__(__printf__, 7, 8))) void macroAsyncLog(eLevel level, const char* file, const char* filename,
                                                                int line, const char* function,
                                                                const char* format, ...);

    __attribute__((__format__(__printf__, 7, 8))) void macroLog(eLevel level, const char* file, const char* filename,
                                                           int line, const char* function, const char* format,
                                                           ...);

    __attribute__((__format__(__printf__, 3, 4))) void asyncLog(eLevel level, const char* format, ...);

    __attribute__((__format__(__printf__, 3, 4))) void log(eLevel level, const char* format, ...);

    __attribute__((__format__(__printf__, 2, 3))) void format(const char* format, ...);

    void logFmt(eLevel level, const char* file, const char* filename, int line, const char* function);
    void logStream(eLevel level, const char* file, const char* filename, int line, const char* function, const std::ostringstream& oss);

    void printMessage(Message& message) const;

    std::string name;

  private:
    Logger(const Logger&){}; // disable copy
    Logger& operator=(const Logger&) {
        return *this;
    }; // disable copy

    struct DebugPerf;

    // format options
    struct Format {
        Format() :
        time(""),
        pid(0),
        threadId(0),
        nsecDivisor(1),
        str(""),
        origin("") {}

        std::string time;
        pid_t pid;
        pthread_t threadId;
        int nsecDivisor;

        std::string str;
        std::string strWOInfo;
        std::string origin;
    };

    enum eFormat {
        UNKNOWN_FORMAT = 0,
        NAME_FORMAT = 1,
        LEVEL_FORMAT = 2,
        PATH_FORMAT = 3,
        FILE_FORMAT = 4,
        LINE_FORMAT = 5,
        FUNC_FORMAT = 6,
        PID_FORMAT = 7,
        TIME_FORMAT = 8,
        DECIMAL_FORMAT = 9,
        MESSAGE_FORMAT = 10,
        TID_FORMAT = 11
    };

    static void* _threadLogger(void* e);
    void _threadLog();

    bool _isStarted;
    pthread_mutex_t _logMutex;
    pthread_mutex_t _logQueue;
    pthread_cond_t _condLog;
    pthread_cond_t _condFlush;
    sem_t _queueSemaphore;
    pthread_t _threadLogId;
    volatile unsigned int _currentMessageId;
    int _levelFilter;

    FILE* _pfile;
    Message* _messages;
    Message* _messagesSwap;

    static Format _formatContructor(const char* format);
    Format* _formats;

    static eFormat _nameToEnumFormat(const std::string& name);
    static const char* _idToDefaultFormat(const eFormat& id);

    DebugPerf* _perf;
};

} // namespace blet

#endif // #ifndef _BLET_LOGGER_H_