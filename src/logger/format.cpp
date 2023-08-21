#include "blet/logger/format.h"

#include <unistd.h>
#include <stdio.h>

#include <iostream>
#include <list>
#include <map>
#include <sstream>

#define LOGGER_OPEN_BRACE static_cast<char>(-41)
#define LOGGER_SEPARATOR static_cast<char>(-42)
#define LOGGER_CLOSE_BRACE static_cast<char>(-43)

#ifndef LOGGER_DEFAULT_TIME_FORMAT
#define LOGGER_DEFAULT_TIME_FORMAT "%x %X"
#endif

namespace blet {

namespace logger {

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

static void s_escapePercent(std::string& str) {
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%') {
            str.insert(i, "%");
            ++i;
        }
    }
}

static Format::eAction s_nameToEnumAction(const std::string& name) {
    static const std::pair<std::string, Format::eAction> nameToActionPairs[] = {
        std::pair<std::string, Format::eAction>("name", Format::NAME_ACTION),
        std::pair<std::string, Format::eAction>("level", Format::LEVEL_ACTION),
        std::pair<std::string, Format::eAction>("path", Format::PATH_ACTION),
        std::pair<std::string, Format::eAction>("file", Format::FILE_ACTION),
        std::pair<std::string, Format::eAction>("line", Format::LINE_ACTION),
        std::pair<std::string, Format::eAction>("func", Format::FUNC_ACTION),
        std::pair<std::string, Format::eAction>("pid", Format::PID_ACTION),
        std::pair<std::string, Format::eAction>("time", Format::TIME_ACTION),
        std::pair<std::string, Format::eAction>("decimal", Format::DECIMAL_ACTION),
        std::pair<std::string, Format::eAction>("message", Format::MESSAGE_ACTION),
        std::pair<std::string, Format::eAction>("tid", Format::TID_ACTION)};
    static const std::map<std::string, Format::eAction> nameToEnumAction(
        nameToActionPairs, nameToActionPairs + sizeof(nameToActionPairs) / sizeof(*nameToActionPairs));

    std::map<std::string, Format::eAction>::const_iterator cit = nameToEnumAction.find(name);
    if (cit == nameToEnumAction.end()) {
        return Format::PRINT_ACTION;
    }
    return cit->second;
}

static const char* s_idToDefaultFormat(const Format::eAction& id) {
    static const char* idToDefaultFormat[] = {
        "unknown",
        "%s", // name
        "%s", // level
        "%s", // path
        "%s", // file
        "%d", // line
        "%s", // func
        "%d", // pid
        "%s", // time
        "%d", // decimal
        "%s", // message
        "%X"  // threadid
    };
    return idToDefaultFormat[id];
}

static int s_getDecimalDivisor(const std::string& format) {
    int value = 1;
    std::size_t percentPos = format.find('%');
    if (percentPos != format.npos) {
        int log_10;
        std::stringstream ss("");
        ss << format.c_str() + percentPos + 1;
        ss >> log_10;
        if (log_10 < 0) {
            log_10 = -log_10;
        }
        log_10 = 9 - log_10;
        for (int i = 0; i < log_10; ++i) {
            value *= 10;
        }
    }
    return value;
}

static const char* s_nameToColorFormat(const std::string& name) {
    static const std::pair<std::string, const char*> nameToColorPairs[] = {
        std::pair<std::string, const char*>("color_reset", "\033[0m"),
        std::pair<std::string, const char*>("color_bold", "\033[1m"),
        std::pair<std::string, const char*>("color_dim", "\033[2m"),
        std::pair<std::string, const char*>("color_italic", "\033[3m"),
        std::pair<std::string, const char*>("color_underline", "\033[4m"),
        std::pair<std::string, const char*>("color_blink", "\033[5m"),
        std::pair<std::string, const char*>("color_rev", "\033[7m"),
        std::pair<std::string, const char*>("color_hide", "\033[8m"),
        std::pair<std::string, const char*>("fg_black", "\033[30m"),
        std::pair<std::string, const char*>("fg_red", "\033[31m"),
        std::pair<std::string, const char*>("fg_green", "\033[32m"),
        std::pair<std::string, const char*>("fg_yellow", "\033[33m"),
        std::pair<std::string, const char*>("fg_blue", "\033[34m"),
        std::pair<std::string, const char*>("fg_magenta", "\033[35m"),
        std::pair<std::string, const char*>("fg_cyan", "\033[36m"),
        std::pair<std::string, const char*>("fg_white", "\033[37m"),
        std::pair<std::string, const char*>("bg_black", "\033[40m"),
        std::pair<std::string, const char*>("bg_red", "\033[41m"),
        std::pair<std::string, const char*>("bg_green", "\033[42m"),
        std::pair<std::string, const char*>("bg_yellow", "\033[43m"),
        std::pair<std::string, const char*>("bg_blue", "\033[44m"),
        std::pair<std::string, const char*>("bg_magenta", "\033[45m"),
        std::pair<std::string, const char*>("bg_cyan", "\033[46m"),
        std::pair<std::string, const char*>("bg_white", "\033[47m")};
    static const std::map<std::string, const char*> nameToColorFormat(
        nameToColorPairs, nameToColorPairs + sizeof(nameToColorPairs) / sizeof(*nameToColorPairs));

    const char* ret = "";
    std::map<std::string, const char*>::const_iterator cit = nameToColorFormat.find(name);
    if (cit != nameToColorFormat.end()) {
        ret = cit->second;
    }
    return ret;
}

Format::Format() :
    time(""),
    pid(0),
    threadId(0),
    nsecDivisor(1)
    {}

Format::Format(const std::string& loggerName, const char* format) :
    time(""),
    pid(0),
    threadId(0),
    nsecDivisor(1)
    {
    std::string sFormat(format);
    // transform "{:}" non escape characters
    s_formatSerialize(sFormat);
    // search first occurence of '{'
    std::size_t lastIndexStart = 0;
    std::size_t indexStart = sFormat.find(LOGGER_OPEN_BRACE);
    std::size_t indexEnd;
    std::size_t indexFormat;
    while (indexStart != std::string::npos) {
        // search first occurrence of '}' after indexStart
        indexEnd = sFormat.find(LOGGER_CLOSE_BRACE, indexStart);
        if (indexEnd == std::string::npos) {
            break;
        }
        // substr the part before key
        if (lastIndexStart - indexStart > 0) {
            std::string beforeKey = sFormat.substr(lastIndexStart, indexStart - lastIndexStart);
            s_formatDeserialize(beforeKey);
            s_escapePercent(beforeKey);
            actions.push_back(Action(PRINT_ACTION, beforeKey));
        }
        lastIndexStart = indexEnd + 1;
        // search first occurrence of ':' after indexStart
        indexFormat = sFormat.find(LOGGER_SEPARATOR, indexStart);
        // if ':' not found or ':' is not between '{' and '}'
        if (indexFormat == std::string::npos || indexFormat > indexEnd) {
            // get name of key {[...]}
            std::string key = sFormat.substr(indexStart + 1, indexEnd - indexStart - 1);
            // get the id of key
            eAction actionId = s_nameToEnumAction(key);
            if (actionId != PRINT_ACTION) {
                switch (actionId) {
                    case PID_ACTION:
                        pid = ::getpid();
                        break;
                    case TID_ACTION:
                        threadId = ::pthread_self();
                        break;
                    case TIME_ACTION:
                        time = LOGGER_DEFAULT_TIME_FORMAT;
                        break;
                    default:
                        break;
                }
                actions.push_back(Action(actionId, s_idToDefaultFormat(actionId)));
            }
            else {
                // check if color
                const char* color = s_nameToColorFormat(key);
                actions.push_back(Action(PRINT_ACTION, color));
            }
            indexStart = sFormat.find(LOGGER_OPEN_BRACE, indexStart + 1);
            // find other key
            continue;
        }
        else {
            // get name of key {[...]:...}
            std::string key = sFormat.substr(indexStart + 1, indexFormat - indexStart - 1);
            // get format of key {...:[...]}
            std::string formatKey = sFormat.substr(indexFormat + 1, indexEnd - indexFormat - 1);
            // replace no print character by real
            s_formatDeserialize(formatKey);
            // get the id of key
            eAction actionId = s_nameToEnumAction(key);
            if (actionId != PRINT_ACTION) {
                switch (actionId) {
                    case PID_ACTION:
                        pid = ::getpid();
                        break;
                    case TID_ACTION:
                        threadId = ::pthread_self();
                        break;
                    case TIME_ACTION:
                        time = formatKey;
                        formatKey = "%s";
                        break;
                    case DECIMAL_ACTION:
                        nsecDivisor = s_getDecimalDivisor(formatKey);
                        break;
                    default:
                        break;
                }
                actions.push_back(Action(actionId, formatKey));
            }
            else {
                s_escapePercent(formatKey);
                actions.push_back(Action(PRINT_ACTION, formatKey));
            }
            // parse other key
            indexStart = sFormat.find(LOGGER_OPEN_BRACE, indexStart + 1);
            continue;
        }
    }
    // get last characters in format
    if (lastIndexStart - sFormat.size() > 0) {
        std::string lastFormat = sFormat.substr(lastIndexStart, sFormat.size() - lastIndexStart);
        s_formatDeserialize(lastFormat);
        s_escapePercent(lastFormat);
        actions.push_back(Action(PRINT_ACTION, lastFormat));
    }
    // search if message is exist
    std::list<Action>::iterator it;
    for (it = actions.begin(); it != actions.end(); ++it) {
        if (it->action == MESSAGE_ACTION) {
            break;
        }
    }
    // add message action if not exist
    if (it == actions.end()) {
        actions.push_back(Action(MESSAGE_ACTION, s_idToDefaultFormat(MESSAGE_ACTION)));
    }
    // add \n on last action
    actions.push_back(Action(PRINT_ACTION, "\n"));
    // compress actions
    it = actions.begin();
    while (it != actions.end()) {
        // replace action by string
        if (it->action == NAME_ACTION ||
            it->action == PID_ACTION ||
            it->action == TID_ACTION) {
            char buffer[128];
            switch (it->action) {
                case NAME_ACTION:
                    ::snprintf(buffer, sizeof(buffer), it->format.c_str(), loggerName.c_str());
                    break;
                case PID_ACTION:
                    ::snprintf(buffer, sizeof(buffer), it->format.c_str(), pid);
                    break;
                case TID_ACTION:
                    ::snprintf(buffer, sizeof(buffer), it->format.c_str(), threadId);
                    break;
                default:
                    break;
            }
            std::string str(buffer);
            s_escapePercent(str);
            it->action = PRINT_ACTION;
            it->format = str;
        }
        std::list<Action>::iterator prev = it;
        ++it;
        if (prev->action == PRINT_ACTION) {
            // concat with next if exist
            if (it != actions.end()) {
                it->format = prev->format + it->format;
                it = actions.erase(prev);
            }
            // concat with prev
            else {
                --it;
                --it;
                std::list<Action>::iterator prev = it;
                ++it;
                prev->format = prev->format + it->format;
                it = actions.erase(it);
            }
        }
    }
}

Format::~Format() {}

} // namespace logger

} // namespace blet

#undef LOGGER_OPEN_BRACE
#undef LOGGER_SEPARATOR
#undef LOGGER_CLOSE_BRACE