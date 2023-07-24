# Logger

Printf logger C++98 library

## Quickstart
```cpp
#include "blet/logger.h"

int main(int /*argc*/, char** /*argv*/) {
    LOGGER_MAIN().setAllFormat("{name}: {message}");
    LOGGER_INFO("Start");
    LOGGER_DEBUG("Hello" << " world");
    LOGGER_DEBUG("Hello " << 42 << " world");
// if LOGGER_VARIDIC_MACRO is defined
#ifdef LOGGER_VARIDIC_MACRO
    LOGGER_WARN_FMT("%s %s", "Hello", "world");
#else
    // if pedantic c++98
    LOGGER_WARN_FMT_P(("%s %s", "Hello", "world"));
#endif
    LOGGER_INFO("End");
    return 0;
}
```

## Format
|Keyword|Default|Description|
|-|-|-|
|name|%s|Name|
|level|%s|String level|
|path|%s|Path of file|
|file|%s|Filename of file|
|line|%d|Line of file|
|func|%s|Function name of file|
|pid|%d|Process id|
|tid|%d|Thread id|
|time|%x %X|Strftime|
|decimal|%d|Decimal time|
|message|%s|Message|