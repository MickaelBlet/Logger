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

#include <exception>
#include <string>

#define LOGGER_FILENAME (const char*)(::strrchr(__FILE__, '/') + 1)
#define LOGGER_MAIN() blet::Logger::getMain()

#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(X) /* do nothing */
#endif
#endif

#define LOGGER_ASYNC(logger, type, ...) \
    logger.asyncLog(type, \
                    __FILE__, \
                    LOGGER_FILENAME, \
                    __LINE__, \
                    __func__, \
                    ##__VA_ARGS__)

#define LOGGER_LOG(logger, type, ...) \
    logger.log(type, \
               __FILE__, \
               LOGGER_FILENAME, \
               __LINE__, \
               __func__, \
               ##__VA_ARGS__)

#ifdef LOGGER_SYNC
#define _LOGGER_LOG(...) LOGGER_LOG(__VA_ARGS__)
#else
#define _LOGGER_LOG(...) LOGGER_ASYNC(__VA_ARGS__)
#endif

#define LOGGER_EMERG(...) _LOGGER_LOG(LOGGER_MAIN(), blet::Logger::EMERGENCY, __VA_ARGS__)
#define LOGGER_ALERT(...) _LOGGER_LOG(LOGGER_MAIN(), blet::Logger::ALERT, __VA_ARGS__)
#define LOGGER_CRIT(...) _LOGGER_LOG(LOGGER_MAIN(), blet::Logger::CRITICAL, __VA_ARGS__)
#define LOGGER_ERROR(...) _LOGGER_LOG(LOGGER_MAIN(), blet::Logger::ERROR, __VA_ARGS__)
#define LOGGER_WARN(...) _LOGGER_LOG(LOGGER_MAIN(), blet::Logger::WARNING, __VA_ARGS__)
#define LOGGER_NOTICE(...) _LOGGER_LOG(LOGGER_MAIN(), blet::Logger::NOTICE, __VA_ARGS__)
#define LOGGER_INFO(...) _LOGGER_LOG(LOGGER_MAIN(), blet::Logger::INFO, __VA_ARGS__)
#define LOGGER_DEBUG(...) _LOGGER_LOG(LOGGER_MAIN(), blet::Logger::DEBUG, __VA_ARGS__)

#define LOGGER_TO_DEBUG(logger, ...) _LOGGER_LOG(logger, blet::Logger::DEBUG, __VA_ARGS__)
#define LOGGER_TO_INFO(logger, ...) _LOGGER_LOG(logger, blet::Logger::INFO, __VA_ARGS__)
#define LOGGER_TO_NOTICE(logger, ...) _LOGGER_LOG(logger, blet::Logger::NOTICE, __VA_ARGS__)
#define LOGGER_TO_WARN(logger, ...) _LOGGER_LOG(logger, blet::Logger::WARNING, __VA_ARGS__)
#define LOGGER_TO_ERR(logger, ...) _LOGGER_LOG(logger, blet::Logger::ERROR, __VA_ARGS__)
#define LOGGER_TO_CRIT(logger, ...) _LOGGER_LOG(logger, blet::Logger::CRITICAL, __VA_ARGS__)
#define LOGGER_TO_ALERT(logger, ...) _LOGGER_LOG(logger, blet::Logger::ALERT, __VA_ARGS__)
#define LOGGER_TO_EMERG(logger, ...) _LOGGER_LOG(logger, blet::Logger::EMERGENCY, __VA_ARGS__)

#define LOGGER_FLUSH() LOGGER_MAIN().flush()
#define LOGGER_TO_FLUSH(logger) logger.flush()

// OPTIONS

#ifndef LOGGER_ASYNC_DROP_OVERFLOW
#define LOGGER_ASYNC_WAIT_PRINT 1
#endif

#ifndef LOGGER_QUEUE_SIZE
#define LOGGER_QUEUE_SIZE 256
#endif

#ifndef LOGGER_MESSAGE_MAX_SIZE
#define LOGGER_MESSAGE_MAX_SIZE 2048
#endif

#ifndef LOGGER_MAX_LOG_THREAD_NB
#define LOGGER_MAX_LOG_THREAD_NB 20
#endif

#ifndef LOGGER_DEFAULT_FORMAT
#define LOGGER_DEFAULT_FORMAT "[{pid}] {name:%-10s}:{level:%-6s}: {path}:{line} {message}"
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
        eLevel level;
        const char* file;
        const char* filename;
        int line;
        const char* function;
        struct timespec ts;
        char message[LOGGER_MESSAGE_MAX_SIZE];
    };

    Logger();
    ~Logger();

    static Logger& getMain() {
        static Logger logger;
        logger.setName("main");
        return logger;
    }

    void setName(const char* name_) {
        name = name_;
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

    __attribute__((__format__(__printf__, 7, 8))) void asyncLog(eLevel level, const char* file, const char* filename,
                                                                int line, const char* function,
                                                                const char* format, ...);

    __attribute__((__format__(__printf__, 7, 8))) void log(eLevel level, const char* file, const char* filename,
                                                           int line, const char* function, const char* format,
                                                           ...);

    void printMessage(Message& message) const;

    std::string name;

  private:
    Logger(const Logger&){}; // disable copy
    Logger& operator=(const Logger&) {
        return *this;
    }; // disable copy

    static void* _threadLogger(void* e);
    void _threadLog();

    bool _isStarted;
    pthread_mutex_t _logMutex;
    pthread_cond_t _condLog;
    sem_t _queueSemaphore;
    pthread_t _threadLogId;
    unsigned int _currentMessageId;

    FILE* _pfile;
    Message* _messages;
    Message* _messagesSwap;

    // format options
    struct Format {
        Format() :
        hasTime(false),
        hasLine(false),
        hasPid(false),
        hasThread(false),
        hasMicroSec(false),
        hasMilliSec(false),
        hasNanoSec(false),
        time(""),
        pid(0),
        threadId(0),
        nsecDivisor(1),
        str(""),
        origin("") {}

        bool hasTime;
        bool hasLine;
        bool hasPid;
        bool hasThread;
        bool hasMicroSec;
        bool hasMilliSec;
        bool hasNanoSec;

        std::string time;
        pid_t pid;
        pthread_t threadId;
        int nsecDivisor;

        std::string str;
        std::string origin;
    };

    static Format _formatContructor(const char* format);
    Format _format;
    Format _emergencyFormat;
    Format _alertFormat;
    Format _criticalFormat;
    Format _errorFormat;
    Format _warningFormat;
    Format _noticeFormat;
    Format _infoFormat;
    Format _debugFormat;

#ifdef LOGGER_PERF_DEBUG
    ::timespec _startTs;
    unsigned int _messageCount;
    unsigned int _messagePrinted;
#endif
};

} // namespace blet

#endif // #ifndef _BLET_LOGGER_H_