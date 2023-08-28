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
// if LOGGER_VARIADIC_MACRO is defined
#ifdef LOGGER_VARIADIC_MACRO
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

### Color
|Keyword|Value|Description|
|-|-|-|
|color_reset|"\033[0m"|reset all SGR effects to their default|
|color_bold|"\033[1m"|bold or increased intensity|
|color_dim|"\033[2m"|diminute or decreased intensity|
|color_italic|"\033[3m"|italic mode|
|color_underline|"\033[4m"|singly underlined|
|color_blink|"\033[5m"|slow blink|
|color_rev|"\033[7m"|reverse color|
|color_hide|"\033[8m"|hide foreground|
|fg_black|"\033[30m"|foreground color BLACK|
|fg_red|"\033[31m"|foreground color RED|
|fg_green|"\033[32m"|foreground color GREEN|
|fg_yellow|"\033[33m"|foreground color YELLOW|
|fg_blue|"\033[34m"|foreground color BLUE|
|fg_magenta|"\033[35m"|foreground color MAGENTA|
|fg_cyan|"\033[36m"|foreground color CYAN|
|fg_white|"\033[37m"|foreground color WHITE|
|bg_black|"\033[40m"|background color BLACK|
|bg_red|"\033[41m"|background color RED|
|bg_green|"\033[42m"|background color GREEN|
|bg_yellow|"\033[43m"|background color YELLOW|
|bg_blue|"\033[44m"|background color BLUE|
|bg_magenta|"\033[45m"|background color MAGENTA|
|bg_cyan|"\033[46m"|background color CYAN|
|bg_white|"\033[47m"|background color WHITE|