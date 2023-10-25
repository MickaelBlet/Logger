/**
 * logger/format.h
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

#ifndef _BLET_LOGGER_FORMAT_H_
#define _BLET_LOGGER_FORMAT_H_

#include <pthread.h>

#include <list>
#include <string>

namespace blet {

namespace logger {

struct Format {
    enum eAction {
        PRINT_ACTION = 0,
        NAME_ACTION = 1,
        LEVEL_ACTION = 2,
        PATH_ACTION = 3,
        FILE_ACTION = 4,
        LINE_ACTION = 5,
        FUNC_ACTION = 6,
        PID_ACTION = 7,
        TID_ACTION = 8,
        TIME_ACTION = 9,
        DECIMAL_ACTION = 10,
        MESSAGE_ACTION = 11
    };

    Format();
    Format(const std::string& loggerName, const char* format);
    ~Format();

    std::string time;
    pid_t pid;
    pthread_t threadId;
    int nsecDivisor;

    struct Action {
        Action(eAction action_, const std::string& format_) :
            action(action_),
            format(format_) {}
        eAction action;
        std::string format;
    };

    std::list<Action> actions;
};

} // namespace logger

} // namespace blet

#endif // #ifndef _BLET_LOGGER_FORMAT_H_